// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/display_snapshot_proxy.h"

#include "ui/ozone/common/display_mode_proxy.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

namespace ui {

namespace {

bool SameModes(const DisplayMode_Params& lhs, const DisplayMode_Params& rhs) {
  return lhs.size == rhs.size && lhs.is_interlaced == rhs.is_interlaced &&
         lhs.refresh_rate == rhs.refresh_rate;
}

// Exclude 4K@60kHz becaseu this doesn't work in most devices/configuration now.
// TODO(marcheu|oshima): Revisit this. crbug.com/39397
bool IsModeBlackListed(const DisplayMode_Params& mode_params) {
  return mode_params.size.width() >= 3840 && mode_params.size.width() >= 2160 &&
         mode_params.refresh_rate >= 60.0f;
}

}  // namespace

DisplaySnapshotProxy::DisplaySnapshotProxy(const DisplaySnapshot_Params& params)
    : DisplaySnapshot(params.display_id,
                      params.origin,
                      params.physical_size,
                      params.type,
                      params.is_aspect_preserving_scaling,
                      params.has_overscan,
                      params.display_name,
                      std::vector<const DisplayMode*>(),
                      NULL,
                      NULL),
      string_representation_(params.string_representation) {
  for (size_t i = 0; i < params.modes.size(); ++i) {
    const DisplayMode_Params& mode_params = params.modes[i];
    if (IsModeBlackListed(mode_params))
      continue;
    modes_.push_back(new DisplayModeProxy(mode_params));

    if (params.has_current_mode &&
        SameModes(params.modes[i], params.current_mode))
      current_mode_ = modes_.back();

    if (params.has_native_mode &&
        SameModes(params.modes[i], params.native_mode))
      native_mode_ = modes_.back();
  }

  product_id_ = params.product_id;
}

DisplaySnapshotProxy::~DisplaySnapshotProxy() {
}

std::string DisplaySnapshotProxy::ToString() const {
  return string_representation_;
}

}  // namespace ui
