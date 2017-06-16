// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_SWITCHES_H_
#define UI_COMPOSITOR_COMPOSITOR_SWITCHES_H_

#include "ui/compositor/compositor_export.h"

namespace switches {

COMPOSITOR_EXPORT extern const char kEnableHardwareOverlays[];
COMPOSITOR_EXPORT extern const char kEnablePixelOutputInTests[];
COMPOSITOR_EXPORT extern const char kUIDisableThreadedCompositing[];
COMPOSITOR_EXPORT extern const char kUIEnableCompositorAnimationTimelines[];
COMPOSITOR_EXPORT extern const char kUIEnableZeroCopy[];
COMPOSITOR_EXPORT extern const char kUIShowPaintRects[];

}  // namespace switches

namespace ui {

bool IsUIZeroCopyEnabled();
bool IsUIOneCopyEnabled();

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_SWITCHES_H_
