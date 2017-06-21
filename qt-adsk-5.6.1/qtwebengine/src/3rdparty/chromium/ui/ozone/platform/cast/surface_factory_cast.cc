// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/surface_factory_cast.h"

#include "base/callback_helpers.h"
#include "chromecast/public/cast_egl_platform.h"
#include "chromecast/public/graphics_types.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/ozone/platform/cast/surface_ozone_egl_cast.h"
#include "ui/ozone/public/native_pixmap.h"

using chromecast::CastEglPlatform;

namespace ui {
namespace {
chromecast::Size FromGfxSize(const gfx::Size& size) {
  return chromecast::Size(size.width(), size.height());
}

// Initial display size to create, needed before first window is created.
gfx::Size GetInitialDisplaySize() {
  return gfx::Size(1280, 720);
}

// Hard lower bound on display resolution
gfx::Size GetMinDisplaySize() {
  return gfx::Size(1280, 720);
}

}  // namespace

SurfaceFactoryCast::SurfaceFactoryCast(scoped_ptr<CastEglPlatform> egl_platform)
    : state_(kUninitialized),
      destroy_window_pending_state_(kNoDestroyPending),
      display_type_(0),
      have_display_type_(false),
      window_(0),
      display_size_(GetInitialDisplaySize()),
      new_display_size_(GetInitialDisplaySize()),
      egl_platform_(egl_platform.Pass()) {
}

SurfaceFactoryCast::~SurfaceFactoryCast() {
  DestroyDisplayTypeAndWindow();
}

void SurfaceFactoryCast::InitializeHardware() {
  if (state_ == kInitialized) {
    return;
  }
  CHECK_EQ(state_, kUninitialized);

  if (egl_platform_->InitializeHardware()) {
    state_ = kInitialized;
  } else {
    ShutdownHardware();
    state_ = kFailed;
  }
}

void SurfaceFactoryCast::ShutdownHardware() {
  DestroyDisplayTypeAndWindow();

  egl_platform_->ShutdownHardware();

  state_ = kUninitialized;
}

intptr_t SurfaceFactoryCast::GetNativeDisplay() {
  CreateDisplayTypeAndWindowIfNeeded();
  return reinterpret_cast<intptr_t>(display_type_);
}

void SurfaceFactoryCast::CreateDisplayTypeAndWindowIfNeeded() {
  if (state_ == kUninitialized) {
    InitializeHardware();
  }
  if (new_display_size_ != display_size_) {
    DestroyDisplayTypeAndWindow();
    display_size_ = new_display_size_;
  }
  DCHECK_EQ(state_, kInitialized);
  if (!have_display_type_) {
    chromecast::Size create_size = FromGfxSize(display_size_);
    display_type_ = egl_platform_->CreateDisplayType(create_size);
    have_display_type_ = true;
  }
  if (!window_) {
    chromecast::Size create_size = FromGfxSize(display_size_);
    window_ = egl_platform_->CreateWindow(display_type_, create_size);
    if (!window_) {
      DestroyDisplayTypeAndWindow();
      state_ = kFailed;
      LOG(FATAL) << "Create EGLNativeWindowType(" << display_size_.ToString()
                 << ") failed.";
    }
  }
}

intptr_t SurfaceFactoryCast::GetNativeWindow() {
  CreateDisplayTypeAndWindowIfNeeded();
  return reinterpret_cast<intptr_t>(window_);
}

bool SurfaceFactoryCast::ResizeDisplay(gfx::Size size) {
  // set size to at least 1280x720 even if passed 1x1
  size.SetToMax(GetMinDisplaySize());
  if (have_display_type_ && size != display_size_) {
    DestroyDisplayTypeAndWindow();
  }
  display_size_ = size;
  return true;
}

void SurfaceFactoryCast::DestroyWindow() {
  if (window_) {
    egl_platform_->DestroyWindow(window_);
    window_ = 0;
  }
}

void SurfaceFactoryCast::DestroyDisplayTypeAndWindow() {
  DestroyWindow();
  if (have_display_type_) {
    egl_platform_->DestroyDisplayType(display_type_);
    display_type_ = 0;
    have_display_type_ = false;
  }
}

scoped_ptr<SurfaceOzoneEGL> SurfaceFactoryCast::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  new_display_size_ = gfx::Size(widget >> 16, widget & 0xFFFF);
  new_display_size_.SetToMax(GetMinDisplaySize());
  destroy_window_pending_state_ = kSurfaceExists;
  SendRelinquishResponse();
  return make_scoped_ptr<SurfaceOzoneEGL>(new SurfaceOzoneEglCast(this));
}

void SurfaceFactoryCast::SetToRelinquishDisplay(const base::Closure& callback) {
  // This is called in response to a RelinquishDisplay message from the
  // browser task. This call may come before or after the display surface
  // is actually destroyed.
  relinquish_display_callback_ = callback;
  switch (destroy_window_pending_state_) {
    case kNoDestroyPending:
    case kSurfaceDestroyedRecently:
      DestroyDisplayTypeAndWindow();
      SendRelinquishResponse();
      destroy_window_pending_state_ = kNoDestroyPending;
      break;
    case kSurfaceExists:
      destroy_window_pending_state_ = kWindowDestroyPending;
      break;
    case kWindowDestroyPending:
      break;
    default:
      NOTREACHED();
  }
}

void SurfaceFactoryCast::ChildDestroyed() {
  if (destroy_window_pending_state_ == kWindowDestroyPending) {
    DestroyDisplayTypeAndWindow();
    SendRelinquishResponse();
    destroy_window_pending_state_ = kNoDestroyPending;
  } else {
    if (egl_platform_->MultipleSurfaceUnsupported()) {
      DestroyWindow();
    }
    destroy_window_pending_state_ = kSurfaceDestroyedRecently;
  }
}

void SurfaceFactoryCast::SendRelinquishResponse() {
  if (!relinquish_display_callback_.is_null()) {
    base::ResetAndReturn(&relinquish_display_callback_).Run();
  }
}

const int32* SurfaceFactoryCast::GetEGLSurfaceProperties(
    const int32* desired_list) {
  return egl_platform_->GetEGLSurfaceProperties(desired_list);
}

scoped_refptr<NativePixmap> SurfaceFactoryCast::CreateNativePixmap(
    gfx::AcceleratedWidget w,
    gfx::Size size,
    BufferFormat format,
    BufferUsage usage) {
  class CastPixmap : public NativePixmap {
   public:
    CastPixmap() {}

    void* GetEGLClientBuffer() override {
      // TODO(halliwell): try to implement this through CastEglPlatform.
      return nullptr;
    }
    int GetDmaBufFd() override { return 0; }
    int GetDmaBufPitch() override { return 0; }
    bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                              int plane_z_order,
                              gfx::OverlayTransform plane_transform,
                              const gfx::Rect& display_bounds,
                              const gfx::RectF& crop_rect) override {
      return true;
    }
    void SetScalingCallback(const ScalingCallback& scaling_callback) override {}
    scoped_refptr<NativePixmap> GetScaledPixmap(gfx::Size new_size) override {
      return nullptr;
    }

   private:
    ~CastPixmap() override {}

    DISALLOW_COPY_AND_ASSIGN(CastPixmap);
  };
  return make_scoped_refptr(new CastPixmap);
}

bool SurfaceFactoryCast::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  if (state_ != kInitialized) {
    InitializeHardware();
    if (state_ != kInitialized) {
      return false;
    }
  }

  void* lib_egl = egl_platform_->GetEglLibrary();
  void* lib_gles2 = egl_platform_->GetGles2Library();
  GLGetProcAddressProc gl_proc = egl_platform_->GetGLProcAddressProc();
  if (!lib_egl || !lib_gles2 || !gl_proc) {
    return false;
  }

  set_gl_get_proc_address.Run(gl_proc);
  add_gl_library.Run(lib_egl);
  add_gl_library.Run(lib_gles2);
  return true;
}

}  // namespace ui
