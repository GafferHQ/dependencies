// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/android/platform_window_android.h"

#include <android/input.h>
#include <android/native_window_jni.h>

#include "base/android/jni_android.h"
#include "jni/PlatformWindowAndroid_jni.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"
#include "ui/gfx/geometry/point.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

namespace {

ui::EventType MotionEventActionToEventType(jint action) {
  switch (action) {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
      return ui::ET_TOUCH_PRESSED;
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_POINTER_UP:
      return ui::ET_TOUCH_RELEASED;
    case AMOTION_EVENT_ACTION_MOVE:
      return ui::ET_TOUCH_MOVED;
    case AMOTION_EVENT_ACTION_CANCEL:
      return ui::ET_TOUCH_CANCELLED;
    case AMOTION_EVENT_ACTION_OUTSIDE:
    case AMOTION_EVENT_ACTION_HOVER_MOVE:
    case AMOTION_EVENT_ACTION_SCROLL:
    case AMOTION_EVENT_ACTION_HOVER_ENTER:
    case AMOTION_EVENT_ACTION_HOVER_EXIT:
    default:
      NOTIMPLEMENTED() << "Unimplemented motion action: " << action;
  }
  return ui::ET_UNKNOWN;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PlatformWindowAndroid, public:

// static
bool PlatformWindowAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

PlatformWindowAndroid::PlatformWindowAndroid(PlatformWindowDelegate* delegate)
    : delegate_(delegate),
      window_(NULL),
      id_generator_(0),
      weak_factory_(this) {
}

PlatformWindowAndroid::~PlatformWindowAndroid() {
  if (window_)
    ReleaseWindow();
  if (!java_platform_window_android_.is_empty()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_PlatformWindowAndroid_detach(
        env, java_platform_window_android_.get(env).obj());
  }
}

void PlatformWindowAndroid::Destroy(JNIEnv* env, jobject obj) {
  delegate_->OnClosed();
}

void PlatformWindowAndroid::SurfaceCreated(JNIEnv* env,
                                           jobject obj,
                                           jobject jsurface,
                                           float device_pixel_ratio) {
  base::android::ScopedJavaLocalRef<jobject> protector(env, jsurface);
  // Note: This ensures that any local references used by
  // ANativeWindow_fromSurface are released immediately. This is needed as a
  // workaround for https://code.google.com/p/android/issues/detail?id=68174
  {
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    window_ = ANativeWindow_fromSurface(env, jsurface);
  }
  delegate_->OnAcceleratedWidgetAvailable(window_, device_pixel_ratio);
}

void PlatformWindowAndroid::SurfaceDestroyed(JNIEnv* env, jobject obj) {
  DCHECK(window_);
  ReleaseWindow();
  delegate_->OnAcceleratedWidgetAvailable(gfx::kNullAcceleratedWidget, 0.f);
}

void PlatformWindowAndroid::SurfaceSetSize(JNIEnv* env,
                                           jobject obj,
                                           jint width,
                                           jint height,
                                           jfloat density) {
  size_ = gfx::Size(static_cast<int>(width), static_cast<int>(height));
  delegate_->OnBoundsChanged(gfx::Rect(size_));
}

bool PlatformWindowAndroid::TouchEvent(JNIEnv* env,
                                       jobject obj,
                                       jlong time_ms,
                                       jint masked_action,
                                       jint pointer_id,
                                       jfloat x,
                                       jfloat y,
                                       jfloat pressure,
                                       jfloat touch_major,
                                       jfloat touch_minor,
                                       jfloat orientation,
                                       jfloat h_wheel,
                                       jfloat v_wheel) {
  ui::EventType event_type = MotionEventActionToEventType(masked_action);
  if (event_type == ui::ET_UNKNOWN)
    return false;
  ui::TouchEvent touch(event_type, gfx::PointF(x, y), ui::EF_NONE, pointer_id,
      base::TimeDelta::FromMilliseconds(time_ms),
      touch_major, touch_minor, orientation, pressure);
  delegate_->DispatchEvent(&touch);
  return true;
}

bool PlatformWindowAndroid::KeyEvent(JNIEnv* env,
                                     jobject obj,
                                     bool pressed,
                                     jint key_code,
                                     jint unicode_character) {
  ui::KeyEvent key_event(pressed ? ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED,
                         ui::KeyboardCodeFromAndroidKeyCode(key_code), 0);
  key_event.set_platform_keycode(key_code);
  delegate_->DispatchEvent(&key_event);
  if (pressed && unicode_character) {
    ui::KeyEvent char_event(unicode_character,
                            ui::KeyboardCodeFromAndroidKeyCode(key_code), 0);
    char_event.set_platform_keycode(key_code);
    delegate_->DispatchEvent(&char_event);
  }
  return true;
}

void PlatformWindowAndroid::ReleaseWindow() {
  ANativeWindow_release(window_);
  window_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformWindowAndroid, PlatformWindow implementation:

void PlatformWindowAndroid::Show() {
  if (!java_platform_window_android_.is_empty())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  java_platform_window_android_ = JavaObjectWeakGlobalRef(
      env, Java_PlatformWindowAndroid_createForActivity(
               env, base::android::GetApplicationContext(),
               reinterpret_cast<jlong>(this)).obj());
}

void PlatformWindowAndroid::Hide() {
  // Nothing to do. View is always visible.
}

void PlatformWindowAndroid::Close() {
  delegate_->OnCloseRequest();
}

void PlatformWindowAndroid::SetBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

gfx::Rect PlatformWindowAndroid::GetBounds() {
  return gfx::Rect(size_);
}

void PlatformWindowAndroid::SetCapture() {
  NOTIMPLEMENTED();
}

void PlatformWindowAndroid::ReleaseCapture() {
  NOTIMPLEMENTED();
}

void PlatformWindowAndroid::ToggleFullscreen() {
  NOTIMPLEMENTED();
}

void PlatformWindowAndroid::Maximize() {
  NOTIMPLEMENTED();
}

void PlatformWindowAndroid::Minimize() {
  NOTIMPLEMENTED();
}

void PlatformWindowAndroid::Restore() {
  NOTIMPLEMENTED();
}

void PlatformWindowAndroid::SetCursor(PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void PlatformWindowAndroid::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void PlatformWindowAndroid::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

}  // namespace ui
