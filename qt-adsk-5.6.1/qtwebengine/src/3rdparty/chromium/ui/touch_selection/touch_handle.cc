// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_handle.h"

#include <cmath>

namespace ui {

namespace {

// Maximum duration of a fade sequence.
const double kFadeDurationMs = 200;

// Maximum amount of travel for a fade sequence. This avoids handle "ghosting"
// when the handle is moving rapidly while the fade is active.
const double kFadeDistanceSquared = 20.f * 20.f;

// Avoid using an empty touch rect, as it may fail the intersection test event
// if it lies within the other rect's bounds.
const float kMinTouchMajorForHitTesting = 1.f;

// The maximum touch size to use when computing whether a touch point is
// targetting a touch handle. This is necessary for devices that misreport
// touch radii, preventing inappropriately largely touch sizes from completely
// breaking handle dragging behavior.
const float kMaxTouchMajorForHitTesting = 36.f;

// Note that the intersection region is boundary *exclusive*.
bool RectIntersectsCircle(const gfx::RectF& rect,
                          const gfx::PointF& circle_center,
                          float circle_radius) {
  DCHECK_GT(circle_radius, 0.f);
  // An intersection occurs if the closest point between the rect and the
  // circle's center is less than the circle's radius.
  gfx::PointF closest_point_in_rect(circle_center);
  closest_point_in_rect.SetToMax(rect.origin());
  closest_point_in_rect.SetToMin(rect.bottom_right());

  gfx::Vector2dF distance = circle_center - closest_point_in_rect;
  return distance.LengthSquared() < (circle_radius * circle_radius);
}

}  // namespace

// TODO(AviD): Remove this once logging(DCHECK) supports enum class.
static std::ostream& operator<<(std::ostream& os,
                                const TouchHandleOrientation& orientation) {
  switch (orientation) {
    case TouchHandleOrientation::LEFT:
      return os << "LEFT";
    case TouchHandleOrientation::RIGHT:
      return os << "RIGHT";
    case TouchHandleOrientation::CENTER:
      return os << "CENTER";
    case TouchHandleOrientation::UNDEFINED:
      return os << "UNDEFINED";
    default:
      return os << "INVALID: " << static_cast<int>(orientation);
  }
}

// Responsible for rendering a selection or insertion handle for text editing.
TouchHandle::TouchHandle(TouchHandleClient* client,
                         TouchHandleOrientation orientation)
    : drawable_(client->CreateDrawable()),
      client_(client),
      orientation_(orientation),
      deferred_orientation_(TouchHandleOrientation::UNDEFINED),
      alpha_(0.f),
      animate_deferred_fade_(false),
      enabled_(true),
      is_visible_(false),
      is_dragging_(false),
      is_drag_within_tap_region_(false) {
  DCHECK_NE(orientation, TouchHandleOrientation::UNDEFINED);
  drawable_->SetEnabled(enabled_);
  drawable_->SetOrientation(orientation_);
  drawable_->SetAlpha(alpha_);
  drawable_->SetFocus(position_);
}

TouchHandle::~TouchHandle() {
}

void TouchHandle::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  if (!enabled) {
    EndDrag();
    EndFade();
  }
  enabled_ = enabled;
  drawable_->SetEnabled(enabled);
}

void TouchHandle::SetVisible(bool visible, AnimationStyle animation_style) {
  DCHECK(enabled_);
  if (is_visible_ == visible)
    return;

  is_visible_ = visible;

  // Handle repositioning may have been deferred while previously invisible.
  if (visible)
    drawable_->SetFocus(position_);

  bool animate = animation_style != ANIMATION_NONE;
  if (is_dragging_) {
    animate_deferred_fade_ = animate;
    return;
  }

  if (animate)
    BeginFade();
  else
    EndFade();
}

void TouchHandle::SetPosition(const gfx::PointF& position) {
  DCHECK(enabled_);
  if (position_ == position)
    return;
  position_ = position;
  // Suppress repositioning a handle while invisible or fading out to prevent it
  // from "ghosting" outside the visible bounds. The position will be pushed to
  // the drawable when the handle regains visibility (see |SetVisible()|).
  if (is_visible_)
    drawable_->SetFocus(position_);
}

void TouchHandle::SetOrientation(TouchHandleOrientation orientation) {
  DCHECK(enabled_);
  DCHECK_NE(orientation, TouchHandleOrientation::UNDEFINED);
  if (is_dragging_) {
    deferred_orientation_ = orientation;
    return;
  }
  DCHECK_EQ(deferred_orientation_, TouchHandleOrientation::UNDEFINED);
  if (orientation_ == orientation)
    return;

  orientation_ = orientation;
  drawable_->SetOrientation(orientation);
}

bool TouchHandle::WillHandleTouchEvent(const MotionEvent& event) {
  if (!enabled_)
    return false;

  if (!is_dragging_ && event.GetAction() != MotionEvent::ACTION_DOWN)
    return false;

  switch (event.GetAction()) {
    case MotionEvent::ACTION_DOWN: {
      if (!is_visible_)
        return false;
      const gfx::PointF touch_point(event.GetX(), event.GetY());
      const float touch_radius = std::max(
          kMinTouchMajorForHitTesting,
          std::min(kMaxTouchMajorForHitTesting, event.GetTouchMajor())) * 0.5f;
      if (!RectIntersectsCircle(drawable_->GetVisibleBounds(),
                                touch_point,
                                touch_radius)) {
        EndDrag();
        return false;
      }
      touch_down_position_ = touch_point;
      touch_drag_offset_ = position_ - touch_down_position_;
      touch_down_time_ = event.GetEventTime();
      BeginDrag();
    } break;

    case MotionEvent::ACTION_MOVE: {
      gfx::PointF touch_move_position(event.GetX(), event.GetY());
      is_drag_within_tap_region_ &=
          client_->IsWithinTapSlop(touch_down_position_ - touch_move_position);

      // Note that we signal drag update even if we're inside the tap region,
      // as there are cases where characters are narrower than the slop length.
      client_->OnDragUpdate(*this, touch_move_position + touch_drag_offset_);
    } break;

    case MotionEvent::ACTION_UP: {
      if (is_drag_within_tap_region_ &&
          (event.GetEventTime() - touch_down_time_) <
              client_->GetTapTimeout()) {
        client_->OnHandleTapped(*this);
      }

      EndDrag();
    } break;

    case MotionEvent::ACTION_CANCEL:
      EndDrag();
      break;

    default:
      break;
  };
  return true;
}

bool TouchHandle::IsActive() const {
  return is_dragging_;
}

bool TouchHandle::Animate(base::TimeTicks frame_time) {
  if (fade_end_time_ == base::TimeTicks())
    return false;

  DCHECK(enabled_);

  float time_u =
      1.f - (fade_end_time_ - frame_time).InMillisecondsF() / kFadeDurationMs;
  float position_u =
      (position_ - fade_start_position_).LengthSquared() / kFadeDistanceSquared;
  float u = std::max(time_u, position_u);
  SetAlpha(is_visible_ ? u : 1.f - u);

  if (u >= 1.f) {
    EndFade();
    return false;
  }

  return true;
}

gfx::RectF TouchHandle::GetVisibleBounds() const {
  if (!is_visible_ || !enabled_)
    return gfx::RectF();

  return drawable_->GetVisibleBounds();
}

void TouchHandle::BeginDrag() {
  DCHECK(enabled_);
  if (is_dragging_)
    return;
  EndFade();
  is_dragging_ = true;
  is_drag_within_tap_region_ = true;
  client_->OnDragBegin(*this, position());
}

void TouchHandle::EndDrag() {
  DCHECK(enabled_);
  if (!is_dragging_)
    return;

  is_dragging_ = false;
  is_drag_within_tap_region_ = false;
  client_->OnDragEnd(*this);

  if (deferred_orientation_ != TouchHandleOrientation::UNDEFINED) {
    TouchHandleOrientation deferred_orientation = deferred_orientation_;
    deferred_orientation_ = TouchHandleOrientation::UNDEFINED;
    SetOrientation(deferred_orientation);
  }

  if (animate_deferred_fade_) {
    BeginFade();
  } else {
    // As drawable visibility assignment is deferred while dragging, push the
    // change by forcing fade completion.
    EndFade();
  }
}

void TouchHandle::BeginFade() {
  DCHECK(enabled_);
  DCHECK(!is_dragging_);
  animate_deferred_fade_ = false;
  const float target_alpha = is_visible_ ? 1.f : 0.f;
  if (target_alpha == alpha_) {
    EndFade();
    return;
  }

  fade_end_time_ = base::TimeTicks::Now() +
                   base::TimeDelta::FromMillisecondsD(
                       kFadeDurationMs * std::abs(target_alpha - alpha_));
  fade_start_position_ = position_;
  client_->SetNeedsAnimate();
}

void TouchHandle::EndFade() {
  DCHECK(enabled_);
  animate_deferred_fade_ = false;
  fade_end_time_ = base::TimeTicks();
  SetAlpha(is_visible_ ? 1.f : 0.f);
}

void TouchHandle::SetAlpha(float alpha) {
  alpha = std::max(0.f, std::min(1.f, alpha));
  if (alpha_ == alpha)
    return;
  alpha_ = alpha;
  drawable_->SetAlpha(alpha);
}

}  // namespace ui
