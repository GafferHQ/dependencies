// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/fake_video_capture_device.h"

#include <algorithm>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace media {

static const int kFakeCaptureBeepCycle = 10;  // Visual beep every 0.5s.

void DrawPacman(bool use_argb,
                uint8_t* const data,
                int frame_count,
                int frame_interval,
                const gfx::Size& frame_size) {
  // |kN32_SkColorType| stands for the appropriiate RGBA/BGRA format.
  const SkColorType colorspace =
      use_argb ? kN32_SkColorType : kAlpha_8_SkColorType;
  const SkImageInfo info = SkImageInfo::Make(frame_size.width(),
                                             frame_size.height(),
                                             colorspace,
                                             kOpaque_SkAlphaType);
  SkBitmap bitmap;
  bitmap.setInfo(info);
  bitmap.setPixels(data);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  SkCanvas canvas(bitmap);

  // Equalize Alpha_8 that has light green background while RGBA has white.
  if (use_argb) {
    const SkRect full_frame = SkRect::MakeWH(frame_size.width(),
                                             frame_size.height());
    paint.setARGB(255, 0, 127, 0);
    canvas.drawRect(full_frame, paint);
  }
  paint.setColor(SK_ColorGREEN);

  // Draw a sweeping circle to show an animation.
  const int end_angle = (3 * kFakeCaptureBeepCycle * frame_count % 361);
  const int radius = std::min(frame_size.width(), frame_size.height()) / 4;
  const SkRect rect = SkRect::MakeXYWH(frame_size.width() / 2 - radius,
                                       frame_size.height() / 2 - radius,
                                       2 * radius,
                                       2 * radius);
  canvas.drawArc(rect, 0, end_angle, true, paint);

  // Draw current time.
  const int elapsed_ms = frame_interval * frame_count;
  const int milliseconds = elapsed_ms % 1000;
  const int seconds = (elapsed_ms / 1000) % 60;
  const int minutes = (elapsed_ms / 1000 / 60) % 60;
  const int hours = (elapsed_ms / 1000 / 60 / 60) % 60;

  const std::string time_string = base::StringPrintf("%d:%02d:%02d:%03d %d",
      hours, minutes, seconds, milliseconds, frame_count);
  canvas.scale(3, 3);
  canvas.drawText(time_string.data(), time_string.length(), 30, 20, paint);
}

FakeVideoCaptureDevice::FakeVideoCaptureDevice(
    FakeVideoCaptureDeviceType device_type)
    : device_type_(device_type),
      frame_count_(0),
      weak_factory_(this) {}

FakeVideoCaptureDevice::~FakeVideoCaptureDevice() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void FakeVideoCaptureDevice::AllocateAndStart(
    const VideoCaptureParams& params,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  client_ = client.Pass();

  // Incoming |params| can be none of the supported formats, so we get the
  // closest thing rounded up. TODO(mcasas): Use the |params|, if they belong to
  // the supported ones, when http://crbug.com/309554 is verified.
  DCHECK_EQ(params.requested_format.pixel_format, PIXEL_FORMAT_I420);
  capture_format_.pixel_format = params.requested_format.pixel_format;
  capture_format_.frame_rate = 30.0;
  if (params.requested_format.frame_size.width() > 1280)
      capture_format_.frame_size.SetSize(1920, 1080);
  else if (params.requested_format.frame_size.width() > 640)
      capture_format_.frame_size.SetSize(1280, 720);
  else if (params.requested_format.frame_size.width() > 320)
    capture_format_.frame_size.SetSize(640, 480);
  else
    capture_format_.frame_size.SetSize(320, 240);

  if (device_type_ == USING_OWN_BUFFERS ||
      device_type_ == USING_OWN_BUFFERS_TRIPLANAR) {
    capture_format_.pixel_storage = PIXEL_STORAGE_CPU;
    fake_frame_.reset(new uint8[VideoFrame::AllocationSize(
        VideoFrame::I420, capture_format_.frame_size)]);
    BeepAndScheduleNextCapture(
        base::TimeTicks::Now(),
        base::Bind(&FakeVideoCaptureDevice::CaptureUsingOwnBuffers,
                   weak_factory_.GetWeakPtr()));
  } else if (device_type_ == USING_CLIENT_BUFFERS) {
    DVLOG(1) << "starting with "
             << (params.use_gpu_memory_buffers ? "GMB" : "ShMem");
    BeepAndScheduleNextCapture(
        base::TimeTicks::Now(),
        base::Bind(
            &FakeVideoCaptureDevice::CaptureUsingClientBuffers,
            weak_factory_.GetWeakPtr(),
            params.use_gpu_memory_buffers ? PIXEL_FORMAT_ARGB
                                          : PIXEL_FORMAT_I420,
            params.use_gpu_memory_buffers ? PIXEL_STORAGE_GPUMEMORYBUFFER
                                          : PIXEL_STORAGE_CPU));
  } else {
    client_->OnError("Unknown Fake Video Capture Device type.");
  }
}

void FakeVideoCaptureDevice::StopAndDeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  client_.reset();
}

void FakeVideoCaptureDevice::CaptureUsingOwnBuffers(
    base::TimeTicks expected_execution_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const size_t frame_size = capture_format_.ImageAllocationSize();
  memset(fake_frame_.get(), 0, frame_size);

  DrawPacman(false  /* use_argb */,
             fake_frame_.get(),
             frame_count_,
             kFakeCapturePeriodMs,
             capture_format_.frame_size);

  // Give the captured frame to the client.
  if (device_type_ == USING_OWN_BUFFERS) {
    client_->OnIncomingCapturedData(fake_frame_.get(),
                                    frame_size,
                                    capture_format_,
                                    0  /* rotation */,
                                    base::TimeTicks::Now());
  } else if (device_type_ == USING_OWN_BUFFERS_TRIPLANAR) {
    client_->OnIncomingCapturedYuvData(
        fake_frame_.get(),
        fake_frame_.get() + capture_format_.frame_size.GetArea(),
        fake_frame_.get() + capture_format_.frame_size.GetArea() * 5 / 4,
        capture_format_.frame_size.width(),
        capture_format_.frame_size.width() / 2,
        capture_format_.frame_size.width() / 2,
        capture_format_,
        0  /* rotation */,
        base::TimeTicks::Now());
  }
  BeepAndScheduleNextCapture(
      expected_execution_time,
      base::Bind(&FakeVideoCaptureDevice::CaptureUsingOwnBuffers,
                 weak_factory_.GetWeakPtr()));
}

void FakeVideoCaptureDevice::CaptureUsingClientBuffers(
    VideoPixelFormat pixel_format,
    VideoPixelStorage pixel_storage,
    base::TimeTicks expected_execution_time) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<VideoCaptureDevice::Client::Buffer> capture_buffer(
      client_->ReserveOutputBuffer(capture_format_.frame_size, pixel_format,
                                   pixel_storage));
  DLOG_IF(ERROR, !capture_buffer) << "Couldn't allocate Capture Buffer";

  if (capture_buffer.get()) {
    uint8_t* const data_ptr = static_cast<uint8_t*>(capture_buffer->data());
    DCHECK(data_ptr) << "Buffer has NO backing memory";
    memset(data_ptr, 0, capture_buffer->size());

    DrawPacman((pixel_format == media::PIXEL_FORMAT_ARGB), /* use_argb */
               data_ptr, frame_count_, kFakeCapturePeriodMs,
               capture_format_.frame_size);

    // Give the captured frame to the client.
    const VideoCaptureFormat format(capture_format_.frame_size,
                                    capture_format_.frame_rate, pixel_format,
                                    pixel_storage);
    client_->OnIncomingCapturedBuffer(capture_buffer.Pass(), format,
                                      base::TimeTicks::Now());
  }

  BeepAndScheduleNextCapture(
      expected_execution_time,
      base::Bind(&FakeVideoCaptureDevice::CaptureUsingClientBuffers,
                 weak_factory_.GetWeakPtr(), pixel_format, pixel_storage));
}

void FakeVideoCaptureDevice::BeepAndScheduleNextCapture(
    base::TimeTicks expected_execution_time,
    const base::Callback<void(base::TimeTicks)>& next_capture) {
  // Generate a synchronized beep sound every so many frames.
  if (frame_count_++ % kFakeCaptureBeepCycle == 0)
    FakeAudioInputStream::BeepOnce();

  // Reschedule next CaptureTask.
  const base::TimeTicks current_time = base::TimeTicks::Now();
  const base::TimeDelta frame_interval =
      base::TimeDelta::FromMilliseconds(kFakeCapturePeriodMs);
  // Don't accumulate any debt if we are lagging behind - just post the next
  // frame immediately and continue as normal.
  const base::TimeTicks next_execution_time =
      std::max(current_time, expected_execution_time + frame_interval);
  const base::TimeDelta delay = next_execution_time - current_time;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(next_capture, next_execution_time), delay);
}

}  // namespace media
