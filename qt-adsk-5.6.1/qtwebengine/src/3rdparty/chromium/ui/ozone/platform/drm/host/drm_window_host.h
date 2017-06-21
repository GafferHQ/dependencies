// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_WINDOW_HOST_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_WINDOW_HOST_H_

#include "base/memory/scoped_ptr.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/host/channel_observer.h"
#include "ui/platform_window/platform_window.h"

namespace ui {

class DrmDisplayHostManager;
class DrmCursor;
class DrmGpuPlatformSupportHost;
class DrmGpuWindow;
class DrmWindowHostManager;
class EventFactoryEvdev;

// Implementation of the platform window. This object and its handle |widget_|
// uniquely identify a window. Since the DRI/GBM platform is split into 2
// pieces (Browser process and GPU process), internally we need to make sure the
// state is synchronized between the 2 processes.
//
// |widget_| is used in both processes to uniquely identify the window. This
// means that any state on the browser side needs to be propagated to the GPU.
// State propagation needs to happen before the state change is acknowledged to
// |delegate_| as |delegate_| is responsible for initializing the surface
// associated with the window (the surface is created on the GPU process).
class DrmWindowHost : public PlatformWindow,
                      public PlatformEventDispatcher,
                      public ChannelObserver {
 public:
  DrmWindowHost(PlatformWindowDelegate* delegate,
                const gfx::Rect& bounds,
                DrmGpuPlatformSupportHost* sender,
                EventFactoryEvdev* event_factory,
                DrmCursor* cursor,
                DrmWindowHostManager* window_manager,
                DrmDisplayHostManager* display_manager);
  ~DrmWindowHost() override;

  void Initialize();

  gfx::AcceleratedWidget GetAcceleratedWidget();

  gfx::Rect GetCursorConfinedBounds() const;

  // PlatformWindow:
  void Show() override;
  void Hide() override;
  void Close() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  // ChannelObserver:
  void OnChannelEstablished() override;
  void OnChannelDestroyed() override;

 private:
  void SendBoundsChange();

  PlatformWindowDelegate* delegate_;        // Not owned.
  DrmGpuPlatformSupportHost* sender_;       // Not owned.
  EventFactoryEvdev* event_factory_;        // Not owned.
  DrmCursor* cursor_;                       // Not owned.
  DrmWindowHostManager* window_manager_;    // Not owned.
  DrmDisplayHostManager* display_manager_;  // Not owned.

  gfx::Rect bounds_;
  gfx::AcceleratedWidget widget_;

  gfx::Rect cursor_confined_bounds_;

  DISALLOW_COPY_AND_ASSIGN(DrmWindowHost);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_WINDOW_HOST_H_
