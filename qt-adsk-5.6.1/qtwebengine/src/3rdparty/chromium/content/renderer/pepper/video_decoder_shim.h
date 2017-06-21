// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_VIDEO_DECODER_SHIM_H_
#define CONTENT_RENDERER_PEPPER_VIDEO_DECODER_SHIM_H_

#include <queue>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/video_decoder_config.h"
#include "media/video/video_decode_accelerator.h"

#include "ppapi/c/pp_codecs.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc_blink {
class ContextProviderWebContext;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace media {
class DecoderBuffer;
}

namespace content {

class PepperVideoDecoderHost;

// This class is a shim to wrap a media::VideoDecoder so that it can be used
// by PepperVideoDecoderHost in place of a media::VideoDecodeAccelerator.
// This class should be constructed, used, and destructed on the main (render)
// thread.
class VideoDecoderShim : public media::VideoDecodeAccelerator {
 public:
  explicit VideoDecoderShim(PepperVideoDecoderHost* host);
  ~VideoDecoderShim() override;

  // media::VideoDecodeAccelerator implementation.
  bool Initialize(media::VideoCodecProfile profile,
                  media::VideoDecodeAccelerator::Client* client) override;
  void Decode(const media::BitstreamBuffer& bitstream_buffer) override;
  void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) override;
  void ReusePictureBuffer(int32 picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;

 private:
  enum State {
    UNINITIALIZED,
    DECODING,
    FLUSHING,
    RESETTING,
  };

  struct PendingDecode;
  struct PendingFrame;
  class DecoderImpl;
  class YUVConverter;

  void OnInitializeComplete(int32_t result, uint32_t texture_pool_size);
  void OnDecodeComplete(int32_t result, uint32_t decode_id);
  void OnOutputComplete(scoped_ptr<PendingFrame> frame);
  void SendPictures();
  void OnResetComplete();
  void NotifyCompletedDecodes();
  void DismissTexture(uint32_t texture_id);
  void DeleteTexture(uint32_t texture_id);
  // Call this whenever we change GL state that the plugin relies on, such as
  // creating picture textures.
  void FlushCommandBuffer();

  scoped_ptr<DecoderImpl> decoder_impl_;
  State state_;

  PepperVideoDecoderHost* host_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<cc_blink::ContextProviderWebContext> context_provider_;

  // The current decoded frame size.
  gfx::Size texture_size_;
  // Map that takes the plugin's GL texture id to the renderer's GL texture id.
  typedef base::hash_map<uint32_t, uint32_t> TextureIdMap;
  TextureIdMap texture_id_map_;
  // Available textures (these are plugin ids.)
  typedef base::hash_set<uint32_t> TextureIdSet;
  TextureIdSet available_textures_;
  // Track textures that are no longer needed (these are plugin ids.)
  TextureIdSet textures_to_dismiss_;
  // Mailboxes for pending texture requests, to write to plugin's textures.
  std::vector<gpu::Mailbox> pending_texture_mailboxes_;

  // Queue of completed decode ids, for notifying the host.
  typedef std::queue<uint32_t> CompletedDecodeQueue;
  CompletedDecodeQueue completed_decodes_;

  // Queue of decoded frames that await rgb->yuv conversion.
  typedef std::queue<linked_ptr<PendingFrame> > PendingFrameQueue;
  PendingFrameQueue pending_frames_;

  // The optimal number of textures to allocate for decoder_impl_.
  uint32_t texture_pool_size_;

  uint32_t num_pending_decodes_;

  scoped_ptr<YUVConverter> yuv_converter_;

  base::WeakPtrFactory<VideoDecoderShim> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderShim);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_VIDEO_DECODER_SHIM_H_
