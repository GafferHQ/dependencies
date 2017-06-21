// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_H_

#include "base/basictypes.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class InputRouterClient;

// The InputRouter allows the embedder to customize how input events are
// sent to the renderer, and how responses are dispatched to the browser.
// While the router should respect the relative order in which events are
// received, it is free to customize when those events are dispatched.
class InputRouter : public IPC::Listener {
 public:
  ~InputRouter() override {}

  // Send and take ownership of the the given InputMsg_*. This should be used
  // only for event types not associated with a WebInputEvent.  Returns true on
  // success and false otherwise.
  virtual bool SendInput(scoped_ptr<IPC::Message> message) = 0;

  // WebInputEvents
  virtual void SendMouseEvent(
      const MouseEventWithLatencyInfo& mouse_event) = 0;
  virtual void SendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) = 0;
  virtual void SendKeyboardEvent(
      const NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info,
      bool is_shortcut) = 0;
  virtual void SendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) = 0;
  virtual void SendTouchEvent(
      const TouchEventWithLatencyInfo& touch_event) = 0;

  // Returns the oldest queued or in-flight keyboard event sent to the router.
  virtual const NativeWebKeyboardEvent* GetLastKeyboardEvent() const = 0;

  // Notify the router about whether the current page is mobile-optimized (i.e.,
  // the site has a mobile-friendly viewport).
  virtual void NotifySiteIsMobileOptimized(bool is_mobile_optimized) = 0;

  // Request a notification from the input router when all events have been
  // fully dispatched and there are no longer any pending events.
  // Note: This may trigger a synchronous notification if the router is empty.
  virtual void RequestNotificationWhenFlushed() = 0;

  // Whether there are any events pending dispatch to or ack from the renderer.
  virtual bool HasPendingEvents() const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_H_
