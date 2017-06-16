// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_video_capture_device.h"

#include "base/bind_helpers.h"
#include "base/debug/debugger.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/media/capture/web_contents_capture_util.h"
#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/base/yuv_convert.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/layout.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/screen.h"

namespace content {
namespace {

const int kTestWidth = 320;
const int kTestHeight = 240;
const int kTestFramesPerSecond = 20;
const float kTestDeviceScaleFactor = 2.0f;
const SkColor kNothingYet = 0xdeadbeef;
const SkColor kNotInterested = ~kNothingYet;

void DeadlineExceeded(base::Closure quit_closure) {
  if (!base::debug::BeingDebugged()) {
    quit_closure.Run();
    FAIL() << "Deadline exceeded while waiting, quitting";
  } else {
    LOG(WARNING) << "Deadline exceeded; test would fail if debugger weren't "
                 << "attached.";
  }
}

void RunCurrentLoopWithDeadline() {
  base::Timer deadline(false, false);
  deadline.Start(FROM_HERE, TestTimeouts::action_max_timeout(), base::Bind(
      &DeadlineExceeded, base::MessageLoop::current()->QuitClosure()));
  base::MessageLoop::current()->Run();
  deadline.Stop();
}

SkColor ConvertRgbToYuv(SkColor rgb) {
  uint8 yuv[3];
  media::ConvertRGB32ToYUV(reinterpret_cast<uint8*>(&rgb),
                           yuv, yuv + 1, yuv + 2, 1, 1, 1, 1, 1);
  return SkColorSetRGB(yuv[0], yuv[1], yuv[2]);
}

// Thread-safe class that controls the source pattern to be captured by the
// system under test. The lifetime of this class is greater than the lifetime
// of all objects that reference it, so it does not need to be reference
// counted.
class CaptureTestSourceController {
 public:
  CaptureTestSourceController()
      : color_(SK_ColorMAGENTA),
        copy_result_size_(kTestWidth, kTestHeight),
        can_copy_to_video_frame_(false),
        use_frame_subscriber_(false) {}

  void SetSolidColor(SkColor color) {
    base::AutoLock guard(lock_);
    color_ = color;
  }

  SkColor GetSolidColor() {
    base::AutoLock guard(lock_);
    return color_;
  }

  void SetCopyResultSize(int width, int height) {
    base::AutoLock guard(lock_);
    copy_result_size_ = gfx::Size(width, height);
  }

  gfx::Size GetCopyResultSize() {
    base::AutoLock guard(lock_);
    return copy_result_size_;
  }

  void SignalCopy() {
    // TODO(nick): This actually should always be happening on the UI thread.
    base::AutoLock guard(lock_);
    if (!copy_done_.is_null()) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, copy_done_);
      copy_done_.Reset();
    }
  }

  void SetCanCopyToVideoFrame(bool value) {
    base::AutoLock guard(lock_);
    can_copy_to_video_frame_ = value;
  }

  bool CanCopyToVideoFrame() {
    base::AutoLock guard(lock_);
    return can_copy_to_video_frame_;
  }

  void SetUseFrameSubscriber(bool value) {
    base::AutoLock guard(lock_);
    use_frame_subscriber_ = value;
  }

  bool CanUseFrameSubscriber() {
    base::AutoLock guard(lock_);
    return use_frame_subscriber_;
  }

  void WaitForNextCopy() {
    {
      base::AutoLock guard(lock_);
      copy_done_ = base::MessageLoop::current()->QuitClosure();
    }

    RunCurrentLoopWithDeadline();
  }

 private:
  base::Lock lock_;  // Guards changes to all members.
  SkColor color_;
  gfx::Size copy_result_size_;
  bool can_copy_to_video_frame_;
  bool use_frame_subscriber_;
  base::Closure copy_done_;

  DISALLOW_COPY_AND_ASSIGN(CaptureTestSourceController);
};

// A stub implementation which returns solid-color bitmaps in calls to
// CopyFromCompositingSurfaceToVideoFrame(), and which allows the video-frame
// readback path to be switched on and off. The behavior is controlled by a
// CaptureTestSourceController.
class CaptureTestView : public TestRenderWidgetHostView {
 public:
  explicit CaptureTestView(RenderWidgetHostImpl* rwh,
                           CaptureTestSourceController* controller)
      : TestRenderWidgetHostView(rwh),
        controller_(controller),
        fake_bounds_(100, 100, 100 + kTestWidth, 100 + kTestHeight) {}

  ~CaptureTestView() override {}

  // TestRenderWidgetHostView overrides.
  gfx::Rect GetViewBounds() const override {
    return fake_bounds_;
  }

  void SetSize(const gfx::Size& size) override {
    SetBounds(gfx::Rect(fake_bounds_.origin(), size));
  }

  void SetBounds(const gfx::Rect& rect) override {
    fake_bounds_ = rect;
  }

  bool CanCopyToVideoFrame() const override {
    return controller_->CanCopyToVideoFrame();
  }

  void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) override {
    SkColor c = ConvertRgbToYuv(controller_->GetSolidColor());
    media::FillYUV(
        target.get(), SkColorGetR(c), SkColorGetG(c), SkColorGetB(c));
    callback.Run(true);
    controller_->SignalCopy();
  }

  void BeginFrameSubscription(
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) override {
    subscriber_.reset(subscriber.release());
  }

  void EndFrameSubscription() override { subscriber_.reset(); }

  // Simulate a compositor paint event for our subscriber.
  void SimulateUpdate() {
    const base::TimeTicks present_time = base::TimeTicks::Now();
    RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback callback;
    scoped_refptr<media::VideoFrame> target;
    if (subscriber_ && subscriber_->ShouldCaptureFrame(
            gfx::Rect(), present_time, &target, &callback)) {
      SkColor c = ConvertRgbToYuv(controller_->GetSolidColor());
      media::FillYUV(
          target.get(), SkColorGetR(c), SkColorGetG(c), SkColorGetB(c));
      BrowserThread::PostTask(BrowserThread::UI,
                              FROM_HERE,
                              base::Bind(callback, present_time, true));
      controller_->SignalCopy();
    }
  }

 private:
  scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber_;
  CaptureTestSourceController* const controller_;
  gfx::Rect fake_bounds_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CaptureTestView);
};

#if defined(COMPILER_MSVC)
// MSVC warns on diamond inheritance. See comment for same warning on
// RenderViewHostImpl.
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

// A stub implementation which returns solid-color bitmaps in calls to
// CopyFromBackingStore(). The behavior is controlled by a
// CaptureTestSourceController.
class CaptureTestRenderViewHost : public TestRenderViewHost {
 public:
  CaptureTestRenderViewHost(SiteInstance* instance,
                            RenderViewHostDelegate* delegate,
                            RenderWidgetHostDelegate* widget_delegate,
                            int routing_id,
                            int main_frame_routing_id,
                            bool swapped_out,
                            CaptureTestSourceController* controller)
      : TestRenderViewHost(instance, delegate, widget_delegate, routing_id,
                           main_frame_routing_id, swapped_out),
        controller_(controller) {
    // Override the default view installed by TestRenderViewHost; we need
    // our special subclass which has mocked-out tab capture support.
    RenderWidgetHostView* old_view = GetView();
    SetView(new CaptureTestView(this, controller));
    delete old_view;
  }

  // TestRenderViewHost overrides.
  void CopyFromBackingStore(const gfx::Rect& src_rect,
                            const gfx::Size& accelerated_dst_size,
                            ReadbackRequestCallback& callback,
                            const SkColorType color_type) override {
    gfx::Size size = controller_->GetCopyResultSize();
    SkColor color = controller_->GetSolidColor();

    // Although it's not necessary, use a PlatformBitmap here (instead of a
    // regular SkBitmap) to exercise possible threading issues.
    skia::PlatformBitmap output;
    EXPECT_TRUE(output.Allocate(size.width(), size.height(), false));
    {
      SkAutoLockPixels locker(output.GetBitmap());
      output.GetBitmap().eraseColor(color);
    }
    callback.Run(output.GetBitmap(), content::READBACK_SUCCESS);
    controller_->SignalCopy();
  }

 private:
  CaptureTestSourceController* controller_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CaptureTestRenderViewHost);
};

#if defined(COMPILER_MSVC)
// Re-enable warning 4250
#pragma warning(pop)
#endif

class CaptureTestRenderViewHostFactory : public RenderViewHostFactory {
 public:
  explicit CaptureTestRenderViewHostFactory(
      CaptureTestSourceController* controller) : controller_(controller) {
    RegisterFactory(this);
  }

  ~CaptureTestRenderViewHostFactory() override { UnregisterFactory(); }

  // RenderViewHostFactory implementation.
  RenderViewHost* CreateRenderViewHost(
      SiteInstance* instance,
      RenderViewHostDelegate* delegate,
      RenderWidgetHostDelegate* widget_delegate,
      int routing_id,
      int main_frame_routing_id,
      bool swapped_out) override {
    return new CaptureTestRenderViewHost(instance, delegate, widget_delegate,
                                         routing_id, main_frame_routing_id,
                                         swapped_out, controller_);
  }
 private:
  CaptureTestSourceController* controller_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CaptureTestRenderViewHostFactory);
};

// A stub consumer of captured video frames, which checks the output of
// WebContentsVideoCaptureDevice.
class StubClient : public media::VideoCaptureDevice::Client {
 public:
  StubClient(
      const base::Callback<void(SkColor, const gfx::Size&)>& report_callback,
      const base::Closure& error_callback)
      : report_callback_(report_callback),
        error_callback_(error_callback) {
    buffer_pool_ = new VideoCaptureBufferPool(2);
  }
  ~StubClient() override {}

  MOCK_METHOD5(OnIncomingCapturedData,
               void(const uint8* data,
                    int length,
                    const media::VideoCaptureFormat& frame_format,
                    int rotation,
                    const base::TimeTicks& timestamp));
  MOCK_METHOD9(OnIncomingCapturedYuvData,
               void (const uint8* y_data,
                     const uint8* u_data,
                     const uint8* v_data,
                     size_t y_stride,
                     size_t u_stride,
                     size_t v_stride,
                     const media::VideoCaptureFormat& frame_format,
                     int clockwise_rotation,
                     const base::TimeTicks& timestamp));

  MOCK_METHOD0(DoOnIncomingCapturedBuffer, void(void));

  scoped_ptr<media::VideoCaptureDevice::Client::Buffer> ReserveOutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelFormat format,
      media::VideoPixelStorage storage) override {
    CHECK_EQ(format, media::PIXEL_FORMAT_I420);
    int buffer_id_to_drop = VideoCaptureBufferPool::kInvalidId;  // Ignored.
    const int buffer_id = buffer_pool_->ReserveForProducer(
        format, storage, dimensions, &buffer_id_to_drop);
    if (buffer_id == VideoCaptureBufferPool::kInvalidId)
      return NULL;

    return scoped_ptr<media::VideoCaptureDevice::Client::Buffer>(
        new AutoReleaseBuffer(
            buffer_pool_, buffer_pool_->GetBufferHandle(buffer_id), buffer_id));
  }
  // Trampoline method to workaround GMOCK problems with scoped_ptr<>.
  void OnIncomingCapturedBuffer(scoped_ptr<Buffer> buffer,
                                const media::VideoCaptureFormat& frame_format,
                                const base::TimeTicks& timestamp) override {
    DoOnIncomingCapturedBuffer();
  }

  void OnIncomingCapturedVideoFrame(
      scoped_ptr<Buffer> buffer,
      const scoped_refptr<media::VideoFrame>& frame,
      const base::TimeTicks& timestamp) override {
    EXPECT_FALSE(frame->visible_rect().IsEmpty());
    EXPECT_EQ(media::VideoFrame::I420, frame->format());
    double frame_rate = 0;
    EXPECT_TRUE(
        frame->metadata()->GetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                     &frame_rate));
    EXPECT_EQ(kTestFramesPerSecond, frame_rate);

    // TODO(miu): We just look at the center pixel presently, because if the
    // analysis is too slow, the backlog of frames will grow without bound and
    // trouble erupts. http://crbug.com/174519
    using media::VideoFrame;
    const gfx::Point center = frame->visible_rect().CenterPoint();
    const int center_offset_y =
        (frame->stride(VideoFrame::kYPlane) * center.y()) + center.x();
    const int center_offset_uv =
        (frame->stride(VideoFrame::kUPlane) * (center.y() / 2)) +
            (center.x() / 2);
    report_callback_.Run(
        SkColorSetRGB(frame->data(VideoFrame::kYPlane)[center_offset_y],
                      frame->data(VideoFrame::kUPlane)[center_offset_uv],
                      frame->data(VideoFrame::kVPlane)[center_offset_uv]),
        frame->visible_rect().size());
  }

  void OnError(const std::string& reason) override { error_callback_.Run(); }

  double GetBufferPoolUtilization() const override { return 0.0; }

 private:
  class AutoReleaseBuffer : public media::VideoCaptureDevice::Client::Buffer {
   public:
    AutoReleaseBuffer(
        const scoped_refptr<VideoCaptureBufferPool>& pool,
        scoped_ptr<VideoCaptureBufferPool::BufferHandle> buffer_handle,
        int buffer_id)
        : id_(buffer_id),
          pool_(pool),
          buffer_handle_(buffer_handle.Pass()) {
      DCHECK(pool_.get());
    }
    int id() const override { return id_; }
    size_t size() const override { return buffer_handle_->size(); }
    void* data() override { return buffer_handle_->data(); }
    ClientBuffer AsClientBuffer() override { return nullptr; }
#if defined(OS_POSIX)
    base::FileDescriptor AsPlatformFile() override {
      return base::FileDescriptor();
    }
#endif

   private:
    ~AutoReleaseBuffer() override { pool_->RelinquishProducerReservation(id_); }

    const int id_;
    const scoped_refptr<VideoCaptureBufferPool> pool_;
    const scoped_ptr<VideoCaptureBufferPool::BufferHandle> buffer_handle_;
  };

  scoped_refptr<VideoCaptureBufferPool> buffer_pool_;
  base::Callback<void(SkColor, const gfx::Size&)> report_callback_;
  base::Closure error_callback_;

  DISALLOW_COPY_AND_ASSIGN(StubClient);
};

class StubClientObserver {
 public:
  StubClientObserver()
      : error_encountered_(false),
        wait_color_yuv_(0xcafe1950),
        wait_size_(kTestWidth, kTestHeight) {
    client_.reset(new StubClient(
        base::Bind(&StubClientObserver::DidDeliverFrame,
                   base::Unretained(this)),
        base::Bind(&StubClientObserver::OnError, base::Unretained(this))));
  }

  virtual ~StubClientObserver() {}

  scoped_ptr<media::VideoCaptureDevice::Client> PassClient() {
    return client_.Pass();
  }

  void QuitIfConditionsMet(SkColor color, const gfx::Size& size) {
    base::AutoLock guard(lock_);
    if (error_encountered_)
      base::MessageLoop::current()->Quit();
    else if (wait_color_yuv_ == color && wait_size_.IsEmpty())
      base::MessageLoop::current()->Quit();
    else if (wait_color_yuv_ == color && wait_size_ == size)
      base::MessageLoop::current()->Quit();
  }

  // Run the current loop until a frame is delivered with the |expected_color|
  // and any non-empty frame size.
  void WaitForNextColor(SkColor expected_color) {
    WaitForNextColorAndFrameSize(expected_color, gfx::Size());
  }

  // Run the current loop until a frame is delivered with the |expected_color|
  // and is of the |expected_size|.
  void WaitForNextColorAndFrameSize(SkColor expected_color,
                                    const gfx::Size& expected_size) {
    {
      base::AutoLock guard(lock_);
      wait_color_yuv_ = ConvertRgbToYuv(expected_color);
      wait_size_ = expected_size;
      error_encountered_ = false;
    }
    RunCurrentLoopWithDeadline();
    {
      base::AutoLock guard(lock_);
      ASSERT_FALSE(error_encountered_);
    }
  }

  void WaitForError() {
    {
      base::AutoLock guard(lock_);
      wait_color_yuv_ = kNotInterested;
      wait_size_ = gfx::Size();
      error_encountered_ = false;
    }
    RunCurrentLoopWithDeadline();
    {
      base::AutoLock guard(lock_);
      ASSERT_TRUE(error_encountered_);
    }
  }

  bool HasError() {
    base::AutoLock guard(lock_);
    return error_encountered_;
  }

  void OnError() {
    {
      base::AutoLock guard(lock_);
      error_encountered_ = true;
    }
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &StubClientObserver::QuitIfConditionsMet,
        base::Unretained(this),
        kNothingYet,
        gfx::Size()));
  }

  void DidDeliverFrame(SkColor color, const gfx::Size& size) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &StubClientObserver::QuitIfConditionsMet,
        base::Unretained(this),
        color,
        size));
  }

 private:
  base::Lock lock_;
  bool error_encountered_;
  SkColor wait_color_yuv_;
  gfx::Size wait_size_;
  scoped_ptr<StubClient> client_;

  DISALLOW_COPY_AND_ASSIGN(StubClientObserver);
};

// A dummy implementation of gfx::Screen, since WebContentsVideoCaptureDevice
// needs access to a gfx::Display's device scale factor.
class FakeScreen : public gfx::Screen {
 public:
  FakeScreen() : the_one_display_(0x1337, gfx::Rect(0, 0, 2560, 1440)) {
    the_one_display_.set_device_scale_factor(kTestDeviceScaleFactor);
  }
  ~FakeScreen() override {}

  // gfx::Screen implementation (only what's needed for testing).
  gfx::Point GetCursorScreenPoint() override { return gfx::Point(); }
  gfx::NativeWindow GetWindowUnderCursor() override { return NULL; }
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    return NULL;
  }
  int GetNumDisplays() const override { return 1; }
  std::vector<gfx::Display> GetAllDisplays() const override {
    return std::vector<gfx::Display>(1, the_one_display_);
  }
  gfx::Display GetDisplayNearestWindow(gfx::NativeView view) const override {
    return the_one_display_;
  }
  gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const override {
    return the_one_display_;
  }
  gfx::Display GetDisplayMatching(const gfx::Rect& match_rect) const override {
    return the_one_display_;
  }
  gfx::Display GetPrimaryDisplay() const override { return the_one_display_; }
  void AddObserver(gfx::DisplayObserver* observer) override {}
  void RemoveObserver(gfx::DisplayObserver* observer) override {}

 private:
  gfx::Display the_one_display_;

  DISALLOW_COPY_AND_ASSIGN(FakeScreen);
};

// Test harness that sets up a minimal environment with necessary stubs.
class WebContentsVideoCaptureDeviceTest : public testing::Test {
 public:
  // This is public because C++ method pointer scoping rules are silly and make
  // this hard to use with Bind().
  void ResetWebContents() {
    web_contents_.reset();
  }

 protected:
  void SetUp() override {
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, &fake_screen_);
    ASSERT_EQ(&fake_screen_, gfx::Screen::GetNativeScreen());

    // TODO(nick): Sadness and woe! Much "mock-the-world" boilerplate could be
    // eliminated here, if only we could use RenderViewHostTestHarness. The
    // catch is that we need our TestRenderViewHost to support a
    // CopyFromBackingStore operation that we control. To accomplish that,
    // either RenderViewHostTestHarness would have to support installing a
    // custom RenderViewHostFactory, or else we implant some kind of delegated
    // CopyFromBackingStore functionality into TestRenderViewHost itself.

    render_process_host_factory_.reset(new MockRenderProcessHostFactory());
    // Create our (self-registering) RVH factory, so that when we create a
    // WebContents, it in turn creates CaptureTestRenderViewHosts.
    render_view_host_factory_.reset(
        new CaptureTestRenderViewHostFactory(&controller_));

    browser_context_.reset(new TestBrowserContext());

    scoped_refptr<SiteInstance> site_instance =
        SiteInstance::Create(browser_context_.get());
    SiteInstanceImpl::set_render_process_host_factory(
        render_process_host_factory_.get());
    web_contents_.reset(
        TestWebContents::Create(browser_context_.get(), site_instance.get()));
    RenderFrameHost* const main_frame = web_contents_->GetMainFrame();
    device_.reset(WebContentsVideoCaptureDevice::Create(
        base::StringPrintf("web-contents-media-stream://%d:%d",
                           main_frame->GetProcess()->GetID(),
                           main_frame->GetRoutingID())));

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // Tear down in opposite order of set-up.

    // The device is destroyed asynchronously, and will notify the
    // CaptureTestSourceController when it finishes destruction.
    // Trigger this, and wait.
    if (device_) {
      device_->StopAndDeAllocate();
      device_.reset();
    }

    base::RunLoop().RunUntilIdle();

    // Destroy the browser objects.
    web_contents_.reset();
    browser_context_.reset();

    base::RunLoop().RunUntilIdle();

    SiteInstanceImpl::set_render_process_host_factory(NULL);
    render_view_host_factory_.reset();
    render_process_host_factory_.reset();

    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, NULL);
  }

  // Accessors.
  CaptureTestSourceController* source() { return &controller_; }
  WebContents* web_contents() const { return web_contents_.get(); }
  media::VideoCaptureDevice* device() { return device_.get(); }

  // Returns the device scale factor of the capture target's native view.  This
  // is necessary because, architecturally, the FakeScreen implementation is
  // ignored on Mac platforms (when determining the device scale factor for a
  // particular window).
  float GetDeviceScaleFactor() const {
    RenderWidgetHostView* const view =
        web_contents_->GetRenderViewHost()->GetView();
    CHECK(view);
    return ui::GetScaleFactorForNativeView(view->GetNativeView());
  }

  void SimulateDrawEvent() {
    if (source()->CanUseFrameSubscriber()) {
      // Print
      CaptureTestView* test_view = static_cast<CaptureTestView*>(
          web_contents_->GetRenderViewHost()->GetView());
      test_view->SimulateUpdate();
    } else {
      // Simulate a non-accelerated paint.
      NotificationService::current()->Notify(
          NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
          Source<RenderWidgetHost>(web_contents_->GetRenderViewHost()),
          NotificationService::NoDetails());
    }
  }

  void SimulateSourceSizeChange(const gfx::Size& size) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    CaptureTestView* test_view = static_cast<CaptureTestView*>(
        web_contents_->GetRenderViewHost()->GetView());
    test_view->SetSize(size);
    // Normally, RenderWidgetHostImpl would notify WebContentsImpl that the size
    // has changed.  However, in this test setup where there is no render
    // process, we must notify WebContentsImpl directly.
    WebContentsImpl* const as_web_contents_impl =
        static_cast<WebContentsImpl*>(web_contents_.get());
    RenderWidgetHostDelegate* const as_rwh_delegate =
        static_cast<RenderWidgetHostDelegate*>(as_web_contents_impl);
    as_rwh_delegate->RenderWidgetWasResized(
        as_web_contents_impl->GetMainFrame()->GetRenderWidgetHost(), true);
  }

  void DestroyVideoCaptureDevice() { device_.reset(); }

  StubClientObserver* client_observer() {
    return &client_observer_;
  }

 private:
  FakeScreen fake_screen_;

  StubClientObserver client_observer_;

  // The controller controls which pixel patterns to produce.
  CaptureTestSourceController controller_;

  // Self-registering RenderProcessHostFactory.
  scoped_ptr<MockRenderProcessHostFactory> render_process_host_factory_;

  // Creates capture-capable RenderViewHosts whose pixel content production is
  // under the control of |controller_|.
  scoped_ptr<CaptureTestRenderViewHostFactory> render_view_host_factory_;

  // A mocked-out browser and tab.
  scoped_ptr<TestBrowserContext> browser_context_;
  scoped_ptr<WebContents> web_contents_;

  // Finally, the WebContentsVideoCaptureDevice under test.
  scoped_ptr<media::VideoCaptureDevice> device_;

  TestBrowserThreadBundle thread_bundle_;
};

TEST_F(WebContentsVideoCaptureDeviceTest, InvalidInitialWebContentsError) {
  // Before the installs itself on the UI thread up to start capturing, we'll
  // delete the web contents. This should trigger an error which can happen in
  // practice; we should be able to recover gracefully.
  ResetWebContents();

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  device()->AllocateAndStart(capture_params, client_observer()->PassClient());
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForError());
  device()->StopAndDeAllocate();
}

TEST_F(WebContentsVideoCaptureDeviceTest, WebContentsDestroyed) {
  const float device_scale_factor = GetDeviceScaleFactor();
  const gfx::Size capture_preferred_size(
      static_cast<int>(kTestWidth / device_scale_factor),
      static_cast<int>(kTestHeight / device_scale_factor));
  ASSERT_NE(capture_preferred_size, web_contents()->GetPreferredSize());

  // We'll simulate the tab being closed after the capture pipeline is up and
  // running.
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  device()->AllocateAndStart(capture_params, client_observer()->PassClient());
  // Do one capture to prove
  source()->SetSolidColor(SK_ColorRED);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorRED));

  base::RunLoop().RunUntilIdle();

  // Check that the preferred size of the WebContents matches the one provided
  // by WebContentsVideoCaptureDevice.
  EXPECT_EQ(capture_preferred_size, web_contents()->GetPreferredSize());

  // Post a task to close the tab. We should see an error reported to the
  // consumer.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&WebContentsVideoCaptureDeviceTest::ResetWebContents,
                 base::Unretained(this)));
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForError());
  device()->StopAndDeAllocate();
}

TEST_F(WebContentsVideoCaptureDeviceTest,
       StopDeviceBeforeCaptureMachineCreation) {
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  device()->AllocateAndStart(capture_params, client_observer()->PassClient());

  // Make a point of not running the UI messageloop here.
  device()->StopAndDeAllocate();
  DestroyVideoCaptureDevice();

  // Currently, there should be CreateCaptureMachineOnUIThread() and
  // DestroyCaptureMachineOnUIThread() tasks pending on the current (UI) message
  // loop. These should both succeed without crashing, and the machine should
  // wind up in the idle state.
  base::RunLoop().RunUntilIdle();
}

TEST_F(WebContentsVideoCaptureDeviceTest, StopWithRendererWorkToDo) {
  // Set up the test to use RGB copies and an normal
  source()->SetCanCopyToVideoFrame(false);
  source()->SetUseFrameSubscriber(false);
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  device()->AllocateAndStart(capture_params, client_observer()->PassClient());

  base::RunLoop().RunUntilIdle();

  for (int i = 0; i < 10; ++i)
    SimulateDrawEvent();

  ASSERT_FALSE(client_observer()->HasError());
  device()->StopAndDeAllocate();
  ASSERT_FALSE(client_observer()->HasError());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(client_observer()->HasError());
}

TEST_F(WebContentsVideoCaptureDeviceTest, DeviceRestart) {
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  device()->AllocateAndStart(capture_params, client_observer()->PassClient());
  base::RunLoop().RunUntilIdle();
  source()->SetSolidColor(SK_ColorRED);
  SimulateDrawEvent();
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorRED));
  SimulateDrawEvent();
  SimulateDrawEvent();
  source()->SetSolidColor(SK_ColorGREEN);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorGREEN));
  device()->StopAndDeAllocate();

  // Device is stopped, but content can still be animating.
  SimulateDrawEvent();
  SimulateDrawEvent();
  base::RunLoop().RunUntilIdle();

  StubClientObserver observer2;
  device()->AllocateAndStart(capture_params, observer2.PassClient());
  source()->SetSolidColor(SK_ColorBLUE);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(observer2.WaitForNextColor(SK_ColorBLUE));
  source()->SetSolidColor(SK_ColorYELLOW);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(observer2.WaitForNextColor(SK_ColorYELLOW));
  device()->StopAndDeAllocate();
}

// The "happy case" test.  No scaling is needed, so we should be able to change
// the picture emitted from the source and expect to see each delivered to the
// consumer. The test will alternate between the three capture paths, simulating
// falling in and out of accelerated compositing.
TEST_F(WebContentsVideoCaptureDeviceTest, GoesThroughAllTheMotions) {
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  device()->AllocateAndStart(capture_params, client_observer()->PassClient());

  for (int i = 0; i < 6; i++) {
    const char* name = NULL;
    switch (i % 3) {
      case 0:
        source()->SetCanCopyToVideoFrame(true);
        source()->SetUseFrameSubscriber(false);
        name = "VideoFrame";
        break;
      case 1:
        source()->SetCanCopyToVideoFrame(false);
        source()->SetUseFrameSubscriber(true);
        name = "Subscriber";
        break;
      case 2:
        source()->SetCanCopyToVideoFrame(false);
        source()->SetUseFrameSubscriber(false);
        name = "SkBitmap";
        break;
      default:
        FAIL();
    }

    SCOPED_TRACE(base::StringPrintf("Using %s path, iteration #%d", name, i));

    source()->SetSolidColor(SK_ColorRED);
    SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorRED));

    source()->SetSolidColor(SK_ColorGREEN);
    SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorGREEN));

    source()->SetSolidColor(SK_ColorBLUE);
    SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorBLUE));

    source()->SetSolidColor(SK_ColorBLACK);
    SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorBLACK));
  }
  device()->StopAndDeAllocate();
}

TEST_F(WebContentsVideoCaptureDeviceTest, BadFramesGoodFrames) {
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  // 1x1 is too small to process; we intend for this to result in an error.
  source()->SetCopyResultSize(1, 1);
  source()->SetSolidColor(SK_ColorRED);
  device()->AllocateAndStart(capture_params, client_observer()->PassClient());

  // These frames ought to be dropped during the Render stage. Let
  // several captures to happen.
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());

  // Now push some good frames through; they should be processed normally.
  source()->SetCopyResultSize(kTestWidth, kTestHeight);
  source()->SetSolidColor(SK_ColorGREEN);
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorGREEN));
  source()->SetSolidColor(SK_ColorRED);
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorRED));

  device()->StopAndDeAllocate();
}

// Tests that, when configured with the FIXED_ASPECT_RATIO resolution change
// policy, the source size changes result in video frames of possibly varying
// resolutions, but all with the same aspect ratio.
TEST_F(WebContentsVideoCaptureDeviceTest, VariableResolution_FixedAspectRatio) {
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.resolution_change_policy =
      media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO;

  device()->AllocateAndStart(capture_params, client_observer()->PassClient());

  source()->SetUseFrameSubscriber(true);

  // Source size equals maximum size.  Expect delivered frames to be
  // kTestWidth by kTestHeight.
  source()->SetSolidColor(SK_ColorRED);
  const float device_scale_factor = GetDeviceScaleFactor();
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(
      device_scale_factor, gfx::Size(kTestWidth, kTestHeight)));
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColorAndFrameSize(
      SK_ColorRED, gfx::Size(kTestWidth, kTestHeight)));

  // Source size is half in both dimensions.  Expect delivered frames to be of
  // the same aspect ratio as kTestWidth by kTestHeight, but larger than the
  // half size because the minimum height is 180 lines.
  source()->SetSolidColor(SK_ColorGREEN);
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(
      device_scale_factor, gfx::Size(kTestWidth / 2, kTestHeight / 2)));
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColorAndFrameSize(
      SK_ColorGREEN, gfx::Size(180 * kTestWidth / kTestHeight, 180)));

  // Source size changes aspect ratio.  Expect delivered frames to be padded
  // in the horizontal dimension to preserve aspect ratio.
  source()->SetSolidColor(SK_ColorBLUE);
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(
      device_scale_factor, gfx::Size(kTestWidth / 2, kTestHeight)));
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColorAndFrameSize(
      SK_ColorBLUE, gfx::Size(kTestWidth, kTestHeight)));

  // Source size changes aspect ratio again.  Expect delivered frames to be
  // padded in the vertical dimension to preserve aspect ratio.
  source()->SetSolidColor(SK_ColorBLACK);
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(
      device_scale_factor, gfx::Size(kTestWidth, kTestHeight / 2)));
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColorAndFrameSize(
      SK_ColorBLACK, gfx::Size(kTestWidth, kTestHeight)));

  device()->StopAndDeAllocate();
}

// Tests that, when configured with the ANY_WITHIN_LIMIT resolution change
// policy, the source size changes result in video frames of possibly varying
// resolutions.
TEST_F(WebContentsVideoCaptureDeviceTest, VariableResolution_AnyWithinLimits) {
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.resolution_change_policy =
      media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT;

  device()->AllocateAndStart(capture_params, client_observer()->PassClient());

  source()->SetUseFrameSubscriber(true);

  // Source size equals maximum size.  Expect delivered frames to be
  // kTestWidth by kTestHeight.
  source()->SetSolidColor(SK_ColorRED);
  const float device_scale_factor = GetDeviceScaleFactor();
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(
      device_scale_factor, gfx::Size(kTestWidth, kTestHeight)));
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColorAndFrameSize(
      SK_ColorRED, gfx::Size(kTestWidth, kTestHeight)));

  // Source size is half in both dimensions.  Expect delivered frames to also
  // be half in both dimensions.
  source()->SetSolidColor(SK_ColorGREEN);
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(
      device_scale_factor, gfx::Size(kTestWidth / 2, kTestHeight / 2)));
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColorAndFrameSize(
      SK_ColorGREEN, gfx::Size(kTestWidth / 2, kTestHeight / 2)));

  // Source size changes to something arbitrary.  Since the source size is
  // less than the maximum size, expect delivered frames to be the same size
  // as the source size.
  source()->SetSolidColor(SK_ColorBLUE);
  gfx::Size arbitrary_source_size(kTestWidth / 2 + 42, kTestHeight - 10);
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(device_scale_factor,
                                                 arbitrary_source_size));
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColorAndFrameSize(
      SK_ColorBLUE, arbitrary_source_size));

  // Source size changes to something arbitrary that exceeds the maximum frame
  // size.  Since the source size exceeds the maximum size, expect delivered
  // frames to be downscaled.
  source()->SetSolidColor(SK_ColorBLACK);
  arbitrary_source_size = gfx::Size(kTestWidth * 2, kTestHeight / 2);
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(device_scale_factor,
                                                 arbitrary_source_size));
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColorAndFrameSize(
      SK_ColorBLACK, gfx::Size(kTestWidth,
                               kTestWidth * arbitrary_source_size.height() /
                                   arbitrary_source_size.width())));

  device()->StopAndDeAllocate();
}

}  // namespace
}  // namespace content
