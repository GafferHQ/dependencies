// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_RENDERER_FACTORY_IMPL_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_RENDERER_FACTORY_IMPL_H_

#include "content/public/renderer/media_stream_renderer_factory.h"

namespace content {

class MediaStreamRendererFactoryImpl : public MediaStreamRendererFactory {
 public:
  MediaStreamRendererFactoryImpl();
  ~MediaStreamRendererFactoryImpl() override;

  scoped_refptr<VideoFrameProvider> GetVideoFrameProvider(
      const GURL& url,
      const base::Closure& error_cb,
      const VideoFrameProvider::RepaintCB& repaint_cb) override;

  scoped_refptr<MediaStreamAudioRenderer> GetAudioRenderer(
      const GURL& url,
      int render_frame_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamRendererFactoryImpl);
};

}  // namespace content

#endif // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_RENDERER_FACTORY_IMPL_H_
