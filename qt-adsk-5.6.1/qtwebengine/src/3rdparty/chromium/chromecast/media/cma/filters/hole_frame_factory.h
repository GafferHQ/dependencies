// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_FILTERS_HOLE_FRAME_FACTORY_H_
#define CHROMECAST_MEDIA_CMA_FILTERS_HOLE_FRAME_FACTORY_H_

#include <GLES2/gl2.h>

#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/common/mailbox.h"

namespace gfx {
class Size;
}

namespace media {
class GpuVideoAcceleratorFactories;
class VideoFrame;
}

namespace chromecast {
namespace media {

// Creates VideoFrames for CMA - currently supports both overlay frames
// (native textures that get turned into transparent holes in the browser
// compositor), and legacy VIDEO_HOLE codepath.
// All calls (including ctor/dtor) must be on media thread.
class HoleFrameFactory {
 public:
  explicit HoleFrameFactory(const scoped_refptr<
      ::media::GpuVideoAcceleratorFactories>& gpu_factories);
  ~HoleFrameFactory();

  scoped_refptr<::media::VideoFrame> CreateHoleFrame(const gfx::Size& size);

 private:
  scoped_refptr<::media::GpuVideoAcceleratorFactories> gpu_factories_;
  gpu::Mailbox mailbox_;
  GLuint texture_;
  GLuint image_id_;
  GLuint sync_point_;
  bool use_legacy_hole_punching_;

  DISALLOW_COPY_AND_ASSIGN(HoleFrameFactory);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_FILTERS_HOLE_FRAME_FACTORY_H_
