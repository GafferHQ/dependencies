// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_
#define UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/native_library.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/ozone_base_export.h"

namespace ui {

class NativePixmap;
class SurfaceOzoneCanvas;
class SurfaceOzoneEGL;

// The Ozone interface allows external implementations to hook into Chromium to
// provide a system specific implementation. The Ozone interface supports two
// drawing modes: 1) accelerated drawing through EGL and 2) software drawing
// through Skia.
//
// If you want to paint on a window with ozone, you need to create a
// SurfaceOzoneEGL or SurfaceOzoneCanvas for that window. The platform can
// support software, EGL, or both for painting on the window.
// The following functionality is specific to the drawing mode and may not have
// any meaningful implementation in the other mode. An implementation must
// provide functionality for at least one mode.
//
// 1) Accelerated Drawing (EGL path):
//
// The following functions are specific to EGL:
//  - GetNativeDisplay
//  - LoadEGLGLES2Bindings
//  - GetEGLSurfaceProperties (optional if the properties match the default
//  Chromium ones).
//  - CreateEGLSurfaceForWidget
//
// 2) Software Drawing (Skia):
//
// The following function is specific to the software path:
//  - CreateCanvasForWidget
//
// The accelerated path can optionally provide support for the software drawing
// path.
//
// The remaining functions are not covered since they are needed in both drawing
// modes (See comments bellow for descriptions).
class OZONE_BASE_EXPORT SurfaceFactoryOzone {
 public:
  // Describes overlay buffer format.
  // TODO: this is a placeholder for now and will be populated with more
  // formats once we know what sorts of content, video, etc. we can support.
  enum BufferFormat {
    UNKNOWN,
    BGRA_8888,
    RGBX_8888,
    BUFFER_FORMAT_LAST = RGBX_8888
  };

  enum BufferUsage {
    MAP,
    PERSISTENT_MAP,
    SCANOUT,
  };

  typedef void* (*GLGetProcAddressProc)(const char* name);
  typedef base::Callback<void(base::NativeLibrary)> AddGLLibraryCallback;
  typedef base::Callback<void(GLGetProcAddressProc)>
      SetGLGetProcAddressProcCallback;

  // Returns native platform display handle. This is used to obtain the EGL
  // display connection for the native display.
  virtual intptr_t GetNativeDisplay();

  // Create SurfaceOzoneEGL for the specified gfx::AcceleratedWidget.
  //
  // Note: When used from content, this is called in the GPU process. The
  // platform must support creation of SurfaceOzoneEGL from the GPU process
  // using only the handle contained in gfx::AcceleratedWidget.
  virtual scoped_ptr<SurfaceOzoneEGL> CreateEGLSurfaceForWidget(
      gfx::AcceleratedWidget widget);

  // Create an EGL surface that isn't backed by any buffers, and is used
  // for overlay-only displays. This will return NULL if this mode is
  // not supported.
  virtual scoped_ptr<SurfaceOzoneEGL> CreateSurfacelessEGLSurfaceForWidget(
      gfx::AcceleratedWidget widget);

  // Create SurfaceOzoneCanvas for the specified gfx::AcceleratedWidget.
  //
  // Note: The platform must support creation of SurfaceOzoneCanvas from the
  // Browser Process using only the handle contained in gfx::AcceleratedWidget.
  virtual scoped_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget);

  // Sets up GL bindings for the native surface. Takes two callback parameters
  // that allow Ozone to register the GL bindings.
  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) = 0;

  // Returns an array of EGL properties, which can be used in any EGL function
  // used to select a display configuration. Note that all properties should be
  // immediately followed by the corresponding desired value and array should be
  // terminated with EGL_NONE. Ownership of the array is not transferred to
  // caller. desired_list contains list of desired EGL properties and values.
  virtual const int32* GetEGLSurfaceProperties(const int32* desired_list);

  // Create a single native buffer to be used for overlay planes or zero copy
  // for |widget| representing a particular display controller or default
  // display controller for kNullAcceleratedWidget.
  // It can be called on any thread.
  virtual scoped_refptr<NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      BufferFormat format,
      BufferUsage usage);

  // Returns true if overlays can be shown at z-index 0, replacing the main
  // surface. Combined with surfaceless extensions, it allows for an
  // overlay-only mode.
  virtual bool CanShowPrimaryPlaneAsOverlay();

  // Returns true if the platform is able to create buffers for a specific usage
  // such as MAP for zero copy or SCANOUT for display controller.
  virtual bool CanCreateNativePixmap(BufferUsage usage);

 protected:
  SurfaceFactoryOzone();
  virtual ~SurfaceFactoryOzone();

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceFactoryOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_
