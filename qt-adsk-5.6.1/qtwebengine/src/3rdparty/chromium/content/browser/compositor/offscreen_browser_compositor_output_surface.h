// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_OFFSCREEN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_OFFSCREEN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/cancelable_callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"

namespace ui {
class CompositorVSyncManager;
}

namespace content {
class CommandBufferProxyImpl;
class ReflectorTexture;

class OffscreenBrowserCompositorOutputSurface
    : public BrowserCompositorOutputSurface {
 public:
  OffscreenBrowserCompositorOutputSurface(
      const scoped_refptr<ContextProviderCommandBuffer>& context,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
      scoped_ptr<BrowserCompositorOverlayCandidateValidator>
          overlay_candidate_validator);

  ~OffscreenBrowserCompositorOutputSurface() override;

 protected:
  // cc::OutputSurface:
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void Reshape(const gfx::Size& size, float scale_factor) override;
  void BindFramebuffer() override;
  void SwapBuffers(cc::CompositorFrame* frame) override;

  // BrowserCompositorOutputSurface
  void OnReflectorChanged() override;
  base::Closure CreateCompositionStartedCallback() override;
#if defined(OS_MACOSX)
  void OnSurfaceDisplayed() override {};
  void SetSurfaceSuspendedForRecycle(bool suspended) override {};
  bool SurfaceShouldNotShowFramesAfterSuspendForRecycle() const override;
#endif

  uint32 fbo_;
  bool is_backbuffer_discarded_;
  scoped_ptr<ReflectorTexture> reflector_texture_;

  base::WeakPtrFactory<OffscreenBrowserCompositorOutputSurface>
      weak_ptr_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OffscreenBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_OFFSCREEN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
