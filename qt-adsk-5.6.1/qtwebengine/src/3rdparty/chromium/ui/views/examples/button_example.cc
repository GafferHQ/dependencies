// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/button_example.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;

namespace {
const char kLabelButton[] = "Label Button";
const char kMultiLineText[] = "Multi-Line\nButton Text Is Here To Stay!\n123";
const char kLongText[] = "Start of Really Really Really Really Really Really "
                         "Really Really Really Really Really Really Really "
                         "Really Really Really Really Really Long Button Text";
}  // namespace

namespace views {
namespace examples {

ButtonExample::ButtonExample()
    : ExampleBase("Button"),
      label_button_(NULL),
      image_button_(NULL),
      icon_(NULL),
      count_(0) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  icon_ = rb.GetImageNamed(IDR_CLOSE_H).ToImageSkia();
}

ButtonExample::~ButtonExample() {
}

void ButtonExample::CreateExampleView(View* container) {
  container->set_background(Background::CreateSolidBackground(SK_ColorWHITE));
  container->SetLayoutManager(new BoxLayout(BoxLayout::kVertical, 10, 10, 10));

  label_button_ = new LabelButton(this, ASCIIToUTF16(kLabelButton));
  label_button_->SetFocusable(true);
  container->AddChildView(label_button_);

  LabelButton* disabled_button =
      new LabelButton(this, ASCIIToUTF16("Disabled Button"));
  disabled_button->SetStyle(Button::STYLE_BUTTON);
  disabled_button->SetState(Button::STATE_DISABLED);
  container->AddChildView(disabled_button);

  container->AddChildView(new BlueButton(this, ASCIIToUTF16("Blue Button")));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  image_button_ = new ImageButton(this);
  image_button_->SetFocusable(true);
  image_button_->SetImage(ImageButton::STATE_NORMAL,
                          rb.GetImageNamed(IDR_CLOSE).ToImageSkia());
  image_button_->SetImage(ImageButton::STATE_HOVERED,
                          rb.GetImageNamed(IDR_CLOSE_H).ToImageSkia());
  image_button_->SetImage(ImageButton::STATE_PRESSED,
                          rb.GetImageNamed(IDR_CLOSE_P).ToImageSkia());
  container->AddChildView(image_button_);
}

void ButtonExample::LabelButtonPressed(const ui::Event& event) {
  PrintStatus("Label Button Pressed! count: %d", ++count_);
  if (event.IsControlDown()) {
    if (event.IsShiftDown()) {
      if (event.IsAltDown()) {
        label_button_->SetTextMultiLine(!label_button_->GetTextMultiLine());
        label_button_->SetText(ASCIIToUTF16(
            label_button_->GetTextMultiLine() ? kMultiLineText : kLabelButton));
      } else {
        label_button_->SetText(ASCIIToUTF16(
            label_button_->GetText().empty() ? kLongText :
                label_button_->GetText().length() > 50 ? kLabelButton : ""));
      }
    } else if (event.IsAltDown()) {
      label_button_->SetImage(Button::STATE_NORMAL,
          label_button_->GetImage(Button::STATE_NORMAL).isNull() ?
          *icon_ : gfx::ImageSkia());
    } else {
      label_button_->SetHorizontalAlignment(
          static_cast<gfx::HorizontalAlignment>(
              (label_button_->GetHorizontalAlignment() + 1) % 3));
    }
  } else if (event.IsShiftDown()) {
    if (event.IsAltDown()) {
      label_button_->SetFocusable(!label_button_->IsFocusable());
    } else {
      label_button_->SetStyle(static_cast<Button::ButtonStyle>(
          (label_button_->style() + 1) % Button::STYLE_COUNT));
    }
  } else if (event.IsAltDown()) {
    label_button_->SetIsDefault(!label_button_->is_default());
  } else {
    label_button_->SetMinSize(gfx::Size());
  }
  example_view()->GetLayoutManager()->Layout(example_view());
}

void ButtonExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == label_button_)
    LabelButtonPressed(event);
  else
    PrintStatus("Image Button Pressed! count: %d", ++count_);
}

}  // namespace examples
}  // namespace views
