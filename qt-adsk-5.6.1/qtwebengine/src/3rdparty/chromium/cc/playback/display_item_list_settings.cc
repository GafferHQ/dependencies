// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/display_item_list_settings.h"

namespace cc {

DisplayItemListSettings::DisplayItemListSettings()
    : use_cached_picture(false),
      max_sidecar_size(0),
      sidecar_destroyer([](void* sidecar) {}) {
}

DisplayItemListSettings::~DisplayItemListSettings() {
}

}  // namespace cc
