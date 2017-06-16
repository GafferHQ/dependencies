// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/video_decoder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/values.h"
#include "media/base/video_util.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
// VPX_CODEC_DISABLE_COMPAT excludes parts of the libvpx API that provide
// backwards compatibility for legacy applications using the library.
#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace cast {

// Base class that handles the common problem of detecting dropped frames, and
// then invoking the Decode() method implemented by the subclasses to convert
// the encoded payload data into a usable video frame.
class VideoDecoder::ImplBase
    : public base::RefCountedThreadSafe<VideoDecoder::ImplBase> {
 public:
  ImplBase(const scoped_refptr<CastEnvironment>& cast_environment,
           Codec codec)
      : cast_environment_(cast_environment),
        codec_(codec),
        operational_status_(STATUS_UNINITIALIZED),
        seen_first_frame_(false) {}

  OperationalStatus InitializationResult() const {
    return operational_status_;
  }

  void DecodeFrame(scoped_ptr<EncodedFrame> encoded_frame,
                   const DecodeFrameCallback& callback) {
    DCHECK_EQ(operational_status_, STATUS_INITIALIZED);

    static_assert(sizeof(encoded_frame->frame_id) == sizeof(last_frame_id_),
                  "size of frame_id types do not match");
    bool is_continuous = true;
    if (seen_first_frame_) {
      const uint32 frames_ahead = encoded_frame->frame_id - last_frame_id_;
      if (frames_ahead > 1) {
        RecoverBecauseFramesWereDropped();
        is_continuous = false;
      }
    } else {
      seen_first_frame_ = true;
    }
    last_frame_id_ = encoded_frame->frame_id;

    const scoped_refptr<VideoFrame> decoded_frame = Decode(
        encoded_frame->mutable_bytes(),
        static_cast<int>(encoded_frame->data.size()));
    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(callback, decoded_frame, is_continuous));
  }

 protected:
  friend class base::RefCountedThreadSafe<ImplBase>;
  virtual ~ImplBase() {}

  virtual void RecoverBecauseFramesWereDropped() {}

  // Note: Implementation of Decode() is allowed to mutate |data|.
  virtual scoped_refptr<VideoFrame> Decode(uint8* data, int len) = 0;

  const scoped_refptr<CastEnvironment> cast_environment_;
  const Codec codec_;

  // Subclass' ctor is expected to set this to STATUS_INITIALIZED.
  OperationalStatus operational_status_;

 private:
  bool seen_first_frame_;
  uint32 last_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(ImplBase);
};

class VideoDecoder::Vp8Impl : public VideoDecoder::ImplBase {
 public:
  explicit Vp8Impl(const scoped_refptr<CastEnvironment>& cast_environment)
      : ImplBase(cast_environment, CODEC_VIDEO_VP8) {
    if (ImplBase::operational_status_ != STATUS_UNINITIALIZED)
      return;

    vpx_codec_dec_cfg_t cfg = {0};
    // TODO(miu): Revisit this for typical multi-core desktop use case.  This
    // feels like it should be 4 or 8.
    cfg.threads = 1;

    DCHECK(vpx_codec_get_caps(vpx_codec_vp8_dx()) & VPX_CODEC_CAP_POSTPROC);
    if (vpx_codec_dec_init(&context_,
                           vpx_codec_vp8_dx(),
                           &cfg,
                           VPX_CODEC_USE_POSTPROC) != VPX_CODEC_OK) {
      ImplBase::operational_status_ = STATUS_INVALID_CONFIGURATION;
      return;
    }
    ImplBase::operational_status_ = STATUS_INITIALIZED;
  }

 private:
  ~Vp8Impl() final {
    if (ImplBase::operational_status_ == STATUS_INITIALIZED)
      CHECK_EQ(VPX_CODEC_OK, vpx_codec_destroy(&context_));
  }

  scoped_refptr<VideoFrame> Decode(uint8* data, int len) final {
    if (len <= 0 || vpx_codec_decode(&context_,
                                     data,
                                     static_cast<unsigned int>(len),
                                     NULL,
                                     0) != VPX_CODEC_OK) {
      return NULL;
    }

    vpx_codec_iter_t iter = NULL;
    vpx_image_t* const image = vpx_codec_get_frame(&context_, &iter);
    if (!image)
      return NULL;
    if (image->fmt != VPX_IMG_FMT_I420 && image->fmt != VPX_IMG_FMT_YV12) {
      NOTREACHED();
      return NULL;
    }
    DCHECK(vpx_codec_get_frame(&context_, &iter) == NULL)
        << "Should have only decoded exactly one frame.";

    const gfx::Size frame_size(image->d_w, image->d_h);
    // Note: Timestamp for the VideoFrame will be set in VideoReceiver.
    const scoped_refptr<VideoFrame> decoded_frame =
        VideoFrame::CreateFrame(VideoFrame::YV12,
                                frame_size,
                                gfx::Rect(frame_size),
                                frame_size,
                                base::TimeDelta());
    CopyYPlane(image->planes[VPX_PLANE_Y],
               image->stride[VPX_PLANE_Y],
               image->d_h,
               decoded_frame.get());
    CopyUPlane(image->planes[VPX_PLANE_U],
               image->stride[VPX_PLANE_U],
               (image->d_h + 1) / 2,
               decoded_frame.get());
    CopyVPlane(image->planes[VPX_PLANE_V],
               image->stride[VPX_PLANE_V],
               (image->d_h + 1) / 2,
               decoded_frame.get());
    return decoded_frame;
  }

  // VPX decoder context (i.e., an instantiation).
  vpx_codec_ctx_t context_;

  DISALLOW_COPY_AND_ASSIGN(Vp8Impl);
};

#ifndef OFFICIAL_BUILD
// A fake video decoder that always output 2x2 black frames.
class VideoDecoder::FakeImpl : public VideoDecoder::ImplBase {
 public:
  explicit FakeImpl(const scoped_refptr<CastEnvironment>& cast_environment)
      : ImplBase(cast_environment, CODEC_VIDEO_FAKE),
        last_decoded_id_(-1) {
    if (ImplBase::operational_status_ != STATUS_UNINITIALIZED)
      return;
    ImplBase::operational_status_ = STATUS_INITIALIZED;
  }

 private:
  ~FakeImpl() final {}

  scoped_refptr<VideoFrame> Decode(uint8* data, int len) final {
    // Make sure this is a JSON string.
    if (!len || data[0] != '{')
      return NULL;
    base::JSONReader reader;
    scoped_ptr<base::Value> values(
        reader.Read(base::StringPiece(reinterpret_cast<char*>(data), len)));
    if (!values)
      return NULL;
    base::DictionaryValue* dict = NULL;
    values->GetAsDictionary(&dict);

    bool key = false;
    int id = 0;
    int ref = 0;
    dict->GetBoolean("key", &key);
    dict->GetInteger("id", &id);
    dict->GetInteger("ref", &ref);
    DCHECK(id == last_decoded_id_ + 1);
    last_decoded_id_ = id;
    return media::VideoFrame::CreateBlackFrame(gfx::Size(2, 2));
  }

  int last_decoded_id_;

  DISALLOW_COPY_AND_ASSIGN(FakeImpl);
};
#endif

VideoDecoder::VideoDecoder(
    const scoped_refptr<CastEnvironment>& cast_environment,
    Codec codec)
    : cast_environment_(cast_environment) {
  switch (codec) {
#ifndef OFFICIAL_BUILD
    case CODEC_VIDEO_FAKE:
      impl_ = new FakeImpl(cast_environment);
      break;
#endif
    case CODEC_VIDEO_VP8:
      impl_ = new Vp8Impl(cast_environment);
      break;
    case CODEC_VIDEO_H264:
      // TODO(miu): Need implementation.
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED() << "Unknown or unspecified codec.";
      break;
  }
}

VideoDecoder::~VideoDecoder() {}

OperationalStatus VideoDecoder::InitializationResult() const {
  if (impl_.get())
    return impl_->InitializationResult();
  return STATUS_UNSUPPORTED_CODEC;
}

void VideoDecoder::DecodeFrame(
    scoped_ptr<EncodedFrame> encoded_frame,
    const DecodeFrameCallback& callback) {
  DCHECK(encoded_frame.get());
  DCHECK(!callback.is_null());
  if (!impl_.get() || impl_->InitializationResult() != STATUS_INITIALIZED) {
    callback.Run(make_scoped_refptr<VideoFrame>(NULL), false);
    return;
  }
  cast_environment_->PostTask(CastEnvironment::VIDEO,
                              FROM_HERE,
                              base::Bind(&VideoDecoder::ImplBase::DecodeFrame,
                                         impl_,
                                         base::Passed(&encoded_frame),
                                         callback));
}

}  // namespace cast
}  // namespace media
