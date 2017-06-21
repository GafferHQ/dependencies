// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_VIDEO_ENCODER_SHIM_H_
#define CONTENT_RENDERER_PEPPER_VIDEO_ENCODER_SHIM_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/video/video_encode_accelerator.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gfx {
class Size;
}

namespace content {

class PepperVideoEncoderHost;

// This class is a shim to wrap a media::cast::SoftwareVideoEncoder so that it
// can be used by PepperVideoEncoderHost in place of a
// media::VideoEncodeAccelerator. This class should be constructed, used, and
// destructed on the main (render) thread.
class VideoEncoderShim : public media::VideoEncodeAccelerator {
 public:
  explicit VideoEncoderShim(PepperVideoEncoderHost* host);
  ~VideoEncoderShim() override;

  // media::VideoEncodeAccelerator implementation.
  media::VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles()
      override;
  bool Initialize(media::VideoFrame::Format input_format,
                  const gfx::Size& input_visible_size,
                  media::VideoCodecProfile output_profile,
                  uint32 initial_bitrate,
                  media::VideoEncodeAccelerator::Client* client) override;
  void Encode(const scoped_refptr<media::VideoFrame>& frame,
              bool force_keyframe) override;
  void UseOutputBitstreamBuffer(const media::BitstreamBuffer& buffer) override;
  void RequestEncodingParametersChange(uint32 bitrate,
                                       uint32 framerate) override;
  void Destroy() override;

 private:
  class EncoderImpl;

  void OnRequireBitstreamBuffers(unsigned int input_count,
                                 const gfx::Size& input_coded_size,
                                 size_t output_buffer_size);
  void OnBitstreamBufferReady(scoped_refptr<media::VideoFrame> frame,
                              int32 bitstream_buffer_id,
                              size_t payload_size,
                              bool key_frame);
  void OnNotifyError(media::VideoEncodeAccelerator::Error error);

  scoped_ptr<EncoderImpl> encoder_impl_;

  PepperVideoEncoderHost* host_;

  // Task doing the encoding.
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  base::WeakPtrFactory<VideoEncoderShim> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoEncoderShim);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_VIDEO_ENCODER_SHIM_H_
