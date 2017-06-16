// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_GPU_JPEG_DECODER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_GPU_JPEG_DECODER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/jpeg_decode_accelerator.h"

namespace media {
class VideoFrame;
}

namespace content {
class GpuChannelHost;

// Adapter to GpuJpegDecodeAccelerator for VideoCaptureDevice::Client. It takes
// care of GpuJpegDecodeAccelerator creation, shared memory, and threading
// issues.
//
// All public methods except JpegDecodeAccelerator::Client ones should be called
// on the same thread. JpegDecodeAccelerator::Client methods should be called on
// the IO thread.
class CONTENT_EXPORT VideoCaptureGpuJpegDecoder
    : public media::JpegDecodeAccelerator::Client,
      public base::NonThreadSafe,
      public base::SupportsWeakPtr<VideoCaptureGpuJpegDecoder> {
 public:
  typedef base::Callback<void(
      scoped_ptr<media::VideoCaptureDevice::Client::Buffer>,
      const scoped_refptr<media::VideoFrame>&,
      const base::TimeTicks&)> DecodeDoneCB;
  typedef base::Callback<void(const std::string&)> ErrorCB;

  // Returns true if JPEG hardware decoding is supported on this device.
  static bool Supported();

  // |decode_done_cb| is called on the IO thread when decode succeed.
  // |error_cb| is called when error. This can be on any thread.
  // Both |decode_done_cb| and |error_cb| are never called after
  // VideoCaptureGpuJpegDecoder is destroyed.
  VideoCaptureGpuJpegDecoder(const DecodeDoneCB& decode_done_cb,
                             const ErrorCB& error_cb);
  ~VideoCaptureGpuJpegDecoder() override;

  // Creates and intializes decoder asynchronously.
  void Initialize();

  // Returns true if Initialize is done and it's okay to call
  // DecodeCapturedData.
  bool ReadyToDecode();

  // Decodes a JPEG picture.
  void DecodeCapturedData(
      const uint8_t* data,
      size_t in_buffer_size,
      const media::VideoCaptureFormat& frame_format,
      const base::TimeTicks& timestamp,
      scoped_ptr<media::VideoCaptureDevice::Client::Buffer> out_buffer);

  // JpegDecodeAccelerator::Client implementation.
  // These will be called on IO thread.
  void VideoFrameReady(int32_t buffer_id) override;
  void NotifyError(int32_t buffer_id,
                   media::JpegDecodeAccelerator::Error error) override;

 private:
  // Initialization helper, to establish GPU channel.
  static void EstablishGpuChannelOnUIThread(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::WeakPtr<VideoCaptureGpuJpegDecoder> weak_this);

  static void GpuChannelEstablishedOnUIThread(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::WeakPtr<VideoCaptureGpuJpegDecoder> weak_this);

  void InitializeDone(scoped_refptr<GpuChannelHost> gpu_channel_host);

  // Returns true if the decoding of last frame is not finished yet.
  bool IsDecoding_Locked();

  scoped_refptr<GpuChannelHost> gpu_channel_host_;

  // The underlying JPEG decode accelerator.
  scoped_ptr<media::JpegDecodeAccelerator> decoder_;

  // The callback to run when decode succeeds.
  const DecodeDoneCB decode_done_cb_;

  // Guards |decode_done_closure_| and |error_cb_|.
  base::Lock lock_;

  // The callback to run when an error occurs.
  const ErrorCB error_cb_;

  // The closure of |decode_done_cb_| with bound parameters.
  base::Closure decode_done_closure_;

  // Next id for input BitstreamBuffer.
  int32_t next_bitstream_buffer_id_;

  // The id for current input BitstreamBuffer being decoded.
  int32_t in_buffer_id_;

  // Shared memory to store JPEG stream buffer. The input BitstreamBuffer is
  // backed by this.
  scoped_ptr<base::SharedMemory> in_shared_memory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureGpuJpegDecoder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_GPU_JPEG_DECODER_H_
