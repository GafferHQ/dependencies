// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"

#include <gbm.h>

#include "base/files/file_path.h"
#include "third_party/khronos/EGL/egl.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/gbm_buffer.h"
#include "ui/ozone/platform/drm/gpu/gbm_device.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface.h"
#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {

GbmSurfaceFactory::GbmSurfaceFactory(bool allow_surfaceless)
    : DrmSurfaceFactory(NULL), allow_surfaceless_(allow_surfaceless) {
}

GbmSurfaceFactory::~GbmSurfaceFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void GbmSurfaceFactory::InitializeGpu(DrmDeviceManager* drm_device_manager,
                                      ScreenManager* screen_manager) {
  drm_device_manager_ = drm_device_manager;
  screen_manager_ = screen_manager;
}

intptr_t GbmSurfaceFactory::GetNativeDisplay() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return EGL_DEFAULT_DISPLAY;
}

const int32* GbmSurfaceFactory::GetEGLSurfaceProperties(
    const int32* desired_list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  static const int32 kConfigAttribs[] = {EGL_BUFFER_SIZE,
                                         32,
                                         EGL_ALPHA_SIZE,
                                         8,
                                         EGL_BLUE_SIZE,
                                         8,
                                         EGL_GREEN_SIZE,
                                         8,
                                         EGL_RED_SIZE,
                                         8,
                                         EGL_RENDERABLE_TYPE,
                                         EGL_OPENGL_ES2_BIT,
                                         EGL_SURFACE_TYPE,
                                         EGL_WINDOW_BIT,
                                         EGL_NONE};

  return kConfigAttribs;
}

bool GbmSurfaceFactory::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return LoadDefaultEGLGLES2Bindings(add_gl_library, set_gl_get_proc_address);
}

scoped_ptr<SurfaceOzoneCanvas> GbmSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG(FATAL) << "Software rendering mode is not supported with GBM platform";
  return nullptr;
}

scoped_ptr<SurfaceOzoneEGL> GbmSurfaceFactory::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<GbmDevice> gbm = GetGbmDevice(widget);
  DCHECK(gbm);

  scoped_ptr<GbmSurface> surface(
      new GbmSurface(screen_manager_->GetWindow(widget), gbm));
  if (!surface->Initialize())
    return nullptr;

  return surface.Pass();
}

scoped_ptr<SurfaceOzoneEGL>
GbmSurfaceFactory::CreateSurfacelessEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!allow_surfaceless_)
    return nullptr;

  return make_scoped_ptr(new GbmSurfaceless(screen_manager_->GetWindow(widget),
                                            drm_device_manager_));
}

scoped_refptr<ui::NativePixmap> GbmSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    BufferFormat format,
    BufferUsage usage) {
  if (usage == MAP)
    return nullptr;

  scoped_refptr<GbmDevice> gbm = GetGbmDevice(widget);
  DCHECK(gbm);

  scoped_refptr<GbmBuffer> buffer =
      GbmBuffer::CreateBuffer(gbm, format, size, true);
  if (!buffer.get())
    return nullptr;

  scoped_refptr<GbmPixmap> pixmap(new GbmPixmap(buffer, screen_manager_));
  if (!pixmap->Initialize())
    return nullptr;

  return pixmap;
}

bool GbmSurfaceFactory::CanShowPrimaryPlaneAsOverlay() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return allow_surfaceless_;
}

bool GbmSurfaceFactory::CanCreateNativePixmap(BufferUsage usage) {
  DCHECK(thread_checker_.CalledOnValidThread());
  switch (usage) {
    case MAP:
      return false;
    case PERSISTENT_MAP:
      return false;
    case SCANOUT:
      return true;
  }
  NOTREACHED();
  return false;
}

scoped_refptr<GbmDevice> GbmSurfaceFactory::GetGbmDevice(
    gfx::AcceleratedWidget widget) {
  return static_cast<GbmDevice*>(
      drm_device_manager_->GetDrmDevice(widget).get());
}

}  // namespace ui
