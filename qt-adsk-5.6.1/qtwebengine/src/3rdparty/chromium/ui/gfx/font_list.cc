// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_list.h"

#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/gfx/font_list_impl.h"

namespace {

// Font description of the default font set.
base::LazyInstance<std::string>::Leaky g_default_font_description =
    LAZY_INSTANCE_INITIALIZER;

// The default instance of gfx::FontListImpl.
base::LazyInstance<scoped_refptr<gfx::FontListImpl> >::Leaky g_default_impl =
    LAZY_INSTANCE_INITIALIZER;
bool g_default_impl_initialized = false;

}  // namespace

namespace gfx {

// static
bool FontList::ParseDescription(const std::string& description,
                                std::vector<std::string>* families_out,
                                int* style_out,
                                int* size_pixels_out) {
  DCHECK(families_out);
  DCHECK(style_out);
  DCHECK(size_pixels_out);

  base::SplitString(description, ',', families_out);
  if (families_out->empty())
    return false;
  for (auto& family : *families_out)
    base::TrimWhitespaceASCII(family, base::TRIM_ALL, &family);

  // The last item is "[STYLE1] [STYLE2] [...] SIZE".
  std::vector<std::string> styles;
  base::SplitStringAlongWhitespace(families_out->back(), &styles);
  families_out->pop_back();
  if (styles.empty())
    return false;

  // The size takes the form "<INT>px".
  std::string size_string = styles.back();
  styles.pop_back();
  if (!base::EndsWith(size_string, "px", true /* case_sensitive */))
    return false;
  size_string.resize(size_string.size() - 2);
  if (!base::StringToInt(size_string, size_pixels_out) ||
      *size_pixels_out <= 0)
    return false;

  // Font supports BOLD and ITALIC; underline is supported via RenderText.
  *style_out = gfx::Font::NORMAL;
  for (const auto& style_string : styles) {
    if (style_string == "Bold")
      *style_out |= gfx::Font::BOLD;
    else if (style_string == "Italic")
      *style_out |= gfx::Font::ITALIC;
    else
      return false;
  }

  return true;
}

FontList::FontList() : impl_(GetDefaultImpl()) {}

FontList::FontList(const FontList& other) : impl_(other.impl_) {}

FontList::FontList(const std::string& font_description_string)
    : impl_(new FontListImpl(font_description_string)) {}

FontList::FontList(const std::vector<std::string>& font_names,
                   int font_style,
                   int font_size)
    : impl_(new FontListImpl(font_names, font_style, font_size)) {}

FontList::FontList(const std::vector<Font>& fonts)
    : impl_(new FontListImpl(fonts)) {}

FontList::FontList(const Font& font) : impl_(new FontListImpl(font)) {}

FontList::~FontList() {}

FontList& FontList::operator=(const FontList& other) {
  impl_ = other.impl_;
  return *this;
}

// static
void FontList::SetDefaultFontDescription(const std::string& font_description) {
  // The description string must end with "px" for size in pixel, or must be
  // the empty string, which specifies to use a single default font.
  DCHECK(font_description.empty() ||
         base::EndsWith(font_description, "px", true));

  g_default_font_description.Get() = font_description;
  g_default_impl_initialized = false;
}

FontList FontList::Derive(int size_delta, int font_style) const {
  return FontList(impl_->Derive(size_delta, font_style));
}

FontList FontList::DeriveWithSizeDelta(int size_delta) const {
  return Derive(size_delta, GetFontStyle());
}

FontList FontList::DeriveWithStyle(int font_style) const {
  return Derive(0, font_style);
}

gfx::FontList FontList::DeriveWithHeightUpperBound(int height) const {
  gfx::FontList font_list(*this);
  for (int font_size = font_list.GetFontSize(); font_size > 1; --font_size) {
    const int internal_leading =
        font_list.GetBaseline() - font_list.GetCapHeight();
    // Some platforms don't support getting the cap height, and simply return
    // the entire font ascent from GetCapHeight().  Centering the ascent makes
    // the font look too low, so if GetCapHeight() returns the ascent, center
    // the entire font height instead.
    const int space =
        height - ((internal_leading != 0) ?
                  font_list.GetCapHeight() : font_list.GetHeight());
    const int y_offset = space / 2 - internal_leading;
    const int space_at_bottom = height - (y_offset + font_list.GetHeight());
    if ((y_offset >= 0) && (space_at_bottom >= 0))
      break;
    font_list = font_list.DeriveWithSizeDelta(-1);
  }
  return font_list;
}

int FontList::GetHeight() const {
  return impl_->GetHeight();
}

int FontList::GetBaseline() const {
  return impl_->GetBaseline();
}

int FontList::GetCapHeight() const {
  return impl_->GetCapHeight();
}

int FontList::GetExpectedTextWidth(int length) const {
  return impl_->GetExpectedTextWidth(length);
}

int FontList::GetFontStyle() const {
  return impl_->GetFontStyle();
}

int FontList::GetFontSize() const {
  return impl_->GetFontSize();
}

const std::vector<Font>& FontList::GetFonts() const {
  return impl_->GetFonts();
}

const Font& FontList::GetPrimaryFont() const {
  return impl_->GetPrimaryFont();
}

FontList::FontList(FontListImpl* impl) : impl_(impl) {}

// static
const scoped_refptr<FontListImpl>& FontList::GetDefaultImpl() {
  // SetDefaultFontDescription() must be called and the default font description
  // must be set earlier than any call of this function.
  DCHECK(!(g_default_font_description == NULL))  // != is not overloaded.
      << "SetDefaultFontDescription has not been called.";

  if (!g_default_impl_initialized) {
    g_default_impl.Get() =
        g_default_font_description.Get().empty() ?
            new FontListImpl(Font()) :
            new FontListImpl(g_default_font_description.Get());
    g_default_impl_initialized = true;
  }

  return g_default_impl.Get();
}

}  // namespace gfx
