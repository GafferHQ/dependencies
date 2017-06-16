// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_media_stream_video_track_host.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/yuv_convert.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_media_stream_video_track.h"
#include "ppapi/c/ppb_video_frame.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/media_stream_buffer.h"

// IS_ALIGNED is also defined in
// third_party/webrtc/overrides/webrtc/base/basictypes.h
// TODO(ronghuawu): Avoid undef.
#undef IS_ALIGNED
#include "third_party/libyuv/include/libyuv.h"

using media::VideoFrame;
using ppapi::host::HostMessageContext;
using ppapi::MediaStreamVideoTrackShared;

namespace {

const int32_t kDefaultNumberOfBuffers = 4;
const int32_t kMaxNumberOfBuffers = 8;
// Filter mode for scaling frames.
const libyuv::FilterMode kFilterMode = libyuv::kFilterBox;

const char kPepperVideoSourceName[] = "PepperVideoSourceName";

// Default config for output mode.
const int kDefaultOutputFrameRate = 30;

media::VideoPixelFormat ToPixelFormat(PP_VideoFrame_Format format) {
  switch (format) {
    case PP_VIDEOFRAME_FORMAT_YV12:
      return media::PIXEL_FORMAT_YV12;
    case PP_VIDEOFRAME_FORMAT_I420:
      return media::PIXEL_FORMAT_I420;
    default:
      DVLOG(1) << "Unsupported pixel format " << format;
      return media::PIXEL_FORMAT_UNKNOWN;
  }
}

PP_VideoFrame_Format ToPpapiFormat(VideoFrame::Format format) {
  switch (format) {
    case VideoFrame::YV12:
      return PP_VIDEOFRAME_FORMAT_YV12;
    case VideoFrame::I420:
      return PP_VIDEOFRAME_FORMAT_I420;
    default:
      DVLOG(1) << "Unsupported pixel format " << format;
      return PP_VIDEOFRAME_FORMAT_UNKNOWN;
  }
}

VideoFrame::Format FromPpapiFormat(PP_VideoFrame_Format format) {
  switch (format) {
    case PP_VIDEOFRAME_FORMAT_YV12:
      return VideoFrame::YV12;
    case PP_VIDEOFRAME_FORMAT_I420:
      return VideoFrame::I420;
    default:
      DVLOG(1) << "Unsupported pixel format " << format;
      return VideoFrame::UNKNOWN;
  }
}

// Compute size base on the size of frame received from MediaStreamVideoSink
// and size specified by plugin.
gfx::Size GetTargetSize(const gfx::Size& source, const gfx::Size& plugin) {
  return gfx::Size(plugin.width() ? plugin.width() : source.width(),
                   plugin.height() ? plugin.height() : source.height());
}

// Compute format base on the format of frame received from MediaStreamVideoSink
// and format specified by plugin.
PP_VideoFrame_Format GetTargetFormat(PP_VideoFrame_Format source,
                                     PP_VideoFrame_Format plugin) {
  return plugin != PP_VIDEOFRAME_FORMAT_UNKNOWN ? plugin : source;
}

void ConvertFromMediaVideoFrame(const scoped_refptr<media::VideoFrame>& src,
                                PP_VideoFrame_Format dst_format,
                                const gfx::Size& dst_size,
                                uint8_t* dst) {
  CHECK(src->format() == VideoFrame::YV12 || src->format() == VideoFrame::I420);
  if (dst_format == PP_VIDEOFRAME_FORMAT_BGRA) {
    if (src->visible_rect().size() == dst_size) {
      libyuv::I420ToARGB(src->visible_data(VideoFrame::kYPlane),
                         src->stride(VideoFrame::kYPlane),
                         src->visible_data(VideoFrame::kUPlane),
                         src->stride(VideoFrame::kUPlane),
                         src->visible_data(VideoFrame::kVPlane),
                         src->stride(VideoFrame::kVPlane),
                         dst,
                         dst_size.width() * 4,
                         dst_size.width(),
                         dst_size.height());
    } else {
      media::ScaleYUVToRGB32(src->visible_data(VideoFrame::kYPlane),
                             src->visible_data(VideoFrame::kUPlane),
                             src->visible_data(VideoFrame::kVPlane),
                             dst,
                             src->visible_rect().width(),
                             src->visible_rect().height(),
                             dst_size.width(),
                             dst_size.height(),
                             src->stride(VideoFrame::kYPlane),
                             src->stride(VideoFrame::kUPlane),
                             dst_size.width() * 4,
                             media::YV12,
                             media::ROTATE_0,
                             media::FILTER_BILINEAR);
    }
  } else if (dst_format == PP_VIDEOFRAME_FORMAT_YV12 ||
             dst_format == PP_VIDEOFRAME_FORMAT_I420) {
    static const size_t kPlanesOrder[][3] = {
        {VideoFrame::kYPlane, VideoFrame::kVPlane,
         VideoFrame::kUPlane},  // YV12
        {VideoFrame::kYPlane, VideoFrame::kUPlane,
         VideoFrame::kVPlane},  // I420
    };
    const int plane_order = (dst_format == PP_VIDEOFRAME_FORMAT_YV12) ? 0 : 1;
    int dst_width = dst_size.width();
    int dst_height = dst_size.height();
    libyuv::ScalePlane(src->visible_data(kPlanesOrder[plane_order][0]),
                       src->stride(kPlanesOrder[plane_order][0]),
                       src->visible_rect().width(),
                       src->visible_rect().height(),
                       dst,
                       dst_width,
                       dst_width,
                       dst_height,
                       kFilterMode);
    dst += dst_width * dst_height;
    const int src_halfwidth = (src->visible_rect().width() + 1) >> 1;
    const int src_halfheight = (src->visible_rect().height() + 1) >> 1;
    const int dst_halfwidth = (dst_width + 1) >> 1;
    const int dst_halfheight = (dst_height + 1) >> 1;
    libyuv::ScalePlane(src->visible_data(kPlanesOrder[plane_order][1]),
                       src->stride(kPlanesOrder[plane_order][1]),
                       src_halfwidth,
                       src_halfheight,
                       dst,
                       dst_halfwidth,
                       dst_halfwidth,
                       dst_halfheight,
                       kFilterMode);
    dst += dst_halfwidth * dst_halfheight;
    libyuv::ScalePlane(src->visible_data(kPlanesOrder[plane_order][2]),
                       src->stride(kPlanesOrder[plane_order][2]),
                       src_halfwidth,
                       src_halfheight,
                       dst,
                       dst_halfwidth,
                       dst_halfwidth,
                       dst_halfheight,
                       kFilterMode);
  } else {
    NOTREACHED();
  }
}

}  // namespace

namespace content {

// Internal class used for delivering video frames on the IO-thread to
// the MediaStreamVideoSource implementation.
class PepperMediaStreamVideoTrackHost::FrameDeliverer
    : public base::RefCountedThreadSafe<FrameDeliverer> {
 public:
  FrameDeliverer(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                 const VideoCaptureDeliverFrameCB& new_frame_callback);

  void DeliverVideoFrame(const scoped_refptr<media::VideoFrame>& frame);

 private:
  friend class base::RefCountedThreadSafe<FrameDeliverer>;
  virtual ~FrameDeliverer();

  void DeliverFrameOnIO(const scoped_refptr<media::VideoFrame>& frame);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  VideoCaptureDeliverFrameCB new_frame_callback_;

  DISALLOW_COPY_AND_ASSIGN(FrameDeliverer);
};

PepperMediaStreamVideoTrackHost::FrameDeliverer::FrameDeliverer(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const VideoCaptureDeliverFrameCB& new_frame_callback)
    : io_task_runner_(io_task_runner), new_frame_callback_(new_frame_callback) {
}

PepperMediaStreamVideoTrackHost::FrameDeliverer::~FrameDeliverer() {
}

void PepperMediaStreamVideoTrackHost::FrameDeliverer::DeliverVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&FrameDeliverer::DeliverFrameOnIO, this, frame));
}

void PepperMediaStreamVideoTrackHost::FrameDeliverer::DeliverFrameOnIO(
     const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // The time when this frame is generated is unknown so give a null value to
  // |estimated_capture_time|.
  new_frame_callback_.Run(frame, base::TimeTicks());
}

PepperMediaStreamVideoTrackHost::PepperMediaStreamVideoTrackHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    const blink::WebMediaStreamTrack& track)
    : PepperMediaStreamTrackHostBase(host, instance, resource),
      track_(track),
      connected_(false),
      number_of_buffers_(kDefaultNumberOfBuffers),
      source_frame_format_(PP_VIDEOFRAME_FORMAT_UNKNOWN),
      plugin_frame_format_(PP_VIDEOFRAME_FORMAT_UNKNOWN),
      frame_data_size_(0),
      type_(kRead),
      output_started_(false),
      weak_factory_(this) {
  DCHECK(!track_.isNull());
}

PepperMediaStreamVideoTrackHost::PepperMediaStreamVideoTrackHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : PepperMediaStreamTrackHostBase(host, instance, resource),
      connected_(false),
      number_of_buffers_(kDefaultNumberOfBuffers),
      source_frame_format_(PP_VIDEOFRAME_FORMAT_UNKNOWN),
      plugin_frame_format_(PP_VIDEOFRAME_FORMAT_UNKNOWN),
      frame_data_size_(0),
      type_(kWrite),
      output_started_(false),
      weak_factory_(this) {
  InitBlinkTrack();
  DCHECK(!track_.isNull());
}

bool PepperMediaStreamVideoTrackHost::IsMediaStreamVideoTrackHost() {
  return true;
}

PepperMediaStreamVideoTrackHost::~PepperMediaStreamVideoTrackHost() {
  OnClose();
}

void PepperMediaStreamVideoTrackHost::InitBuffers() {
  gfx::Size size = GetTargetSize(source_frame_size_, plugin_frame_size_);
  DCHECK(!size.IsEmpty());

  PP_VideoFrame_Format format =
      GetTargetFormat(source_frame_format_, plugin_frame_format_);
  DCHECK_NE(format, PP_VIDEOFRAME_FORMAT_UNKNOWN);

  if (format == PP_VIDEOFRAME_FORMAT_BGRA) {
    frame_data_size_ = size.width() * size.height() * 4;
  } else {
    frame_data_size_ =
        VideoFrame::AllocationSize(FromPpapiFormat(format), size);
  }

  DCHECK_GT(frame_data_size_, 0U);
  int32_t buffer_size =
      sizeof(ppapi::MediaStreamBuffer::Video) + frame_data_size_;
  bool result = PepperMediaStreamTrackHostBase::InitBuffers(number_of_buffers_,
                                                            buffer_size,
                                                            type_);
  CHECK(result);

  if (type_ == kWrite) {
    for (int32_t i = 0; i < buffer_manager()->number_of_buffers(); ++i) {
      ppapi::MediaStreamBuffer::Video* buffer =
          &(buffer_manager()->GetBufferPointer(i)->video);
      buffer->header.size = buffer_manager()->buffer_size();
      buffer->header.type = ppapi::MediaStreamBuffer::TYPE_VIDEO;
      buffer->format = format;
      buffer->size.width = size.width();
      buffer->size.height = size.height();
      buffer->data_size = frame_data_size_;
    }

    // Make all the frames avaiable to the plugin.
    std::vector<int32_t> indices = buffer_manager()->DequeueBuffers();
    SendEnqueueBuffersMessageToPlugin(indices);
  }
}

void PepperMediaStreamVideoTrackHost::OnClose() {
  if (connected_) {
    MediaStreamVideoSink::RemoveFromVideoTrack(this, track_);
    weak_factory_.InvalidateWeakPtrs();
    connected_ = false;
  }
}

int32_t PepperMediaStreamVideoTrackHost::OnHostMsgEnqueueBuffer(
    ppapi::host::HostMessageContext* context, int32_t index) {
  if (type_ == kRead) {
    return PepperMediaStreamTrackHostBase::OnHostMsgEnqueueBuffer(context,
                                                                  index);
  } else {
    return SendFrameToTrack(index);
  }
}

int32_t PepperMediaStreamVideoTrackHost::SendFrameToTrack(int32_t index) {
  DCHECK_EQ(type_, kWrite);

  if (output_started_) {
    // Sends the frame to blink video track.
    ppapi::MediaStreamBuffer::Video* pp_frame =
        &(buffer_manager()->GetBufferPointer(index)->video);

    int32 y_stride = plugin_frame_size_.width();
    int32 uv_stride = (plugin_frame_size_.width() + 1) / 2;
    uint8* y_data = static_cast<uint8*>(pp_frame->data);
    // Default to I420
    uint8* u_data = y_data + plugin_frame_size_.GetArea();
    uint8* v_data = y_data + (plugin_frame_size_.GetArea() * 5 / 4);
    if (plugin_frame_format_ == PP_VIDEOFRAME_FORMAT_YV12) {
      // Swap u and v for YV12.
      uint8* tmp = u_data;
      u_data = v_data;
      v_data = tmp;
    }

    int64 ts_ms = static_cast<int64>(pp_frame->timestamp *
                                     base::Time::kMillisecondsPerSecond);
    scoped_refptr<VideoFrame> frame = media::VideoFrame::WrapExternalYuvData(
        FromPpapiFormat(plugin_frame_format_),
        plugin_frame_size_,
        gfx::Rect(plugin_frame_size_),
        plugin_frame_size_,
        y_stride,
        uv_stride,
        uv_stride,
        y_data,
        u_data,
        v_data,
        base::TimeDelta::FromMilliseconds(ts_ms));

    frame_deliverer_->DeliverVideoFrame(frame);
  }

  // Makes the frame available again for plugin.
  SendEnqueueBufferMessageToPlugin(index);
  return PP_OK;
}

void PepperMediaStreamVideoTrackHost::OnVideoFrame(
    const scoped_refptr<VideoFrame>& frame,
    const base::TimeTicks& estimated_capture_time) {
  DCHECK(frame.get());
  // TODO(penghuang): Check |frame->end_of_stream()| and close the track.
  PP_VideoFrame_Format ppformat = ToPpapiFormat(frame->format());
  if (ppformat == PP_VIDEOFRAME_FORMAT_UNKNOWN)
    return;

  if (source_frame_size_.IsEmpty()) {
    source_frame_size_ = frame->visible_rect().size();
    source_frame_format_ = ppformat;
    InitBuffers();
  }

  int32_t index = buffer_manager()->DequeueBuffer();
  // Drop frames if the underlying buffer is full.
  if (index < 0) {
    DVLOG(1) << "A frame is dropped.";
    return;
  }

  CHECK_EQ(ppformat, source_frame_format_) << "Frame format is changed.";

  gfx::Size size = GetTargetSize(source_frame_size_, plugin_frame_size_);
  ppformat =
      GetTargetFormat(source_frame_format_, plugin_frame_format_);
  ppapi::MediaStreamBuffer::Video* buffer =
      &(buffer_manager()->GetBufferPointer(index)->video);
  buffer->header.size = buffer_manager()->buffer_size();
  buffer->header.type = ppapi::MediaStreamBuffer::TYPE_VIDEO;
  buffer->timestamp = frame->timestamp().InSecondsF();
  buffer->format = ppformat;
  buffer->size.width = size.width();
  buffer->size.height = size.height();
  buffer->data_size = frame_data_size_;
  ConvertFromMediaVideoFrame(frame, ppformat, size, buffer->data);

  SendEnqueueBufferMessageToPlugin(index);
}

void PepperMediaStreamVideoTrackHost::GetCurrentSupportedFormats(
    int max_requested_width, int max_requested_height,
    double max_requested_frame_rate,
    const VideoCaptureDeviceFormatsCB& callback) {
  if (type_ != kWrite) {
    DVLOG(1) << "GetCurrentSupportedFormats is only supported in output mode.";
    callback.Run(media::VideoCaptureFormats());
    return;
  }

  media::VideoCaptureFormats formats;
  formats.push_back(
      media::VideoCaptureFormat(plugin_frame_size_,
                                kDefaultOutputFrameRate,
                                ToPixelFormat(plugin_frame_format_)));
  callback.Run(formats);
}

void PepperMediaStreamVideoTrackHost::StartSourceImpl(
    const media::VideoCaptureFormat& format,
    const blink::WebMediaConstraints& constraints,
    const VideoCaptureDeliverFrameCB& frame_callback) {
  output_started_ = true;
  frame_deliverer_ = new FrameDeliverer(io_task_runner(), frame_callback);
}

void PepperMediaStreamVideoTrackHost::StopSourceImpl() {
  output_started_ = false;
  frame_deliverer_ = NULL;
}

void PepperMediaStreamVideoTrackHost::DidConnectPendingHostToResource() {
  if (!connected_) {
    MediaStreamVideoSink::AddToVideoTrack(
        this,
        media::BindToCurrentLoop(
            base::Bind(
                &PepperMediaStreamVideoTrackHost::OnVideoFrame,
                weak_factory_.GetWeakPtr())),
        track_);
    connected_ = true;
  }
}

int32_t PepperMediaStreamVideoTrackHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperMediaStreamVideoTrackHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_MediaStreamVideoTrack_Configure, OnHostMsgConfigure)
  PPAPI_END_MESSAGE_MAP()
  return PepperMediaStreamTrackHostBase::OnResourceMessageReceived(msg,
                                                                   context);
}

int32_t PepperMediaStreamVideoTrackHost::OnHostMsgConfigure(
    HostMessageContext* context,
    const MediaStreamVideoTrackShared::Attributes& attributes) {
  CHECK(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

  bool changed = false;
  gfx::Size new_size(attributes.width, attributes.height);
  if (GetTargetSize(source_frame_size_, plugin_frame_size_) !=
      GetTargetSize(source_frame_size_, new_size)) {
    changed = true;
  }
  plugin_frame_size_ = new_size;

  int32_t buffers = attributes.buffers
                        ? std::min(kMaxNumberOfBuffers, attributes.buffers)
                        : kDefaultNumberOfBuffers;
  if (buffers != number_of_buffers_)
    changed = true;
  number_of_buffers_ = buffers;

  if (GetTargetFormat(source_frame_format_, plugin_frame_format_) !=
      GetTargetFormat(source_frame_format_, attributes.format)) {
    changed = true;
  }
  plugin_frame_format_ = attributes.format;

  // If the first frame has been received, we will re-initialize buffers with
  // new settings. Otherwise, we will initialize buffer when we receive
  // the first frame, because plugin can only provide part of attributes
  // which are not enough to initialize buffers.
  if (changed && (type_ == kWrite || !source_frame_size_.IsEmpty()))
    InitBuffers();

  // TODO(ronghuawu): Ask the owner of DOMMediaStreamTrackToResource why
  // source id instead of track id is used there.
  const std::string id = track_.source().id().utf8();
  context->reply_msg = PpapiPluginMsg_MediaStreamVideoTrack_ConfigureReply(id);
  return PP_OK;
}

void PepperMediaStreamVideoTrackHost::InitBlinkTrack() {
  std::string source_id;
  base::Base64Encode(base::RandBytesAsString(64), &source_id);
  blink::WebMediaStreamSource webkit_source;
  webkit_source.initialize(base::UTF8ToUTF16(source_id),
                           blink::WebMediaStreamSource::TypeVideo,
                           base::UTF8ToUTF16(kPepperVideoSourceName),
                           false /* remote */, true /* readonly */);
  webkit_source.setExtraData(this);

  const bool enabled = true;
  blink::WebMediaConstraints constraints;
  constraints.initialize();
  track_ = MediaStreamVideoTrack::CreateVideoTrack(
       this, constraints,
       base::Bind(
           &PepperMediaStreamVideoTrackHost::OnTrackStarted,
           base::Unretained(this)),
       enabled);
}

void PepperMediaStreamVideoTrackHost::OnTrackStarted(
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DVLOG(3) << "OnTrackStarted result: " << result;
}

}  // namespace content
