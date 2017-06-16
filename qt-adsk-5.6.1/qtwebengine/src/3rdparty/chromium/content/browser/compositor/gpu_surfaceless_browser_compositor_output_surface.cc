// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_surfaceless_browser_compositor_output_surface.h"

#include "cc/output/compositor_frame.h"
#include "content/browser/compositor/browser_compositor_overlay_candidate_validator.h"
#include "content/browser/compositor/buffer_queue.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gl_helper.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace content {

GpuSurfacelessBrowserCompositorOutputSurface::
    GpuSurfacelessBrowserCompositorOutputSurface(
        const scoped_refptr<ContextProviderCommandBuffer>& context,
        int surface_id,
        const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
        scoped_ptr<BrowserCompositorOverlayCandidateValidator>
            overlay_candidate_validator,
        unsigned internalformat,
        BrowserGpuMemoryBufferManager* gpu_memory_buffer_manager)
    : GpuBrowserCompositorOutputSurface(context,
                                        vsync_manager,
                                        overlay_candidate_validator.Pass()),
      internalformat_(internalformat),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager) {
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.flipped_output_surface = true;
  // Set |max_frames_pending| to 2 for surfaceless, which aligns scheduling
  // more closely with the previous surfaced behavior.
  // With a surface, swap buffer ack used to return early, before actually
  // presenting the back buffer, enabling the browser compositor to run ahead.
  // Surfaceless implementation acks at the time of actual buffer swap, which
  // shifts the start of the new frame forward relative to the old
  // implementation.
  capabilities_.max_frames_pending = 2;

  gl_helper_.reset(new GLHelper(context_provider_->ContextGL(),
                                context_provider_->ContextSupport()));
  output_surface_.reset(
      new BufferQueue(context_provider_, internalformat_, gl_helper_.get(),
                      gpu_memory_buffer_manager_, surface_id));
  output_surface_->Initialize();
}

GpuSurfacelessBrowserCompositorOutputSurface::
    ~GpuSurfacelessBrowserCompositorOutputSurface() {
}

void GpuSurfacelessBrowserCompositorOutputSurface::SwapBuffers(
    cc::CompositorFrame* frame) {
  DCHECK(output_surface_);

  GLuint texture = output_surface_->current_texture_id();
  output_surface_->SwapBuffers(frame->gl_frame_data->sub_buffer_rect);
  const gfx::Size& size = frame->gl_frame_data->size;
  context_provider_->ContextGL()->ScheduleOverlayPlaneCHROMIUM(
      0,
      GL_OVERLAY_TRANSFORM_NONE_CHROMIUM,
      texture,
      0,
      0,
      size.width(),
      size.height(),
      0,
      0,
      1.0f,
      1.0f);
  GpuBrowserCompositorOutputSurface::SwapBuffers(frame);
}

void GpuSurfacelessBrowserCompositorOutputSurface::OnSwapBuffersComplete() {
  DCHECK(output_surface_);
  output_surface_->PageFlipComplete();
  GpuBrowserCompositorOutputSurface::OnSwapBuffersComplete();
}

void GpuSurfacelessBrowserCompositorOutputSurface::BindFramebuffer() {
  DCHECK(output_surface_);
  output_surface_->BindFramebuffer();
}

void GpuSurfacelessBrowserCompositorOutputSurface::Reshape(
    const gfx::Size& size,
    float scale_factor) {
  GpuBrowserCompositorOutputSurface::Reshape(size, scale_factor);
  DCHECK(output_surface_);
  output_surface_->Reshape(SurfaceSize(), scale_factor);
}

void GpuSurfacelessBrowserCompositorOutputSurface::OnSwapBuffersCompleted(
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::SwapResult result) {
  if (result == gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    // Even through the swap failed, this is a fixable error so we can pretend
    // it succeeded to the rest of the system.
    result = gfx::SwapResult::SWAP_ACK;
    output_surface_->RecreateBuffers();
  }
  GpuBrowserCompositorOutputSurface::OnSwapBuffersCompleted(latency_info,
                                                            result);
}

}  // namespace content
