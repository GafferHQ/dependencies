// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_profile.h"

namespace gfx {

#if defined(OS_WIN) || defined(OS_MACOSX)
void ReadColorProfile(std::vector<char>* profile);
#else
void ReadColorProfile(std::vector<char>* profile) { }
#endif

ColorProfile::ColorProfile() {
  // TODO: support multiple monitors.
  ReadColorProfile(&profile_);
}

ColorProfile::~ColorProfile() {
}

}  // namespace gfx
