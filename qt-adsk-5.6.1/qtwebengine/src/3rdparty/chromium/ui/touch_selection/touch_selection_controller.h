// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_SELECTION_CONTROLLER_H_
#define UI_TOUCH_SELECTION_TOUCH_SELECTION_CONTROLLER_H_

#include "base/time/time.h"
#include "ui/base/touch/selection_bound.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/touch_selection/longpress_drag_selector.h"
#include "ui/touch_selection/selection_event_type.h"
#include "ui/touch_selection/touch_handle.h"
#include "ui/touch_selection/touch_handle_orientation.h"
#include "ui/touch_selection/ui_touch_selection_export.h"

namespace ui {
class MotionEvent;

// Interface through which |TouchSelectionController| issues selection-related
// commands, notifications and requests.
class UI_TOUCH_SELECTION_EXPORT TouchSelectionControllerClient {
 public:
  virtual ~TouchSelectionControllerClient() {}

  virtual bool SupportsAnimation() const = 0;
  virtual void SetNeedsAnimate() = 0;
  virtual void MoveCaret(const gfx::PointF& position) = 0;
  virtual void MoveRangeSelectionExtent(const gfx::PointF& extent) = 0;
  virtual void SelectBetweenCoordinates(const gfx::PointF& base,
                                        const gfx::PointF& extent) = 0;
  virtual void OnSelectionEvent(SelectionEventType event) = 0;
  virtual scoped_ptr<TouchHandleDrawable> CreateDrawable() = 0;
};

// Controller for manipulating text selection via touch input.
class UI_TOUCH_SELECTION_EXPORT TouchSelectionController
    : public TouchHandleClient,
      public LongPressDragSelectorClient {
 public:
  enum ActiveStatus {
    INACTIVE,
    INSERTION_ACTIVE,
    SELECTION_ACTIVE,
  };

  struct UI_TOUCH_SELECTION_EXPORT Config {
    Config();
    ~Config();

    // Defaults to 100 ms.
    base::TimeDelta tap_timeout;

    // Defaults to 8 DIPs.
    float tap_slop;

    // Controls whether drag selection after a longpress is enabled.
    // Defaults to false.
    bool enable_longpress_drag_selection;

    // Controls whether an insertion handle is shown on a tap for an empty
    // editable text. Defauls to false.
    bool show_on_tap_for_empty_editable;
  };

  TouchSelectionController(TouchSelectionControllerClient* client,
                           const Config& config);
  ~TouchSelectionController() override;

  // To be called when the selection bounds have changed.
  // Note that such updates will trigger handle updates only if preceded
  // by an appropriate call to allow automatic showing.
  void OnSelectionBoundsChanged(const SelectionBound& start,
                                const SelectionBound& end);

  // Allows touch-dragging of the handle.
  // Returns true iff the event was consumed, in which case the caller should
  // cease further handling of the event.
  bool WillHandleTouchEvent(const MotionEvent& event);

  // To be called before forwarding a tap event. This allows automatically
  // showing the insertion handle from subsequent bounds changes.
  bool WillHandleTapEvent(const gfx::PointF& location);

  // To be called before forwarding a longpress event. This allows automatically
  // showing the selection or insertion handles from subsequent bounds changes.
  bool WillHandleLongPressEvent(base::TimeTicks event_time,
                                const gfx::PointF& location);

  // Allow showing the selection handles from the most recent selection bounds
  // update (if valid), or a future valid bounds update.
  void AllowShowingFromCurrentSelection();

  // Hide the handles and suppress bounds updates until the next explicit
  // showing allowance.
  void HideAndDisallowShowingAutomatically();

  // Override the handle visibility according to |hidden|.
  void SetTemporarilyHidden(bool hidden);

  // To be called when the editability of the focused region changes.
  void OnSelectionEditable(bool editable);

  // To be called when the contents of the focused region changes.
  void OnSelectionEmpty(bool empty);

  // Ticks an active animation, as requested to the client by |SetNeedsAnimate|.
  // Returns true if an animation is active and requires further ticking.
  bool Animate(base::TimeTicks animate_time);

  // Returns the rect between the two active selection bounds. If just one of
  // the bounds is visible, the rect is simply the (one-dimensional) rect of
  // that bound. If no selection is active, an empty rect will be returned.
  gfx::RectF GetRectBetweenBounds() const;

  // Returns the visible rect of specified touch handle. For an active insertion
  // these values will be identical.
  gfx::RectF GetStartHandleRect() const;
  gfx::RectF GetEndHandleRect() const;

  // Returns the focal point of the start and end bounds, as defined by
  // their bottom coordinate.
  const gfx::PointF& GetStartPosition() const;
  const gfx::PointF& GetEndPosition() const;

  const SelectionBound& start() const { return start_; }
  const SelectionBound& end() const { return end_; }

  ActiveStatus active_status() const { return active_status_; }

 private:
  friend class TouchSelectionControllerTestApi;

  enum InputEventType { TAP, LONG_PRESS, INPUT_EVENT_TYPE_NONE };

  // TouchHandleClient implementation.
  void OnDragBegin(const TouchSelectionDraggable& draggable,
                   const gfx::PointF& drag_position) override;
  void OnDragUpdate(const TouchSelectionDraggable& draggable,
                    const gfx::PointF& drag_position) override;
  void OnDragEnd(const TouchSelectionDraggable& draggable) override;
  bool IsWithinTapSlop(const gfx::Vector2dF& delta) const override;
  void OnHandleTapped(const TouchHandle& handle) override;
  void SetNeedsAnimate() override;
  scoped_ptr<TouchHandleDrawable> CreateDrawable() override;
  base::TimeDelta GetTapTimeout() const override;

  // LongPressDragSelectorClient implementation.
  void OnLongPressDragActiveStateChanged() override;
  gfx::PointF GetSelectionStart() const override;
  gfx::PointF GetSelectionEnd() const override;

  void ShowInsertionHandleAutomatically();
  void ShowSelectionHandlesAutomatically();
  bool WillHandleTapOrLongPress(const gfx::PointF& location);

  void OnInsertionChanged();
  void OnSelectionChanged();

  // Returns true if insertion mode was newly (re)activated.
  bool ActivateInsertionIfNecessary();
  void DeactivateInsertion();
  // Returns true if selection mode was newly (re)activated.
  bool ActivateSelectionIfNecessary();
  void DeactivateSelection();
  void ForceNextUpdateIfInactive();

  bool WillHandleTouchEventForLongPressDrag(const MotionEvent& event);
  void SetTemporarilyHiddenForLongPressDrag(bool hidden);
  void RefreshHandleVisibility();

  gfx::Vector2dF GetStartLineOffset() const;
  gfx::Vector2dF GetEndLineOffset() const;
  bool GetStartVisible() const;
  bool GetEndVisible() const;
  TouchHandle::AnimationStyle GetAnimationStyle(bool was_active) const;

  void LogSelectionEnd();

  TouchSelectionControllerClient* const client_;
  const Config config_;

  // Whether to force an update on the next selection event even if the
  // cached selection matches the new selection.
  bool force_next_update_;

  InputEventType response_pending_input_event_;

  SelectionBound start_;
  SelectionBound end_;
  TouchHandleOrientation start_orientation_;
  TouchHandleOrientation end_orientation_;

  ActiveStatus active_status_;

  scoped_ptr<TouchHandle> insertion_handle_;
  bool activate_insertion_automatically_;

  scoped_ptr<TouchHandle> start_selection_handle_;
  scoped_ptr<TouchHandle> end_selection_handle_;
  bool activate_selection_automatically_;

  bool selection_empty_;
  bool selection_editable_;

  bool temporarily_hidden_;

  // Whether to use the start bound (if false, the end bound) for computing the
  // appropriate text line offset when performing a selection drag. This helps
  // ensure that the initial selection induced by the drag doesn't "jump"
  // between lines.
  bool anchor_drag_to_selection_start_;

  // Longpress drag allows direct manipulation of longpress-initiated selection.
  LongPressDragSelector longpress_drag_selector_;

  base::TimeTicks selection_start_time_;
  // Whether a selection handle was dragged during the current 'selection
  // session' - i.e. since the current selection has been activated.
  bool selection_handle_dragged_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionController);
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_SELECTION_CONTROLLER_H_
