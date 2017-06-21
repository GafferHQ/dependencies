// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_gpu_jpeg_decoder.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/common/gpu/client/gpu_jpeg_decode_accelerator_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_frame.h"

namespace content {

// static
bool VideoCaptureGpuJpegDecoder::Supported() {
  // A lightweight check for caller to avoid IPC latency for known unsupported
  // platform. Initialize() can do the real platform supporting check but it
  // requires an IPC even for platforms that do not support HW decoder.
  // TODO(kcwu): move this information to GpuInfo. https://crbug.com/503568
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAcceleratedMjpegDecode)) {
    return true;
  }
#endif
  return false;
}

VideoCaptureGpuJpegDecoder::VideoCaptureGpuJpegDecoder(
    const DecodeDoneCB& decode_done_cb,
    const ErrorCB& error_cb)
    : decode_done_cb_(decode_done_cb),
      error_cb_(error_cb),
      next_bitstream_buffer_id_(0),
      in_buffer_id_(media::JpegDecodeAccelerator::kInvalidBitstreamBufferId) {
}

VideoCaptureGpuJpegDecoder::~VideoCaptureGpuJpegDecoder() {
  DCHECK(CalledOnValidThread());

  // |decoder_| guarantees no more JpegDecodeAccelerator::Client callbacks
  // on IO thread after deletion.
  decoder_.reset();

  // |gpu_channel_host_| should outlive |decoder_|, so |gpu_channel_host_|
  // must be released after |decoder_| has been destroyed.
  gpu_channel_host_ = nullptr;
}

bool VideoCaptureGpuJpegDecoder::IsDecoding_Locked() {
  lock_.AssertAcquired();
  return !decode_done_closure_.is_null();
}

void VideoCaptureGpuJpegDecoder::Initialize() {
  DCHECK(CalledOnValidThread());

  const scoped_refptr<base::SingleThreadTaskRunner> current_task_runner(
      base::ThreadTaskRunnerHandle::Get());
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&EstablishGpuChannelOnUIThread,
                                     current_task_runner, AsWeakPtr()));
}

bool VideoCaptureGpuJpegDecoder::ReadyToDecode() {
  DCHECK(CalledOnValidThread());
  return decoder_;
}

// static
void VideoCaptureGpuJpegDecoder::EstablishGpuChannelOnUIThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::WeakPtr<VideoCaptureGpuJpegDecoder> weak_this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(BrowserGpuChannelHostFactory::instance());

  BrowserGpuChannelHostFactory::instance()->EstablishGpuChannel(
      CAUSE_FOR_GPU_LAUNCH_JPEGDECODEACCELERATOR_INITIALIZE,
      base::Bind(&VideoCaptureGpuJpegDecoder::GpuChannelEstablishedOnUIThread,
                 task_runner, weak_this));
}

// static
void VideoCaptureGpuJpegDecoder::GpuChannelEstablishedOnUIThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::WeakPtr<VideoCaptureGpuJpegDecoder> weak_this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_refptr<GpuChannelHost> gpu_channel_host(
      BrowserGpuChannelHostFactory::instance()->GetGpuChannel());
  task_runner->PostTask(FROM_HERE,
                        base::Bind(&VideoCaptureGpuJpegDecoder::InitializeDone,
                                   weak_this, base::Passed(&gpu_channel_host)));
}

void VideoCaptureGpuJpegDecoder::InitializeDone(
    scoped_refptr<GpuChannelHost> gpu_channel_host) {
  DCHECK(CalledOnValidThread());
  if (!gpu_channel_host) {
    LOG(ERROR) << "Failed to establish GPU channel for JPEG decoder";
    return;
  }
  gpu_channel_host_ = gpu_channel_host.Pass();

  decoder_ = gpu_channel_host_->CreateJpegDecoder(this);
}

void VideoCaptureGpuJpegDecoder::VideoFrameReady(int32_t bitstream_buffer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  TRACE_EVENT0("jpeg", "VideoCaptureGpuJpegDecoder::VideoFrameReady");
  base::AutoLock lock(lock_);

  if (!IsDecoding_Locked()) {
    LOG(ERROR) << "Got decode response while not decoding";
    return;
  }

  if (bitstream_buffer_id != in_buffer_id_) {
    LOG(ERROR) << "Unexpected bitstream_buffer_id " << bitstream_buffer_id
               << ", expected " << in_buffer_id_;
    return;
  }
  in_buffer_id_ = media::JpegDecodeAccelerator::kInvalidBitstreamBufferId;

  decode_done_closure_.Run();
  decode_done_closure_.Reset();

  TRACE_EVENT_ASYNC_END0("jpeg", "VideoCaptureGpuJpegDecoder decoding",
                         bitstream_buffer_id);
}

void VideoCaptureGpuJpegDecoder::NotifyError(
    int32_t bitstream_buffer_id,
    media::JpegDecodeAccelerator::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  LOG(ERROR) << "Decode error, bitstream_buffer_id=" << bitstream_buffer_id
             << ", error=" << error;

  base::AutoLock lock(lock_);
  decode_done_closure_.Reset();

  error_cb_.Run(
      base::StringPrintf("JPEG Decode error, bitstream_buffer_id=%d, error=%d",
                         bitstream_buffer_id, error));
}

void VideoCaptureGpuJpegDecoder::DecodeCapturedData(
    const uint8_t* data,
    size_t in_buffer_size,
    const media::VideoCaptureFormat& frame_format,
    const base::TimeTicks& timestamp,
    scoped_ptr<media::VideoCaptureDevice::Client::Buffer> out_buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK(decoder_);

  TRACE_EVENT_ASYNC_BEGIN0("jpeg", "VideoCaptureGpuJpegDecoder decoding",
                           next_bitstream_buffer_id_);
  TRACE_EVENT0("jpeg", "VideoCaptureGpuJpegDecoder::DecodeCapturedData");

  // TODO(kcwu): enqueue decode requests in case decoding is not fast enough
  // (say, if decoding time is longer than 16ms for 60fps 4k image)
  {
    base::AutoLock lock(lock_);
    if (IsDecoding_Locked()) {
      DVLOG(1) << "Drop captured frame. Previous jpeg frame is still decoding";
      return;
    }
  }

  // Enlarge input buffer if necessary.
  if (!in_shared_memory_.get() ||
      in_buffer_size > in_shared_memory_->mapped_size()) {
    // Reserve 2x space to avoid frequent reallocations for initial frames.
    const size_t reserved_size = 2 * in_buffer_size;
    in_shared_memory_.reset(new base::SharedMemory);
    if (!in_shared_memory_->CreateAndMapAnonymous(reserved_size)) {
      base::AutoLock lock(lock_);
      error_cb_.Run(base::StringPrintf("CreateAndMapAnonymous failed, size=%zd",
                                       reserved_size));
      return;
    }
  }
  memcpy(in_shared_memory_->memory(), data, in_buffer_size);

  // No need to lock for |in_buffer_id_| since IsDecoding_Locked() is false.
  in_buffer_id_ = next_bitstream_buffer_id_;
  media::BitstreamBuffer in_buffer(in_buffer_id_, in_shared_memory_->handle(),
                                   in_buffer_size);
  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x3FFFFFFF;

#if defined(OS_POSIX) && !(defined(OS_MACOSX) && !defined(OS_IOS))
  const gfx::Size dimensions = frame_format.frame_size;
  base::SharedMemoryHandle out_handle = out_buffer->AsPlatformFile();
  scoped_refptr<media::VideoFrame> out_frame =
      media::VideoFrame::WrapExternalSharedMemory(
          media::VideoFrame::I420,                    // format
          dimensions,                                 // coded_size
          gfx::Rect(dimensions),                      // visible_rect
          dimensions,                                 // natural_size
          static_cast<uint8_t*>(out_buffer->data()),  // data
          out_buffer->size(),                         // data_size
          out_handle,                                 // handle
          0,                                          // shared_memory_offset
          base::TimeDelta());                         // timestamp
  if (!out_frame) {
    base::AutoLock lock(lock_);
    error_cb_.Run("DecodeCapturedData: WrapExternalSharedMemory");
    return;
  }
  out_frame->metadata()->SetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                   frame_format.frame_rate);

  {
    base::AutoLock lock(lock_);
    decode_done_closure_ = base::Bind(
        decode_done_cb_, base::Passed(&out_buffer), out_frame, timestamp);
  }
  decoder_->Decode(in_buffer, out_frame);
#else
  NOTREACHED();
#endif
}

}  // namespace content
