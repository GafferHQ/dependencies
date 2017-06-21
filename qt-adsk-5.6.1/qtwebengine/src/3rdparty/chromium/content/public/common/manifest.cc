// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/manifest.h"

namespace content {

const double Manifest::Icon::kDefaultDensity = 1;
const size_t Manifest::kMaxIPCStringLength = 4 * 1024;

Manifest::Icon::Icon()
    : density(kDefaultDensity) {
}

Manifest::Icon::~Icon() {
}

Manifest::RelatedApplication::RelatedApplication() {
}

Manifest::RelatedApplication::~RelatedApplication() {
}

Manifest::Manifest()
    : display(DISPLAY_MODE_UNSPECIFIED),
      orientation(blink::WebScreenOrientationLockDefault),
      prefer_related_applications(false) {
}

Manifest::~Manifest() {
}

bool Manifest::IsEmpty() const {
  return name.is_null() &&
         short_name.is_null() &&
         start_url.is_empty() &&
         display == DISPLAY_MODE_UNSPECIFIED &&
         orientation == blink::WebScreenOrientationLockDefault &&
         icons.empty() &&
         related_applications.empty() &&
         !prefer_related_applications &&
         gcm_sender_id.is_null();
}

} // namespace content
