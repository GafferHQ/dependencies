// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

namespace ui {

class DrmDevice;
class HardwareDisplayControllerInfo;
class ScreenManager;

struct GammaRampRGBEntry;

class DrmDisplay {
 public:
  DrmDisplay(ScreenManager* screen_manager,
             const scoped_refptr<DrmDevice>& drm);
  ~DrmDisplay();

  int64_t display_id() const { return display_id_; }
  scoped_refptr<DrmDevice> drm() const { return drm_; }
  uint32_t crtc() const { return crtc_; }
  uint32_t connector() const { return connector_; }
  const std::vector<drmModeModeInfo>& modes() const { return modes_; }

  DisplaySnapshot_Params Update(HardwareDisplayControllerInfo* info,
                                size_t display_index);

  bool Configure(const drmModeModeInfo* mode, const gfx::Point& origin);
  bool GetHDCPState(HDCPState* state);
  bool SetHDCPState(HDCPState state);
  void SetGammaRamp(const std::vector<GammaRampRGBEntry>& lut);

 private:
  ScreenManager* screen_manager_;  // Not owned.

  int64_t display_id_ = -1;
  scoped_refptr<DrmDevice> drm_;
  uint32_t crtc_ = 0;
  uint32_t connector_ = 0;
  std::vector<drmModeModeInfo> modes_;
  gfx::Point origin_;

  DISALLOW_COPY_AND_ASSIGN(DrmDisplay);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_H_
