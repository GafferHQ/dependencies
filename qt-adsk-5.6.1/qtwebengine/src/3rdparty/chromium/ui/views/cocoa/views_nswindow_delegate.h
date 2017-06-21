// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_VIEWS_NSWINDOW_DELEGATE_H_
#define UI_VIEWS_COCOA_VIEWS_NSWINDOW_DELEGATE_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "ui/views/views_export.h"

namespace views {
class NativeWidgetMac;
class BridgedNativeWidget;
}

// The delegate set on the NSWindow when a views::BridgedNativeWidget is
// initialized.
VIEWS_EXPORT
@interface ViewsNSWindowDelegate : NSObject<NSWindowDelegate> {
 @private
  views::BridgedNativeWidget* parent_;  // Weak. Owns this.
  base::scoped_nsobject<NSCursor> cursor_;
}

// The NativeWidgetMac that created the window this is attached to. Returns
// NULL if not created by NativeWidgetMac.
@property(nonatomic, readonly) views::NativeWidgetMac* nativeWidgetMac;

// If set, the cursor set in -[NSResponder updateCursor:] when the window is
// reached along the responder chain.
@property(retain, nonatomic) NSCursor* cursor;

// Initialize with the given |parent|.
- (id)initWithBridgedNativeWidget:(views::BridgedNativeWidget*)parent;

// Notify that the window is about to be reordered on screen. This ensures a
// paint will occur, even if Cocoa has not yet updated the window visibility.
- (void)onWindowOrderWillChange:(NSWindowOrderingMode)orderingMode;

// Notify that the window has been reordered in (or removed from) the window
// server's screen list. This is a substitute for -[NSWindowDelegate
// windowDidExpose:], which is only sent for nonretained windows (those without
// a backing store). |notification| is optional and can be set when redirecting
// a notification such as NSApplicationDidHideNotification.
- (void)onWindowOrderChanged:(NSNotification*)notification;

// Notify when -[NSWindow display] is being called on the window.
- (void)onWindowWillDisplay;

// Called on the delegate of a modal sheet when its modal session ends.
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)returnCode
        contextInfo:(void*)contextInfo;

// Redeclare methods defined in the protocol NSWindowDelegate which are only
// available on OSX 10.7+.
- (void)windowDidFailToEnterFullScreen:(NSWindow*)window;
- (void)windowDidFailToExitFullScreen:(NSWindow*)window;

// Called when the application receives a mouse-down, but before the event
// is processed by NSWindows. Returns NO if the event should be processed as-is,
// or YES if the event should be reposted to handle window dragging. Events are
// reposted at the CGSessionEventTap level because window dragging happens there
// before the application receives the event.
- (BOOL)shouldRepostPendingLeftMouseDown:(NSPoint)locationInWindow;

@end

#endif  // UI_VIEWS_COCOA_VIEWS_NSWINDOW_DELEGATE_H_
