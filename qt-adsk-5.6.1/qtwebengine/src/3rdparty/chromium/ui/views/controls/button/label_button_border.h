// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_BORDER_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_BORDER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/painter.h"

namespace views {

// A Border that paints a LabelButton's background frame.
class VIEWS_EXPORT LabelButtonBorder : public Border {
 public:
  explicit LabelButtonBorder(Button::ButtonStyle style);
  ~LabelButtonBorder() override;

  // Returns the default insets for a given |style|.
  static gfx::Insets GetDefaultInsetsForStyle(Button::ButtonStyle style);

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

  void set_insets(const gfx::Insets& insets) { insets_ = insets; }

  // Get or set the painter used for the specified |focused| button |state|.
  // LabelButtonBorder takes and retains ownership of |painter|.
  Painter* GetPainter(bool focused, Button::ButtonState state);
  void SetPainter(bool focused, Button::ButtonState state, Painter* painter);

 private:
  // The painters used for each unfocused or focused button state.
  scoped_ptr<Painter> painters_[2][Button::STATE_COUNT];

  gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(LabelButtonBorder);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_BORDER_H_
