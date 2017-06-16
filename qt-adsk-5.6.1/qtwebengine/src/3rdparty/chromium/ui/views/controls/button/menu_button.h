// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"

namespace views {

class MenuButtonListener;

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton
//
//  A button that shows a menu when the left mouse button is pushed
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT MenuButton : public LabelButton {
 public:
  // A scoped lock for keeping the MenuButton in STATE_PRESSED e.g., while a
  // menu is running. These are cumulative.
  class VIEWS_EXPORT PressedLock {
   public:
    explicit PressedLock(MenuButton* menu_button);
    ~PressedLock();

   private:
    base::WeakPtr<MenuButton> menu_button_;

    DISALLOW_COPY_AND_ASSIGN(PressedLock);
  };

  static const char kViewClassName[];

  // How much padding to put on the left and right of the menu marker.
  static const int kMenuMarkerPaddingLeft;
  static const int kMenuMarkerPaddingRight;

  // Create a Button.
  MenuButton(ButtonListener* listener,
             const base::string16& text,
             MenuButtonListener* menu_button_listener,
             bool show_menu_marker);
  ~MenuButton() override;

  bool show_menu_marker() const { return show_menu_marker_; }
  void set_menu_marker(const gfx::ImageSkia* menu_marker) {
    menu_marker_ = menu_marker;
  }
  const gfx::ImageSkia* menu_marker() const { return menu_marker_; }

  const gfx::Point& menu_offset() const { return menu_offset_; }
  void set_menu_offset(int x, int y) { menu_offset_.SetPoint(x, y); }

  // Activate the button (called when the button is pressed).
  virtual bool Activate();

  // Overridden from View:
  gfx::Size GetPreferredSize() const override;
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void GetAccessibleState(ui::AXViewState* state) override;

 protected:
  // Paint the menu marker image.
  void PaintMenuMarker(gfx::Canvas* canvas);

  // Overridden from LabelButton:
  gfx::Rect GetChildAreaBounds() override;

  // Overridden from CustomButton:
  bool ShouldEnterPushedState(const ui::Event& event) override;
  void StateChanged() override;

  // Offset of the associated menu position.
  gfx::Point menu_offset_;

 private:
  friend class PressedLock;

  // Increment/decrement the number of "pressed" locks this button has, and
  // set the state accordingly.
  void IncrementPressedLocked();
  void DecrementPressedLocked();

  // Compute the maximum X coordinate for the current screen. MenuButtons
  // use this to make sure a menu is never shown off screen.
  int GetMaximumScreenXCoordinate();

  // We use a time object in order to keep track of when the menu was closed.
  // The time is used for simulating menu behavior for the menu button; that
  // is, if the menu is shown and the button is pressed, we need to close the
  // menu. There is no clean way to get the second click event because the
  // menu is displayed using a modal loop and, unlike regular menus in Windows,
  // the button is not part of the displayed menu.
  base::TimeTicks menu_closed_time_;

  // Our listener. Not owned.
  MenuButtonListener* listener_;

  // Whether or not we're showing a drop marker.
  bool show_menu_marker_;

  // The down arrow used to differentiate the menu button from normal buttons.
  const gfx::ImageSkia* menu_marker_;

  // If non-null the destuctor sets this to true. This is set while the menu is
  // showing and used to detect if the menu was deleted while running.
  bool* destroyed_flag_;

  // The current number of "pressed" locks this button has.
  int pressed_lock_count_;

  // True if the button was in a disabled state when a menu was run, and should
  // return to it once the press is complete. This can happen if, e.g., we
  // programmatically show a menu on a disabled button.
  bool should_disable_after_press_;

  base::WeakPtrFactory<MenuButton> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MenuButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_
