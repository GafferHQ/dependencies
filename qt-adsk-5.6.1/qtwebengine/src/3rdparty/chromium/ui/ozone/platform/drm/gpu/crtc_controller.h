// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_CRTC_CONTROLLER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_CRTC_CONTROLLER_H_

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"
#include "ui/ozone/platform/drm/gpu/overlay_plane.h"

namespace ui {

class DrmDevice;
class PageFlipRequest;

// Wrapper around a CRTC.
//
// One CRTC can be paired up with one or more connectors. The simplest
// configuration represents one CRTC driving one monitor, while pairing up a
// CRTC with multiple connectors results in hardware mirroring.
class OZONE_EXPORT CrtcController
    : public base::SupportsWeakPtr<CrtcController> {
 public:
  CrtcController(const scoped_refptr<DrmDevice>& drm,
                 uint32_t crtc,
                 uint32_t connector);
  ~CrtcController();

  drmModeModeInfo mode() const { return mode_; }
  uint32_t crtc() const { return crtc_; }
  uint32_t connector() const { return connector_; }
  const scoped_refptr<DrmDevice>& drm() const { return drm_; }
  bool is_disabled() const { return is_disabled_; }
  uint64_t time_of_last_flip() const { return time_of_last_flip_; }

  // Perform the initial modesetting operation using |plane| as the buffer for
  // the primary plane. The CRTC configuration is specified by |mode|.
  bool Modeset(const OverlayPlane& plane, drmModeModeInfo mode);

  // Disables the controller.
  bool Disable();

  // Schedule a page flip event and present the overlays in |planes|.
  bool SchedulePageFlip(HardwareDisplayPlaneList* plane_list,
                        const OverlayPlaneList& planes,
                        bool test_only,
                        scoped_refptr<PageFlipRequest> page_flip_request);

  // Called if the page flip for this CRTC fails after being scheduled.
  void PageFlipFailed();

  // Called when the page flip event occurred. The event is provided by the
  // kernel when a VBlank event finished. This allows the controller to
  // update internal state and propagate the update to the surface.
  // The tuple (seconds, useconds) represents the event timestamp. |seconds|
  // represents the number of seconds while |useconds| represents the
  // microseconds (< 1 second) in the timestamp.
  void OnPageFlipEvent(unsigned int frame,
                       unsigned int seconds,
                       unsigned int useconds);

  bool SetCursor(const scoped_refptr<ScanoutBuffer>& buffer);
  bool MoveCursor(const gfx::Point& location);

 private:
  bool ResetCursor();

  void SignalPageFlipRequest();

  scoped_refptr<DrmDevice> drm_;

  HardwareDisplayPlaneManager* overlay_plane_manager_;  // Not owned.

  // Buffers need to be declared first so that they are destroyed last. Needed
  // since the controllers may reference the buffers.
  OverlayPlaneList current_planes_;
  OverlayPlaneList pending_planes_;
  scoped_refptr<ScanoutBuffer> cursor_buffer_;
  scoped_refptr<PageFlipRequest> page_flip_request_;

  uint32_t crtc_;

  // TODO(dnicoara) Add support for hardware mirroring (multiple connectors).
  uint32_t connector_;

  drmModeModeInfo mode_;

  // Keeps track of the CRTC state. If a surface has been bound, then the value
  // is set to false. Otherwise it is true.
  bool is_disabled_ = true;

  // The time of the last page flip event as reported by the kernel callback.
  uint64_t time_of_last_flip_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CrtcController);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_CRTC_CONTROLLER_H_
