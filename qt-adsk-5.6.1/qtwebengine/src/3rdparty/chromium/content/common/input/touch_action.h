// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_TOUCH_ACTION_H_
#define CONTENT_COMMON_INPUT_TOUCH_ACTION_H_

namespace content {

// The current touch action specifies what accelerated browser operations
// (panning and zooming) are currently permitted via touch input.
// See http://www.w3.org/TR/pointerevents/#the-touch-action-css-property.
enum TouchAction {
  // All actions are pemitted (the default).
  TOUCH_ACTION_AUTO = 0,

  // No scrolling or zooming allowed.
  TOUCH_ACTION_NONE = 1 << 0,

  TOUCH_ACTION_PAN_LEFT = 1 << 1,

  TOUCH_ACTION_PAN_RIGHT = 1 << 2,

  TOUCH_ACTION_PAN_X = TOUCH_ACTION_PAN_LEFT | TOUCH_ACTION_PAN_RIGHT,

  TOUCH_ACTION_PAN_UP = 1 << 3,

  TOUCH_ACTION_PAN_DOWN = 1 << 4,

  TOUCH_ACTION_PAN_Y = TOUCH_ACTION_PAN_UP | TOUCH_ACTION_PAN_DOWN,

  TOUCH_ACTION_PAN_X_Y = TOUCH_ACTION_PAN_X | TOUCH_ACTION_PAN_Y,

  TOUCH_ACTION_PINCH_ZOOM = 1 << 5,

  TOUCH_ACTION_MANIPULATION = TOUCH_ACTION_PAN_X_Y | TOUCH_ACTION_PINCH_ZOOM,

  TOUCH_ACTION_MAX = (1 << 6) - 1
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_TOUCH_ACTION_H_
