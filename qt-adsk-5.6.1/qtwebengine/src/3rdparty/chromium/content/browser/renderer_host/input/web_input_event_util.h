// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_UTIL_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_UTIL_H_

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {
struct GestureEventData;
struct GestureEventDetails;
class MotionEvent;
}

namespace content {

// Update |event|'s windowsKeyCode and keyIdentifer properties using the
// provided |windows_key_code|.
CONTENT_EXPORT void UpdateWindowsKeyCodeAndKeyIdentifier(
    blink::WebKeyboardEvent* event,
    ui::KeyboardCode windows_key_code);

int WebEventModifiersToEventFlags(int modifiers);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_UTIL_H_
