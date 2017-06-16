// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/cocoa_mouse_capture.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#import "ui/views/cocoa/cocoa_mouse_capture_delegate.h"

namespace views {

// The ActiveEventTap is a RAII handle on the resources being used to capture
// events. There is either 0 or 1 active instance of this class. If a second
// instance is created, it will destroy the other instance before returning from
// its constructor.
class CocoaMouseCapture::ActiveEventTap {
 public:
  explicit ActiveEventTap(CocoaMouseCapture* owner);
  ~ActiveEventTap();
  void Init();

 private:
  // The currently active event tap, or null if there is none.
  static ActiveEventTap* g_active_event_tap;

  CocoaMouseCapture* owner_;  // Weak. Owns this.
  id local_monitor_;
  id global_monitor_;

  DISALLOW_COPY_AND_ASSIGN(ActiveEventTap);
};

CocoaMouseCapture::ActiveEventTap*
    CocoaMouseCapture::ActiveEventTap::g_active_event_tap = nullptr;

CocoaMouseCapture::ActiveEventTap::ActiveEventTap(CocoaMouseCapture* owner)
    : owner_(owner), local_monitor_(nil), global_monitor_(nil) {
  if (g_active_event_tap)
    g_active_event_tap->owner_->OnOtherClientGotCapture();
  DCHECK(!g_active_event_tap);
  g_active_event_tap = this;
}

CocoaMouseCapture::ActiveEventTap::~ActiveEventTap() {
  DCHECK_EQ(g_active_event_tap, this);
  [NSEvent removeMonitor:global_monitor_];
  [NSEvent removeMonitor:local_monitor_];
  g_active_event_tap = nullptr;
  owner_->delegate_->OnMouseCaptureLost();
}

void CocoaMouseCapture::ActiveEventTap::Init() {
  NSEventMask event_mask =
      NSLeftMouseDownMask | NSLeftMouseUpMask | NSRightMouseDownMask |
      NSRightMouseUpMask | NSMouseMovedMask | NSLeftMouseDraggedMask |
      NSRightMouseDraggedMask | NSMouseEnteredMask | NSMouseExitedMask |
      NSScrollWheelMask | NSOtherMouseDownMask | NSOtherMouseUpMask |
      NSOtherMouseDraggedMask;

  local_monitor_ = [NSEvent addLocalMonitorForEventsMatchingMask:event_mask
      handler:^NSEvent*(NSEvent* event) {
        owner_->delegate_->PostCapturedEvent(event);
        return nil;  // Swallow all local events.
      }];
  global_monitor_ = [NSEvent addGlobalMonitorForEventsMatchingMask:event_mask
      handler:^void(NSEvent* event) {
        owner_->delegate_->PostCapturedEvent(event);
      }];
}

CocoaMouseCapture::CocoaMouseCapture(CocoaMouseCaptureDelegate* delegate)
    : delegate_(delegate), active_handle_(new ActiveEventTap(this)) {
  active_handle_->Init();
}

CocoaMouseCapture::~CocoaMouseCapture() {
}

void CocoaMouseCapture::OnOtherClientGotCapture() {
  DCHECK(active_handle_);
  active_handle_.reset();
}

}  // namespace views
