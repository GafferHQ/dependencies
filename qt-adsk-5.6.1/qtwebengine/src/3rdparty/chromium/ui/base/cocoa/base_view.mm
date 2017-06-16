// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/base_view.h"

#include "base/mac/mac_util.h"

NSString* kViewDidBecomeFirstResponder =
    @"Chromium.kViewDidBecomeFirstResponder";
NSString* kSelectionDirection = @"Chromium.kSelectionDirection";

@implementation BaseView

- (instancetype)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    [self enableTracking];
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder])) {
    [self enableTracking];
  }
  return self;
}

- (void)dealloc {
  [self disableTracking];
  [super dealloc];
}

- (void)enableTracking {
  if (trackingArea_.get())
    return;

  NSTrackingAreaOptions trackingOptions = NSTrackingMouseEnteredAndExited |
                                          NSTrackingMouseMoved |
                                          NSTrackingActiveAlways |
                                          NSTrackingInVisibleRect;
  trackingArea_.reset([[CrTrackingArea alloc] initWithRect:NSZeroRect
                                                   options:trackingOptions
                                                     owner:self
                                                  userInfo:nil]);
  [self addTrackingArea:trackingArea_.get()];
}

- (void)disableTracking {
  if (trackingArea_.get()) {
    [self removeTrackingArea:trackingArea_.get()];
    trackingArea_.reset();
  }
}

- (void)updateTrackingAreas {
  [super updateTrackingAreas];

  // NSTrackingInVisibleRect doesn't work correctly with Lion's window resizing,
  // http://crbug.com/176725 / http://openradar.appspot.com/radar?id=2773401 .
  // Work around it by reinstalling the tracking area after window resize.
  // This AppKit bug is fixed on Yosemite, so we only apply this workaround on
  // 10.7 to 10.9.
  if (base::mac::IsOSMavericksOrEarlier() && base::mac::IsOSLionOrLater()) {
    [self disableTracking];
    [self enableTracking];
  }
}

- (void)mouseEvent:(NSEvent*)theEvent {
  // This method left intentionally blank.
}

- (EventHandled)keyEvent:(NSEvent*)theEvent {
  // The default implementation of this method does not handle any key events.
  // Derived classes should return kEventHandled if they handled an event,
  // otherwise it will be forwarded on to |super|.
  return kEventNotHandled;
}

- (void)mouseDown:(NSEvent*)theEvent {
  dragging_ = YES;
  [self mouseEvent:theEvent];
}

- (void)rightMouseDown:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)otherMouseDown:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)mouseUp:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];

  dragging_ = NO;
  if (pendingExitEvent_.get()) {
    NSEvent* exitEvent =
        [NSEvent enterExitEventWithType:NSMouseExited
                               location:[theEvent locationInWindow]
                          modifierFlags:[theEvent modifierFlags]
                              timestamp:[theEvent timestamp]
                           windowNumber:[theEvent windowNumber]
                                context:[theEvent context]
                            eventNumber:[pendingExitEvent_.get() eventNumber]
                         trackingNumber:[pendingExitEvent_.get() trackingNumber]
                               userData:[pendingExitEvent_.get() userData]];
    [self mouseEvent:exitEvent];
    pendingExitEvent_.reset();
  }
}

- (void)rightMouseUp:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)otherMouseUp:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)mouseMoved:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)mouseDragged:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)rightMouseDragged:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)otherMouseDragged:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)mouseEntered:(NSEvent*)theEvent {
  if (pendingExitEvent_.get()) {
    pendingExitEvent_.reset();
    return;
  }

  [self mouseEvent:theEvent];
}

- (void)mouseExited:(NSEvent*)theEvent {
  // The tracking area will send an exit event even during a drag, which isn't
  // how the event flow for drags should work. This stores the exit event, and
  // sends it when the drag completes instead.
  if (dragging_) {
    pendingExitEvent_.reset([theEvent retain]);
    return;
  }

  [self mouseEvent:theEvent];
}

- (void)keyDown:(NSEvent*)theEvent {
  if ([self keyEvent:theEvent] != kEventHandled)
    [super keyDown:theEvent];
}

- (void)keyUp:(NSEvent*)theEvent {
  if ([self keyEvent:theEvent] != kEventHandled)
    [super keyUp:theEvent];
}

- (void)flagsChanged:(NSEvent*)theEvent {
  if ([self keyEvent:theEvent] != kEventHandled)
    [super flagsChanged:theEvent];
}

- (gfx::Rect)flipNSRectToRect:(NSRect)rect {
  gfx::Rect new_rect(NSRectToCGRect(rect));
  new_rect.set_y(NSHeight([self bounds]) - new_rect.bottom());
  return new_rect;
}

- (NSRect)flipRectToNSRect:(gfx::Rect)rect {
  NSRect new_rect(NSRectFromCGRect(rect.ToCGRect()));
  new_rect.origin.y = NSHeight([self bounds]) - NSMaxY(new_rect);
  return new_rect;
}

@end
