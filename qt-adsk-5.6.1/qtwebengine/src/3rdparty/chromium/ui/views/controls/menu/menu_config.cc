// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "build/build_config.h"

namespace views {

MenuConfig::MenuConfig(const ui::NativeTheme* theme)
    : arrow_color(SK_ColorBLACK),
      menu_vertical_border_size(3),
      menu_horizontal_border_size(0),
      submenu_horizontal_inset(3),
      item_top_margin(4),
      item_bottom_margin(3),
      item_no_icon_top_margin(4),
      item_no_icon_bottom_margin(4),
      item_left_margin(10),
      label_to_arrow_padding(10),
      arrow_to_edge_padding(5),
      icon_to_label_padding(10),
      gutter_to_label(5),
      check_width(16),
      check_height(16),
      radio_width(16),
      arrow_width(9),
      separator_height(11),
      separator_upper_height(3),
      separator_lower_height(4),
      separator_spacing_height(3),
      show_mnemonics(false),
      scroll_arrow_height(3),
      label_to_minor_text_padding(10),
      item_min_height(0),
      show_accelerators(true),
      always_use_icon_to_label_padding(false),
      align_arrow_and_shortcut(false),
      offset_context_menus(false),
      native_theme(theme),
      show_delay(400),
      corner_radius(0) {
  Init(theme);
}

MenuConfig::~MenuConfig() {}

}  // namespace views
