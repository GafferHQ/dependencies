// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_LINK_H_
#define UI_VIEWS_CONTROLS_LINK_H_

#include <string>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/label.h"

namespace views {

class LinkListener;

////////////////////////////////////////////////////////////////////////////////
//
// Link class
//
// A Link is a label subclass that looks like an HTML link. It has a
// controller which is notified when a click occurs.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT Link : public Label {
 public:
  Link();
  explicit Link(const base::string16& title);
  ~Link() override;

  static SkColor GetDefaultEnabledColor();

  const LinkListener* listener() { return listener_; }
  void set_listener(LinkListener* listener) { listener_ = listener; }

  // Label:
  const char* GetClassName() const override;
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;
  bool CanProcessEventsWithinSubtree() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void GetAccessibleState(ui::AXViewState* state) override;
  void OnEnabledChanged() override;
  void OnFocus() override;
  void OnBlur() override;
  void SetFontList(const gfx::FontList& font_list) override;
  void SetText(const base::string16& text) override;
  void SetEnabledColor(SkColor color) override;

  void SetPressedColor(SkColor color);
  void SetUnderline(bool underline);

  static const char kViewClassName[];

 private:
  void Init();

  void SetPressed(bool pressed);

  void RecalculateFont();

  LinkListener* listener_;

  // Whether the link should be underlined when enabled.
  bool underline_;

  // Whether the link is currently pressed.
  bool pressed_;

  // The color when the link is neither pressed nor disabled.
  SkColor requested_enabled_color_;

  // The color when the link is pressed.
  SkColor requested_pressed_color_;

  DISALLOW_COPY_AND_ASSIGN(Link);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_LINK_H_
