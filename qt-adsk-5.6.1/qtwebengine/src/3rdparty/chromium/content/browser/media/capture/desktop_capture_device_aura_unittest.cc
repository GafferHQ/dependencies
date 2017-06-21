// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_device_aura.h"

#include "base/synchronization/waitable_event.h"
#include "content/browser/browser_thread_impl.h"
#include "content/public/browser/desktop_media_id.h"
#include "media/base/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/wm/core/default_activation_client.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Expectation;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;

namespace media {
class VideoFrame;
}  // namespace media

namespace content {
namespace {

const int kFrameRate = 30;

class MockDeviceClient : public media::VideoCaptureDevice::Client {
 public:
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
  MOCK_METHOD0(DoReserveOutputBuffer, void(void));
  MOCK_METHOD0(DoOnIncomingCapturedBuffer, void(void));
  MOCK_METHOD0(DoOnIncomingCapturedVideoFrame, void(void));
  MOCK_METHOD1(OnError, void(const std::string& reason));

  // Trampoline methods to workaround GMOCK problems with scoped_ptr<>.
  scoped_ptr<Buffer> ReserveOutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelFormat format,
      media::VideoPixelStorage storage) override {
    EXPECT_TRUE((format == media::PIXEL_FORMAT_I420 &&
                 storage == media::PIXEL_STORAGE_CPU) ||
                (format == media::PIXEL_FORMAT_ARGB &&
                 storage == media::PIXEL_STORAGE_TEXTURE));
    DoReserveOutputBuffer();
    return scoped_ptr<Buffer>();
  }
  void OnIncomingCapturedBuffer(scoped_ptr<Buffer> buffer,
                                const media::VideoCaptureFormat& frame_format,
                                const base::TimeTicks& timestamp) override {
    DoOnIncomingCapturedBuffer();
  }
  void OnIncomingCapturedVideoFrame(
      scoped_ptr<Buffer> buffer,
      const scoped_refptr<media::VideoFrame>& frame,
      const base::TimeTicks& timestamp) override {
    DoOnIncomingCapturedVideoFrame();
  }

  double GetBufferPoolUtilization() const override { return 0.0; }
};

// Test harness that sets up a minimal environment with necessary stubs.
class DesktopCaptureDeviceAuraTest : public testing::Test {
 public:
  DesktopCaptureDeviceAuraTest()
      : browser_thread_for_ui_(BrowserThread::UI, &message_loop_) {}
  ~DesktopCaptureDeviceAuraTest() override {}

 protected:
  void SetUp() override {
    // The ContextFactory must exist before any Compositors are created.
    bool enable_pixel_output = false;
    ui::ContextFactory* context_factory =
        ui::InitializeContextFactoryForTests(enable_pixel_output);
    helper_.reset(new aura::test::AuraTestHelper(&message_loop_));
    helper_->SetUp(context_factory);
    new wm::DefaultActivationClient(helper_->root_window());

    // We need a window to cover desktop area so that DesktopCaptureDeviceAura
    // can use gfx::NativeWindow::GetWindowAtScreenPoint() to locate the
    // root window associated with the primary display.
    gfx::Rect desktop_bounds = root_window()->bounds();
    window_delegate_.reset(new aura::test::TestWindowDelegate());
    desktop_window_.reset(new aura::Window(window_delegate_.get()));
    desktop_window_->Init(ui::LAYER_TEXTURED);
    desktop_window_->SetBounds(desktop_bounds);
    aura::client::ParentWindowWithContext(
        desktop_window_.get(), root_window(), desktop_bounds);
    desktop_window_->Show();
  }

  void TearDown() override {
    helper_->RunAllPendingInMessageLoop();
    root_window()->RemoveChild(desktop_window_.get());
    desktop_window_.reset();
    window_delegate_.reset();
    helper_->TearDown();
    ui::TerminateContextFactoryForTests();
  }

  aura::Window* root_window() { return helper_->root_window(); }

 private:
  base::MessageLoopForUI message_loop_;
  BrowserThreadImpl browser_thread_for_ui_;
  scoped_ptr<aura::test::AuraTestHelper> helper_;
  scoped_ptr<aura::Window> desktop_window_;
  scoped_ptr<aura::test::TestWindowDelegate> window_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DesktopCaptureDeviceAuraTest);
};

TEST_F(DesktopCaptureDeviceAuraTest, StartAndStop) {
  scoped_ptr<media::VideoCaptureDevice> capture_device(
      DesktopCaptureDeviceAura::Create(
          content::DesktopMediaID::RegisterAuraWindow(root_window())));

  scoped_ptr<MockDeviceClient> client(new MockDeviceClient());
  EXPECT_CALL(*client, OnError(_)).Times(0);

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_device->AllocateAndStart(capture_params, client.Pass());
  capture_device->StopAndDeAllocate();
}

}  // namespace
}  // namespace content
