// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/ozone_platform_cast.h"

#include "base/command_line.h"
#include "chromecast/public/cast_egl_platform.h"
#include "chromecast/public/cast_egl_platform_shlib.h"
#include "ui/ozone/common/native_display_delegate_ozone.h"
#include "ui/ozone/platform/cast/gpu_platform_support_cast.h"
#include "ui/ozone/platform/cast/overlay_manager_cast.h"
#include "ui/ozone/platform/cast/platform_window_cast.h"
#include "ui/ozone/platform/cast/surface_factory_cast.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"

using chromecast::CastEglPlatform;

namespace ui {
namespace {

// Ozone platform implementation for Cast.  Implements functionality
// common to all Cast implementations:
//  - Always one window with window size equal to display size
//  - No input, cursor support
//  - Relinquish GPU resources flow for switching to external applications
// Meanwhile, platform-specific implementation details are abstracted out
// to the CastEglPlatform interface.
class OzonePlatformCast : public OzonePlatform {
 public:
  explicit OzonePlatformCast(scoped_ptr<CastEglPlatform> egl_platform)
      : egl_platform_(egl_platform.Pass()) {}
  ~OzonePlatformCast() override {}

  // OzonePlatform implementation:
  SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_.get();
  }
  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }
  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_.get();
  }
  InputController* GetInputController() override {
    return input_controller_.get();
  }
  GpuPlatformSupport* GetGpuPlatformSupport() override {
    return gpu_platform_support_.get();
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }
  scoped_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;  // no input injection support
  }
  scoped_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    return make_scoped_ptr<PlatformWindow>(
        new PlatformWindowCast(delegate, bounds));
  }
  scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate() override {
    return make_scoped_ptr(new NativeDisplayDelegateOzone());
  }

  void InitializeUI() override {
    overlay_manager_.reset(new OverlayManagerCast());
    cursor_factory_.reset(new CursorFactoryOzone());
    input_controller_ = CreateStubInputController();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }
  void InitializeGPU() override {
    surface_factory_.reset(new SurfaceFactoryCast(egl_platform_.Pass()));
    gpu_platform_support_.reset(
        new GpuPlatformSupportCast(surface_factory_.get()));
  }

 private:
  scoped_ptr<CastEglPlatform> egl_platform_;
  scoped_ptr<SurfaceFactoryCast> surface_factory_;
  scoped_ptr<CursorFactoryOzone> cursor_factory_;
  scoped_ptr<InputController> input_controller_;
  scoped_ptr<GpuPlatformSupportCast> gpu_platform_support_;
  scoped_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  scoped_ptr<OverlayManagerOzone> overlay_manager_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformCast);
};

}  // namespace

OzonePlatform* CreateOzonePlatformCast() {
  const std::vector<std::string>& argv =
      base::CommandLine::ForCurrentProcess()->argv();
  scoped_ptr<chromecast::CastEglPlatform> platform(
      chromecast::CastEglPlatformShlib::Create(argv));
  return new OzonePlatformCast(platform.Pass());
}

}  // namespace ui
