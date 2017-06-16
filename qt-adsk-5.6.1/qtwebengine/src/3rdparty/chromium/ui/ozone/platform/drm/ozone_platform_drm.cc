// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/ozone_platform_drm.h"

#include "base/at_exit.h"
#include "base/thread_task_runner_handle.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_platform_support.h"
#include "ui/ozone/platform/drm/gpu/drm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"
#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"
#include "ui/ozone/platform/drm/host/drm_native_display_delegate.h"
#include "ui/ozone/platform/drm/host/drm_overlay_manager.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#include "ui/ozone/public/ozone_platform.h"

#if defined(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

namespace ui {

namespace {

// OzonePlatform for Linux DRM (Direct Rendering Manager)
//
// This platform is Linux without any display server (no X, wayland, or
// anything). This means chrome alone owns the display and input devices.
class OzonePlatformDrm : public OzonePlatform {
 public:
  OzonePlatformDrm()
      : buffer_generator_(new DrmBufferGenerator()),
        screen_manager_(new ScreenManager(buffer_generator_.get())),
        device_manager_(CreateDeviceManager()) {}
  ~OzonePlatformDrm() override {}

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_ozone_.get();
  }
  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }
  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
  }
  InputController* GetInputController() override {
    return event_factory_ozone_->input_controller();
  }
  GpuPlatformSupport* GetGpuPlatformSupport() override {
    return gpu_platform_support_.get();
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }
  scoped_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return event_factory_ozone_->CreateSystemInputInjector();
  }
  scoped_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    scoped_ptr<DrmWindowHost> platform_window(
        new DrmWindowHost(delegate, bounds, gpu_platform_support_host_.get(),
                          event_factory_ozone_.get(), cursor_.get(),
                          window_manager_.get(), display_manager_.get()));
    platform_window->Initialize();
    return platform_window.Pass();
  }
  scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate() override {
    return make_scoped_ptr(
        new DrmNativeDisplayDelegate(display_manager_.get()));
  }
  void InitializeUI() override {
    drm_device_manager_.reset(new DrmDeviceManager(
        scoped_ptr<DrmDeviceGenerator>(new DrmDeviceGenerator())));
    window_manager_.reset(new DrmWindowHostManager());
    cursor_.reset(new DrmCursor(window_manager_.get()));
#if defined(USE_XKBCOMMON)
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(make_scoped_ptr(
        new XkbKeyboardLayoutEngine(xkb_evdev_code_converter_)));
#else
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        make_scoped_ptr(new StubKeyboardLayoutEngine()));
#endif
    event_factory_ozone_.reset(new EventFactoryEvdev(
        cursor_.get(), device_manager_.get(),
        KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()));
    overlay_manager_.reset(new DrmOverlayManager(false, nullptr));
    surface_factory_ozone_.reset(new DrmSurfaceFactory(screen_manager_.get()));
    scoped_ptr<DrmGpuDisplayManager> display_manager(new DrmGpuDisplayManager(
        screen_manager_.get(), drm_device_manager_.get()));
    gpu_platform_support_.reset(new DrmGpuPlatformSupport(
        drm_device_manager_.get(), screen_manager_.get(),
        buffer_generator_.get(), display_manager.Pass()));
    gpu_platform_support_host_.reset(
        new DrmGpuPlatformSupportHost(cursor_.get()));
    display_manager_.reset(new DrmDisplayHostManager(
        gpu_platform_support_host_.get(), device_manager_.get(),
        event_factory_ozone_->input_controller()));
    cursor_factory_ozone_.reset(new BitmapCursorFactoryOzone);

    if (!gpu_helper_.Initialize(base::ThreadTaskRunnerHandle::Get(),
                                base::ThreadTaskRunnerHandle::Get()))
      LOG(FATAL) << "Failed to initialize dummy channel.";
  }

  void InitializeGPU() override {}

 private:
  // Objects in the "GPU" process.
  scoped_ptr<DrmDeviceManager> drm_device_manager_;
  scoped_ptr<DrmBufferGenerator> buffer_generator_;
  scoped_ptr<ScreenManager> screen_manager_;
  scoped_ptr<DrmGpuPlatformSupport> gpu_platform_support_;

  // Objects in the "Browser" process.
  scoped_ptr<OverlayManagerOzone> overlay_manager_;
  scoped_ptr<DeviceManager> device_manager_;
  scoped_ptr<BitmapCursorFactoryOzone> cursor_factory_ozone_;
  scoped_ptr<DrmWindowHostManager> window_manager_;
  scoped_ptr<DrmCursor> cursor_;
  scoped_ptr<EventFactoryEvdev> event_factory_ozone_;
  scoped_ptr<DrmGpuPlatformSupportHost> gpu_platform_support_host_;
  scoped_ptr<DrmDisplayHostManager> display_manager_;

#if defined(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  // Objects on both processes.
  scoped_ptr<DrmSurfaceFactory> surface_factory_ozone_;

  OzoneGpuTestHelper gpu_helper_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformDrm);
};

}  // namespace

OzonePlatform* CreateOzonePlatformDri() {
  return new OzonePlatformDrm;
}

OzonePlatform* CreateOzonePlatformDrm() {
  return new OzonePlatformDrm;
}

}  // namespace ui
