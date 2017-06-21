// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_GL_RENDERER_H_
#define UI_OZONE_DEMO_GL_RENDERER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/demo/renderer_base.h"

namespace gfx {
class GLContext;
class GLSurface;
}  // namespace gfx

namespace ui {

class GlRenderer : public RendererBase {
 public:
  GlRenderer(gfx::AcceleratedWidget widget, const gfx::Size& size);
  ~GlRenderer() override;

  void PostRenderFrameTask(gfx::SwapResult result);

  // Renderer:
  bool Initialize() override;

 protected:
  virtual void RenderFrame();
  virtual scoped_refptr<gfx::GLSurface> CreateSurface();

  scoped_refptr<gfx::GLSurface> surface_;
  scoped_refptr<gfx::GLContext> context_;

 private:
  base::WeakPtrFactory<GlRenderer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GlRenderer);
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_GL_RENDERER_H_
