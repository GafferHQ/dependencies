// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_IOSURFACE_MAC_H_
#define CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_IOSURFACE_MAC_H_

#include <list>

#include "content/common/gpu/image_transport_surface_fbo_mac.h"

// Note that this must be included after gl_bindings.h to avoid conflicts.
#include <OpenGL/CGLIOSurface.h>

namespace content {

// Allocate IOSurface-backed storage for an FBO image transport surface.
class IOSurfaceStorageProvider
    : public ImageTransportSurfaceFBO::StorageProvider {
 public:
  IOSurfaceStorageProvider(ImageTransportSurfaceFBO* transport_surface);
  ~IOSurfaceStorageProvider() override;

  // ImageTransportSurfaceFBO::StorageProvider implementation:
  gfx::Size GetRoundedSize(gfx::Size size) override;
  bool AllocateColorBufferStorage(
      CGLContextObj context, const base::Closure& context_dirtied_callback,
      GLuint texture, gfx::Size pixel_size, float scale_factor) override;
  void FreeColorBufferStorage() override;
  void FrameSizeChanged(
      const gfx::Size& pixel_size, float scale_factor) override;
  void SwapBuffers(const gfx::Rect& dirty_rect) override;
  void WillWriteToBackbuffer() override;
  void DiscardBackbuffer() override;
  void SwapBuffersAckedByBrowser(bool disable_throttling) override;

 private:
  ImageTransportSurfaceFBO* transport_surface_;

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  gfx::Size frame_pixel_size_;
  float frame_scale_factor_;

  // The list of IOSurfaces that have been sent to the browser process but have
  // not been opened in the browser process yet. This list should never have
  // more than one entry.
  std::list<base::ScopedCFTypeRef<IOSurfaceRef>> pending_swapped_surfaces_;

  // The id of |io_surface_| or 0 if that's NULL.
  IOSurfaceID io_surface_id_;

  DISALLOW_COPY_AND_ASSIGN(IOSurfaceStorageProvider);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_IOSURFACE_MAC_H_
