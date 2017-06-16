// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_SOFTWARE_RENDERER_H_
#define UI_OZONE_DEMO_SOFTWARE_RENDERER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/ozone/demo/renderer_base.h"

namespace gfx {
class VSyncProvider;
}  // namespace gfx

namespace ui {

class SurfaceOzoneCanvas;

class SoftwareRenderer : public RendererBase {
 public:
  SoftwareRenderer(gfx::AcceleratedWidget widget, const gfx::Size& size);
  ~SoftwareRenderer() override;

  // Renderer:
  bool Initialize() override;

 private:
  void RenderFrame();

  void UpdateVSyncParameters(const base::TimeTicks timebase,
                             const base::TimeDelta interval);

  scoped_ptr<SurfaceOzoneCanvas> software_surface_;

  scoped_ptr<gfx::VSyncProvider> vsync_provider_;

  // Timer for animation.
  base::RepeatingTimer<SoftwareRenderer> timer_;

  base::TimeDelta vsync_period_;

  base::WeakPtrFactory<SoftwareRenderer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareRenderer);
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_SOFTWARE_RENDERER_H_
