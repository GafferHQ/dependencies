// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "media/base/video_capture_types.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_device_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "media/video/capture/win/video_capture_device_factory_win.h"
#endif

#if defined(OS_MACOSX)
#include "media/base/mac/avfoundation_glue.h"
#include "media/video/capture/mac/video_capture_device_factory_mac.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "media/video/capture/android/video_capture_device_android.h"
#endif

#if defined(OS_MACOSX)
// Mac/QTKit will always give you the size you ask for and this case will fail.
#define MAYBE_AllocateBadSize DISABLED_AllocateBadSize
// We will always get YUYV from the Mac QTKit/AVFoundation implementations.
#define MAYBE_CaptureMjpeg DISABLED_CaptureMjpeg
#elif defined(OS_WIN)
#define MAYBE_AllocateBadSize AllocateBadSize
#define MAYBE_CaptureMjpeg CaptureMjpeg
#elif defined(OS_ANDROID)
// TODO(wjia): enable those tests on Android.
// On Android, native camera (JAVA) delivers frames on UI thread which is the
// main thread for tests. This results in no frame received by
// VideoCaptureAndroid.
#define MAYBE_AllocateBadSize DISABLED_AllocateBadSize
#define ReAllocateCamera DISABLED_ReAllocateCamera
#define DeAllocateCameraWhileRunning DISABLED_DeAllocateCameraWhileRunning
#define DeAllocateCameraWhileRunning DISABLED_DeAllocateCameraWhileRunning
#define MAYBE_CaptureMjpeg DISABLED_CaptureMjpeg
#else
#define MAYBE_AllocateBadSize AllocateBadSize
#define MAYBE_CaptureMjpeg CaptureMjpeg
#endif

using ::testing::_;
using ::testing::SaveArg;

namespace media {
namespace {

static const gfx::Size kCaptureSizes[] = {
    gfx::Size(640, 480),
    gfx::Size(1280, 720)
};

class MockClient : public VideoCaptureDevice::Client {
 public:
  MOCK_METHOD9(OnIncomingCapturedYuvData,
               void(const uint8* y_data,
                    const uint8* u_data,
                    const uint8* v_data,
                    size_t y_stride,
                    size_t u_stride,
                    size_t v_stride,
                    const VideoCaptureFormat& frame_format,
                    int clockwise_rotation,
                    const base::TimeTicks& timestamp));
  MOCK_METHOD0(DoReserveOutputBuffer, void(void));
  MOCK_METHOD0(DoOnIncomingCapturedBuffer, void(void));
  MOCK_METHOD0(DoOnIncomingCapturedVideoFrame, void(void));
  MOCK_METHOD1(OnError, void(const std::string& reason));
  MOCK_CONST_METHOD0(GetBufferPoolUtilization, double(void));

  explicit MockClient(base::Callback<void(const VideoCaptureFormat&)> frame_cb)
      : main_thread_(base::ThreadTaskRunnerHandle::Get()),
      frame_cb_(frame_cb) {}

  void OnIncomingCapturedData(const uint8* data,
                              int length,
                              const VideoCaptureFormat& format,
                              int rotation,
                              const base::TimeTicks& timestamp) override {
    ASSERT_GT(length, 0);
    ASSERT_TRUE(data != NULL);
    main_thread_->PostTask(FROM_HERE, base::Bind(frame_cb_, format));
  }

  // Trampoline methods to workaround GMOCK problems with scoped_ptr<>.
  scoped_ptr<Buffer> ReserveOutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelFormat format,
      media::VideoPixelStorage storage) override {
    DoReserveOutputBuffer();
    NOTREACHED() << "This should never be called";
    return scoped_ptr<Buffer>();
  }
  void OnIncomingCapturedBuffer(scoped_ptr<Buffer> buffer,
                                const VideoCaptureFormat& frame_format,
                                const base::TimeTicks& timestamp) override {
    DoOnIncomingCapturedBuffer();
  }
  void OnIncomingCapturedVideoFrame(scoped_ptr<Buffer> buffer,
                                    const scoped_refptr<VideoFrame>& frame,
                                    const base::TimeTicks& timestamp) override {
    DoOnIncomingCapturedVideoFrame();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  base::Callback<void(const VideoCaptureFormat&)> frame_cb_;
};

class DeviceEnumerationListener :
    public base::RefCounted<DeviceEnumerationListener> {
 public:
  MOCK_METHOD1(OnEnumeratedDevicesCallbackPtr,
               void(VideoCaptureDevice::Names* names));
  // GMock doesn't support move-only arguments, so we use this forward method.
  void OnEnumeratedDevicesCallback(
      scoped_ptr<VideoCaptureDevice::Names> names) {
    OnEnumeratedDevicesCallbackPtr(names.release());
  }
 private:
  friend class base::RefCounted<DeviceEnumerationListener>;
  virtual ~DeviceEnumerationListener() {}
};

}  // namespace

class VideoCaptureDeviceTest :
    public testing::TestWithParam<gfx::Size> {
 protected:
  typedef VideoCaptureDevice::Client Client;

  VideoCaptureDeviceTest()
      : loop_(new base::MessageLoop()),
        client_(
            new MockClient(base::Bind(&VideoCaptureDeviceTest::OnFrameCaptured,
                                      base::Unretained(this)))),
        video_capture_device_factory_(VideoCaptureDeviceFactory::CreateFactory(
            base::ThreadTaskRunnerHandle::Get())) {
    device_enumeration_listener_ = new DeviceEnumerationListener();
  }

  void SetUp() override {
#if defined(OS_ANDROID)
    VideoCaptureDeviceAndroid::RegisterVideoCaptureDevice(
        base::android::AttachCurrentThread());
#endif
#if defined(OS_MACOSX)
    AVFoundationGlue::InitializeAVFoundation();
#endif
    EXPECT_CALL(*client_, OnIncomingCapturedYuvData(_,_,_,_,_,_,_,_,_))
               .Times(0);
    EXPECT_CALL(*client_, DoReserveOutputBuffer()).Times(0);
    EXPECT_CALL(*client_, DoOnIncomingCapturedBuffer()).Times(0);
    EXPECT_CALL(*client_, DoOnIncomingCapturedVideoFrame()).Times(0);
  }

  void ResetWithNewClient() {
    client_.reset(new MockClient(base::Bind(
        &VideoCaptureDeviceTest::OnFrameCaptured, base::Unretained(this))));
  }

  void OnFrameCaptured(const VideoCaptureFormat& format) {
    last_format_ = format;
    run_loop_->QuitClosure().Run();
  }

  void WaitForCapturedFrame() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  scoped_ptr<VideoCaptureDevice::Names> EnumerateDevices() {
    VideoCaptureDevice::Names* names;
    EXPECT_CALL(*device_enumeration_listener_.get(),
                OnEnumeratedDevicesCallbackPtr(_)).WillOnce(SaveArg<0>(&names));

    video_capture_device_factory_->EnumerateDeviceNames(
        base::Bind(&DeviceEnumerationListener::OnEnumeratedDevicesCallback,
                   device_enumeration_listener_));
    base::MessageLoop::current()->RunUntilIdle();
    return scoped_ptr<VideoCaptureDevice::Names>(names);
  }

  const VideoCaptureFormat& last_format() const { return last_format_; }

  scoped_ptr<VideoCaptureDevice::Name> GetFirstDeviceNameSupportingPixelFormat(
      const VideoPixelFormat& pixel_format) {
    names_ = EnumerateDevices();
    if (names_->empty()) {
      DVLOG(1) << "No camera available.";
      return scoped_ptr<VideoCaptureDevice::Name>();
    }
    for (const auto& names_iterator : *names_) {
      VideoCaptureFormats supported_formats;
      video_capture_device_factory_->GetDeviceSupportedFormats(
          names_iterator,
          &supported_formats);
      for (const auto& formats_iterator : supported_formats) {
        if (formats_iterator.pixel_format == pixel_format) {
          return scoped_ptr<VideoCaptureDevice::Name>(
              new VideoCaptureDevice::Name(names_iterator));
        }
      }
    }
    DVLOG_IF(1, pixel_format != PIXEL_FORMAT_MAX) << "No camera can capture the"
        << " format: " << VideoCaptureFormat::PixelFormatToString(pixel_format);
    return scoped_ptr<VideoCaptureDevice::Name>();
  }

  bool IsCaptureSizeSupported(const VideoCaptureDevice::Name& device,
                              const gfx::Size& size) {
    VideoCaptureFormats supported_formats;
    video_capture_device_factory_->GetDeviceSupportedFormats(
        device, &supported_formats);
    const auto it =
        std::find_if(supported_formats.begin(), supported_formats.end(),
                     [&size](VideoCaptureFormat const& f) {
                       return f.frame_size == size;
                     });
    if (it == supported_formats.end()) {
      DVLOG(1) << "Size " << size.ToString() << " is not supported.";
      return false;
    }
    return true;
  }

#if defined(OS_WIN)
  base::win::ScopedCOMInitializer initialize_com_;
#endif
  scoped_ptr<VideoCaptureDevice::Names> names_;
  scoped_ptr<base::MessageLoop> loop_;
  scoped_ptr<base::RunLoop> run_loop_;
  scoped_ptr<MockClient> client_;
  scoped_refptr<DeviceEnumerationListener> device_enumeration_listener_;
  VideoCaptureFormat last_format_;
  scoped_ptr<VideoCaptureDeviceFactory> video_capture_device_factory_;
};

// Cause hangs on Windows Debug. http://crbug.com/417824
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_OpenInvalidDevice DISABLED_OpenInvalidDevice
#else
#define MAYBE_OpenInvalidDevice OpenInvalidDevice
#endif

TEST_F(VideoCaptureDeviceTest, MAYBE_OpenInvalidDevice) {
#if defined(OS_WIN)
  VideoCaptureDevice::Name::CaptureApiType api_type =
      VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation()
          ? VideoCaptureDevice::Name::MEDIA_FOUNDATION
          : VideoCaptureDevice::Name::DIRECT_SHOW;
  VideoCaptureDevice::Name device_name("jibberish", "jibberish", api_type);
#elif defined(OS_MACOSX)
  VideoCaptureDevice::Name device_name("jibberish", "jibberish",
      VideoCaptureDeviceFactoryMac::PlatformSupportsAVFoundation()
          ? VideoCaptureDevice::Name::AVFOUNDATION
          : VideoCaptureDevice::Name::QTKIT);
#else
  VideoCaptureDevice::Name device_name("jibberish", "jibberish");
#endif
  scoped_ptr<VideoCaptureDevice> device =
      video_capture_device_factory_->Create(device_name);
#if !defined(OS_MACOSX)
  EXPECT_TRUE(device == NULL);
#else
  if (VideoCaptureDeviceFactoryMac::PlatformSupportsAVFoundation()) {
    EXPECT_TRUE(device == NULL);
  } else {
    // The presence of the actual device is only checked on AllocateAndStart()
    // and not on creation for QTKit API in Mac OS X platform.
    EXPECT_CALL(*client_, OnError(_)).Times(1);

    VideoCaptureParams capture_params;
    capture_params.requested_format.frame_size.SetSize(640, 480);
    capture_params.requested_format.frame_rate = 30;
    capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
    device->AllocateAndStart(capture_params, client_.Pass());
    device->StopAndDeAllocate();
  }
#endif
}

TEST_P(VideoCaptureDeviceTest, CaptureWithSize) {
  names_ = EnumerateDevices();
  if (names_->empty()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }

  const gfx::Size& size = GetParam();
  if (!IsCaptureSizeSupported(names_->front(), size))
    return;
  const int width = size.width();
  const int height = size.height();

  scoped_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->Create(names_->front()));
  ASSERT_TRUE(device);
  DVLOG(1) << names_->front().id();

  EXPECT_CALL(*client_, OnError(_)).Times(0);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(width, height);
  capture_params.requested_format.frame_rate = 30.0f;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  device->AllocateAndStart(capture_params, client_.Pass());
  // Get captured video frames.
  WaitForCapturedFrame();
  EXPECT_EQ(last_format().frame_size.width(), width);
  EXPECT_EQ(last_format().frame_size.height(), height);
  if (last_format().pixel_format != PIXEL_FORMAT_MJPEG)
    EXPECT_EQ(size.GetArea(), last_format().frame_size.GetArea());
  device->StopAndDeAllocate();
}

#if !defined(OS_ANDROID)
INSTANTIATE_TEST_CASE_P(MAYBE_VideoCaptureDeviceTests,
                        VideoCaptureDeviceTest,
                        testing::ValuesIn(kCaptureSizes));
#endif

TEST_F(VideoCaptureDeviceTest, MAYBE_AllocateBadSize) {
  names_ = EnumerateDevices();
  if (names_->empty()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->Create(names_->front()));
  ASSERT_TRUE(device);

  EXPECT_CALL(*client_, OnError(_)).Times(0);

  const gfx::Size input_size(640, 480);
  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(637, 472);
  capture_params.requested_format.frame_rate = 35;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  device->AllocateAndStart(capture_params, client_.Pass());
  WaitForCapturedFrame();
  device->StopAndDeAllocate();
  EXPECT_EQ(last_format().frame_size.width(), input_size.width());
  EXPECT_EQ(last_format().frame_size.height(), input_size.height());
  if (last_format().pixel_format != PIXEL_FORMAT_MJPEG)
    EXPECT_EQ(input_size.GetArea(), last_format().frame_size.GetArea());
}

// Cause hangs on Windows Debug. http://crbug.com/417824
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_ReAllocateCamera DISABLED_ReAllocateCamera
#else
#define MAYBE_ReAllocateCamera ReAllocateCamera
#endif

TEST_F(VideoCaptureDeviceTest, MAYBE_ReAllocateCamera) {
  names_ = EnumerateDevices();
  if (names_->empty()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }

  // First, do a number of very fast device start/stops.
  for (int i = 0; i <= 5; i++) {
    ResetWithNewClient();
    scoped_ptr<VideoCaptureDevice> device(
        video_capture_device_factory_->Create(names_->front()));
    gfx::Size resolution;
    if (i % 2) {
      resolution = gfx::Size(640, 480);
    } else {
      resolution = gfx::Size(1280, 1024);
    }
    VideoCaptureParams capture_params;
    capture_params.requested_format.frame_size = resolution;
    capture_params.requested_format.frame_rate = 30;
    capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
    device->AllocateAndStart(capture_params, client_.Pass());
    device->StopAndDeAllocate();
  }

  // Finally, do a device start and wait for it to finish.
  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(320, 240);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;

  ResetWithNewClient();
  scoped_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->Create(names_->front()));

  device->AllocateAndStart(capture_params, client_.Pass());
  WaitForCapturedFrame();
  device->StopAndDeAllocate();
  device.reset();
  EXPECT_EQ(last_format().frame_size.width(), 320);
  EXPECT_EQ(last_format().frame_size.height(), 240);
}

TEST_F(VideoCaptureDeviceTest, DeAllocateCameraWhileRunning) {
  names_ = EnumerateDevices();
  if (names_->empty()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->Create(names_->front()));
  ASSERT_TRUE(device);

  EXPECT_CALL(*client_, OnError(_)).Times(0);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  device->AllocateAndStart(capture_params, client_.Pass());
  // Get captured video frames.
  WaitForCapturedFrame();
  EXPECT_EQ(last_format().frame_size.width(), 640);
  EXPECT_EQ(last_format().frame_size.height(), 480);
  EXPECT_EQ(last_format().frame_rate, 30);
  device->StopAndDeAllocate();
}

// Start the camera in 720p to capture MJPEG instead of a raw format.
TEST_F(VideoCaptureDeviceTest, MAYBE_CaptureMjpeg) {
  scoped_ptr<VideoCaptureDevice::Name> name =
      GetFirstDeviceNameSupportingPixelFormat(PIXEL_FORMAT_MJPEG);
  if (!name) {
    DVLOG(1) << "No camera supports MJPEG format. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->Create(*name));
  ASSERT_TRUE(device);

  EXPECT_CALL(*client_, OnError(_)).Times(0);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(1280, 720);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_MJPEG;
  device->AllocateAndStart(capture_params, client_.Pass());
  // Get captured video frames.
  WaitForCapturedFrame();
  // Verify we get MJPEG from the device. Not all devices can capture 1280x720
  // @ 30 fps, so we don't care about the exact resolution we get.
  EXPECT_EQ(last_format().pixel_format, PIXEL_FORMAT_MJPEG);
  EXPECT_GE(static_cast<size_t>(1280 * 720),
            last_format().ImageAllocationSize());
  device->StopAndDeAllocate();
}

TEST_F(VideoCaptureDeviceTest, GetDeviceSupportedFormats) {
  // Use PIXEL_FORMAT_MAX to iterate all device names for testing
  // GetDeviceSupportedFormats().
  scoped_ptr<VideoCaptureDevice::Name> name =
      GetFirstDeviceNameSupportingPixelFormat(PIXEL_FORMAT_MAX);
  // Verify no camera returned for PIXEL_FORMAT_MAX. Nothing else to test here
  // since we cannot forecast the hardware capabilities.
  ASSERT_FALSE(name);
}

};  // namespace media
