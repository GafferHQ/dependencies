// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/surface_factory_ozone.h"

#include <stdlib.h>

#include "base/command_line.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {

SurfaceFactoryOzone::SurfaceFactoryOzone() {
}

SurfaceFactoryOzone::~SurfaceFactoryOzone() {
}

intptr_t SurfaceFactoryOzone::GetNativeDisplay() {
  return 0;
}

scoped_ptr<SurfaceOzoneEGL> SurfaceFactoryOzone::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  NOTIMPLEMENTED();
  return nullptr;
}

scoped_ptr<SurfaceOzoneEGL>
SurfaceFactoryOzone::CreateSurfacelessEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  NOTIMPLEMENTED();
  return nullptr;
}

scoped_ptr<SurfaceOzoneCanvas> SurfaceFactoryOzone::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  NOTIMPLEMENTED();
  return nullptr;
}

const int32* SurfaceFactoryOzone::GetEGLSurfaceProperties(
    const int32* desired_attributes) {
  return desired_attributes;
}

scoped_refptr<ui::NativePixmap> SurfaceFactoryOzone::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    BufferFormat format,
    BufferUsage usage) {
  return NULL;
}

bool SurfaceFactoryOzone::CanShowPrimaryPlaneAsOverlay() {
  return false;
}

bool SurfaceFactoryOzone::CanCreateNativePixmap(BufferUsage usage) {
  return false;
}

}  // namespace ui
