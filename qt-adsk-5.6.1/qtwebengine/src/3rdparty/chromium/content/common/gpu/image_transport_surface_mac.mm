// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

#include "content/common/gpu/gpu_messages.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_osmesa.h"

namespace content {

scoped_refptr<gfx::GLSurface> ImageTransportSurfaceCreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    gfx::PluginWindowHandle handle);

namespace {

// A subclass of GLSurfaceOSMesa that doesn't print an error message when
// SwapBuffers() is called.
class DRTSurfaceOSMesa : public gfx::GLSurfaceOSMesa {
 public:
  // Size doesn't matter, the surface is resized to the right size later.
  DRTSurfaceOSMesa()
      : GLSurfaceOSMesa(gfx::OSMesaSurfaceFormatRGBA, gfx::Size(1, 1)) {}

  // Implement a subset of GLSurface.
  gfx::SwapResult SwapBuffers() override;

 private:
  ~DRTSurfaceOSMesa() override {}
  DISALLOW_COPY_AND_ASSIGN(DRTSurfaceOSMesa);
};

gfx::SwapResult DRTSurfaceOSMesa::SwapBuffers() {
  return gfx::SwapResult::SWAP_ACK;
}

bool g_allow_os_mesa = false;

}  //  namespace

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& surface_handle) {
  DCHECK(surface_handle.transport_type == gfx::NATIVE_DIRECT ||
         surface_handle.transport_type == gfx::NULL_TRANSPORT);

  switch (gfx::GetGLImplementation()) {
    case gfx::kGLImplementationDesktopGL:
    case gfx::kGLImplementationDesktopGLCoreProfile:
    case gfx::kGLImplementationAppleGL:
      return ImageTransportSurfaceCreateNativeSurface(manager, stub,
                                                      surface_handle.handle);
    default:
      // Content shell in DRT mode spins up a gpu process which needs an
      // image transport surface, but that surface isn't used to read pixel
      // baselines. So this is mostly a dummy surface.
      if (!g_allow_os_mesa) {
        NOTREACHED();
        return scoped_refptr<gfx::GLSurface>();
      }
      scoped_refptr<gfx::GLSurface> surface(new DRTSurfaceOSMesa());
      if (!surface.get() || !surface->Initialize())
        return surface;
      return scoped_refptr<gfx::GLSurface>(new PassThroughImageTransportSurface(
          manager, stub, surface.get()));
  }
}

// static
void ImageTransportSurface::SetAllowOSMesaForTesting(bool allow) {
  g_allow_os_mesa = allow;
}

}  // namespace content
