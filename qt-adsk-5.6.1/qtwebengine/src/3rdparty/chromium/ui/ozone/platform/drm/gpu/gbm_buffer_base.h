// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_BASE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_BASE_H_

#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

struct gbm_bo;

namespace ui {

class DrmDevice;

// Wrapper for a GBM buffer. The base class provides common functionality
// required to prepare the buffer for scanout. It does not provide any ownership
// of the buffer. Implementations of this base class should deal with buffer
// ownership.
class GbmBufferBase : public ScanoutBuffer {
 public:
  gbm_bo* bo() const { return bo_; }

  // ScanoutBuffer:
  uint32_t GetFramebufferId() const override;
  uint32_t GetHandle() const override;
  gfx::Size GetSize() const override;

 protected:
  GbmBufferBase(const scoped_refptr<DrmDevice>& drm, gbm_bo* bo, bool scanout);
  ~GbmBufferBase() override;

 private:
  scoped_refptr<DrmDevice> drm_;
  gbm_bo* bo_;
  uint32_t framebuffer_ = 0;

  DISALLOW_COPY_AND_ASSIGN(GbmBufferBase);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_BASE_H_
