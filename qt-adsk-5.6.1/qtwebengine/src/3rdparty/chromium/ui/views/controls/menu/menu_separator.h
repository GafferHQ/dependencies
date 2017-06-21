// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_SEPARATOR_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_SEPARATOR_H_

#include "base/compiler_specific.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/views/view.h"

namespace views {

class MenuItemView;

class MenuSeparator : public View {
 public:
  MenuSeparator(MenuItemView* parent, ui::MenuSeparatorType type)
      : type_(type),
        parent_menu_item_(parent) {}

  // View overrides.
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size GetPreferredSize() const override;

 private:
  // Gets the bounds where the separator should be painted.
  gfx::Rect GetPaintBounds();

  void OnPaintAura(gfx::Canvas* canvas);

  // The type of the separator.
  const ui::MenuSeparatorType type_;

  // Our parent.
  MenuItemView* parent_menu_item_;

  DISALLOW_COPY_AND_ASSIGN(MenuSeparator);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_SEPARATOR_H_
