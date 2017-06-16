// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_event_ack.h"

namespace content {

InputEventAck::InputEventAck(
    blink::WebInputEvent::Type type,
    InputEventAckState state,
    const ui::LatencyInfo& latency,
    scoped_ptr<content::DidOverscrollParams> overscroll,
    uint32 unique_touch_event_id)
    : type(type),
      state(state),
      latency(latency),
      overscroll(overscroll.Pass()),
      unique_touch_event_id(unique_touch_event_id) {
}

InputEventAck::InputEventAck(blink::WebInputEvent::Type type,
                             InputEventAckState state,
                             const ui::LatencyInfo& latency,
                             uint32 unique_touch_event_id)
    : InputEventAck(type, state, latency, nullptr, unique_touch_event_id) {
}

InputEventAck::InputEventAck(blink::WebInputEvent::Type type,
                             InputEventAckState state,
                             uint32 unique_touch_event_id)
    : InputEventAck(type, state, ui::LatencyInfo(), unique_touch_event_id) {
}

InputEventAck::InputEventAck(blink::WebInputEvent::Type type,
                             InputEventAckState state)
    : InputEventAck(type, state, 0) {
}

InputEventAck::InputEventAck()
    : InputEventAck(blink::WebInputEvent::Undefined,
                    INPUT_EVENT_ACK_STATE_UNKNOWN) {
}

InputEventAck::~InputEventAck() {
}

}  // namespace content
