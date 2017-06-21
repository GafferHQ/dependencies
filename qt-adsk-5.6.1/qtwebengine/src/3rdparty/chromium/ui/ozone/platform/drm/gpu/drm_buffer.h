// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_BUFFER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_BUFFER_H_

#include "base/macros.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

namespace ui {

class DrmDevice;

// Wrapper for a DRM allocated buffer. Keeps track of the native properties of
// the buffer and wraps the pixel memory into a SkSurface which can be used to
// draw into using Skia.
class OZONE_EXPORT DrmBuffer : public ScanoutBuffer {
 public:
  DrmBuffer(const scoped_refptr<DrmDevice>& drm);

  // Allocates the backing pixels and wraps them in |surface_|. |info| is used
  // to describe the buffer characteristics (size, color format).
  // |should_register_framebuffer| is used to distinguish the buffers that are
  // used for modesetting.
  bool Initialize(const SkImageInfo& info, bool should_register_framebuffer);

  SkCanvas* GetCanvas() const;

  // ScanoutBuffer:
  uint32_t GetFramebufferId() const override;
  uint32_t GetHandle() const override;
  gfx::Size GetSize() const override;

 protected:
  ~DrmBuffer() override;

  scoped_refptr<DrmDevice> drm_;

  // Length of a row of pixels.
  uint32_t stride_ = 0;

  // Buffer handle used by the DRM allocator.
  uint32_t handle_ = 0;

  // Base address for memory mapping.
  void* mmap_base_ = 0;

  // Size for memory mapping.
  size_t mmap_size_ = 0;

  // Buffer ID used by the DRM modesettings API. This is set when the buffer is
  // registered with the CRTC.
  uint32_t framebuffer_ = 0;

  // Wrapper around the native pixel memory.
  skia::RefPtr<SkSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(DrmBuffer);
};

class OZONE_EXPORT DrmBufferGenerator : public ScanoutBufferGenerator {
 public:
  DrmBufferGenerator();
  ~DrmBufferGenerator() override;

  // ScanoutBufferGenerator:
  scoped_refptr<ScanoutBuffer> Create(const scoped_refptr<DrmDevice>& drm,
                                      const gfx::Size& size) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DrmBufferGenerator);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_BUFFER_H_
