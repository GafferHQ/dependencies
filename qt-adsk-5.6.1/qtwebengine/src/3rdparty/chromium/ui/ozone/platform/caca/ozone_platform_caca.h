// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CACA_OZONE_PLATFORM_CACA_H_
#define UI_OZONE_PLATFORM_CACA_OZONE_PLATFORM_CACA_H_

namespace ui {

class OzonePlatform;

// Constructor hook for use in ozone_platform_list.cc
OzonePlatform* CreateOzonePlatformCaca();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CACA_OZONE_PLATFORM_CACA_H_
