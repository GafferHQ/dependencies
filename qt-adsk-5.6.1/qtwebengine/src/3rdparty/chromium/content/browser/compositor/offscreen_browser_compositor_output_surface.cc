// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/offscreen_browser_compositor_output_surface.h"

#include "base/logging.h"
#include "cc/base/util.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/gl_frame_data.h"
#include "cc/output/output_surface_client.h"
#include "cc/resources/resource_provider.h"
#include "content/browser/compositor/browser_compositor_overlay_candidate_validator.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/browser/compositor/reflector_texture.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using cc::CompositorFrame;
using cc::GLFrameData;
using cc::ResourceProvider;
using gpu::gles2::GLES2Interface;

namespace content {

OffscreenBrowserCompositorOutputSurface::
    OffscreenBrowserCompositorOutputSurface(
        const scoped_refptr<ContextProviderCommandBuffer>& context,
        const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
        scoped_ptr<BrowserCompositorOverlayCandidateValidator>
            overlay_candidate_validator)
    : BrowserCompositorOutputSurface(context,
                                     vsync_manager,
                                     overlay_candidate_validator.Pass()),
      fbo_(0),
      is_backbuffer_discarded_(false),
      weak_ptr_factory_(this) {
  capabilities_.max_frames_pending = 1;
  capabilities_.uses_default_gl_framebuffer = false;
}

OffscreenBrowserCompositorOutputSurface::
    ~OffscreenBrowserCompositorOutputSurface() {
  DiscardBackbuffer();
}

void OffscreenBrowserCompositorOutputSurface::EnsureBackbuffer() {
  is_backbuffer_discarded_ = false;

  if (!reflector_texture_.get()) {
    reflector_texture_.reset(new ReflectorTexture(context_provider()));

    GLES2Interface* gl = context_provider_->ContextGL();
    cc::ResourceFormat format = cc::RGBA_8888;
    gl->BindTexture(GL_TEXTURE_2D, reflector_texture_->texture_id());
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->TexImage2D(GL_TEXTURE_2D, 0, GLInternalFormat(format),
                   surface_size_.width(), surface_size_.height(), 0,
                   GLDataFormat(format), GLDataType(format), nullptr);
    if (!fbo_)
      gl->GenFramebuffers(1, &fbo_);

    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, reflector_texture_->texture_id(),
                             0);
    reflector_->OnSourceTextureMailboxUpdated(
        reflector_texture_->mailbox());
  }
}

void OffscreenBrowserCompositorOutputSurface::DiscardBackbuffer() {
  is_backbuffer_discarded_ = true;

  GLES2Interface* gl = context_provider_->ContextGL();

  if (reflector_texture_) {
    reflector_texture_.reset();
    if (reflector_)
      reflector_->OnSourceTextureMailboxUpdated(nullptr);
  }

  if (fbo_) {
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->DeleteFramebuffers(1, &fbo_);
    fbo_ = 0;
  }
}

void OffscreenBrowserCompositorOutputSurface::Reshape(const gfx::Size& size,
                                                      float scale_factor) {
  if (size == surface_size_)
    return;

  surface_size_ = size;
  device_scale_factor_ = scale_factor;
  DiscardBackbuffer();
  EnsureBackbuffer();
}

void OffscreenBrowserCompositorOutputSurface::BindFramebuffer() {
  bool need_to_bind = !!reflector_texture_.get();
  EnsureBackbuffer();
  DCHECK(reflector_texture_.get());

  if (need_to_bind) {
    GLES2Interface* gl = context_provider_->ContextGL();
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  }
}

void OffscreenBrowserCompositorOutputSurface::SwapBuffers(
    cc::CompositorFrame* frame) {
  if (reflector_) {
    if (frame->gl_frame_data->sub_buffer_rect ==
        gfx::Rect(frame->gl_frame_data->size))
      reflector_->OnSourceSwapBuffers();
    else
      reflector_->OnSourcePostSubBuffer(frame->gl_frame_data->sub_buffer_rect);
  }

  client_->DidSwapBuffers();

  // TODO(oshima): sync with the reflector's SwapBuffersComplete
  // (crbug.com/520567).
  // The original implementation had a flickering issue (crbug.com/515332).
  uint32_t sync_point =
      context_provider_->ContextGL()->InsertSyncPointCHROMIUM();
  context_provider_->ContextSupport()->SignalSyncPoint(
      sync_point, base::Bind(&OutputSurface::OnSwapBuffersComplete,
                             weak_ptr_factory_.GetWeakPtr()));
}

void OffscreenBrowserCompositorOutputSurface::OnReflectorChanged() {
  if (reflector_)
    EnsureBackbuffer();
}

base::Closure
OffscreenBrowserCompositorOutputSurface::CreateCompositionStartedCallback() {
  return base::Closure();
}

#if defined(OS_MACOSX)

bool OffscreenBrowserCompositorOutputSurface::
SurfaceShouldNotShowFramesAfterSuspendForRecycle() const {
  return true;
}

#endif

}  // namespace content
