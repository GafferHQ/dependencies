// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PLATFORM_FONT_MAC_H_
#define UI_GFX_PLATFORM_FONT_MAC_H_

#include "base/compiler_specific.h"
#include "base/mac/scoped_nsobject.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/platform_font.h"

namespace gfx {

class PlatformFontMac : public PlatformFont {
 public:
  PlatformFontMac();
  explicit PlatformFontMac(NativeFont native_font);
  PlatformFontMac(const std::string& font_name,
                  int font_size);

  // Overridden from PlatformFont:
  Font DeriveFont(int size_delta, int style) const override;
  int GetHeight() const override;
  int GetBaseline() const override;
  int GetCapHeight() const override;
  int GetExpectedTextWidth(int length) const override;
  int GetStyle() const override;
  std::string GetFontName() const override;
  std::string GetActualFontNameForTesting() const override;
  int GetFontSize() const override;
  const FontRenderParams& GetFontRenderParams() override;
  NativeFont GetNativeFont() const override;

 private:
  PlatformFontMac(const std::string& font_name, int font_size, int font_style);
  ~PlatformFontMac() override;

  // Calculates and caches the font metrics and inits |render_params_|.
  void CalculateMetricsAndInitRenderParams();

  // The NSFont instance for this object. If this object was constructed from an
  // NSFont instance, this holds that NSFont instance. Otherwise this NSFont
  // instance is constructed from the name, size, and style, and if there is no
  // active font that matched those criteria, this object may be nil.
  base::scoped_nsobject<NSFont> native_font_;

  // The name/size/style trio that specify the font. Initialized in the
  // constructors.
  std::string font_name_;  // Corresponds to -[NSFont fontFamily].
  int font_size_;
  int font_style_;

  // Cached metrics, generated in CalculateMetrics().
  int height_;
  int ascent_;
  int cap_height_;
  int average_width_;

  // Details about how the font should be rendered.
  FontRenderParams render_params_;

  DISALLOW_COPY_AND_ASSIGN(PlatformFontMac);
};

}  // namespace gfx

#endif  // UI_GFX_PLATFORM_FONT_MAC_H_
