// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_device_client.h"

#include <algorithm>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_gpu_jpeg_decoder.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_frame.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/libyuv/include/libyuv.h"

using media::VideoCaptureFormat;
using media::VideoFrame;
using media::VideoFrameMetadata;

namespace content {

namespace {

#if !defined(OS_ANDROID)
// Modelled after GpuProcessTransportFactory::CreateContextCommon().
scoped_ptr<content::WebGraphicsContext3DCommandBufferImpl> CreateContextCommon(
    scoped_refptr<content::GpuChannelHost> gpu_channel_host,
    int surface_id) {
  if (!content::GpuDataManagerImpl::GetInstance()->
        CanUseGpuBrowserCompositor()) {
    DLOG(ERROR) << "No accelerated graphics found. Check chrome://gpu";
    return scoped_ptr<content::WebGraphicsContext3DCommandBufferImpl>();
  }
  blink::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  attrs.depth = false;
  attrs.stencil = false;
  attrs.antialias = false;
  attrs.noAutomaticFlushes = true;

  if (!gpu_channel_host.get()) {
    DLOG(ERROR) << "Failed to establish GPU channel.";
    return scoped_ptr<content::WebGraphicsContext3DCommandBufferImpl>();
  }
  GURL url("chrome://gpu/GpuProcessTransportFactory::CreateCaptureContext");
  return make_scoped_ptr(
      new WebGraphicsContext3DCommandBufferImpl(
          surface_id,
          url,
          gpu_channel_host.get(),
          attrs,
          true  /* lose_context_when_out_of_memory */,
          content::WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits(),
          NULL));
}

// Modelled after
// GpuProcessTransportFactory::CreateOffscreenCommandBufferContext().
scoped_ptr<content::WebGraphicsContext3DCommandBufferImpl>
CreateOffscreenCommandBufferContext() {
  content::CauseForGpuLaunch cause = content::CAUSE_FOR_GPU_LAUNCH_CANVAS_2D;
  // Android does not support synchronous opening of GPU channels. Should use
  // EstablishGpuChannel() instead.
  if (!content::BrowserGpuChannelHostFactory::instance())
    return scoped_ptr<content::WebGraphicsContext3DCommandBufferImpl>();
  scoped_refptr<content::GpuChannelHost> gpu_channel_host(
      content::BrowserGpuChannelHostFactory::instance()->
          EstablishGpuChannelSync(cause));
  DCHECK(gpu_channel_host);
  return CreateContextCommon(gpu_channel_host, 0);
}
#endif

typedef base::Callback<void(scoped_refptr<ContextProviderCommandBuffer>)>
    ProcessContextCallback;

void CreateContextOnUIThread(ProcessContextCallback bottom_half) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if !defined(OS_ANDROID)
  bottom_half.Run(ContextProviderCommandBuffer::Create(
      CreateOffscreenCommandBufferContext(), OFFSCREEN_VIDEO_CAPTURE_CONTEXT));
  return;
#endif
}

void ResetLostContextCallback(
    const scoped_refptr<ContextProviderCommandBuffer>& capture_thread_context) {
  capture_thread_context->SetLostContextCallback(
      cc::ContextProvider::LostContextCallback());
}

}  // anonymous namespace

// Class combining a Client::Buffer interface implementation and a pool buffer
// implementation to guarantee proper cleanup on destruction on our side.
class AutoReleaseBuffer : public media::VideoCaptureDevice::Client::Buffer {
 public:
  AutoReleaseBuffer(const scoped_refptr<VideoCaptureBufferPool>& pool,
                    int buffer_id)
      : id_(buffer_id),
        pool_(pool),
        buffer_handle_(pool_->GetBufferHandle(buffer_id).Pass()) {
    DCHECK(pool_.get());
  }
  int id() const override { return id_; }
  size_t size() const override { return buffer_handle_->size(); }
  void* data() override { return buffer_handle_->data(); }
  ClientBuffer AsClientBuffer() override {
    return buffer_handle_->AsClientBuffer();
  }
#if defined(OS_POSIX)
  base::FileDescriptor AsPlatformFile() override {
    return buffer_handle_->AsPlatformFile();
  }
#endif

 private:
  ~AutoReleaseBuffer() override { pool_->RelinquishProducerReservation(id_); }

  const int id_;
  const scoped_refptr<VideoCaptureBufferPool> pool_;
  const scoped_ptr<VideoCaptureBufferPool::BufferHandle> buffer_handle_;
};

// Internal ref-counted class wrapping an incoming GpuMemoryBuffer into a
// Texture backed VideoFrame. This VideoFrame creation is balanced by a waiting
// on the associated |sync_point|. After VideoFrame consumption the inserted
// ReleaseCallback() will be called, where the Texture is destroyed.
//
// This class jumps between threads due to GPU-related thread limitations, i.e.
// some objects cannot be accessed from IO Thread whereas others need to be
// constructed on UI Thread. For this reason most of the operations are carried
// out on Capture Thread (|capture_task_runner_|).
class VideoCaptureDeviceClient::TextureWrapHelper final
    : public base::RefCountedThreadSafe<TextureWrapHelper> {
 public:
  TextureWrapHelper(
      const base::WeakPtr<VideoCaptureController>& controller,
      const scoped_refptr<base::SingleThreadTaskRunner>& capture_task_runner);

  // Wraps the GpuMemoryBuffer-backed |buffer| into a Texture, and sends it to
  // |controller_| wrapped in a VideoFrame.
  void OnIncomingCapturedGpuMemoryBuffer(
      scoped_ptr<media::VideoCaptureDevice::Client::Buffer> buffer,
      const media::VideoCaptureFormat& frame_format,
      const base::TimeTicks& timestamp);

 private:
  friend class base::RefCountedThreadSafe<TextureWrapHelper>;
  ~TextureWrapHelper();

  // Creates some necessary members in |capture_task_runner_|.
  void Init();
  // Runs the bottom half of the GlHelper creation.
  void CreateGlHelper(
      scoped_refptr<ContextProviderCommandBuffer> capture_thread_context);

  // Recycles |memory_buffer|, deletes Image and Texture on VideoFrame release.
  void ReleaseCallback(GLuint image_id,
                       GLuint texture_id,
                       uint32 sync_point);

  // The Command Buffer lost the GL context, f.i. GPU process crashed. Signal
  // error to our owner so the capture can be torn down.
  void LostContextCallback();

  // Prints the error |message| and notifies |controller_| of an error.
  void OnError(const std::string& message);

  // |controller_| should only be used on IO thread.
  const base::WeakPtr<VideoCaptureController> controller_;
  const scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;

  // Command buffer reference, needs to be destroyed when unused. It is created
  // on UI Thread and bound to Capture Thread. In particular, it cannot be used
  // from IO Thread.
  scoped_refptr<ContextProviderCommandBuffer> capture_thread_context_;
  // Created and used from Capture Thread. Cannot be used from IO Thread.
  scoped_ptr<GLHelper> gl_helper_;

  DISALLOW_COPY_AND_ASSIGN(TextureWrapHelper);
};

VideoCaptureDeviceClient::VideoCaptureDeviceClient(
    const base::WeakPtr<VideoCaptureController>& controller,
    const scoped_refptr<VideoCaptureBufferPool>& buffer_pool,
    const scoped_refptr<base::SingleThreadTaskRunner>& capture_task_runner)
    : controller_(controller),
      external_jpeg_decoder_initialized_(false),
      buffer_pool_(buffer_pool),
      capture_task_runner_(capture_task_runner),
      last_captured_pixel_format_(media::PIXEL_FORMAT_UNKNOWN) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

VideoCaptureDeviceClient::~VideoCaptureDeviceClient() {
  // This should be on the platform auxiliary thread since
  // |external_jpeg_decoder_| need to be destructed on the same thread as
  // OnIncomingCapturedData.
}

void VideoCaptureDeviceClient::OnIncomingCapturedData(
    const uint8* data,
    int length,
    const VideoCaptureFormat& frame_format,
    int rotation,
    const base::TimeTicks& timestamp) {
  TRACE_EVENT0("video", "VideoCaptureDeviceClient::OnIncomingCapturedData");
  DCHECK_EQ(media::PIXEL_STORAGE_CPU, frame_format.pixel_storage);

  if (last_captured_pixel_format_ != frame_format.pixel_format) {
    OnLog("Pixel format: " +
          VideoCaptureFormat::PixelFormatToString(frame_format.pixel_format));
    last_captured_pixel_format_ = frame_format.pixel_format;

    if (frame_format.pixel_format == media::PIXEL_FORMAT_MJPEG &&
        VideoCaptureGpuJpegDecoder::Supported()) {
      if (!external_jpeg_decoder_initialized_) {
        external_jpeg_decoder_initialized_ = true;
        // base::Unretained is safe because |this| outlives
        // |external_jpeg_decoder_| and the callbacks are never called after
        // |external_jpeg_decoder_| is destroyed.
        external_jpeg_decoder_.reset(new VideoCaptureGpuJpegDecoder(
            base::Bind(
                &VideoCaptureController::DoIncomingCapturedVideoFrameOnIOThread,
                controller_),
            // TODO(kcwu): fallback to software decode if error.
            // https://crbug.com/503532
            base::Bind(&VideoCaptureDeviceClient::OnError,
                       base::Unretained(this))));
        external_jpeg_decoder_->Initialize();
      }
    }
  }

  if (!frame_format.IsValid())
    return;

  // |chopped_{width,height} and |new_unrotated_{width,height}| are the lowest
  // bit decomposition of {width, height}, grabbing the odd and even parts.
  const int chopped_width = frame_format.frame_size.width() & 1;
  const int chopped_height = frame_format.frame_size.height() & 1;
  const int new_unrotated_width = frame_format.frame_size.width() & ~1;
  const int new_unrotated_height = frame_format.frame_size.height() & ~1;

  int destination_width = new_unrotated_width;
  int destination_height = new_unrotated_height;
  if (rotation == 90 || rotation == 270)
    std::swap(destination_width, destination_height);

  DCHECK_EQ(0, rotation % 90)
      << " Rotation must be a multiple of 90, now: " << rotation;
  libyuv::RotationMode rotation_mode = libyuv::kRotate0;
  if (rotation == 90)
    rotation_mode = libyuv::kRotate90;
  else if (rotation == 180)
    rotation_mode = libyuv::kRotate180;
  else if (rotation == 270)
    rotation_mode = libyuv::kRotate270;

  const gfx::Size dimensions(destination_width, destination_height);
  if (!VideoFrame::IsValidConfig(VideoFrame::I420,
                                 VideoFrame::STORAGE_UNKNOWN,
                                 dimensions,
                                 gfx::Rect(dimensions),
                                 dimensions)) {
    return;
  }

  scoped_ptr<Buffer> buffer(ReserveOutputBuffer(
      dimensions, media::PIXEL_FORMAT_I420, media::PIXEL_STORAGE_CPU));
  if (!buffer.get())
    return;

  const size_t y_plane_size = VideoFrame::PlaneSize(
      VideoFrame::I420, VideoFrame::kYPlane, dimensions).GetArea();
  const size_t u_plane_size = VideoFrame::PlaneSize(
      VideoFrame::I420, VideoFrame::kUPlane, dimensions).GetArea();
  uint8* const yplane = reinterpret_cast<uint8*>(buffer->data());
  uint8* const uplane = yplane + y_plane_size;
  uint8* const vplane = uplane + u_plane_size;

  const int yplane_stride = dimensions.width();
  const int uv_plane_stride = yplane_stride / 2;
  int crop_x = 0;
  int crop_y = 0;
  libyuv::FourCC origin_colorspace = libyuv::FOURCC_ANY;

  bool flip = false;
  switch (frame_format.pixel_format) {
    case media::PIXEL_FORMAT_UNKNOWN:  // Color format not set.
      break;
    case media::PIXEL_FORMAT_I420:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_I420;
      break;
    case media::PIXEL_FORMAT_YV12:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_YV12;
      break;
    case media::PIXEL_FORMAT_NV12:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_NV12;
      break;
    case media::PIXEL_FORMAT_NV21:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_NV21;
      break;
    case media::PIXEL_FORMAT_YUY2:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_YUY2;
      break;
    case media::PIXEL_FORMAT_UYVY:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_UYVY;
      break;
    case media::PIXEL_FORMAT_RGB24:
      origin_colorspace = libyuv::FOURCC_24BG;
#if defined(OS_WIN)
      // TODO(wjia): Currently, for RGB24 on WIN, capture device always
      // passes in positive src_width and src_height. Remove this hardcoded
      // value when nagative src_height is supported. The negative src_height
      // indicates that vertical flipping is needed.
      flip = true;
#endif
      break;
    case media::PIXEL_FORMAT_RGB32:
// Fallback to PIXEL_FORMAT_ARGB setting |flip| in Windows platforms.
#if defined(OS_WIN)
      flip = true;
#endif
    case media::PIXEL_FORMAT_ARGB:
      origin_colorspace = libyuv::FOURCC_ARGB;
      break;
    case media::PIXEL_FORMAT_MJPEG:
      origin_colorspace = libyuv::FOURCC_MJPG;
      break;
    default:
      NOTREACHED();
  }

  // The input |length| can be greater than the required buffer size because of
  // paddings and/or alignments, but it cannot be smaller.
  DCHECK_GE(static_cast<size_t>(length), frame_format.ImageAllocationSize());

  if (external_jpeg_decoder_ &&
      frame_format.pixel_format == media::PIXEL_FORMAT_MJPEG && rotation == 0 &&
      !flip && external_jpeg_decoder_->ReadyToDecode()) {
    external_jpeg_decoder_->DecodeCapturedData(data, length, frame_format,
                                               timestamp, buffer.Pass());
    return;
  }

  if (libyuv::ConvertToI420(data,
                            length,
                            yplane,
                            yplane_stride,
                            uplane,
                            uv_plane_stride,
                            vplane,
                            uv_plane_stride,
                            crop_x,
                            crop_y,
                            frame_format.frame_size.width(),
                            (flip ? -1 : 1) * frame_format.frame_size.height(),
                            new_unrotated_width,
                            new_unrotated_height,
                            rotation_mode,
                            origin_colorspace) != 0) {
    DLOG(WARNING) << "Failed to convert buffer's pixel format to I420 from "
                  << VideoCaptureFormat::PixelFormatToString(
                         frame_format.pixel_format);
    return;
  }

  const VideoCaptureFormat output_format = VideoCaptureFormat(
      dimensions, frame_format.frame_rate, media::PIXEL_FORMAT_I420,
      media::PIXEL_STORAGE_CPU);
  OnIncomingCapturedBuffer(buffer.Pass(), output_format, timestamp);
}

void
VideoCaptureDeviceClient::OnIncomingCapturedYuvData(
    const uint8* y_data,
    const uint8* u_data,
    const uint8* v_data,
    size_t y_stride,
    size_t u_stride,
    size_t v_stride,
    const VideoCaptureFormat& frame_format,
    int clockwise_rotation,
    const base::TimeTicks& timestamp) {
  TRACE_EVENT0("video", "VideoCaptureDeviceClient::OnIncomingCapturedYuvData");
  DCHECK_EQ(media::PIXEL_FORMAT_I420, frame_format.pixel_format);
  DCHECK_EQ(media::PIXEL_STORAGE_CPU, frame_format.pixel_storage);
  DCHECK_EQ(0, clockwise_rotation) << "Rotation not supported";

  scoped_ptr<Buffer> buffer(ReserveOutputBuffer(frame_format.frame_size,
                                                frame_format.pixel_format,
                                                frame_format.pixel_storage));
  if (!buffer.get())
    return;

  // Blit (copy) here from y,u,v into buffer.data()). Needed so we can return
  // the parameter buffer synchronously to the driver.
  const size_t y_plane_size = VideoFrame::PlaneSize(
      VideoFrame::I420, VideoFrame::kYPlane, frame_format.frame_size).GetArea();
  const size_t u_plane_size = VideoFrame::PlaneSize(
      VideoFrame::I420, VideoFrame::kUPlane, frame_format.frame_size).GetArea();
  uint8* const dst_y = reinterpret_cast<uint8*>(buffer->data());
  uint8* const dst_u = dst_y + y_plane_size;
  uint8* const dst_v = dst_u + u_plane_size;

  const size_t dst_y_stride = VideoFrame::RowBytes(
      VideoFrame::kYPlane, VideoFrame::I420, frame_format.frame_size.width());
  const size_t dst_u_stride = VideoFrame::RowBytes(
      VideoFrame::kUPlane, VideoFrame::I420, frame_format.frame_size.width());
  const size_t dst_v_stride = VideoFrame::RowBytes(
      VideoFrame::kVPlane, VideoFrame::I420, frame_format.frame_size.width());
  DCHECK_GE(y_stride, dst_y_stride);
  DCHECK_GE(u_stride, dst_u_stride);
  DCHECK_GE(v_stride, dst_v_stride);

  if (libyuv::I420Copy(y_data, y_stride,
                       u_data, u_stride,
                       v_data, v_stride,
                       dst_y, dst_y_stride,
                       dst_u, dst_u_stride,
                       dst_v, dst_v_stride,
                       frame_format.frame_size.width(),
                       frame_format.frame_size.height())) {
    DLOG(WARNING) << "Failed to copy buffer";
    return;
  }

  OnIncomingCapturedBuffer(buffer.Pass(), frame_format, timestamp);
};

scoped_ptr<media::VideoCaptureDevice::Client::Buffer>
VideoCaptureDeviceClient::ReserveOutputBuffer(
    const gfx::Size& frame_size,
    media::VideoPixelFormat pixel_format,
    media::VideoPixelStorage pixel_storage) {
  DCHECK(pixel_format == media::PIXEL_FORMAT_I420 ||
         pixel_format == media::PIXEL_FORMAT_ARGB);
  DCHECK_GT(frame_size.width(), 0);
  DCHECK_GT(frame_size.height(), 0);

  if (pixel_storage == media::PIXEL_STORAGE_GPUMEMORYBUFFER &&
      !texture_wrap_helper_) {
    texture_wrap_helper_ =
        new TextureWrapHelper(controller_, capture_task_runner_);
  }

  // TODO(mcasas): For PIXEL_STORAGE_GPUMEMORYBUFFER, find a way to indicate if
  // it's a ShMem GMB or a DmaBuf GMB.
  int buffer_id_to_drop = VideoCaptureBufferPool::kInvalidId;
  const int buffer_id = buffer_pool_->ReserveForProducer(
      pixel_format, pixel_storage, frame_size, &buffer_id_to_drop);
  if (buffer_id == VideoCaptureBufferPool::kInvalidId)
    return NULL;

  scoped_ptr<media::VideoCaptureDevice::Client::Buffer> output_buffer(
      new AutoReleaseBuffer(buffer_pool_, buffer_id));

  if (buffer_id_to_drop != VideoCaptureBufferPool::kInvalidId) {
    BrowserThread::PostTask(BrowserThread::IO,
        FROM_HERE,
        base::Bind(&VideoCaptureController::DoBufferDestroyedOnIOThread,
                   controller_, buffer_id_to_drop));
  }

  return output_buffer.Pass();
}

void VideoCaptureDeviceClient::OnIncomingCapturedBuffer(
    scoped_ptr<Buffer> buffer,
    const VideoCaptureFormat& frame_format,
    const base::TimeTicks& timestamp) {
  if (frame_format.pixel_storage == media::PIXEL_STORAGE_GPUMEMORYBUFFER) {
    capture_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&TextureWrapHelper::OnIncomingCapturedGpuMemoryBuffer,
                   texture_wrap_helper_,
                   base::Passed(&buffer),
                   frame_format,
                   timestamp));
  } else {
    DCHECK(frame_format.pixel_format == media::PIXEL_FORMAT_I420 ||
           frame_format.pixel_format == media::PIXEL_FORMAT_ARGB);
    scoped_refptr<VideoFrame> video_frame =
        VideoFrame::WrapExternalData(
            VideoFrame::I420,
            frame_format.frame_size,
            gfx::Rect(frame_format.frame_size),
            frame_format.frame_size,
            reinterpret_cast<uint8*>(buffer->data()),
            VideoFrame::AllocationSize(VideoFrame::I420,
                                       frame_format.frame_size),
            base::TimeDelta());
    DCHECK(video_frame.get());
    video_frame->metadata()->SetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                       frame_format.frame_rate);
    OnIncomingCapturedVideoFrame(buffer.Pass(), video_frame, timestamp);
  }
}

void VideoCaptureDeviceClient::OnIncomingCapturedVideoFrame(
    scoped_ptr<Buffer> buffer,
    const scoped_refptr<VideoFrame>& frame,
    const base::TimeTicks& timestamp) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &VideoCaptureController::DoIncomingCapturedVideoFrameOnIOThread,
          controller_,
          base::Passed(&buffer),
          frame,
          timestamp));
}

void VideoCaptureDeviceClient::OnError(
    const std::string& reason) {
  const std::string log_message = base::StringPrintf(
      "Error on video capture: %s, OS message: %s",
      reason.c_str(),
      logging::SystemErrorCodeToString(
          logging::GetLastSystemErrorCode()).c_str());
  DLOG(ERROR) << log_message;
  OnLog(log_message);
  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoErrorOnIOThread, controller_));
}

void VideoCaptureDeviceClient::OnLog(
    const std::string& message) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&VideoCaptureController::DoLogOnIOThread,
                                     controller_, message));
}

double VideoCaptureDeviceClient::GetBufferPoolUtilization() const {
  // VideoCaptureBufferPool::GetBufferPoolUtilization() is thread-safe.
  return buffer_pool_->GetBufferPoolUtilization();
}

VideoCaptureDeviceClient::TextureWrapHelper::TextureWrapHelper(
    const base::WeakPtr<VideoCaptureController>& controller,
    const scoped_refptr<base::SingleThreadTaskRunner>& capture_task_runner)
  : controller_(controller),
    capture_task_runner_(capture_task_runner) {
  capture_task_runner_->PostTask(FROM_HERE,
      base::Bind(&TextureWrapHelper::Init, this));
}

void
VideoCaptureDeviceClient::TextureWrapHelper::OnIncomingCapturedGpuMemoryBuffer(
        scoped_ptr<media::VideoCaptureDevice::Client::Buffer> buffer,
        const media::VideoCaptureFormat& frame_format,
        const base::TimeTicks& timestamp) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(media::PIXEL_FORMAT_ARGB, frame_format.pixel_format);
  DCHECK_EQ(media::PIXEL_STORAGE_GPUMEMORYBUFFER, frame_format.pixel_storage);
  if (!gl_helper_) {
    // |gl_helper_| might not exist due to asynchronous initialization not
    // finished or due to termination in process after a context loss.
    DVLOG(1) << " Skipping ingress frame, no GL context.";
    return;
  }

  gpu::gles2::GLES2Interface* gl = capture_thread_context_->ContextGL();
  GLuint image_id = gl->CreateImageCHROMIUM(buffer->AsClientBuffer(),
                                            frame_format.frame_size.width(),
                                            frame_format.frame_size.height(),
                                            GL_BGRA_EXT);
  DCHECK(image_id);

  const GLuint texture_id = gl_helper_->CreateTexture();
  DCHECK(texture_id);
  {
    content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl, texture_id);
    gl->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id);
  }

  const gpu::MailboxHolder& mailbox_holder(
      gl_helper_->ProduceMailboxHolderFromTexture(texture_id));
  DCHECK(!mailbox_holder.mailbox.IsZero());
  DCHECK(mailbox_holder.mailbox.Verify());
  DCHECK(mailbox_holder.texture_target);
  DCHECK(mailbox_holder.sync_point);

  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::WrapNativeTexture(
          media::VideoFrame::ARGB,
          mailbox_holder,
          media::BindToCurrentLoop(base::Bind(
              &VideoCaptureDeviceClient::TextureWrapHelper::ReleaseCallback,
              this, image_id, texture_id)),
          frame_format.frame_size, gfx::Rect(frame_format.frame_size),
          frame_format.frame_size, base::TimeDelta());
  video_frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY, true);
  video_frame->metadata()->SetDouble(VideoFrameMetadata::FRAME_RATE,
                                     frame_format.frame_rate);
#if defined(OS_LINUX)
// TODO(mcasas): After http://crev.com/1179323002, use |frame_format| to query
// the storage type of the buffer and use the appropriate |video_frame| method.
#if defined(USE_OZONE)
  DCHECK_EQ(1u, media::VideoFrame::NumPlanes(video_frame->format()));
  video_frame->DuplicateFileDescriptors(
      std::vector<int>(1, buffer->AsPlatformFile().fd));
#else
   video_frame->AddSharedMemoryHandle(buffer->AsPlatformFile());
#endif

#endif
  //TODO(mcasas): use AddSharedMemoryHandle() for gfx::SHARED_MEMORY_BUFFER.

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &VideoCaptureController::DoIncomingCapturedVideoFrameOnIOThread,
          controller_, base::Passed(&buffer), video_frame, timestamp));
}

VideoCaptureDeviceClient::TextureWrapHelper::~TextureWrapHelper() {
  // Might not be running on capture_task_runner_'s thread. Ensure owned objects
  // are destroyed on the correct threads.
  if (gl_helper_)
    capture_task_runner_->DeleteSoon(FROM_HERE, gl_helper_.release());

  if (capture_thread_context_) {
    capture_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ResetLostContextCallback, capture_thread_context_));
    capture_thread_context_->AddRef();
    ContextProviderCommandBuffer* raw_capture_thread_context =
        capture_thread_context_.get();
    capture_thread_context_ = nullptr;
    capture_task_runner_->ReleaseSoon(FROM_HERE, raw_capture_thread_context);
  }
}

void VideoCaptureDeviceClient::TextureWrapHelper::Init() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  // In threaded compositing mode, we have to create our own context for Capture
  // to avoid using the GPU command queue from multiple threads. Context
  // creation must happen on UI thread; then the context needs to be bound to
  // the appropriate thread, which is done in CreateGlHelper().
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &CreateContextOnUIThread,
          media::BindToCurrentLoop(base::Bind(
              &VideoCaptureDeviceClient::TextureWrapHelper::CreateGlHelper,
              this))));
}

void VideoCaptureDeviceClient::TextureWrapHelper::CreateGlHelper(
    scoped_refptr<ContextProviderCommandBuffer> capture_thread_context) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (!capture_thread_context.get()) {
    DLOG(ERROR) << "No offscreen GL Context!";
    return;
  }
  // This may not happen in IO Thread. The destructor resets the context lost
  // callback, so base::Unretained is safe; otherwise it'd be a circular ref
  // counted dependency.
  capture_thread_context->SetLostContextCallback(media::BindToCurrentLoop(
      base::Bind(
          &VideoCaptureDeviceClient::TextureWrapHelper::LostContextCallback,
          base::Unretained(this))));
  if (!capture_thread_context->BindToCurrentThread()) {
    capture_thread_context = NULL;
    DLOG(ERROR) << "Couldn't bind the Capture Context to the Capture Thread.";
    return;
  }
  DCHECK(capture_thread_context);
  capture_thread_context_ = capture_thread_context;

  // At this point, |capture_thread_context| is a cc::ContextProvider. Creation
  // of our GLHelper should happen on Capture Thread.
  gl_helper_.reset(new GLHelper(capture_thread_context->ContextGL(),
                                capture_thread_context->ContextSupport()));
  DCHECK(gl_helper_);
}

void VideoCaptureDeviceClient::TextureWrapHelper::ReleaseCallback(
    GLuint image_id,
    GLuint texture_id,
    uint32 sync_point) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (gl_helper_) {
    gl_helper_->DeleteTexture(texture_id);
    capture_thread_context_->ContextGL()->DestroyImageCHROMIUM(image_id);
  }
}

void VideoCaptureDeviceClient::TextureWrapHelper::LostContextCallback() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  // Prevent incoming frames from being processed while OnError gets groked.
  gl_helper_.reset();
  OnError("GLContext lost");
}

void VideoCaptureDeviceClient::TextureWrapHelper::OnError(
    const std::string& message) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DLOG(ERROR) << message;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureController::DoErrorOnIOThread, controller_));
}

}  // namespace content
