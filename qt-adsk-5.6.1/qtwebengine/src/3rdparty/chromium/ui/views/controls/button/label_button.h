// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/native_theme_delegate.h"

namespace views {

class LabelButtonBorder;
class Painter;

// LabelButton is a button with text and an icon, it's not focusable by default.
class VIEWS_EXPORT LabelButton : public CustomButton,
                                 public NativeThemeDelegate {
 public:
  // The length of the hover fade animation.
  static const int kHoverAnimationDurationMs;

  static const char kViewClassName[];

  LabelButton(ButtonListener* listener, const base::string16& text);
  ~LabelButton() override;

  // Get or set the image shown for the specified button state.
  // GetImage returns the image for STATE_NORMAL if the state's image is empty.
  virtual const gfx::ImageSkia& GetImage(ButtonState for_state);
  void SetImage(ButtonState for_state, const gfx::ImageSkia& image);

  // Get or set the text shown on the button.
  const base::string16& GetText() const;
  virtual void SetText(const base::string16& text);

  // Set the text color shown for the specified button state.
  void SetTextColor(ButtonState for_state, SkColor color);

  // Set drop shadows underneath the text.
  void SetTextShadows(const gfx::ShadowValues& shadows);

  // Sets whether subpixel rendering is used on the label.
  void SetTextSubpixelRenderingEnabled(bool enabled);

  // Get or set the text's multi-line property to break on '\n', etc.
  bool GetTextMultiLine() const;
  void SetTextMultiLine(bool text_multi_line);

  // Get or set the font list used by this button.
  const gfx::FontList& GetFontList() const;
  void SetFontList(const gfx::FontList& font_list);

  // Set the elide behavior of this button.
  void SetElideBehavior(gfx::ElideBehavior elide_behavior);

  // Get or set the horizontal alignment used for the button; reversed in RTL.
  // The optional image will lead the text, unless the button is right-aligned.
  gfx::HorizontalAlignment GetHorizontalAlignment() const;
  void SetHorizontalAlignment(gfx::HorizontalAlignment alignment);

  // Call SetMinSize(gfx::Size()) to clear the monotonically increasing size.
  void SetMinSize(const gfx::Size& min_size);
  void SetMaxSize(const gfx::Size& max_size);

  // Get or set the option to handle the return key; false by default.
  bool is_default() const { return is_default_; }
  void SetIsDefault(bool is_default);

  // Get or set the button's overall style; the default is |STYLE_TEXTBUTTON|.
  ButtonStyle style() const { return style_; }
  void SetStyle(ButtonStyle style);

  // Set the spacing between the image and the text. Shrinking the spacing
  // will not shrink the overall button size, as it is monotonically increasing.
  // Call SetMinSize(gfx::Size()) to clear the size if needed.
  void SetImageLabelSpacing(int spacing);

  void SetFocusPainter(scoped_ptr<Painter> focus_painter);
  Painter* focus_painter() { return focus_painter_.get(); }

  // View:
  void SetBorder(scoped_ptr<Border> border) override;
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int w) const override;
  void Layout() override;
  const char* GetClassName() const override;

 protected:
  ImageView* image() const { return image_; }
  Label* label() const { return label_; }

  // Returns the available area for the label and image. Subclasses can change
  // these bounds if they need room to do manual painting.
  virtual gfx::Rect GetChildAreaBounds();

  // View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

  // Fill |params| with information about the button.
  virtual void GetExtraParams(ui::NativeTheme::ExtraParams* params) const;

  // Resets colors from the NativeTheme, explicitly set colors are unchanged.
  virtual void ResetColorsFromNativeTheme();

  // Creates the default border for this button. This can be overridden by
  // subclasses or by LinuxUI.
  virtual scoped_ptr<LabelButtonBorder> CreateDefaultBorder() const;

  // Updates the image view to contain the appropriate button state image.
  void UpdateImage();

  // Updates the border as per the NativeTheme, unless a different border was
  // set with SetBorder.
  void UpdateThemedBorder();

  // NativeThemeDelegate:
  gfx::Rect GetThemePaintRect() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, Init);
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, Label);
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, Image);
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, LabelAndImage);
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, FontList);

  // CustomButton:
  void StateChanged() override;

  // View:
  void ChildPreferredSizeChanged(View* child) override;

  // NativeThemeDelegate:
  ui::NativeTheme::Part GetThemePart() const override;
  ui::NativeTheme::State GetThemeState(
      ui::NativeTheme::ExtraParams* params) const override;
  const gfx::Animation* GetThemeAnimation() const override;
  ui::NativeTheme::State GetBackgroundThemeState(
      ui::NativeTheme::ExtraParams* params) const override;
  ui::NativeTheme::State GetForegroundThemeState(
      ui::NativeTheme::ExtraParams* params) const override;

  // Resets |cached_preferred_size_| and marks |cached_preferred_size_valid_|
  // as false.
  void ResetCachedPreferredSize();

  // The image and label shown in the button.
  ImageView* image_;
  Label* label_;

  // The cached font lists in the normal and bold style.
  gfx::FontList cached_normal_font_list_;
  gfx::FontList cached_bold_font_list_;

  // The images and colors for each button state.
  gfx::ImageSkia button_state_images_[STATE_COUNT];
  SkColor button_state_colors_[STATE_COUNT];

  // Used to track whether SetTextColor() has been invoked.
  bool explicitly_set_colors_[STATE_COUNT];

  // |min_size_| increases monotonically with the preferred size.
  mutable gfx::Size min_size_;
  // |max_size_| may be set to clamp the preferred size.
  gfx::Size max_size_;

  // Cache the last computed preferred size.
  mutable gfx::Size cached_preferred_size_;
  mutable bool cached_preferred_size_valid_;

  // Flag indicating default handling of the return key via an accelerator.
  // Whether or not the button appears or behaves as the default button in its
  // current context;
  bool is_default_;

  // The button's overall style.
  ButtonStyle style_;

  // True if current border was set by UpdateThemedBorder. Defaults to true.
  bool border_is_themed_border_;

  // Spacing between the image and the text.
  int image_label_spacing_;

  scoped_ptr<Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(LabelButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_
