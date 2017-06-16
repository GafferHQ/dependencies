// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_handle.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/motion_event_test_utils.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/touch_selection/touch_handle_orientation.h"

using ui::test::MockMotionEvent;

namespace ui {
namespace {

const int kDefaultTapTimeoutMs = 200;
const double kDefaultTapSlop = 10.;
const float kDefaultDrawableSize = 10.f;

struct MockDrawableData {
  MockDrawableData()
      : orientation(TouchHandleOrientation::UNDEFINED),
        alpha(0.f),
        enabled(false),
        visible(false),
        rect(0, 0, kDefaultDrawableSize, kDefaultDrawableSize) {}
  TouchHandleOrientation orientation;
  float alpha;
  bool enabled;
  bool visible;
  gfx::RectF rect;
};

class MockTouchHandleDrawable : public TouchHandleDrawable {
 public:
  explicit MockTouchHandleDrawable(MockDrawableData* data) : data_(data) {}
  ~MockTouchHandleDrawable() override {}

  void SetEnabled(bool enabled) override { data_->enabled = enabled; }

  void SetOrientation(TouchHandleOrientation orientation) override {
    data_->orientation = orientation;
  }

  void SetAlpha(float alpha) override {
    data_->alpha = alpha;
    data_->visible = alpha > 0;
  }

  void SetFocus(const gfx::PointF& position) override {
    // Anchor focus to the top left of the rect (regardless of orientation).
    data_->rect.set_origin(position);
  }

  gfx::RectF GetVisibleBounds() const override {
    return data_->rect;
  }

 private:
  MockDrawableData* data_;
};

}  // namespace

class TouchHandleTest : public testing::Test, public TouchHandleClient {
 public:
  TouchHandleTest()
      : dragging_(false),
        dragged_(false),
        tapped_(false),
        needs_animate_(false) {}

  ~TouchHandleTest() override {}

  // TouchHandleClient implementation.
  void OnDragBegin(const TouchSelectionDraggable& handler,
                   const gfx::PointF& drag_position) override {
    dragging_ = true;
  }

  void OnDragUpdate(const TouchSelectionDraggable& handler,
                    const gfx::PointF& drag_position) override {
    dragged_ = true;
    drag_position_ = drag_position;
  }

  void OnDragEnd(const TouchSelectionDraggable& handler) override {
    dragging_ = false;
  }

  bool IsWithinTapSlop(const gfx::Vector2dF& delta) const override {
    return delta.LengthSquared() < (kDefaultTapSlop * kDefaultTapSlop);
  }

  void OnHandleTapped(const TouchHandle& handle) override { tapped_ = true; }

  void SetNeedsAnimate() override { needs_animate_ = true; }

  scoped_ptr<TouchHandleDrawable> CreateDrawable() override {
    return make_scoped_ptr(new MockTouchHandleDrawable(&drawable_data_));
  }

  base::TimeDelta GetTapTimeout() const override {
    return base::TimeDelta::FromMilliseconds(kDefaultTapTimeoutMs);
  }

  void Animate(TouchHandle& handle) {
    needs_animate_ = false;
    base::TimeTicks now = base::TimeTicks::Now();
    while (handle.Animate(now))
      now += base::TimeDelta::FromMilliseconds(16);
  }

  bool GetAndResetHandleDragged() {
    bool dragged = dragged_;
    dragged_ = false;
    return dragged;
  }

  bool GetAndResetHandleTapped() {
    bool tapped = tapped_;
    tapped_ = false;
    return tapped;
  }

  bool GetAndResetNeedsAnimate() {
    bool needs_animate = needs_animate_;
    needs_animate_ = false;
    return needs_animate;
  }

  bool IsDragging() const { return dragging_; }
  const gfx::PointF& DragPosition() const { return drag_position_; }
  bool NeedsAnimate() const { return needs_animate_; }

  const MockDrawableData& drawable() { return drawable_data_; }

 private:
  gfx::PointF drag_position_;
  bool dragging_;
  bool dragged_;
  bool tapped_;
  bool needs_animate_;

  MockDrawableData drawable_data_;
};

TEST_F(TouchHandleTest, Visibility) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER);
  EXPECT_FALSE(drawable().visible);

  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);

  handle.SetVisible(false, TouchHandle::ANIMATION_NONE);
  EXPECT_FALSE(drawable().visible);

  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);
}

TEST_F(TouchHandleTest, VisibilityAnimation) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER);
  ASSERT_FALSE(NeedsAnimate());
  ASSERT_FALSE(drawable().visible);
  ASSERT_EQ(0.f, drawable().alpha);

  handle.SetVisible(true, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_TRUE(NeedsAnimate());
  EXPECT_FALSE(drawable().visible);
  EXPECT_EQ(0.f, drawable().alpha);

  Animate(handle);
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);

  ASSERT_FALSE(NeedsAnimate());
  handle.SetVisible(false, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_TRUE(NeedsAnimate());
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);

  Animate(handle);
  EXPECT_FALSE(drawable().visible);
  EXPECT_EQ(0.f, drawable().alpha);

  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);
  EXPECT_EQ(1.f, drawable().alpha);
  EXPECT_FALSE(GetAndResetNeedsAnimate());
  handle.SetVisible(false, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_EQ(1.f, drawable().alpha);
  EXPECT_TRUE(GetAndResetNeedsAnimate());
  handle.SetVisible(true, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_EQ(1.f, drawable().alpha);
  EXPECT_FALSE(GetAndResetNeedsAnimate());
}

TEST_F(TouchHandleTest, Orientation) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER);
  EXPECT_EQ(TouchHandleOrientation::CENTER, drawable().orientation);

  handle.SetOrientation(TouchHandleOrientation::LEFT);
  EXPECT_EQ(TouchHandleOrientation::LEFT, drawable().orientation);

  handle.SetOrientation(TouchHandleOrientation::RIGHT);
  EXPECT_EQ(TouchHandleOrientation::RIGHT, drawable().orientation);

  handle.SetOrientation(TouchHandleOrientation::CENTER);
  EXPECT_EQ(TouchHandleOrientation::CENTER, drawable().orientation);
}

TEST_F(TouchHandleTest, Position) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER);
  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);

  gfx::PointF position;
  EXPECT_EQ(gfx::PointF(), drawable().rect.origin());

  position = gfx::PointF(7.3f, -3.7f);
  handle.SetPosition(position);
  EXPECT_EQ(position, drawable().rect.origin());

  position = gfx::PointF(-7.3f, 3.7f);
  handle.SetPosition(position);
  EXPECT_EQ(position, drawable().rect.origin());
}

TEST_F(TouchHandleTest, PositionNotUpdatedWhileFadingOrInvisible) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER);

  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);
  ASSERT_TRUE(drawable().visible);
  ASSERT_FALSE(NeedsAnimate());

  gfx::PointF old_position(7.3f, -3.7f);
  handle.SetPosition(old_position);
  ASSERT_EQ(old_position, drawable().rect.origin());

  handle.SetVisible(false, TouchHandle::ANIMATION_SMOOTH);
  ASSERT_TRUE(NeedsAnimate());

  gfx::PointF new_position(3.7f, -3.7f);
  handle.SetPosition(new_position);
  EXPECT_EQ(old_position, drawable().rect.origin());
  EXPECT_TRUE(NeedsAnimate());

  // While the handle is fading, the new position should not take affect.
  base::TimeTicks now = base::TimeTicks::Now();
  while (handle.Animate(now)) {
    EXPECT_EQ(old_position, drawable().rect.origin());
    now += base::TimeDelta::FromMilliseconds(16);
  }

  // Even after the animation terminates, the new position will not be pushed.
  EXPECT_EQ(old_position, drawable().rect.origin());

  // As soon as the handle becomes visible, the new position will be pushed.
  handle.SetVisible(true, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_EQ(new_position, drawable().rect.origin());
}

TEST_F(TouchHandleTest, Enabled) {
  // A newly created handle defaults to enabled.
  TouchHandle handle(this, TouchHandleOrientation::CENTER);
  EXPECT_TRUE(drawable().enabled);

  handle.SetVisible(true, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_TRUE(GetAndResetNeedsAnimate());
  EXPECT_EQ(0.f, drawable().alpha);
  handle.SetEnabled(false);
  EXPECT_FALSE(drawable().enabled);

  // Dragging should not be allowed while the handle is disabled.
  base::TimeTicks event_time = base::TimeTicks::Now();
  const float kOffset = kDefaultDrawableSize / 2.f;
  MockMotionEvent event(
      MockMotionEvent::ACTION_DOWN, event_time, kOffset, kOffset);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));

  // Disabling mid-animation should cancel the animation.
  handle.SetEnabled(true);
  handle.SetVisible(false, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_TRUE(drawable().visible);
  EXPECT_TRUE(GetAndResetNeedsAnimate());
  handle.SetEnabled(false);
  EXPECT_FALSE(drawable().enabled);
  EXPECT_FALSE(drawable().visible);
  EXPECT_FALSE(handle.Animate(base::TimeTicks::Now()));

  // Disabling mid-drag should cancel the drag.
  handle.SetEnabled(true);
  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());
  handle.SetEnabled(false);
  EXPECT_FALSE(IsDragging());
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
}

TEST_F(TouchHandleTest, Drag) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER);

  base::TimeTicks event_time = base::TimeTicks::Now();
  const float kOffset = kDefaultDrawableSize / 2.f;

  // The handle must be visible to trigger drag.
  MockMotionEvent event(
      MockMotionEvent::ACTION_DOWN, event_time, kOffset, kOffset);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());
  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);

  // ACTION_DOWN must fall within the drawable region to trigger drag.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time, 50, 50);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  // Only ACTION_DOWN will trigger drag.
  event = MockMotionEvent(
      MockMotionEvent::ACTION_MOVE, event_time, kOffset, kOffset);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  // Start the drag.
  event = MockMotionEvent(
      MockMotionEvent::ACTION_DOWN, event_time, kOffset, kOffset);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  event = MockMotionEvent(
      MockMotionEvent::ACTION_MOVE, event_time, kOffset + 10, kOffset + 15);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetHandleDragged());
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(gfx::PointF(10, 15), DragPosition());

  event = MockMotionEvent(
      MockMotionEvent::ACTION_MOVE, event_time, kOffset - 10, kOffset - 15);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetHandleDragged());
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(gfx::PointF(-10, -15), DragPosition());

  event = MockMotionEvent(MockMotionEvent::ACTION_UP);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleDragged());
  EXPECT_FALSE(IsDragging());

  // Non-ACTION_DOWN events after the drag has terminated should not be handled.
  event = MockMotionEvent(MockMotionEvent::ACTION_CANCEL);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
}

TEST_F(TouchHandleTest, DragDefersOrientationChange) {
  TouchHandle handle(this, TouchHandleOrientation::RIGHT);
  ASSERT_EQ(drawable().orientation, TouchHandleOrientation::RIGHT);
  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);

  MockMotionEvent event(MockMotionEvent::ACTION_DOWN);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  // Orientation changes will be deferred until the drag ends.
  handle.SetOrientation(TouchHandleOrientation::LEFT);
  EXPECT_EQ(TouchHandleOrientation::RIGHT, drawable().orientation);

  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetHandleDragged());
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(TouchHandleOrientation::RIGHT, drawable().orientation);

  event = MockMotionEvent(MockMotionEvent::ACTION_UP);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleDragged());
  EXPECT_FALSE(IsDragging());
  EXPECT_EQ(TouchHandleOrientation::LEFT, drawable().orientation);
}

TEST_F(TouchHandleTest, DragDefersFade) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER);
  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);

  MockMotionEvent event(MockMotionEvent::ACTION_DOWN);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  // Fade will be deferred until the drag ends.
  handle.SetVisible(false, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_FALSE(NeedsAnimate());
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);

  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(NeedsAnimate());
  EXPECT_TRUE(drawable().visible);

  event = MockMotionEvent(MockMotionEvent::ACTION_UP);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());
  EXPECT_TRUE(NeedsAnimate());

  Animate(handle);
  EXPECT_FALSE(drawable().visible);
  EXPECT_EQ(0.f, drawable().alpha);
}

TEST_F(TouchHandleTest, DragTargettingUsesTouchSize) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER);
  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);

  base::TimeTicks event_time = base::TimeTicks::Now();
  const float kTouchSize = 24.f;
  const float kOffset = kDefaultDrawableSize + kTouchSize / 2.001f;

  MockMotionEvent event(
      MockMotionEvent::ACTION_DOWN, event_time, kOffset, 0);
  event.SetTouchMajor(0.f);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  event.SetTouchMajor(kTouchSize / 2.f);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  event.SetTouchMajor(kTouchSize);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  event.SetTouchMajor(kTouchSize * 2.f);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  // The touch hit test region should be circular.
  event = MockMotionEvent(
      MockMotionEvent::ACTION_DOWN, event_time, kOffset, kOffset);
  event.SetTouchMajor(kTouchSize);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  event.SetTouchMajor(kTouchSize * std::sqrt(2.f) - 0.1f);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  event.SetTouchMajor(kTouchSize * std::sqrt(2.f) + 0.1f);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  // Ensure a touch size of 0 can still register a hit.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN,
                          event_time,
                          kDefaultDrawableSize / 2.f,
                          kDefaultDrawableSize / 2.f);
  event.SetTouchMajor(0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());
}

TEST_F(TouchHandleTest, Tap) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER);
  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);

  base::TimeTicks event_time = base::TimeTicks::Now();

  // ACTION_CANCEL shouldn't trigger a tap.
  MockMotionEvent event(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event_time += base::TimeDelta::FromMilliseconds(50);
  event = MockMotionEvent(MockMotionEvent::ACTION_CANCEL, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleTapped());

  // Long press shouldn't trigger a tap.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event_time += 2 * GetTapTimeout();
  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleTapped());

  // Only a brief tap within the slop region should trigger a tap.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event_time += GetTapTimeout() / 2;
  event = MockMotionEvent(
      MockMotionEvent::ACTION_MOVE, event_time, kDefaultTapSlop / 2.f, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event = MockMotionEvent(
      MockMotionEvent::ACTION_UP, event_time, kDefaultTapSlop / 2.f, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetHandleTapped());

  // Moving beyond the slop region shouldn't trigger a tap.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event_time += GetTapTimeout() / 2;
  event = MockMotionEvent(
      MockMotionEvent::ACTION_MOVE, event_time, kDefaultTapSlop * 2.f, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event = MockMotionEvent(
      MockMotionEvent::ACTION_UP, event_time, kDefaultTapSlop * 2.f, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleTapped());
}

}  // namespace ui
