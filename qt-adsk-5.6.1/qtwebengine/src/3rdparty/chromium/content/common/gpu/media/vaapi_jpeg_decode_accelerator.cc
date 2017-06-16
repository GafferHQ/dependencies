// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/vaapi_jpeg_decode_accelerator.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/media/vaapi_picture.h"
#include "media/base/video_frame.h"
#include "media/filters/jpeg_parser.h"
#include "third_party/libyuv/include/libyuv.h"

namespace content {

namespace {
// UMA errors that the VaapiJpegDecodeAccelerator class reports.
enum VAJDADecoderFailure {
  VAAPI_ERROR = 0,
  VAJDA_DECODER_FAILURES_MAX,
};

static void ReportToUMA(VAJDADecoderFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAJDA.DecoderFailure", failure,
                            VAJDA_DECODER_FAILURES_MAX);
}
}  // namespace

VaapiJpegDecodeAccelerator::DecodeRequest::DecodeRequest(
    const media::BitstreamBuffer& bitstream_buffer,
    scoped_ptr<base::SharedMemory> shm,
    const scoped_refptr<media::VideoFrame>& video_frame)
    : bitstream_buffer(bitstream_buffer),
      shm(shm.Pass()),
      video_frame(video_frame) {
}

VaapiJpegDecodeAccelerator::DecodeRequest::~DecodeRequest() {
}

void VaapiJpegDecodeAccelerator::NotifyError(int32_t bitstream_buffer_id,
                                             Error error) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DLOG(ERROR) << "Notifying of error " << error;
  DCHECK(client_);
  client_->NotifyError(bitstream_buffer_id, error);
}

void VaapiJpegDecodeAccelerator::NotifyErrorFromDecoderThread(
    int32_t bitstream_buffer_id,
    Error error) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&VaapiJpegDecodeAccelerator::NotifyError,
                                    weak_this_, bitstream_buffer_id, error));
}

void VaapiJpegDecodeAccelerator::VideoFrameReady(int32_t bitstream_buffer_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->VideoFrameReady(bitstream_buffer_id);
}

VaapiJpegDecodeAccelerator::VaapiJpegDecodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(io_task_runner),
      decoder_thread_("VaapiJpegDecoderThread"),
      va_surface_id_(VA_INVALID_SURFACE),
      weak_this_factory_(this) {
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VaapiJpegDecodeAccelerator::~VaapiJpegDecodeAccelerator() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "Destroying VaapiJpegDecodeAccelerator";

  weak_this_factory_.InvalidateWeakPtrs();
  decoder_thread_.Stop();
}

bool VaapiJpegDecodeAccelerator::Initialize(Client* client) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  client_ = client;

  vaapi_wrapper_ =
      VaapiWrapper::Create(VaapiWrapper::kDecode, VAProfileJPEGBaseline,
                           base::Bind(&ReportToUMA, VAAPI_ERROR));

  if (!vaapi_wrapper_.get()) {
    DLOG(ERROR) << "Failed initializing VAAPI";
    return false;
  }

  if (!decoder_thread_.Start()) {
    DLOG(ERROR) << "Failed to start decoding thread.";
    return false;
  }
  decoder_task_runner_ = decoder_thread_.task_runner();

  return true;
}

bool VaapiJpegDecodeAccelerator::OutputPicture(
    VASurfaceID va_surface_id,
    int32_t input_buffer_id,
    const scoped_refptr<media::VideoFrame>& video_frame) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  TRACE_EVENT1("jpeg", "VaapiJpegDecodeAccelerator::OutputPicture",
               "input_buffer_id", input_buffer_id);

  DVLOG(3) << "Outputting VASurface " << va_surface_id
           << " into video_frame associated with input buffer id "
           << input_buffer_id;

  VAImage image;
  VAImageFormat format;
  const uint32_t kI420Fourcc = VA_FOURCC('I', '4', '2', '0');
  memset(&image, 0, sizeof(image));
  memset(&format, 0, sizeof(format));
  format.fourcc = kI420Fourcc;
  format.byte_order = VA_LSB_FIRST;
  format.bits_per_pixel = 12;  // 12 for I420

  uint8_t* mem = nullptr;
  gfx::Size coded_size = video_frame->coded_size();
  if (!vaapi_wrapper_->GetVaImage(va_surface_id, &format, coded_size, &image,
                                  reinterpret_cast<void**>(&mem))) {
    DLOG(ERROR) << "Cannot get VAImage";
    return false;
  }

  // Copy image content from VAImage to VideoFrame.
  // The component order of VAImage I420 are Y, U, and V.
  DCHECK_EQ(image.num_planes, 3u);
  DCHECK_GE(image.width, coded_size.width());
  DCHECK_GE(image.height, coded_size.height());
  const uint8_t* src_y = mem + image.offsets[0];
  const uint8_t* src_u = mem + image.offsets[1];
  const uint8_t* src_v = mem + image.offsets[2];
  size_t src_y_stride = image.pitches[0];
  size_t src_u_stride = image.pitches[1];
  size_t src_v_stride = image.pitches[2];
  uint8_t* dst_y = video_frame->data(media::VideoFrame::kYPlane);
  uint8_t* dst_u = video_frame->data(media::VideoFrame::kUPlane);
  uint8_t* dst_v = video_frame->data(media::VideoFrame::kVPlane);
  size_t dst_y_stride = video_frame->stride(media::VideoFrame::kYPlane);
  size_t dst_u_stride = video_frame->stride(media::VideoFrame::kUPlane);
  size_t dst_v_stride = video_frame->stride(media::VideoFrame::kVPlane);

  if (libyuv::I420Copy(src_y, src_y_stride,  // Y
                       src_u, src_u_stride,  // U
                       src_v, src_v_stride,  // V
                       dst_y, dst_y_stride,  // Y
                       dst_u, dst_u_stride,  // U
                       dst_v, dst_v_stride,  // V
                       coded_size.width(), coded_size.height())) {
    DLOG(ERROR) << "I420Copy failed";
    return false;
  }

  vaapi_wrapper_->ReturnVaImage(&image);

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&VaapiJpegDecodeAccelerator::VideoFrameReady,
                            weak_this_, input_buffer_id));

  return true;
}

void VaapiJpegDecodeAccelerator::DecodeTask(
    const scoped_ptr<DecodeRequest>& request) {
  DVLOG(3) << __func__;
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("jpeg", "DecodeTask");

  media::JpegParseResult parse_result;
  if (!media::ParseJpegPicture(
          reinterpret_cast<const uint8_t*>(request->shm->memory()),
          request->bitstream_buffer.size(), &parse_result)) {
    DLOG(ERROR) << "ParseJpegPicture failed";
    NotifyErrorFromDecoderThread(
        request->bitstream_buffer.id(),
        media::JpegDecodeAccelerator::PARSE_JPEG_FAILED);
    return;
  }

  // Reuse VASurface if size doesn't change.
  gfx::Size new_coded_size(parse_result.frame_header.coded_width,
                           parse_result.frame_header.coded_height);
  if (new_coded_size != coded_size_ || va_surface_id_ == VA_INVALID_SURFACE) {
    vaapi_wrapper_->DestroySurfaces();
    va_surface_id_ = VA_INVALID_SURFACE;

    std::vector<VASurfaceID> va_surfaces;
    if (!vaapi_wrapper_->CreateSurfaces(new_coded_size, 1, &va_surfaces)) {
      LOG(ERROR) << "Create VA surface failed";
      NotifyErrorFromDecoderThread(
          request->bitstream_buffer.id(),
          media::JpegDecodeAccelerator::PLATFORM_FAILURE);
      return;
    }
    va_surface_id_ = va_surfaces[0];
    coded_size_ = new_coded_size;
  }

  if (!VaapiJpegDecoder::Decode(vaapi_wrapper_.get(), parse_result,
                                va_surface_id_)) {
    LOG(ERROR) << "Decode JPEG failed";
    NotifyErrorFromDecoderThread(
        request->bitstream_buffer.id(),
        media::JpegDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }

  if (!OutputPicture(va_surface_id_, request->bitstream_buffer.id(),
                     request->video_frame)) {
    LOG(ERROR) << "Output picture failed";
    NotifyErrorFromDecoderThread(
        request->bitstream_buffer.id(),
        media::JpegDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
}

void VaapiJpegDecodeAccelerator::Decode(
    const media::BitstreamBuffer& bitstream_buffer,
    const scoped_refptr<media::VideoFrame>& video_frame) {
  DVLOG(3) << __func__;
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("jpeg", "Decode", "input_id", bitstream_buffer.id());

  DVLOG(4) << "Mapping new input buffer id: " << bitstream_buffer.id()
           << " size: " << bitstream_buffer.size();
  scoped_ptr<base::SharedMemory> shm(
      new base::SharedMemory(bitstream_buffer.handle(), true));

  if (!shm->Map(bitstream_buffer.size())) {
    LOG(ERROR) << "Failed to map input buffer";
    NotifyErrorFromDecoderThread(bitstream_buffer.id(), UNREADABLE_INPUT);
    return;
  }

  scoped_ptr<DecodeRequest> request(
      new DecodeRequest(bitstream_buffer, shm.Pass(), video_frame));

  decoder_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VaapiJpegDecodeAccelerator::DecodeTask,
                            base::Unretained(this), base::Passed(&request)));
}

}  // namespace content
