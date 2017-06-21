// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_switches.h"

#include "base/command_line.h"

namespace switches {

// Enable compositing individual elements via hardware overlays when
// permitted by device.
const char kEnableHardwareOverlays[] = "enable-hardware-overlays";

// Forces tests to produce pixel output when they normally wouldn't.
const char kEnablePixelOutputInTests[] = "enable-pixel-output-in-tests";

const char kUIDisableThreadedCompositing[] = "ui-disable-threaded-compositing";

const char kUIEnableCompositorAnimationTimelines[] =
    "ui-enable-compositor-animation-timelines";

const char kUIEnableZeroCopy[] = "ui-enable-zero-copy";

const char kUIShowPaintRects[] = "ui-show-paint-rects";

}  // namespace switches

namespace ui {

bool IsUIZeroCopyEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(switches::kUIEnableZeroCopy);
}

bool IsUIOneCopyEnabled() {
  // One-copy is on by default unless zero copy is enabled.
  return !IsUIZeroCopyEnabled();
}

}  // namespace ui
