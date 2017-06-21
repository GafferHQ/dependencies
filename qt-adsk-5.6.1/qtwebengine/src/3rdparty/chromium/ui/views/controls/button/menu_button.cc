// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/menu_button.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/text_constants.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

using base::TimeTicks;
using base::TimeDelta;

namespace views {

// Default menu offset.
static const int kDefaultMenuOffsetX = -2;
static const int kDefaultMenuOffsetY = -4;

// static
const char MenuButton::kViewClassName[] = "MenuButton";
const int MenuButton::kMenuMarkerPaddingLeft = 3;
const int MenuButton::kMenuMarkerPaddingRight = -1;

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton::PressedLock
//
////////////////////////////////////////////////////////////////////////////////

MenuButton::PressedLock::PressedLock(MenuButton* menu_button)
    : menu_button_(menu_button->weak_factory_.GetWeakPtr()) {
  menu_button_->IncrementPressedLocked();
}

MenuButton::PressedLock::~PressedLock() {
  if (menu_button_.get())
    menu_button_->DecrementPressedLocked();
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - constructors, destructors, initialization
//
////////////////////////////////////////////////////////////////////////////////

MenuButton::MenuButton(ButtonListener* listener,
                       const base::string16& text,
                       MenuButtonListener* menu_button_listener,
                       bool show_menu_marker)
    : LabelButton(listener, text),
      menu_offset_(kDefaultMenuOffsetX, kDefaultMenuOffsetY),
      listener_(menu_button_listener),
      show_menu_marker_(show_menu_marker),
      menu_marker_(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_MENU_DROPARROW).ToImageSkia()),
      destroyed_flag_(NULL),
      pressed_lock_count_(0),
      should_disable_after_press_(false),
      weak_factory_(this) {
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

MenuButton::~MenuButton() {
  if (destroyed_flag_)
    *destroyed_flag_ = true;
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - Public APIs
//
////////////////////////////////////////////////////////////////////////////////

bool MenuButton::Activate() {
  SetState(STATE_PRESSED);
  if (listener_) {
    gfx::Rect lb = GetLocalBounds();

    // The position of the menu depends on whether or not the locale is
    // right-to-left.
    gfx::Point menu_position(lb.right(), lb.bottom());
    if (base::i18n::IsRTL())
      menu_position.set_x(lb.x());

    View::ConvertPointToScreen(this, &menu_position);
    if (base::i18n::IsRTL())
      menu_position.Offset(-menu_offset_.x(), menu_offset_.y());
    else
      menu_position.Offset(menu_offset_.x(), menu_offset_.y());

    int max_x_coordinate = GetMaximumScreenXCoordinate();
    if (max_x_coordinate && max_x_coordinate <= menu_position.x())
      menu_position.set_x(max_x_coordinate - 1);

    // We're about to show the menu from a mouse press. By showing from the
    // mouse press event we block RootView in mouse dispatching. This also
    // appears to cause RootView to get a mouse pressed BEFORE the mouse
    // release is seen, which means RootView sends us another mouse press no
    // matter where the user pressed. To force RootView to recalculate the
    // mouse target during the mouse press we explicitly set the mouse handler
    // to NULL.
    static_cast<internal::RootView*>(GetWidget()->GetRootView())->
        SetMouseHandler(NULL);

    bool destroyed = false;
    destroyed_flag_ = &destroyed;

    // We don't set our state here. It's handled in the MenuController code or
    // by our click listener.

    listener_->OnMenuButtonClicked(this, menu_position);

    if (destroyed) {
      // The menu was deleted while showing. Don't attempt any processing.
      return false;
    }

    destroyed_flag_ = NULL;

    menu_closed_time_ = TimeTicks::Now();

    // We must return false here so that the RootView does not get stuck
    // sending all mouse pressed events to us instead of the appropriate
    // target.
    return false;
  }
  return true;
}

void MenuButton::OnPaint(gfx::Canvas* canvas) {
  LabelButton::OnPaint(canvas);

  if (show_menu_marker_)
    PaintMenuMarker(canvas);
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - Events
//
////////////////////////////////////////////////////////////////////////////////

gfx::Size MenuButton::GetPreferredSize() const {
  gfx::Size prefsize = LabelButton::GetPreferredSize();
  if (show_menu_marker_) {
    prefsize.Enlarge(menu_marker_->width() + kMenuMarkerPaddingLeft +
                         kMenuMarkerPaddingRight,
                     0);
  }
  return prefsize;
}

const char* MenuButton::GetClassName() const {
  return kViewClassName;
}

bool MenuButton::OnMousePressed(const ui::MouseEvent& event) {
  RequestFocus();
  if (state() != STATE_DISABLED && ShouldEnterPushedState(event) &&
      HitTestPoint(event.location())) {
    TimeDelta delta = TimeTicks::Now() - menu_closed_time_;
    if (delta.InMilliseconds() > kMinimumMsBetweenButtonClicks)
      return Activate();
  }
  return true;
}

void MenuButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (state() != STATE_DISABLED && ShouldEnterPushedState(event) &&
      HitTestPoint(event.location()) && !InDrag()) {
    Activate();
  } else {
    LabelButton::OnMouseReleased(event);
  }
}

void MenuButton::OnMouseEntered(const ui::MouseEvent& event) {
  if (pressed_lock_count_ == 0)  // Ignore mouse movement if state is locked.
    LabelButton::OnMouseEntered(event);
}

void MenuButton::OnMouseExited(const ui::MouseEvent& event) {
  if (pressed_lock_count_ == 0)  // Ignore mouse movement if state is locked.
    LabelButton::OnMouseExited(event);
}

void MenuButton::OnMouseMoved(const ui::MouseEvent& event) {
  if (pressed_lock_count_ == 0)  // Ignore mouse movement if state is locked.
    LabelButton::OnMouseMoved(event);
}

void MenuButton::OnGestureEvent(ui::GestureEvent* event) {
  if (state() != STATE_DISABLED) {
    if (ShouldEnterPushedState(*event) && !Activate()) {
      // When |Activate()| returns |false|, it means that a menu is shown and
      // has handled the gesture event. So, there is no need to further process
      // the gesture event here.
      return;
    }
    if (switches::IsTouchFeedbackEnabled()) {
      if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
        event->SetHandled();
        SetState(Button::STATE_HOVERED);
      } else if (state() == Button::STATE_HOVERED &&
                 (event->type() == ui::ET_GESTURE_TAP_CANCEL ||
                  event->type() == ui::ET_GESTURE_END)) {
        SetState(Button::STATE_NORMAL);
      }
    }
  }
  LabelButton::OnGestureEvent(event);
}

bool MenuButton::OnKeyPressed(const ui::KeyEvent& event) {
  switch (event.key_code()) {
    case ui::VKEY_SPACE:
      // Alt-space on windows should show the window menu.
      if (event.IsAltDown())
        break;
    case ui::VKEY_RETURN:
    case ui::VKEY_UP:
    case ui::VKEY_DOWN: {
      // WARNING: we may have been deleted by the time Activate returns.
      Activate();
      // This is to prevent the keyboard event from being dispatched twice.  If
      // the keyboard event is not handled, we pass it to the default handler
      // which dispatches the event back to us causing the menu to get displayed
      // again. Return true to prevent this.
      return true;
    }
    default:
      break;
  }
  return false;
}

bool MenuButton::OnKeyReleased(const ui::KeyEvent& event) {
  // Override CustomButton's implementation, which presses the button when
  // you press space and clicks it when you release space.  For a MenuButton
  // we always activate the menu on key press.
  return false;
}

void MenuButton::GetAccessibleState(ui::AXViewState* state) {
  CustomButton::GetAccessibleState(state);
  state->role = ui::AX_ROLE_POP_UP_BUTTON;
  state->default_action = l10n_util::GetStringUTF16(IDS_APP_ACCACTION_PRESS);
  state->AddStateFlag(ui::AX_STATE_HASPOPUP);
}

void MenuButton::PaintMenuMarker(gfx::Canvas* canvas) {
  gfx::Insets insets = GetInsets();

  // Using the Views mirroring infrastructure incorrectly flips icon content.
  // Instead, manually mirror the position of the down arrow.
  gfx::Rect arrow_bounds(width() - insets.right() -
                         menu_marker_->width() - kMenuMarkerPaddingRight,
                         height() / 2 - menu_marker_->height() / 2,
                         menu_marker_->width(),
                         menu_marker_->height());
  arrow_bounds.set_x(GetMirroredXForRect(arrow_bounds));
  canvas->DrawImageInt(*menu_marker_, arrow_bounds.x(), arrow_bounds.y());
}

gfx::Rect MenuButton::GetChildAreaBounds() {
  gfx::Size s = size();

  if (show_menu_marker_) {
    s.set_width(s.width() - menu_marker_->width() - kMenuMarkerPaddingLeft -
                kMenuMarkerPaddingRight);
  }

  return gfx::Rect(s);
}

bool MenuButton::ShouldEnterPushedState(const ui::Event& event) {
  if (event.IsMouseEvent()) {
    const ui::MouseEvent& mouseev = static_cast<const ui::MouseEvent&>(event);
    // Active on left mouse button only, to prevent a menu from being activated
    // when a right-click would also activate a context menu.
    if (!mouseev.IsOnlyLeftMouseButton())
      return false;
    // If dragging is supported activate on release, otherwise activate on
    // pressed.
    ui::EventType active_on =
        GetDragOperations(mouseev.location()) == ui::DragDropTypes::DRAG_NONE
            ? ui::ET_MOUSE_PRESSED
            : ui::ET_MOUSE_RELEASED;
    return event.type() == active_on;
  }

  return event.type() == ui::ET_GESTURE_TAP;
}

void MenuButton::StateChanged() {
  if (pressed_lock_count_ != 0) {
    // The button's state was changed while it was supposed to be locked in a
    // pressed state. This shouldn't happen, but conceivably could if a caller
    // tries to switch from enabled to disabled or vice versa while the button
    // is pressed.
    if (state() == STATE_NORMAL)
      should_disable_after_press_ = false;
    else if (state() == STATE_DISABLED)
      should_disable_after_press_ = true;
  }
}

void MenuButton::IncrementPressedLocked() {
  ++pressed_lock_count_;
  should_disable_after_press_ = state() == STATE_DISABLED;
  SetState(STATE_PRESSED);
}

void MenuButton::DecrementPressedLocked() {
  --pressed_lock_count_;
  DCHECK_GE(pressed_lock_count_, 0);

  // If this was the last lock, manually reset state to the desired state.
  if (pressed_lock_count_ == 0) {
    ButtonState desired_state = STATE_NORMAL;
    if (should_disable_after_press_) {
      desired_state = STATE_DISABLED;
      should_disable_after_press_ = false;
    } else if (IsMouseHovered()) {
      desired_state = STATE_HOVERED;
    }
    SetState(desired_state);
  }
}

int MenuButton::GetMaximumScreenXCoordinate() {
  if (!GetWidget()) {
    NOTREACHED();
    return 0;
  }

  gfx::Rect monitor_bounds = GetWidget()->GetWorkAreaBoundsInScreen();
  return monitor_bounds.right() - 1;
}

}  // namespace views
