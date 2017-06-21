// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_IMPL_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_IMPL_H_

#include "content/renderer/media/android/stream_texture_factory.h"

namespace cc {
class ContextProvider;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}  // namespace gles2
}  // namespace gpu

namespace content {

class GpuChannelHost;

class StreamTextureFactoryImpl : public StreamTextureFactory {
 public:
  static scoped_refptr<StreamTextureFactoryImpl> Create(
      const scoped_refptr<cc::ContextProvider>& context_provider,
      GpuChannelHost* channel,
      int frame_id);

  // StreamTextureFactory implementation.
  StreamTextureProxy* CreateProxy() override;
  void EstablishPeer(int32 stream_id, int player_id) override;
  unsigned CreateStreamTexture(unsigned texture_target,
                               unsigned* texture_id,
                               gpu::Mailbox* texture_mailbox) override;
  void SetStreamTextureSize(int32 texture_id, const gfx::Size& size) override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  void AddObserver(StreamTextureFactoryContextObserver* obs) override;
  void RemoveObserver(StreamTextureFactoryContextObserver* obs) override;

 private:
  friend class base::RefCounted<StreamTextureFactoryImpl>;
  StreamTextureFactoryImpl(
      const scoped_refptr<cc::ContextProvider>& context_provider,
      GpuChannelHost* channel,
      int frame_id);
  ~StreamTextureFactoryImpl() override;

  scoped_refptr<cc::ContextProvider> context_provider_;
  scoped_refptr<GpuChannelHost> channel_;
  int frame_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamTextureFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_IMPL_H_
