// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event.h"

#if defined(USE_X11)
#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#endif

#include <cmath>
#include <cstring>

#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

#if defined(USE_X11)
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#elif defined(USE_OZONE)
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#endif

namespace {

std::string EventTypeName(ui::EventType type) {
#define RETURN_IF_TYPE(t) if (type == ui::t)  return #t
#define CASE_TYPE(t) case ui::t:  return #t
  switch (type) {
    CASE_TYPE(ET_UNKNOWN);
    CASE_TYPE(ET_MOUSE_PRESSED);
    CASE_TYPE(ET_MOUSE_DRAGGED);
    CASE_TYPE(ET_MOUSE_RELEASED);
    CASE_TYPE(ET_MOUSE_MOVED);
    CASE_TYPE(ET_MOUSE_ENTERED);
    CASE_TYPE(ET_MOUSE_EXITED);
    CASE_TYPE(ET_KEY_PRESSED);
    CASE_TYPE(ET_KEY_RELEASED);
    CASE_TYPE(ET_MOUSEWHEEL);
    CASE_TYPE(ET_MOUSE_CAPTURE_CHANGED);
    CASE_TYPE(ET_TOUCH_RELEASED);
    CASE_TYPE(ET_TOUCH_PRESSED);
    CASE_TYPE(ET_TOUCH_MOVED);
    CASE_TYPE(ET_TOUCH_CANCELLED);
    CASE_TYPE(ET_DROP_TARGET_EVENT);
    CASE_TYPE(ET_GESTURE_SCROLL_BEGIN);
    CASE_TYPE(ET_GESTURE_SCROLL_END);
    CASE_TYPE(ET_GESTURE_SCROLL_UPDATE);
    CASE_TYPE(ET_GESTURE_SHOW_PRESS);
    CASE_TYPE(ET_GESTURE_WIN8_EDGE_SWIPE);
    CASE_TYPE(ET_GESTURE_TAP);
    CASE_TYPE(ET_GESTURE_TAP_DOWN);
    CASE_TYPE(ET_GESTURE_TAP_CANCEL);
    CASE_TYPE(ET_GESTURE_BEGIN);
    CASE_TYPE(ET_GESTURE_END);
    CASE_TYPE(ET_GESTURE_TWO_FINGER_TAP);
    CASE_TYPE(ET_GESTURE_PINCH_BEGIN);
    CASE_TYPE(ET_GESTURE_PINCH_END);
    CASE_TYPE(ET_GESTURE_PINCH_UPDATE);
    CASE_TYPE(ET_GESTURE_LONG_PRESS);
    CASE_TYPE(ET_GESTURE_LONG_TAP);
    CASE_TYPE(ET_GESTURE_SWIPE);
    CASE_TYPE(ET_GESTURE_TAP_UNCONFIRMED);
    CASE_TYPE(ET_GESTURE_DOUBLE_TAP);
    CASE_TYPE(ET_SCROLL);
    CASE_TYPE(ET_SCROLL_FLING_START);
    CASE_TYPE(ET_SCROLL_FLING_CANCEL);
    CASE_TYPE(ET_CANCEL_MODE);
    CASE_TYPE(ET_UMA_DATA);
    case ui::ET_LAST: NOTREACHED(); return std::string();
    // Don't include default, so that we get an error when new type is added.
  }
#undef CASE_TYPE

  NOTREACHED();
  return std::string();
}

bool IsX11SendEventTrue(const base::NativeEvent& event) {
#if defined(USE_X11)
  return event && event->xany.send_event;
#else
  return false;
#endif
}

bool X11EventHasNonStandardState(const base::NativeEvent& event) {
#if defined(USE_X11)
  const unsigned int kAllStateMask =
      Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask |
      Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask | ShiftMask |
      LockMask | ControlMask | AnyModifier;
  return event && (event->xkey.state & ~kAllStateMask) != 0;
#else
  return false;
#endif
}

}  // namespace

namespace ui {

////////////////////////////////////////////////////////////////////////////////
// Event

// static
scoped_ptr<Event> Event::Clone(const Event& event) {
  if (event.IsKeyEvent()) {
    return make_scoped_ptr(new KeyEvent(static_cast<const KeyEvent&>(event)));
  }

  if (event.IsMouseEvent()) {
    if (event.IsMouseWheelEvent()) {
      return make_scoped_ptr(
          new MouseWheelEvent(static_cast<const MouseWheelEvent&>(event)));
    }

    return make_scoped_ptr(
        new MouseEvent(static_cast<const MouseEvent&>(event)));
  }

  if (event.IsTouchEvent()) {
    return make_scoped_ptr(
        new TouchEvent(static_cast<const TouchEvent&>(event)));
  }

  if (event.IsGestureEvent()) {
    return make_scoped_ptr(
        new GestureEvent(static_cast<const GestureEvent&>(event)));
  }

  if (event.IsScrollEvent()) {
    return make_scoped_ptr(
        new ScrollEvent(static_cast<const ScrollEvent&>(event)));
  }

  return make_scoped_ptr(new Event(event));
}

Event::~Event() {
  if (delete_native_event_)
    ReleaseCopiedNativeEvent(native_event_);
}

GestureEvent* Event::AsGestureEvent() {
  CHECK(IsGestureEvent());
  return static_cast<GestureEvent*>(this);
}

const GestureEvent* Event::AsGestureEvent() const {
  CHECK(IsGestureEvent());
  return static_cast<const GestureEvent*>(this);
}

bool Event::HasNativeEvent() const {
  base::NativeEvent null_event;
  std::memset(&null_event, 0, sizeof(null_event));
  return !!std::memcmp(&native_event_, &null_event, sizeof(null_event));
}

void Event::StopPropagation() {
  // TODO(sad): Re-enable these checks once View uses dispatcher to dispatch
  // events.
  // CHECK(phase_ != EP_PREDISPATCH && phase_ != EP_POSTDISPATCH);
  CHECK(cancelable_);
  result_ = static_cast<EventResult>(result_ | ER_CONSUMED);
}

void Event::SetHandled() {
  // TODO(sad): Re-enable these checks once View uses dispatcher to dispatch
  // events.
  // CHECK(phase_ != EP_PREDISPATCH && phase_ != EP_POSTDISPATCH);
  CHECK(cancelable_);
  result_ = static_cast<EventResult>(result_ | ER_HANDLED);
}

Event::Event(EventType type, base::TimeDelta time_stamp, int flags)
    : type_(type),
      time_stamp_(time_stamp),
      flags_(flags),
      native_event_(base::NativeEvent()),
      delete_native_event_(false),
      cancelable_(true),
      target_(NULL),
      phase_(EP_PREDISPATCH),
      result_(ER_UNHANDLED),
      source_device_id_(ED_UNKNOWN_DEVICE) {
  if (type_ < ET_LAST)
    name_ = EventTypeName(type_);
}

Event::Event(const base::NativeEvent& native_event,
             EventType type,
             int flags)
    : type_(type),
      time_stamp_(EventTimeFromNative(native_event)),
      flags_(flags),
      native_event_(native_event),
      delete_native_event_(false),
      cancelable_(true),
      target_(NULL),
      phase_(EP_PREDISPATCH),
      result_(ER_UNHANDLED),
      source_device_id_(ED_UNKNOWN_DEVICE) {
  base::TimeDelta delta = EventTimeForNow() - time_stamp_;
  if (type_ < ET_LAST)
    name_ = EventTypeName(type_);
  base::HistogramBase::Sample delta_sample =
      static_cast<base::HistogramBase::Sample>(delta.InMicroseconds());
  UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.Browser", delta_sample, 1, 1000000,
                              100);
  std::string name_for_event =
      base::StringPrintf("Event.Latency.Browser.%s", name_.c_str());
  base::HistogramBase* counter_for_type =
      base::Histogram::FactoryGet(
          name_for_event,
          1,
          1000000,
          100,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  counter_for_type->Add(delta_sample);

#if defined(USE_X11)
  if (native_event->type == GenericEvent) {
    XIDeviceEvent* xiev =
        static_cast<XIDeviceEvent*>(native_event->xcookie.data);
    source_device_id_ = xiev->sourceid;
  }
#endif
#if defined(USE_OZONE)
  source_device_id_ =
      static_cast<const Event*>(native_event)->source_device_id();
#endif
}

Event::Event(const Event& copy)
    : type_(copy.type_),
      time_stamp_(copy.time_stamp_),
      latency_(copy.latency_),
      flags_(copy.flags_),
      native_event_(CopyNativeEvent(copy.native_event_)),
      delete_native_event_(true),
      cancelable_(true),
      target_(NULL),
      phase_(EP_PREDISPATCH),
      result_(ER_UNHANDLED),
      source_device_id_(copy.source_device_id_) {
  if (type_ < ET_LAST)
    name_ = EventTypeName(type_);
}

void Event::SetType(EventType type) {
  if (type_ < ET_LAST)
    name_ = std::string();
  type_ = type;
  if (type_ < ET_LAST)
    name_ = EventTypeName(type_);
}

////////////////////////////////////////////////////////////////////////////////
// CancelModeEvent

CancelModeEvent::CancelModeEvent()
    : Event(ET_CANCEL_MODE, base::TimeDelta(), 0) {
  set_cancelable(false);
}

CancelModeEvent::~CancelModeEvent() {
}

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent

LocatedEvent::~LocatedEvent() {
}

LocatedEvent::LocatedEvent(const base::NativeEvent& native_event)
    : Event(native_event,
            EventTypeFromNative(native_event),
            EventFlagsFromNative(native_event)),
      location_(EventLocationFromNative(native_event)),
      root_location_(location_) {
}

LocatedEvent::LocatedEvent(EventType type,
                           const gfx::PointF& location,
                           const gfx::PointF& root_location,
                           base::TimeDelta time_stamp,
                           int flags)
    : Event(type, time_stamp, flags),
      location_(location),
      root_location_(root_location) {
}

void LocatedEvent::UpdateForRootTransform(
    const gfx::Transform& reversed_root_transform) {
  // Transform has to be done at root level.
  gfx::Point3F p(location_);
  reversed_root_transform.TransformPoint(&p);
  location_ = p.AsPointF();
  root_location_ = location_;
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent

MouseEvent::MouseEvent(const base::NativeEvent& native_event)
    : LocatedEvent(native_event),
      changed_button_flags_(
          GetChangedMouseButtonFlagsFromNative(native_event)) {
  if (type() == ET_MOUSE_PRESSED || type() == ET_MOUSE_RELEASED)
    SetClickCount(GetRepeatCount(*this));
}

MouseEvent::MouseEvent(EventType type,
                       const gfx::PointF& location,
                       const gfx::PointF& root_location,
                       base::TimeDelta time_stamp,
                       int flags,
                       int changed_button_flags)
    : LocatedEvent(type, location, root_location, time_stamp, flags),
      changed_button_flags_(changed_button_flags) {
  if (this->type() == ET_MOUSE_MOVED && IsAnyButton())
    SetType(ET_MOUSE_DRAGGED);
}

// static
bool MouseEvent::IsRepeatedClickEvent(
    const MouseEvent& event1,
    const MouseEvent& event2) {
  // These values match the Windows defaults.
  static const int kDoubleClickTimeMS = 500;
  static const int kDoubleClickWidth = 4;
  static const int kDoubleClickHeight = 4;

  if (event1.type() != ET_MOUSE_PRESSED ||
      event2.type() != ET_MOUSE_PRESSED)
    return false;

  // Compare flags, but ignore EF_IS_DOUBLE_CLICK to allow triple clicks.
  if ((event1.flags() & ~EF_IS_DOUBLE_CLICK) !=
      (event2.flags() & ~EF_IS_DOUBLE_CLICK))
    return false;

  // The new event has been created from the same native event.
  if (event1.time_stamp() == event2.time_stamp())
    return false;

  base::TimeDelta time_difference = event2.time_stamp() - event1.time_stamp();

  if (time_difference.InMilliseconds() > kDoubleClickTimeMS)
    return false;

  if (std::abs(event2.x() - event1.x()) > kDoubleClickWidth / 2)
    return false;

  if (std::abs(event2.y() - event1.y()) > kDoubleClickHeight / 2)
    return false;

  return true;
}

// static
int MouseEvent::GetRepeatCount(const MouseEvent& event) {
  int click_count = 1;
  if (last_click_event_) {
    if (event.type() == ui::ET_MOUSE_RELEASED) {
      if (event.changed_button_flags() ==
              last_click_event_->changed_button_flags()) {
        last_click_complete_ = true;
        return last_click_event_->GetClickCount();
      } else {
        // If last_click_event_ has changed since this button was pressed
        // return a click count of 1.
        return click_count;
      }
    }
    if (event.time_stamp() != last_click_event_->time_stamp())
      last_click_complete_ = true;
    if (!last_click_complete_ ||
        IsX11SendEventTrue(event.native_event())) {
      click_count = last_click_event_->GetClickCount();
    } else if (IsRepeatedClickEvent(*last_click_event_, event)) {
      click_count = last_click_event_->GetClickCount() + 1;
    }
    delete last_click_event_;
  }
  last_click_event_ = new MouseEvent(event);
  last_click_complete_ = false;
  if (click_count > 3)
    click_count = 3;
  last_click_event_->SetClickCount(click_count);
  return click_count;
}

void MouseEvent::ResetLastClickForTest() {
  if (last_click_event_) {
    delete last_click_event_;
    last_click_event_ = NULL;
    last_click_complete_ = false;
  }
}

// static
MouseEvent* MouseEvent::last_click_event_ = NULL;
bool MouseEvent::last_click_complete_ = false;

int MouseEvent::GetClickCount() const {
  if (type() != ET_MOUSE_PRESSED && type() != ET_MOUSE_RELEASED)
    return 0;

  if (flags() & EF_IS_TRIPLE_CLICK)
    return 3;
  else if (flags() & EF_IS_DOUBLE_CLICK)
    return 2;
  else
    return 1;
}

void MouseEvent::SetClickCount(int click_count) {
  if (type() != ET_MOUSE_PRESSED && type() != ET_MOUSE_RELEASED)
    return;

  DCHECK(click_count > 0);
  DCHECK(click_count <= 3);

  int f = flags();
  switch (click_count) {
    case 1:
      f &= ~EF_IS_DOUBLE_CLICK;
      f &= ~EF_IS_TRIPLE_CLICK;
      break;
    case 2:
      f |= EF_IS_DOUBLE_CLICK;
      f &= ~EF_IS_TRIPLE_CLICK;
      break;
    case 3:
      f &= ~EF_IS_DOUBLE_CLICK;
      f |= EF_IS_TRIPLE_CLICK;
      break;
  }
  set_flags(f);
}

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent

MouseWheelEvent::MouseWheelEvent(const base::NativeEvent& native_event)
    : MouseEvent(native_event),
      offset_(GetMouseWheelOffset(native_event)) {
}

MouseWheelEvent::MouseWheelEvent(const ScrollEvent& scroll_event)
    : MouseEvent(scroll_event),
      offset_(gfx::ToRoundedInt(scroll_event.x_offset()),
              gfx::ToRoundedInt(scroll_event.y_offset())) {
  SetType(ET_MOUSEWHEEL);
}

MouseWheelEvent::MouseWheelEvent(const MouseEvent& mouse_event,
                                 int x_offset,
                                 int y_offset)
    : MouseEvent(mouse_event), offset_(x_offset, y_offset) {
  DCHECK(type() == ET_MOUSEWHEEL);
}

MouseWheelEvent::MouseWheelEvent(const MouseWheelEvent& mouse_wheel_event)
    : MouseEvent(mouse_wheel_event),
      offset_(mouse_wheel_event.offset()) {
  DCHECK(type() == ET_MOUSEWHEEL);
}

MouseWheelEvent::MouseWheelEvent(const gfx::Vector2d& offset,
                                 const gfx::PointF& location,
                                 const gfx::PointF& root_location,
                                 base::TimeDelta time_stamp,
                                 int flags,
                                 int changed_button_flags)
    : MouseEvent(ui::ET_MOUSEWHEEL,
                 location,
                 root_location,
                 time_stamp,
                 flags,
                 changed_button_flags),
      offset_(offset) {
}

#if defined(OS_WIN)
// This value matches windows WHEEL_DELTA.
// static
const int MouseWheelEvent::kWheelDelta = 120;
#else
// This value matches GTK+ wheel scroll amount.
const int MouseWheelEvent::kWheelDelta = 53;
#endif

////////////////////////////////////////////////////////////////////////////////
// TouchEvent

TouchEvent::TouchEvent(const base::NativeEvent& native_event)
    : LocatedEvent(native_event),
      touch_id_(GetTouchId(native_event)),
      unique_event_id_(ui::GetNextTouchEventId()),
      radius_x_(GetTouchRadiusX(native_event)),
      radius_y_(GetTouchRadiusY(native_event)),
      rotation_angle_(GetTouchAngle(native_event)),
      force_(GetTouchForce(native_event)),
      may_cause_scrolling_(false),
      should_remove_native_touch_id_mapping_(false) {
  latency()->AddLatencyNumberWithTimestamp(
      INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, 0,
      base::TimeTicks::FromInternalValue(time_stamp().ToInternalValue()), 1);
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);

  FixRotationAngle();
  if (type() == ET_TOUCH_RELEASED || type() == ET_TOUCH_CANCELLED)
    should_remove_native_touch_id_mapping_ = true;
}

TouchEvent::TouchEvent(EventType type,
                       const gfx::PointF& location,
                       int touch_id,
                       base::TimeDelta time_stamp)
    : LocatedEvent(type, location, location, time_stamp, 0),
      touch_id_(touch_id),
      unique_event_id_(ui::GetNextTouchEventId()),
      radius_x_(0.0f),
      radius_y_(0.0f),
      rotation_angle_(0.0f),
      force_(0.0f),
      may_cause_scrolling_(false),
      should_remove_native_touch_id_mapping_(false) {
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);
}

TouchEvent::TouchEvent(EventType type,
                       const gfx::PointF& location,
                       int flags,
                       int touch_id,
                       base::TimeDelta time_stamp,
                       float radius_x,
                       float radius_y,
                       float angle,
                       float force)
    : LocatedEvent(type, location, location, time_stamp, flags),
      touch_id_(touch_id),
      unique_event_id_(ui::GetNextTouchEventId()),
      radius_x_(radius_x),
      radius_y_(radius_y),
      rotation_angle_(angle),
      force_(force),
      may_cause_scrolling_(false),
      should_remove_native_touch_id_mapping_(false) {
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);
  FixRotationAngle();
}

TouchEvent::TouchEvent(const TouchEvent& copy)
    : LocatedEvent(copy),
      touch_id_(copy.touch_id_),
      unique_event_id_(copy.unique_event_id_),
      radius_x_(copy.radius_x_),
      radius_y_(copy.radius_y_),
      rotation_angle_(copy.rotation_angle_),
      force_(copy.force_),
      may_cause_scrolling_(copy.may_cause_scrolling_),
      should_remove_native_touch_id_mapping_(false) {
  // Copied events should not remove touch id mapping, as this either causes the
  // mapping to be lost before the initial event has finished dispatching, or
  // the copy to attempt to remove the mapping from a null |native_event_|.
}

TouchEvent::~TouchEvent() {
  // In ctor TouchEvent(native_event) we call GetTouchId() which in X11
  // platform setups the tracking_id to slot mapping. So in dtor here,
  // if this touch event is a release event, we clear the mapping accordingly.
  if (should_remove_native_touch_id_mapping_) {
    DCHECK(type() == ET_TOUCH_RELEASED || type() == ET_TOUCH_CANCELLED);
    if (type() == ET_TOUCH_RELEASED || type() == ET_TOUCH_CANCELLED)
      ClearTouchIdIfReleased(native_event());
  }
}

void TouchEvent::UpdateForRootTransform(
    const gfx::Transform& inverted_root_transform) {
  LocatedEvent::UpdateForRootTransform(inverted_root_transform);
  gfx::DecomposedTransform decomp;
  bool success = gfx::DecomposeTransform(&decomp, inverted_root_transform);
  DCHECK(success);
  if (decomp.scale[0])
    radius_x_ *= decomp.scale[0];
  if (decomp.scale[1])
    radius_y_ *= decomp.scale[1];
}

void TouchEvent::DisableSynchronousHandling() {
  DispatcherApi dispatcher_api(this);
  dispatcher_api.set_result(
      static_cast<EventResult>(result() | ER_DISABLE_SYNC_HANDLING));
}

void TouchEvent::FixRotationAngle() {
  while (rotation_angle_ < 0)
    rotation_angle_ += 180;
  while (rotation_angle_ >= 180)
    rotation_angle_ -= 180;
}

////////////////////////////////////////////////////////////////////////////////
// KeyEvent

// static
KeyEvent* KeyEvent::last_key_event_ = NULL;

// static
bool KeyEvent::IsRepeated(const KeyEvent& event) {
  // A safe guard in case if there were continous key pressed events that are
  // not auto repeat.
  const int kMaxAutoRepeatTimeMs = 2000;
  // Ignore key events that have non standard state masks as it may be
  // reposted by an IME. IBUS-GTK uses this field to detect the
  // re-posted event for example. crbug.com/385873.
  if (X11EventHasNonStandardState(event.native_event()))
    return false;
  if (event.is_char())
    return false;
  if (event.type() == ui::ET_KEY_RELEASED) {
    delete last_key_event_;
    last_key_event_ = NULL;
    return false;
  }
  CHECK_EQ(ui::ET_KEY_PRESSED, event.type());
  if (!last_key_event_) {
    last_key_event_ = new KeyEvent(event);
    return false;
  } else if (event.time_stamp() == last_key_event_->time_stamp()) {
    // The KeyEvent is created from the same native event.
    return (last_key_event_->flags() & ui::EF_IS_REPEAT) != 0;
  }
  if (event.key_code() == last_key_event_->key_code() &&
      event.flags() == (last_key_event_->flags() & ~ui::EF_IS_REPEAT) &&
      (event.time_stamp() - last_key_event_->time_stamp()).InMilliseconds() <
          kMaxAutoRepeatTimeMs) {
    last_key_event_->set_time_stamp(event.time_stamp());
    last_key_event_->set_flags(last_key_event_->flags() | ui::EF_IS_REPEAT);
    return true;
  }
  delete last_key_event_;
  last_key_event_ = new KeyEvent(event);
  return false;
}

KeyEvent::KeyEvent(const base::NativeEvent& native_event)
    : Event(native_event,
            EventTypeFromNative(native_event),
            EventFlagsFromNative(native_event)),
      key_code_(KeyboardCodeFromNative(native_event)),
      code_(CodeFromNative(native_event)),
      is_char_(IsCharFromNative(native_event)),
      platform_keycode_(PlatformKeycodeFromNative(native_event)),
      key_(DomKey::NONE),
      character_(0) {
  if (IsRepeated(*this))
    set_flags(flags() | ui::EF_IS_REPEAT);

#if defined(USE_X11)
  NormalizeFlags();
#endif
#if defined(OS_WIN)
  // Only Windows has native character events.
  if (is_char_)
    character_ = native_event.wParam;
#endif
}

KeyEvent::KeyEvent(EventType type,
                   KeyboardCode key_code,
                   int flags)
    : Event(type, EventTimeForNow(), flags),
      key_code_(key_code),
      code_(UsLayoutKeyboardCodeToDomCode(key_code)),
      is_char_(false),
      platform_keycode_(0),
      key_(DomKey::NONE),
      character_() {
}

KeyEvent::KeyEvent(EventType type,
                   KeyboardCode key_code,
                   DomCode code,
                   int flags)
    : Event(type, EventTimeForNow(), flags),
      key_code_(key_code),
      code_(code),
      is_char_(false),
      platform_keycode_(0),
      key_(DomKey::NONE),
      character_(0) {
}

KeyEvent::KeyEvent(EventType type,
                   KeyboardCode key_code,
                   DomCode code,
                   int flags,
                   DomKey key,
                   base::char16 character,
                   base::TimeDelta time_stamp)
    : Event(type, time_stamp, flags),
      key_code_(key_code),
      code_(code),
      is_char_(false),
      platform_keycode_(0),
      key_(key),
      character_(character) {
}

KeyEvent::KeyEvent(base::char16 character, KeyboardCode key_code, int flags)
    : Event(ET_KEY_PRESSED, EventTimeForNow(), flags),
      key_code_(key_code),
      code_(DomCode::NONE),
      is_char_(true),
      platform_keycode_(0),
      key_(DomKey::CHARACTER),
      character_(character) {
}

KeyEvent::KeyEvent(const KeyEvent& rhs)
    : Event(rhs),
      key_code_(rhs.key_code_),
      code_(rhs.code_),
      is_char_(rhs.is_char_),
      platform_keycode_(rhs.platform_keycode_),
      key_(rhs.key_),
      character_(rhs.character_) {
  if (rhs.extended_key_event_data_)
    extended_key_event_data_.reset(rhs.extended_key_event_data_->Clone());
}

KeyEvent& KeyEvent::operator=(const KeyEvent& rhs) {
  if (this != &rhs) {
    Event::operator=(rhs);
    key_code_ = rhs.key_code_;
    code_ = rhs.code_;
    key_ = rhs.key_;
    is_char_ = rhs.is_char_;
    platform_keycode_ = rhs.platform_keycode_;
    character_ = rhs.character_;

    if (rhs.extended_key_event_data_)
      extended_key_event_data_.reset(rhs.extended_key_event_data_->Clone());
  }
  return *this;
}

KeyEvent::~KeyEvent() {}

void KeyEvent::SetExtendedKeyEventData(scoped_ptr<ExtendedKeyEventData> data) {
  extended_key_event_data_ = data.Pass();
}

void KeyEvent::ApplyLayout() const {
  // If the client has set the character (e.g. faked key events from virtual
  // keyboard), it's client's responsibility to set the dom key correctly.
  // Otherwise, set the dom key as unidentified.
  // Please refer to crbug.com/443889.
  if (character_ != 0) {
    key_ = DomKey::UNIDENTIFIED;
    return;
  }
  ui::DomCode code = code_;
  if (code == DomCode::NONE) {
    // Catch old code that tries to do layout without a physical key, and try
    // to recover using the KeyboardCode. Once key events are fully defined
    // on construction (see TODO in event.h) this will go away.
    LOG(WARNING) << "DomCode::NONE keycode=" << key_code_;
    code = UsLayoutKeyboardCodeToDomCode(key_code_);
    if (code == DomCode::NONE) {
      key_ = DomKey::UNIDENTIFIED;
      return;
    }
  }
  KeyboardCode dummy_key_code;
#if defined(OS_WIN)
// Native Windows character events always have is_char_ == true,
// so this is a synthetic or native keystroke event.
// Therefore, perform only the fallback action.
#elif defined(USE_X11)
  // When a control key is held, prefer ASCII characters to non ASCII
  // characters in order to use it for shortcut keys.  GetCharacterFromKeyCode
  // returns 'a' for VKEY_A even if the key is actually bound to 'à' in X11.
  // GetCharacterFromXEvent returns 'à' in that case.
  if (!IsControlDown() && native_event()) {
    GetMeaningFromXEvent(native_event(), &key_, &character_);
    return;
  }
#elif defined(USE_OZONE)
  if (KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()->Lookup(
          code, flags(), &key_, &character_, &dummy_key_code,
          &platform_keycode_)) {
    return;
  }
#else
  if (native_event()) {
    DCHECK(EventTypeFromNative(native_event()) == ET_KEY_PRESSED ||
           EventTypeFromNative(native_event()) == ET_KEY_RELEASED);
  }
#endif
  if (!DomCodeToUsLayoutMeaning(code, flags(), &key_, &character_,
                                &dummy_key_code)) {
    key_ = DomKey::UNIDENTIFIED;
  }
}

DomKey KeyEvent::GetDomKey() const {
  // Determination of character_ and key_ may be done lazily.
  if (key_ == DomKey::NONE)
    ApplyLayout();
  return key_;
}

base::char16 KeyEvent::GetCharacter() const {
  // Determination of character_ and key_ may be done lazily.
  if (key_ == DomKey::NONE)
    ApplyLayout();
  return character_;
}

base::char16 KeyEvent::GetText() const {
  if ((flags() & EF_CONTROL_DOWN) != 0) {
    base::char16 character;
    ui::DomKey key;
    ui::KeyboardCode key_code;
    if (DomCodeToControlCharacter(code_, flags(), &key, &character, &key_code))
      return character;
  }
  return GetUnmodifiedText();
}

base::char16 KeyEvent::GetUnmodifiedText() const {
  if (!is_char_ && (key_code_ == VKEY_RETURN))
    return '\r';
  return GetCharacter();
}

bool KeyEvent::IsUnicodeKeyCode() const {
#if defined(OS_WIN)
  if (!IsAltDown())
    return false;
  const int key = key_code();
  if (key >= VKEY_NUMPAD0 && key <= VKEY_NUMPAD9)
    return true;
  // Check whether the user is using the numeric keypad with num-lock off.
  // In that case, EF_EXTENDED will not be set; if it is set, the key event
  // originated from the relevant non-numpad dedicated key, e.g. [Insert].
  return (!(flags() & EF_EXTENDED) &&
          (key == VKEY_INSERT || key == VKEY_END  || key == VKEY_DOWN ||
           key == VKEY_NEXT   || key == VKEY_LEFT || key == VKEY_CLEAR ||
           key == VKEY_RIGHT  || key == VKEY_HOME || key == VKEY_UP ||
           key == VKEY_PRIOR));
#else
  return false;
#endif
}

void KeyEvent::NormalizeFlags() {
  int mask = 0;
  switch (key_code()) {
    case VKEY_CONTROL:
      mask = EF_CONTROL_DOWN;
      break;
    case VKEY_SHIFT:
      mask = EF_SHIFT_DOWN;
      break;
    case VKEY_MENU:
      mask = EF_ALT_DOWN;
      break;
    case VKEY_CAPITAL:
      mask = EF_CAPS_LOCK_DOWN;
      break;
    default:
      return;
  }
  if (type() == ET_KEY_PRESSED)
    set_flags(flags() | mask);
  else
    set_flags(flags() & ~mask);
}

KeyboardCode KeyEvent::GetLocatedWindowsKeyboardCode() const {
  return NonLocatedToLocatedKeyboardCode(key_code_, code_);
}

uint16 KeyEvent::GetConflatedWindowsKeyCode() const {
  if (is_char_)
    return character_;
  return key_code_;
}

std::string KeyEvent::GetCodeString() const {
  return KeycodeConverter::DomCodeToCodeString(code_);
}

////////////////////////////////////////////////////////////////////////////////
// ScrollEvent

ScrollEvent::ScrollEvent(const base::NativeEvent& native_event)
    : MouseEvent(native_event) {
  if (type() == ET_SCROLL) {
    GetScrollOffsets(native_event,
                     &x_offset_, &y_offset_,
                     &x_offset_ordinal_, &y_offset_ordinal_,
                     &finger_count_);
  } else if (type() == ET_SCROLL_FLING_START ||
             type() == ET_SCROLL_FLING_CANCEL) {
    GetFlingData(native_event,
                 &x_offset_, &y_offset_,
                 &x_offset_ordinal_, &y_offset_ordinal_,
                 NULL);
  } else {
    NOTREACHED() << "Unexpected event type " << type()
        << " when constructing a ScrollEvent.";
  }
}

ScrollEvent::ScrollEvent(EventType type,
                         const gfx::PointF& location,
                         base::TimeDelta time_stamp,
                         int flags,
                         float x_offset,
                         float y_offset,
                         float x_offset_ordinal,
                         float y_offset_ordinal,
                         int finger_count)
    : MouseEvent(type, location, location, time_stamp, flags, 0),
      x_offset_(x_offset),
      y_offset_(y_offset),
      x_offset_ordinal_(x_offset_ordinal),
      y_offset_ordinal_(y_offset_ordinal),
      finger_count_(finger_count) {
  CHECK(IsScrollEvent());
}

void ScrollEvent::Scale(const float factor) {
  x_offset_ *= factor;
  y_offset_ *= factor;
  x_offset_ordinal_ *= factor;
  y_offset_ordinal_ *= factor;
}

////////////////////////////////////////////////////////////////////////////////
// GestureEvent

GestureEvent::GestureEvent(float x,
                           float y,
                           int flags,
                           base::TimeDelta time_stamp,
                           const GestureEventDetails& details)
    : LocatedEvent(details.type(),
                   gfx::PointF(x, y),
                   gfx::PointF(x, y),
                   time_stamp,
                   flags | EF_FROM_TOUCH),
      details_(details) {
}

GestureEvent::~GestureEvent() {
}

}  // namespace ui
