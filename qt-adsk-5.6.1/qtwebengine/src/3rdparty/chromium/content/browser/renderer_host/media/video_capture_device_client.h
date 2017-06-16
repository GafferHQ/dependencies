// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEVICE_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEVICE_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "media/video/capture/video_capture_device.h"

namespace content {
class VideoCaptureBufferPool;
class VideoCaptureController;
class VideoCaptureGpuJpegDecoder;

// Receives events from the VideoCaptureDevice and posts them to a |controller_|
// on the IO thread. An instance of this class may safely outlive its target
// VideoCaptureController. This is a shallow class meant to convert incoming
// frames and holds no significant state.
//
// Methods of this class may be called from any thread, and in practice will
// often be called on some auxiliary thread depending on the platform and the
// device type; including, for example, the DirectShow thread on Windows, the
// v4l2_thread on Linux, and the UI thread for tab capture.
//
// It has an internal ref counted TextureWrapHelper class used to wrap incoming
// GpuMemoryBuffers into Texture backed VideoFrames. This class creates and
// manages the necessary entities to interact with the GPU process, notably an
// offscreen Context to avoid janking the UI thread.
class CONTENT_EXPORT VideoCaptureDeviceClient
    : public media::VideoCaptureDevice::Client,
      public base::SupportsWeakPtr<VideoCaptureDeviceClient> {
 public:
  VideoCaptureDeviceClient(
      const base::WeakPtr<VideoCaptureController>& controller,
      const scoped_refptr<VideoCaptureBufferPool>& buffer_pool,
      const scoped_refptr<base::SingleThreadTaskRunner>& capture_task_runner);
  ~VideoCaptureDeviceClient() override;

  // VideoCaptureDevice::Client implementation.
  void OnIncomingCapturedData(const uint8* data,
                              int length,
                              const media::VideoCaptureFormat& frame_format,
                              int rotation,
                              const base::TimeTicks& timestamp) override;
  void OnIncomingCapturedYuvData(const uint8* y_data,
                                 const uint8* u_data,
                                 const uint8* v_data,
                                 size_t y_stride,
                                 size_t u_stride,
                                 size_t v_stride,
                                 const media::VideoCaptureFormat& frame_format,
                                 int clockwise_rotation,
                                 const base::TimeTicks& timestamp) override;
  scoped_ptr<Buffer> ReserveOutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelFormat format,
      media::VideoPixelStorage storage) override;
  void OnIncomingCapturedBuffer(scoped_ptr<Buffer> buffer,
                                const media::VideoCaptureFormat& frame_format,
                                const base::TimeTicks& timestamp) override;
  void OnIncomingCapturedVideoFrame(
      scoped_ptr<Buffer> buffer,
      const scoped_refptr<media::VideoFrame>& frame,
      const base::TimeTicks& timestamp) override;
  void OnError(const std::string& reason) override;
  void OnLog(const std::string& message) override;
  double GetBufferPoolUtilization() const override;

 private:
  // The controller to which we post events.
  const base::WeakPtr<VideoCaptureController> controller_;

  // Hardware JPEG decoder.
  scoped_ptr<VideoCaptureGpuJpegDecoder> external_jpeg_decoder_;

  // Whether |external_jpeg_decoder_| has been initialized.
  bool external_jpeg_decoder_initialized_;

  // The pool of shared-memory buffers used for capturing.
  const scoped_refptr<VideoCaptureBufferPool> buffer_pool_;

  // Internal delegate for GpuMemoryBuffer-into-VideoFrame wrapping.
  class TextureWrapHelper;
  scoped_refptr<TextureWrapHelper> texture_wrap_helper_;
  // Reference to Capture Thread task runner, where |texture_wrap_helper_|
  // lives.
  const scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;

  media::VideoPixelFormat last_captured_pixel_format_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceClient);
};


}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEVICE_CLIENT_H_
