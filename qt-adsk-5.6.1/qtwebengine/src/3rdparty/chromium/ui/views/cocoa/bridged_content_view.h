// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_BRIDGED_CONTENT_VIEW_H_
#define UI_VIEWS_COCOA_BRIDGED_CONTENT_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/strings/string16.h"
#import "ui/base/cocoa/tool_tip_base_view.h"
#import "ui/base/cocoa/tracking_area.h"

namespace ui {
class TextInputClient;
}

namespace views {
class View;
}

// The NSView that sits as the root contentView of the NSWindow, whilst it has
// a views::RootView present. Bridges requests from Cocoa to the hosted
// views::View.
@interface BridgedContentView
    : ToolTipBaseView<NSTextInputClient, NSUserInterfaceValidations> {
 @private
  // Weak. The hosted RootView, owned by hostedView_->GetWidget().
  views::View* hostedView_;

  // Weak. If non-null the TextInputClient of the currently focused View in the
  // hierarchy rooted at |hostedView_|. Owned by the focused View.
  ui::TextInputClient* textInputClient_;

  // A tracking area installed to enable mouseMoved events.
  ui::ScopedCrTrackingArea cursorTrackingArea_;

  // Whether the view is reacting to a keyDown event on the view.
  BOOL inKeyDown_;

  // The last tooltip text, used to limit updates.
  base::string16 lastTooltipText_;

  // Whether dragging on the view moves the window.
  BOOL mouseDownCanMoveWindow_;
}

@property(readonly, nonatomic) views::View* hostedView;
@property(assign, nonatomic) ui::TextInputClient* textInputClient;

// Extends an atomic, readonly property on NSView to make it assignable.
// This usually returns YES if the view is transparent. We want to control it
// so that BridgedNativeWidget can dynamically enable dragging of the window.
@property(assign) BOOL mouseDownCanMoveWindow;

// Initialize the NSView -> views::View bridge. |viewToHost| must be non-NULL.
- (id)initWithView:(views::View*)viewToHost;

// Clear the hosted view. For example, if it is about to be destroyed.
- (void)clearView;

// Process a mouse event captured while the widget had global mouse capture.
- (void)processCapturedMouseEvent:(NSEvent*)theEvent;

// Mac's version of views::corewm::TooltipController::UpdateIfRequired().
// Updates the tooltip on the ToolTipBaseView if the text needs to change.
// |locationInContent| is the position from the top left of the window's
// contentRect (also this NSView's frame), as given by a ui::LocatedEvent.
- (void)updateTooltipIfRequiredAt:(const gfx::Point&)locationInContent;

@end

#endif  // UI_VIEWS_COCOA_BRIDGED_CONTENT_VIEW_H_
