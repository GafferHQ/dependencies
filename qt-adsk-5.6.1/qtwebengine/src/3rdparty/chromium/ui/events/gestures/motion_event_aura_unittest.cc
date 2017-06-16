// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include <cmath>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/gestures/motion_event_aura.h"
#include "ui/events/test/motion_event_test_utils.h"

namespace {

ui::TouchEvent TouchWithType(ui::EventType type, int id) {
  return ui::TouchEvent(
      type, gfx::PointF(0, 0), id, base::TimeDelta::FromMilliseconds(0));
}

ui::TouchEvent TouchWithPosition(ui::EventType type,
                                 int id,
                                 float x,
                                 float y,
                                 float raw_x,
                                 float raw_y) {
  ui::TouchEvent event(type,
                       gfx::PointF(x, y),
                       0,
                       id,
                       base::TimeDelta::FromMilliseconds(0),
                       0,
                       0,
                       0,
                       0);
  event.set_root_location(gfx::PointF(raw_x, raw_y));
  return event;
}

ui::TouchEvent TouchWithTapParams(ui::EventType type,
                                 int id,
                                 float radius_x,
                                 float radius_y,
                                 float rotation_angle,
                                 float pressure) {
  ui::TouchEvent event(type,
                       gfx::PointF(1, 1),
                       0,
                       id,
                       base::TimeDelta::FromMilliseconds(0),
                       radius_x,
                       radius_y,
                       rotation_angle,
                       pressure);
  event.set_root_location(gfx::PointF(1, 1));
  return event;
}

ui::TouchEvent TouchWithTime(ui::EventType type, int id, int ms) {
  return ui::TouchEvent(
      type, gfx::PointF(0, 0), id, base::TimeDelta::FromMilliseconds(ms));
}

base::TimeTicks MsToTicks(int ms) {
  return base::TimeTicks() + base::TimeDelta::FromMilliseconds(ms);
}

}  // namespace

namespace ui {

TEST(MotionEventAuraTest, PointerCountAndIds) {
  // Test that |PointerCount()| returns the correct number of pointers, and ids
  // are assigned correctly.
  int ids[] = {4, 6, 1};

  MotionEventAura event;
  EXPECT_EQ(0U, event.GetPointerCount());

  TouchEvent press0 = TouchWithType(ET_TOUCH_PRESSED, ids[0]);
  EXPECT_TRUE(event.OnTouch(press0));
  EXPECT_EQ(1U, event.GetPointerCount());

  EXPECT_EQ(ids[0], event.GetPointerId(0));

  TouchEvent press1 = TouchWithType(ET_TOUCH_PRESSED, ids[1]);
  EXPECT_TRUE(event.OnTouch(press1));
  EXPECT_EQ(2U, event.GetPointerCount());

  EXPECT_EQ(ids[0], event.GetPointerId(0));
  EXPECT_EQ(ids[1], event.GetPointerId(1));

  TouchEvent press2 = TouchWithType(ET_TOUCH_PRESSED, ids[2]);
  EXPECT_TRUE(event.OnTouch(press2));
  EXPECT_EQ(3U, event.GetPointerCount());

  EXPECT_EQ(ids[0], event.GetPointerId(0));
  EXPECT_EQ(ids[1], event.GetPointerId(1));
  EXPECT_EQ(ids[2], event.GetPointerId(2));

  TouchEvent release1 = TouchWithType(ET_TOUCH_RELEASED, ids[1]);
  EXPECT_TRUE(event.OnTouch(release1));
  event.CleanupRemovedTouchPoints(release1);
  EXPECT_EQ(2U, event.GetPointerCount());

  EXPECT_EQ(ids[0], event.GetPointerId(0));
  EXPECT_EQ(ids[2], event.GetPointerId(1));

  // Test cloning of pointer count and id information.
  // TODO(mustaq): Make a separate clone test, crbug.com/450655
  scoped_ptr<MotionEvent> clone = event.Clone();
  EXPECT_EQ(2U, clone->GetPointerCount());
  EXPECT_EQ(ids[0], clone->GetPointerId(0));
  EXPECT_EQ(ids[2], clone->GetPointerId(1));
  EXPECT_EQ(event.GetUniqueEventId(), clone->GetUniqueEventId());
  EXPECT_EQ(test::ToString(event), test::ToString(*clone));

  TouchEvent release0 = TouchWithType(ET_TOUCH_RELEASED, ids[0]);
  EXPECT_TRUE(event.OnTouch(release0));
  event.CleanupRemovedTouchPoints(release0);
  EXPECT_EQ(1U, event.GetPointerCount());

  EXPECT_EQ(ids[2], event.GetPointerId(0));

  TouchEvent release2 = TouchWithType(ET_TOUCH_RELEASED, ids[2]);
  EXPECT_TRUE(event.OnTouch(release2));
  event.CleanupRemovedTouchPoints(release2);
  EXPECT_EQ(0U, event.GetPointerCount());
}

TEST(MotionEventAuraTest, GetActionIndexAfterRemoval) {
  // Test that |GetActionIndex()| returns the correct index when points have
  // been removed.
  int ids[] = {4, 6, 9};

  MotionEventAura event;
  EXPECT_EQ(0U, event.GetPointerCount());

  TouchEvent press0 = TouchWithType(ET_TOUCH_PRESSED, ids[0]);
  EXPECT_TRUE(event.OnTouch(press0));
  TouchEvent press1 = TouchWithType(ET_TOUCH_PRESSED, ids[1]);
  EXPECT_TRUE(event.OnTouch(press1));
  EXPECT_EQ(1, event.GetActionIndex());
  TouchEvent press2 = TouchWithType(ET_TOUCH_PRESSED, ids[2]);
  EXPECT_TRUE(event.OnTouch(press2));
  EXPECT_EQ(2, event.GetActionIndex());
  EXPECT_EQ(3U, event.GetPointerCount());

  TouchEvent release1 = TouchWithType(ET_TOUCH_RELEASED, ids[1]);
  EXPECT_TRUE(event.OnTouch(release1));
  EXPECT_EQ(1, event.GetActionIndex());
  event.CleanupRemovedTouchPoints(release1);
  EXPECT_EQ(2U, event.GetPointerCount());

  TouchEvent release2 = TouchWithType(ET_TOUCH_RELEASED, ids[0]);
  EXPECT_TRUE(event.OnTouch(release2));
  EXPECT_EQ(0, event.GetActionIndex());
  event.CleanupRemovedTouchPoints(release2);
  EXPECT_EQ(1U, event.GetPointerCount());

  TouchEvent release0 = TouchWithType(ET_TOUCH_RELEASED, ids[2]);
  EXPECT_TRUE(event.OnTouch(release0));
  event.CleanupRemovedTouchPoints(release0);
  EXPECT_EQ(0U, event.GetPointerCount());
}

TEST(MotionEventAuraTest, PointerLocations) {
  // Test that location information is stored correctly.
  MotionEventAura event;

  const float kRawOffsetX = 11.1f;
  const float kRawOffsetY = 13.3f;

  int ids[] = {15, 13};
  float x;
  float y;
  float raw_x;
  float raw_y;

  x = 14.4f;
  y = 17.3f;
  raw_x = x + kRawOffsetX;
  raw_y = y + kRawOffsetY;
  TouchEvent press0 =
      TouchWithPosition(ET_TOUCH_PRESSED, ids[0], x, y, raw_x, raw_y);
  EXPECT_TRUE(event.OnTouch(press0));

  EXPECT_EQ(1U, event.GetPointerCount());
  EXPECT_FLOAT_EQ(x, event.GetX(0));
  EXPECT_FLOAT_EQ(y, event.GetY(0));
  EXPECT_FLOAT_EQ(raw_x, event.GetRawX(0));
  EXPECT_FLOAT_EQ(raw_y, event.GetRawY(0));

  x = 17.8f;
  y = 12.1f;
  raw_x = x + kRawOffsetX;
  raw_y = y + kRawOffsetY;
  TouchEvent press1 =
      TouchWithPosition(ET_TOUCH_PRESSED, ids[1], x, y, raw_x, raw_y);
  EXPECT_TRUE(event.OnTouch(press1));

  EXPECT_EQ(2U, event.GetPointerCount());
  EXPECT_FLOAT_EQ(x, event.GetX(1));
  EXPECT_FLOAT_EQ(y, event.GetY(1));
  EXPECT_FLOAT_EQ(raw_x, event.GetRawX(1));
  EXPECT_FLOAT_EQ(raw_y, event.GetRawY(1));

  // Test cloning of pointer location information.
  scoped_ptr<MotionEvent> clone = event.Clone();
  EXPECT_EQ(event.GetUniqueEventId(), clone->GetUniqueEventId());
  EXPECT_EQ(test::ToString(event), test::ToString(*clone));
  EXPECT_EQ(2U, clone->GetPointerCount());
  EXPECT_FLOAT_EQ(x, clone->GetX(1));
  EXPECT_FLOAT_EQ(y, clone->GetY(1));
  EXPECT_FLOAT_EQ(raw_x, clone->GetRawX(1));
  EXPECT_FLOAT_EQ(raw_y, clone->GetRawY(1));

  x = 27.9f;
  y = 22.3f;
  raw_x = x + kRawOffsetX;
  raw_y = y + kRawOffsetY;
  TouchEvent move1 =
      TouchWithPosition(ET_TOUCH_MOVED, ids[1], x, y, raw_x, raw_y);
  EXPECT_TRUE(event.OnTouch(move1));

  EXPECT_FLOAT_EQ(x, event.GetX(1));
  EXPECT_FLOAT_EQ(y, event.GetY(1));
  EXPECT_FLOAT_EQ(raw_x, event.GetRawX(1));
  EXPECT_FLOAT_EQ(raw_y, event.GetRawY(1));

  x = 34.6f;
  y = 23.8f;
  raw_x = x + kRawOffsetX;
  raw_y = y + kRawOffsetY;
  TouchEvent move0 =
      TouchWithPosition(ET_TOUCH_MOVED, ids[0], x, y, raw_x, raw_y);
  EXPECT_TRUE(event.OnTouch(move0));

  EXPECT_FLOAT_EQ(x, event.GetX(0));
  EXPECT_FLOAT_EQ(y, event.GetY(0));
  EXPECT_FLOAT_EQ(raw_x, event.GetRawX(0));
  EXPECT_FLOAT_EQ(raw_y, event.GetRawY(0));
}

TEST(MotionEventAuraTest, TapParams) {
  // Test that touch params are stored correctly.
  MotionEventAura event;

  int ids[] = {15, 13, 25, 23};

  float radius_x;
  float radius_y;
  float rotation_angle;
  float pressure;

  // Test case: radius_x > radius_y, rotation_angle < 90
  radius_x = 123.45f;
  radius_y = 67.89f;
  rotation_angle = 23.f;
  pressure = 0.123f;
  TouchEvent press0 = TouchWithTapParams(
      ET_TOUCH_PRESSED, ids[0], radius_x, radius_y, rotation_angle, pressure);
  EXPECT_TRUE(event.OnTouch(press0));

  EXPECT_EQ(1U, event.GetPointerCount());
  EXPECT_FLOAT_EQ(radius_x, event.GetTouchMajor(0) / 2);
  EXPECT_FLOAT_EQ(radius_y, event.GetTouchMinor(0) / 2);
  EXPECT_FLOAT_EQ(rotation_angle, event.GetOrientation(0) * 180 / M_PI + 90);
  EXPECT_FLOAT_EQ(pressure, event.GetPressure(0));

  // Test case: radius_x < radius_y, rotation_angle < 90
  radius_x = 67.89f;
  radius_y = 123.45f;
  rotation_angle = 46.f;
  pressure = 0.456f;
  TouchEvent press1 = TouchWithTapParams(
      ET_TOUCH_PRESSED, ids[1], radius_x, radius_y, rotation_angle, pressure);
  EXPECT_TRUE(event.OnTouch(press1));

  EXPECT_EQ(2U, event.GetPointerCount());
  EXPECT_FLOAT_EQ(radius_y, event.GetTouchMajor(1) / 2);
  EXPECT_FLOAT_EQ(radius_x, event.GetTouchMinor(1) / 2);
  EXPECT_FLOAT_EQ(rotation_angle, event.GetOrientation(1) * 180 / M_PI);
  EXPECT_FLOAT_EQ(pressure, event.GetPressure(1));

  // Test cloning of tap params
  // TODO(mustaq): Make a separate clone test, crbug.com/450655
  scoped_ptr<MotionEvent> clone = event.Clone();
  EXPECT_EQ(event.GetUniqueEventId(), clone->GetUniqueEventId());
  EXPECT_EQ(test::ToString(event), test::ToString(*clone));
  EXPECT_EQ(2U, clone->GetPointerCount());
  EXPECT_FLOAT_EQ(radius_y, clone->GetTouchMajor(1) / 2);
  EXPECT_FLOAT_EQ(radius_x, clone->GetTouchMinor(1) / 2);
  EXPECT_FLOAT_EQ(rotation_angle, clone->GetOrientation(1) * 180 / M_PI);
  EXPECT_FLOAT_EQ(pressure, clone->GetPressure(1));

  // TODO(mustaq): The move test seems out-of-scope here, crbug.com/450655
  radius_x = 76.98f;
  radius_y = 321.54f;
  rotation_angle = 64.f;
  pressure = 0.654f;
  TouchEvent move1 = TouchWithTapParams(
      ET_TOUCH_MOVED, ids[1], radius_x, radius_y, rotation_angle, pressure);
  move1.set_location(gfx::Point(20, 21));
  EXPECT_TRUE(event.OnTouch(move1));

  EXPECT_EQ(2U, event.GetPointerCount());
  EXPECT_FLOAT_EQ(radius_y, event.GetTouchMajor(1) / 2);
  EXPECT_FLOAT_EQ(radius_x, event.GetTouchMinor(1) / 2);
  EXPECT_FLOAT_EQ(rotation_angle, event.GetOrientation(1) * 180 / M_PI);
  EXPECT_FLOAT_EQ(pressure, event.GetPressure(1));

  // Test case: radius_x > radius_y, rotation_angle > 90
  radius_x = 123.45f;
  radius_y = 67.89f;
  rotation_angle = 92.f;
  pressure = 0.789f;
  TouchEvent press2 = TouchWithTapParams(
      ET_TOUCH_PRESSED, ids[2], radius_x, radius_y, rotation_angle, pressure);
  EXPECT_TRUE(event.OnTouch(press2));

  EXPECT_EQ(3U, event.GetPointerCount());
  EXPECT_FLOAT_EQ(radius_x, event.GetTouchMajor(2) / 2);
  EXPECT_FLOAT_EQ(radius_y, event.GetTouchMinor(2) / 2);
  EXPECT_FLOAT_EQ(rotation_angle, event.GetOrientation(2) * 180 / M_PI + 90);
  EXPECT_FLOAT_EQ(pressure, event.GetPressure(2));

  // Test case: radius_x < radius_y, rotation_angle > 90
  radius_x = 67.89f;
  radius_y = 123.45f;
  rotation_angle = 135.f;
  pressure = 0.012f;
  TouchEvent press3 = TouchWithTapParams(
      ET_TOUCH_PRESSED, ids[3], radius_x, radius_y, rotation_angle, pressure);
  EXPECT_TRUE(event.OnTouch(press3));

  EXPECT_EQ(4U, event.GetPointerCount());
  EXPECT_FLOAT_EQ(radius_y, event.GetTouchMajor(3) / 2);
  EXPECT_FLOAT_EQ(radius_x, event.GetTouchMinor(3) / 2);
  EXPECT_FLOAT_EQ(rotation_angle, event.GetOrientation(3) * 180 / M_PI + 180);
  EXPECT_FLOAT_EQ(pressure, event.GetPressure(3));
}

TEST(MotionEventAuraTest, Timestamps) {
  // Test that timestamp information is stored and converted correctly.
  MotionEventAura event;
  int ids[] = {7, 13};
  int times_in_ms[] = {59436, 60263, 82175};

  TouchEvent press0 = TouchWithTime(
      ui::ET_TOUCH_PRESSED, ids[0], times_in_ms[0]);
  EXPECT_TRUE(event.OnTouch(press0));
  EXPECT_EQ(MsToTicks(times_in_ms[0]), event.GetEventTime());

  TouchEvent press1 = TouchWithTime(
      ui::ET_TOUCH_PRESSED, ids[1], times_in_ms[1]);
  EXPECT_TRUE(event.OnTouch(press1));
  EXPECT_EQ(MsToTicks(times_in_ms[1]), event.GetEventTime());

  TouchEvent move0 = TouchWithTime(
      ui::ET_TOUCH_MOVED, ids[0], times_in_ms[2]);
  move0.set_location(gfx::PointF(12, 21));
  EXPECT_TRUE(event.OnTouch(move0));
  EXPECT_EQ(MsToTicks(times_in_ms[2]), event.GetEventTime());

  // Test cloning of timestamp information.
  scoped_ptr<MotionEvent> clone = event.Clone();
  EXPECT_EQ(MsToTicks(times_in_ms[2]), clone->GetEventTime());
}

TEST(MotionEventAuraTest, CachedAction) {
  // Test that the cached action and cached action index are correct.
  int ids[] = {4, 6};
  MotionEventAura event;

  TouchEvent press0 = TouchWithType(ET_TOUCH_PRESSED, ids[0]);
  EXPECT_TRUE(event.OnTouch(press0));
  EXPECT_EQ(MotionEvent::ACTION_DOWN, event.GetAction());
  EXPECT_EQ(1U, event.GetPointerCount());

  TouchEvent press1 = TouchWithType(ET_TOUCH_PRESSED, ids[1]);
  EXPECT_TRUE(event.OnTouch(press1));
  EXPECT_EQ(MotionEvent::ACTION_POINTER_DOWN, event.GetAction());
  EXPECT_EQ(1, event.GetActionIndex());
  EXPECT_EQ(2U, event.GetPointerCount());

  // Test cloning of CachedAction information.
  scoped_ptr<MotionEvent> clone = event.Clone();
  EXPECT_EQ(MotionEvent::ACTION_POINTER_DOWN, clone->GetAction());
  EXPECT_EQ(1, clone->GetActionIndex());

  TouchEvent move0 = TouchWithType(ET_TOUCH_MOVED, ids[0]);
  move0.set_location(gfx::PointF(10, 12));
  EXPECT_TRUE(event.OnTouch(move0));
  EXPECT_EQ(MotionEvent::ACTION_MOVE, event.GetAction());
  EXPECT_EQ(2U, event.GetPointerCount());

  TouchEvent release0 = TouchWithType(ET_TOUCH_RELEASED, ids[0]);
  EXPECT_TRUE(event.OnTouch(release0));
  EXPECT_EQ(MotionEvent::ACTION_POINTER_UP, event.GetAction());
  EXPECT_EQ(2U, event.GetPointerCount());
  event.CleanupRemovedTouchPoints(release0);
  EXPECT_EQ(1U, event.GetPointerCount());

  TouchEvent release1 = TouchWithType(ET_TOUCH_RELEASED, ids[1]);
  EXPECT_TRUE(event.OnTouch(release1));
  EXPECT_EQ(MotionEvent::ACTION_UP, event.GetAction());
  EXPECT_EQ(1U, event.GetPointerCount());
  event.CleanupRemovedTouchPoints(release1);
  EXPECT_EQ(0U, event.GetPointerCount());
}

TEST(MotionEventAuraTest, Cancel) {
  int ids[] = {4, 6};
  MotionEventAura event;

  TouchEvent press0 = TouchWithType(ET_TOUCH_PRESSED, ids[0]);
  EXPECT_TRUE(event.OnTouch(press0));
  EXPECT_EQ(MotionEvent::ACTION_DOWN, event.GetAction());
  EXPECT_EQ(1U, event.GetPointerCount());

  TouchEvent press1 = TouchWithType(ET_TOUCH_PRESSED, ids[1]);
  EXPECT_TRUE(event.OnTouch(press1));
  EXPECT_EQ(MotionEvent::ACTION_POINTER_DOWN, event.GetAction());
  EXPECT_EQ(1, event.GetActionIndex());
  EXPECT_EQ(2U, event.GetPointerCount());

  scoped_ptr<MotionEvent> cancel = event.Cancel();
  EXPECT_EQ(MotionEvent::ACTION_CANCEL, cancel->GetAction());
  EXPECT_EQ(2U, cancel->GetPointerCount());
}

TEST(MotionEventAuraTest, ToolType) {
  MotionEventAura event;

  // For now, all pointers have an unknown tool type.
  // TODO(jdduke): Expand this test when ui::TouchEvent identifies the source
  // touch type, crbug.com/404128.
  EXPECT_TRUE(event.OnTouch(TouchWithType(ET_TOUCH_PRESSED, 7)));
  ASSERT_EQ(1U, event.GetPointerCount());
  EXPECT_EQ(MotionEvent::TOOL_TYPE_UNKNOWN, event.GetToolType(0));
}

TEST(MotionEventAuraTest, Flags) {
  int ids[] = {7, 11};
  MotionEventAura event;

  TouchEvent press0 = TouchWithType(ET_TOUCH_PRESSED, ids[0]);
  press0.set_flags(EF_CONTROL_DOWN);
  EXPECT_TRUE(event.OnTouch(press0));
  EXPECT_EQ(EF_CONTROL_DOWN, event.GetFlags());

  TouchEvent press1 = TouchWithType(ET_TOUCH_PRESSED, ids[1]);
  press1.set_flags(EF_CONTROL_DOWN | EF_CAPS_LOCK_DOWN);
  EXPECT_TRUE(event.OnTouch(press1));
  EXPECT_EQ(EF_CONTROL_DOWN | EF_CAPS_LOCK_DOWN, event.GetFlags());
}

// Once crbug.com/446852 is fixed, we should ignore redundant presses.
TEST(MotionEventAuraTest, DoesntIgnoreRedundantPresses) {
  const int id = 7;
  const int position_1 = 10;
  const int position_2 = 23;

  MotionEventAura event;
  TouchEvent press1 = TouchWithPosition(ET_TOUCH_PRESSED, id, position_1,
                                        position_1, position_1, position_1);
  EXPECT_TRUE(event.OnTouch(press1));
  TouchEvent press2 = TouchWithPosition(ET_TOUCH_PRESSED, id, position_2,
                                        position_2, position_2, position_2);
  EXPECT_TRUE(event.OnTouch(press2));

  EXPECT_EQ(1U, event.GetPointerCount());
  EXPECT_FLOAT_EQ(position_2, event.GetX(0));
}

TEST(MotionEventAuraTest, IgnoresEventsWithoutPress) {
  int id = 7;
  MotionEventAura event;
  EXPECT_FALSE(event.OnTouch(TouchWithType(ET_TOUCH_MOVED, id)));
}

TEST(MotionEventAuraTest, IgnoresStationaryMoves) {
  int id = 7;
  MotionEventAura event;
  EXPECT_TRUE(event.OnTouch(TouchWithType(ET_TOUCH_PRESSED, id)));
  TouchEvent move0 = TouchWithPosition(ET_TOUCH_PRESSED, id, 10, 20, 10, 20);
  EXPECT_TRUE(event.OnTouch(move0));

  TouchEvent move1 = TouchWithPosition(ET_TOUCH_MOVED, id, 11, 21, 11, 21);
  EXPECT_TRUE(event.OnTouch(move1));
  EXPECT_FALSE(event.OnTouch(move1));
}

// Test after converting touch events into motion events, motion events should
// have the same unique_event_id as touch events.
TEST(MotionEventAuraTest, UniqueEventID) {
  MotionEventAura event;

  TouchEvent press0 = TouchWithType(ET_TOUCH_PRESSED, 3);
  EXPECT_TRUE(event.OnTouch(press0));
  EXPECT_EQ(MotionEvent::ACTION_DOWN, event.GetAction());
  ASSERT_EQ(1U, event.GetPointerCount());
  EXPECT_EQ(event.GetUniqueEventId(), press0.unique_event_id());

  TouchEvent press1 = TouchWithType(ET_TOUCH_PRESSED, 6);
  EXPECT_TRUE(event.OnTouch(press1));
  EXPECT_EQ(MotionEvent::ACTION_POINTER_DOWN, event.GetAction());
  EXPECT_EQ(2U, event.GetPointerCount());
  EXPECT_EQ(event.GetUniqueEventId(), press1.unique_event_id());
}

}  // namespace ui
