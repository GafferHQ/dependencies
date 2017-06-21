// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/screen_manager.h"

#include <xf86drmMode.h>

#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_console_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

namespace ui {

namespace {

// Copies the contents of the saved framebuffer from the CRTCs in |controller|
// to the new modeset buffer |buffer|.
void FillModesetBuffer(const scoped_refptr<DrmDevice>& drm,
                       HardwareDisplayController* controller,
                       ScanoutBuffer* buffer) {
  DrmConsoleBuffer modeset_buffer(drm, buffer->GetFramebufferId());
  if (!modeset_buffer.Initialize()) {
    VLOG(2) << "Failed to grab framebuffer " << buffer->GetFramebufferId();
    return;
  }

  auto crtcs = controller->crtc_controllers();
  DCHECK(!crtcs.empty());

  ScopedDrmCrtcPtr saved_crtc(drm->GetCrtc(crtcs[0]->crtc()));
  if (!saved_crtc || !saved_crtc->buffer_id) {
    VLOG(2) << "Crtc has no saved state or wasn't modeset";
    return;
  }

  // If the display controller is in mirror mode, the CRTCs should be sharing
  // the same framebuffer.
  DrmConsoleBuffer saved_buffer(drm, saved_crtc->buffer_id);
  if (!saved_buffer.Initialize()) {
    VLOG(2) << "Failed to grab saved framebuffer " << saved_crtc->buffer_id;
    return;
  }

  // Don't copy anything if the sizes mismatch. This can happen when the user
  // changes modes.
  if (saved_buffer.canvas()->getBaseLayerSize() !=
      modeset_buffer.canvas()->getBaseLayerSize()) {
    VLOG(2) << "Previous buffer has a different size than modeset buffer";
    return;
  }

  skia::RefPtr<SkImage> image = saved_buffer.image();
  SkPaint paint;
  // Copy the source buffer. Do not perform any blending.
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  modeset_buffer.canvas()->drawImage(image.get(), 0, 0, &paint);
}

CrtcController* GetCrtcController(HardwareDisplayController* controller,
                                  const scoped_refptr<DrmDevice>& drm,
                                  uint32_t crtc) {
  for (CrtcController* crtc_controller : controller->crtc_controllers()) {
    if (crtc_controller->crtc() == crtc)
      return crtc_controller;
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace

ScreenManager::ScreenManager(ScanoutBufferGenerator* buffer_generator)
    : buffer_generator_(buffer_generator) {
}

ScreenManager::~ScreenManager() {
  DCHECK(window_map_.empty());
}

void ScreenManager::AddDisplayController(const scoped_refptr<DrmDevice>& drm,
                                         uint32_t crtc,
                                         uint32_t connector) {
  HardwareDisplayControllers::iterator it = FindDisplayController(drm, crtc);
  // TODO(dnicoara): Turn this into a DCHECK when async display configuration is
  // properly supported. (When there can't be a race between forcing initial
  // display configuration in ScreenManager and NativeDisplayDelegate creating
  // the display controllers.)
  if (it != controllers_.end()) {
    LOG(WARNING) << "Display controller (crtc=" << crtc << ") already present.";
    return;
  }

  controllers_.push_back(new HardwareDisplayController(
      scoped_ptr<CrtcController>(new CrtcController(drm, crtc, connector)),
      gfx::Point()));
}

void ScreenManager::RemoveDisplayController(const scoped_refptr<DrmDevice>& drm,
                                            uint32_t crtc) {
  HardwareDisplayControllers::iterator it = FindDisplayController(drm, crtc);
  if (it != controllers_.end()) {
    bool is_mirrored = (*it)->IsMirrored();
    (*it)->RemoveCrtc(drm, crtc);
    if (!is_mirrored) {
      controllers_.erase(it);
      UpdateControllerToWindowMapping();
    }
  }
}

bool ScreenManager::ConfigureDisplayController(
    const scoped_refptr<DrmDevice>& drm,
    uint32_t crtc,
    uint32_t connector,
    const gfx::Point& origin,
    const drmModeModeInfo& mode) {
  bool status =
      ActualConfigureDisplayController(drm, crtc, connector, origin, mode);
  if (status)
    UpdateControllerToWindowMapping();

  return status;
}

bool ScreenManager::ActualConfigureDisplayController(
    const scoped_refptr<DrmDevice>& drm,
    uint32_t crtc,
    uint32_t connector,
    const gfx::Point& origin,
    const drmModeModeInfo& mode) {
  gfx::Rect modeset_bounds(origin.x(), origin.y(), mode.hdisplay,
                           mode.vdisplay);
  HardwareDisplayControllers::iterator it = FindDisplayController(drm, crtc);
  DCHECK(controllers_.end() != it) << "Display controller (crtc=" << crtc
                                   << ") doesn't exist.";

  HardwareDisplayController* controller = *it;
  CrtcController* crtc_controller = GetCrtcController(controller, drm, crtc);
  // If nothing changed just enable the controller. Note, we perform an exact
  // comparison on the mode since the refresh rate may have changed.
  if (SameMode(mode, crtc_controller->mode()) &&
      origin == controller->origin()) {
    if (controller->IsDisabled()) {
      HardwareDisplayControllers::iterator mirror =
          FindActiveDisplayControllerByLocation(modeset_bounds);
      // If there is an active controller at the same location then start mirror
      // mode.
      if (mirror != controllers_.end())
        return HandleMirrorMode(it, mirror, drm, crtc, connector, mode);
    }

    // Just re-enable the controller to re-use the current state.
    return EnableController(controller);
  }

  // Either the mode or the location of the display changed, so exit mirror
  // mode and configure the display independently. If the caller still wants
  // mirror mode, subsequent calls configuring the other controllers will
  // restore mirror mode.
  if (controller->IsMirrored()) {
    controller = new HardwareDisplayController(
        controller->RemoveCrtc(drm, crtc), controller->origin());
    controllers_.push_back(controller);
    it = controllers_.end() - 1;
  }

  HardwareDisplayControllers::iterator mirror =
      FindActiveDisplayControllerByLocation(modeset_bounds);
  // Handle mirror mode.
  if (mirror != controllers_.end() && it != mirror)
    return HandleMirrorMode(it, mirror, drm, crtc, connector, mode);

  return ModesetController(controller, origin, mode);
}

bool ScreenManager::DisableDisplayController(
    const scoped_refptr<DrmDevice>& drm,
    uint32_t crtc) {
  HardwareDisplayControllers::iterator it = FindDisplayController(drm, crtc);
  if (it != controllers_.end()) {
    HardwareDisplayController* controller = *it;
    if (controller->IsMirrored()) {
      controller = new HardwareDisplayController(
          controller->RemoveCrtc(drm, crtc), controller->origin());
      controllers_.push_back(controller);
    }

    controller->Disable();
    UpdateControllerToWindowMapping();
    return true;
  }

  LOG(ERROR) << "Failed to find display controller crtc=" << crtc;
  return false;
}

HardwareDisplayController* ScreenManager::GetDisplayController(
    const gfx::Rect& bounds) {
  HardwareDisplayControllers::iterator it =
      FindActiveDisplayControllerByLocation(bounds);
  if (it != controllers_.end())
    return *it;

  return nullptr;
}

void ScreenManager::AddWindow(gfx::AcceleratedWidget widget,
                              scoped_ptr<DrmWindow> window) {
  std::pair<WidgetToWindowMap::iterator, bool> result =
      window_map_.add(widget, window.Pass());
  DCHECK(result.second) << "Window already added.";
  UpdateControllerToWindowMapping();
}

scoped_ptr<DrmWindow> ScreenManager::RemoveWindow(
    gfx::AcceleratedWidget widget) {
  scoped_ptr<DrmWindow> window = window_map_.take_and_erase(widget);
  DCHECK(window) << "Attempting to remove non-existing window for " << widget;
  UpdateControllerToWindowMapping();
  return window.Pass();
}

DrmWindow* ScreenManager::GetWindow(gfx::AcceleratedWidget widget) {
  WidgetToWindowMap::iterator it = window_map_.find(widget);
  if (it != window_map_.end())
    return it->second;

  NOTREACHED() << "Attempting to get non-existing window for " << widget;
  return nullptr;
}

ScreenManager::HardwareDisplayControllers::iterator
ScreenManager::FindDisplayController(const scoped_refptr<DrmDevice>& drm,
                                     uint32_t crtc) {
  for (HardwareDisplayControllers::iterator it = controllers_.begin();
       it != controllers_.end(); ++it) {
    if ((*it)->HasCrtc(drm, crtc))
      return it;
  }

  return controllers_.end();
}

ScreenManager::HardwareDisplayControllers::iterator
ScreenManager::FindActiveDisplayControllerByLocation(const gfx::Rect& bounds) {
  for (HardwareDisplayControllers::iterator it = controllers_.begin();
       it != controllers_.end(); ++it) {
    gfx::Rect controller_bounds((*it)->origin(), (*it)->GetModeSize());
    if (controller_bounds == bounds && !(*it)->IsDisabled())
      return it;
  }

  return controllers_.end();
}

bool ScreenManager::HandleMirrorMode(
    HardwareDisplayControllers::iterator original,
    HardwareDisplayControllers::iterator mirror,
    const scoped_refptr<DrmDevice>& drm,
    uint32_t crtc,
    uint32_t connector,
    const drmModeModeInfo& mode) {
  gfx::Point last_origin = (*original)->origin();
  // There should only be one CRTC in this controller.
  drmModeModeInfo last_mode = (*original)->crtc_controllers()[0]->mode();

  // Modeset the CRTC with its mode in the original controller so that only this
  // CRTC is affected by the mode. Otherwise it could apply a mode with the same
  // resolution and refresh rate but with different timings to the other CRTC.
  // TODO(dnicoara): This is hacky, instead the DrmDisplay and CrtcController
  // should be merged and picking the mode should be done properly within
  // HardwareDisplayController.
  if (ModesetController(*original, (*mirror)->origin(), mode)) {
    (*mirror)->AddCrtc((*original)->RemoveCrtc(drm, crtc));
    controllers_.erase(original);
    return true;
  }

  LOG(ERROR) << "Failed to switch to mirror mode";

  // When things go wrong revert back to the previous configuration since
  // it is expected that the configuration would not have changed if
  // things fail.
  ModesetController(*original, last_origin, last_mode);
  return false;
}

void ScreenManager::UpdateControllerToWindowMapping() {
  std::map<DrmWindow*, HardwareDisplayController*> window_to_controller_map;
  // First create a unique mapping between a window and a controller. Note, a
  // controller may be associated with at most 1 window.
  for (HardwareDisplayController* controller : controllers_) {
    if (controller->IsDisabled())
      continue;

    DrmWindow* window = FindWindowAt(
        gfx::Rect(controller->origin(), controller->GetModeSize()));
    if (!window)
      continue;

    window_to_controller_map[window] = controller;
  }

  // Apply the new mapping to all windows.
  for (auto pair : window_map_) {
    auto it = window_to_controller_map.find(pair.second);
    if (it != window_to_controller_map.end())
      pair.second->SetController(it->second);
    else
      pair.second->SetController(nullptr);
  }
}

OverlayPlane ScreenManager::GetModesetBuffer(
    HardwareDisplayController* controller,
    const gfx::Rect& bounds) {
  DrmWindow* window = FindWindowAt(bounds);
  if (window) {
    const OverlayPlane* primary = window->GetLastModesetBuffer();
    if (primary && primary->buffer->GetSize() == bounds.size())
      return *primary;
  }

  scoped_refptr<DrmDevice> drm = controller->GetAllocationDrmDevice();
  scoped_refptr<ScanoutBuffer> buffer =
      buffer_generator_->Create(drm, bounds.size());
  if (!buffer) {
    LOG(ERROR) << "Failed to create scanout buffer";
    return OverlayPlane(nullptr, 0, gfx::OVERLAY_TRANSFORM_INVALID, gfx::Rect(),
                        gfx::RectF());
  }

  FillModesetBuffer(drm, controller, buffer.get());
  return OverlayPlane(buffer);
}

bool ScreenManager::EnableController(HardwareDisplayController* controller) {
  DCHECK(!controller->crtc_controllers().empty());
  gfx::Rect rect(controller->origin(), controller->GetModeSize());
  OverlayPlane plane = GetModesetBuffer(controller, rect);
  if (!plane.buffer || !controller->Enable(plane)) {
    LOG(ERROR) << "Failed to enable controller";
    return false;
  }

  return true;
}

bool ScreenManager::ModesetController(HardwareDisplayController* controller,
                                      const gfx::Point& origin,
                                      const drmModeModeInfo& mode) {
  DCHECK(!controller->crtc_controllers().empty());
  gfx::Rect rect(origin, gfx::Size(mode.hdisplay, mode.vdisplay));
  controller->set_origin(origin);

  OverlayPlane plane = GetModesetBuffer(controller, rect);
  if (!plane.buffer || !controller->Modeset(plane, mode)) {
    LOG(ERROR) << "Failed to modeset controller";
    return false;
  }

  return true;
}

DrmWindow* ScreenManager::FindWindowAt(const gfx::Rect& bounds) const {
  for (auto pair : window_map_) {
    if (pair.second->bounds() == bounds)
      return pair.second;
  }

  return nullptr;
}

}  // namespace ui
