/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "ozone_platform_eglfs.h"

#if defined(USE_OZONE)

#include "base/bind.h"
#include "media/ozone/media_ozone_platform.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/ozone/common/native_display_delegate_ozone.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"


namespace media {

MediaOzonePlatform* CreateMediaOzonePlatformEglfs() {
  return new MediaOzonePlatform;
}

}

namespace ui {

namespace {
class EglfsWindow : public PlatformWindow, public PlatformEventDispatcher {
public:
    EglfsWindow(PlatformWindowDelegate* delegate,
                EventFactoryEvdev* event_factory,
                const gfx::Rect& bounds)
        : delegate_(delegate)
        , event_factory_(event_factory)
        , bounds_(bounds)
    {
        ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
    }

    ~EglfsWindow() override
    {
        ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
    }

    // PlatformWindow:
    gfx::Rect GetBounds() override;
    void SetBounds(const gfx::Rect& bounds) override;
    void Show() override { }
    void Hide() override { }
    void Close() override { }
    void SetCapture() override { }
    void ReleaseCapture() override { }
    void ToggleFullscreen() override { }
    void Maximize() override { }
    void Minimize() override { }
    void Restore() override { }
    void SetCursor(PlatformCursor) override { }
    void MoveCursorTo(const gfx::Point&) override { }
    void ConfineCursorToBounds(const gfx::Rect&) override { }

    // PlatformEventDispatcher:
    bool CanDispatchEvent(const PlatformEvent& event) override;
    uint32_t DispatchEvent(const PlatformEvent& event) override;

private:
    PlatformWindowDelegate* delegate_;
    EventFactoryEvdev* event_factory_;
    gfx::Rect bounds_;

    DISALLOW_COPY_AND_ASSIGN(EglfsWindow);
};

gfx::Rect EglfsWindow::GetBounds() {
    return bounds_;
}

void EglfsWindow::SetBounds(const gfx::Rect& bounds) {
    bounds_ = bounds;
    delegate_->OnBoundsChanged(bounds);
}

bool EglfsWindow::CanDispatchEvent(const ui::PlatformEvent& ne) {
    return true;
}

uint32_t EglfsWindow::DispatchEvent(const ui::PlatformEvent& native_event) {
    DispatchEventFromNativeUiEvent(
                native_event, base::Bind(&PlatformWindowDelegate::DispatchEvent,
                                         base::Unretained(delegate_)));

    return ui::POST_DISPATCH_STOP_PROPAGATION;
}
} // namespace

OzonePlatformEglfs::OzonePlatformEglfs() {}

OzonePlatformEglfs::~OzonePlatformEglfs() {}

ui::SurfaceFactoryOzone* OzonePlatformEglfs::GetSurfaceFactoryOzone() {
  return surface_factory_ozone_.get();
}

ui::CursorFactoryOzone* OzonePlatformEglfs::GetCursorFactoryOzone() {
  return cursor_factory_ozone_.get();
}

GpuPlatformSupport* OzonePlatformEglfs::GetGpuPlatformSupport() {
  return gpu_platform_support_.get();
}

GpuPlatformSupportHost* OzonePlatformEglfs::GetGpuPlatformSupportHost() {
  return gpu_platform_support_host_.get();
}

scoped_ptr<PlatformWindow> OzonePlatformEglfs::CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds)
{
    return make_scoped_ptr<PlatformWindow>(
        new EglfsWindow(delegate,
                          event_factory_ozone_.get(),
                          bounds));
}

ui::InputController* OzonePlatformEglfs::GetInputController() {
    return input_controller_.get();
}

scoped_ptr<ui::SystemInputInjector> OzonePlatformEglfs::CreateSystemInputInjector() {
    return nullptr;  // no input injection support.
}

ui::OverlayManagerOzone* OzonePlatformEglfs::GetOverlayManager() {
    return overlay_manager_.get();
}

scoped_ptr<ui::NativeDisplayDelegate> OzonePlatformEglfs::CreateNativeDisplayDelegate()
{
    return scoped_ptr<NativeDisplayDelegate>(new NativeDisplayDelegateOzone());
}

OzonePlatform* CreateOzonePlatformEglfs() { return new OzonePlatformEglfs; }

void OzonePlatformEglfs::InitializeUI() {
  overlay_manager_.reset(new StubOverlayManager());
  device_manager_ = CreateDeviceManager();
  cursor_factory_ozone_.reset(new CursorFactoryOzone());
  event_factory_ozone_.reset(new EventFactoryEvdev(NULL, device_manager_.get(), NULL));
  gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  input_controller_ = CreateStubInputController();
}

void OzonePlatformEglfs::InitializeGPU() {
  surface_factory_ozone_.reset(new QtWebEngineCore::SurfaceFactoryQt());
  gpu_platform_support_.reset(CreateStubGpuPlatformSupport());
}

}  // namespace ui

#endif

