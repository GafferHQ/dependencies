// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_target_android.h"

#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "jni/MotionEventSynthesizer_jni.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/android/view_configuration.h"
#include "ui/gfx/screen.h"

using blink::WebTouchEvent;

namespace content {

SyntheticGestureTargetAndroid::SyntheticGestureTargetAndroid(
    RenderWidgetHostImpl* host,
    base::android::ScopedJavaLocalRef<jobject> touch_event_synthesizer)
    : SyntheticGestureTargetBase(host),
      touch_event_synthesizer_(touch_event_synthesizer) {
  DCHECK(!touch_event_synthesizer_.is_null());
}

SyntheticGestureTargetAndroid::~SyntheticGestureTargetAndroid() {
}

bool SyntheticGestureTargetAndroid::RegisterMotionEventSynthesizer(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void SyntheticGestureTargetAndroid::TouchSetPointer(
    JNIEnv* env, int index, int x, int y, int id) {
  TRACE_EVENT0("input", "SyntheticGestureTargetAndroid::TouchSetPointer");
  Java_MotionEventSynthesizer_setPointer(env, touch_event_synthesizer_.obj(),
                                      index, x, y, id);
}

void SyntheticGestureTargetAndroid::TouchSetScrollDeltas(
    JNIEnv* env, int x, int y, int dx, int dy) {
  TRACE_EVENT0("input", "SyntheticGestureTargetAndroid::TouchSetPointer");
  Java_MotionEventSynthesizer_setScrollDeltas(
      env, touch_event_synthesizer_.obj(), x, y, dx, dy);
}

void SyntheticGestureTargetAndroid::TouchInject(
    JNIEnv* env, Action action, int pointer_count, int64 time_in_ms) {
  TRACE_EVENT0("input", "SyntheticGestureTargetAndroid::TouchInject");
  Java_MotionEventSynthesizer_inject(env, touch_event_synthesizer_.obj(),
                                    static_cast<int>(action), pointer_count,
                                    time_in_ms);
}

void SyntheticGestureTargetAndroid::DispatchWebTouchEventToPlatform(
    const blink::WebTouchEvent& web_touch, const ui::LatencyInfo&) {
  JNIEnv* env = base::android::AttachCurrentThread();

  SyntheticGestureTargetAndroid::Action action =
      SyntheticGestureTargetAndroid::ActionInvalid;
  switch (web_touch.type) {
    case blink::WebInputEvent::TouchStart:
      action = SyntheticGestureTargetAndroid::ActionStart;
      break;
    case blink::WebInputEvent::TouchMove:
      action = SyntheticGestureTargetAndroid::ActionMove;
      break;
    case blink::WebInputEvent::TouchCancel:
      action = SyntheticGestureTargetAndroid::ActionCancel;
      break;
    case blink::WebInputEvent::TouchEnd:
      action = SyntheticGestureTargetAndroid::ActionEnd;
      break;
    default:
      NOTREACHED();
  }
  const unsigned num_touches = web_touch.touchesLength;
  for (unsigned i = 0; i < num_touches; ++i) {
    const blink::WebTouchPoint* point = &web_touch.touches[i];
    TouchSetPointer(env, i, point->position.x, point->position.y, point->id);
  }

  TouchInject(env, action, num_touches,
              static_cast<int64>(web_touch.timeStampSeconds * 1000.0));
}

void SyntheticGestureTargetAndroid::DispatchWebMouseWheelEventToPlatform(
    const blink::WebMouseWheelEvent& web_wheel, const ui::LatencyInfo&) {
  JNIEnv* env = base::android::AttachCurrentThread();
  TouchSetScrollDeltas(env, web_wheel.x, web_wheel.y, web_wheel.deltaX,
                       web_wheel.deltaY);
  Java_MotionEventSynthesizer_inject(
      env, touch_event_synthesizer_.obj(),
      static_cast<int>(SyntheticGestureTargetAndroid::ActionScroll),
      1, static_cast<int64>(web_wheel.timeStampSeconds * 1000.0));
}

void SyntheticGestureTargetAndroid::DispatchWebMouseEventToPlatform(
    const blink::WebMouseEvent& web_mouse, const ui::LatencyInfo&) {
  CHECK(false);
}

SyntheticGestureParams::GestureSourceType
SyntheticGestureTargetAndroid::GetDefaultSyntheticGestureSourceType() const {
  return SyntheticGestureParams::TOUCH_INPUT;
}

float SyntheticGestureTargetAndroid::GetTouchSlopInDips() const {
  return gfx::ViewConfiguration::GetTouchSlopInDips();
}

float SyntheticGestureTargetAndroid::GetMinScalingSpanInDips() const {
  return gfx::ViewConfiguration::GetMinScalingSpanInDips();
}

}  // namespace content
