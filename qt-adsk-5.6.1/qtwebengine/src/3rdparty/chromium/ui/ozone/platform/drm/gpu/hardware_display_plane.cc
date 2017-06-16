// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"

#include <drm.h>
#include <xf86drm.h>

#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

namespace ui {

HardwareDisplayPlane::HardwareDisplayPlane(uint32_t plane_id,
                                           uint32_t possible_crtcs)
    : plane_id_(plane_id), possible_crtcs_(possible_crtcs) {
}

HardwareDisplayPlane::~HardwareDisplayPlane() {
}

bool HardwareDisplayPlane::CanUseForCrtc(uint32_t crtc_index) {
  return possible_crtcs_ & (1 << crtc_index);
}

bool HardwareDisplayPlane::Initialize(DrmDevice* drm) {
  return true;
}

}  // namespace ui
