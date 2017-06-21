// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_event_details.h"

namespace ui {

GestureEventDetails::GestureEventDetails()
    : type_(ET_UNKNOWN), touch_points_(0), oldest_touch_id_(-1) {
}

GestureEventDetails::GestureEventDetails(ui::EventType type)
    : type_(type), touch_points_(1), oldest_touch_id_(-1) {
  DCHECK_GE(type, ET_GESTURE_TYPE_START);
  DCHECK_LE(type, ET_GESTURE_TYPE_END);
}

GestureEventDetails::GestureEventDetails(ui::EventType type,
                                         float delta_x,
                                         float delta_y)
    : type_(type), touch_points_(1), oldest_touch_id_(-1) {
  DCHECK_GE(type, ET_GESTURE_TYPE_START);
  DCHECK_LE(type, ET_GESTURE_TYPE_END);
  switch (type_) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      data_.scroll_begin.x_hint = delta_x;
      data_.scroll_begin.y_hint = delta_y;
      break;

    case ui::ET_GESTURE_SCROLL_UPDATE:
      data_.scroll_update.x = delta_x;
      data_.scroll_update.y = delta_y;
      break;

    case ui::ET_SCROLL_FLING_START:
      data_.fling_velocity.x = delta_x;
      data_.fling_velocity.y = delta_y;
      break;

    case ui::ET_GESTURE_TWO_FINGER_TAP:
      data_.first_finger_enclosing_rectangle.width = delta_x;
      data_.first_finger_enclosing_rectangle.height = delta_y;
      break;

    case ui::ET_GESTURE_SWIPE:
      data_.swipe.left = delta_x < 0;
      data_.swipe.right = delta_x > 0;
      data_.swipe.up = delta_y < 0;
      data_.swipe.down = delta_y > 0;
      break;

    default:
      NOTREACHED() << "Invalid event type for constructor: " << type;
  }
}

GestureEventDetails::GestureEventDetails(ui::EventType type,
                                         const GestureEventDetails& other)
    : type_(type),
      data_(other.data_),
      touch_points_(other.touch_points_),
      bounding_box_(other.bounding_box_),
      oldest_touch_id_(other.oldest_touch_id_) {
  DCHECK_GE(type, ET_GESTURE_TYPE_START);
  DCHECK_LE(type, ET_GESTURE_TYPE_END);
  switch (type) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      // Synthetic creation of SCROLL_BEGIN from PINCH_BEGIN is explicitly
      // allowed as an exception.
      if (other.type() == ui::ET_GESTURE_PINCH_BEGIN)
        break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_SWIPE:
    case ui::ET_GESTURE_PINCH_UPDATE:
      DCHECK_EQ(type, other.type()) << " - Invalid gesture conversion from "
                                    << other.type() << " to " << type;
      break;
    default:
      break;
  }
}

GestureEventDetails::Details::Details() {
  memset(this, 0, sizeof(Details));
}

}  // namespace ui
