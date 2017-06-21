// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font.h"

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "ui/gfx/platform_font_win.h"
#endif

namespace gfx {
namespace {

using FontTest = testing::Test;

#if defined(OS_WIN)
class ScopedMinimumFontSizeCallback {
 public:
  explicit ScopedMinimumFontSizeCallback(int minimum_size) {
    minimum_size_ = minimum_size;
    old_callback_ = PlatformFontWin::get_minimum_font_size_callback;
    PlatformFontWin::get_minimum_font_size_callback = &GetMinimumFontSize;
  }

  ~ScopedMinimumFontSizeCallback() {
    PlatformFontWin::get_minimum_font_size_callback = old_callback_;
  }

 private:
  static int GetMinimumFontSize() {
    return minimum_size_;
  }

  PlatformFontWin::GetMinimumFontSizeCallback old_callback_;
  static int minimum_size_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMinimumFontSizeCallback);
};

int ScopedMinimumFontSizeCallback::minimum_size_ = 0;
#endif  // defined(OS_WIN)


TEST_F(FontTest, LoadArial) {
  Font cf("Arial", 16);
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_IOS)
  EXPECT_TRUE(cf.GetNativeFont());
#endif
  EXPECT_EQ(cf.GetStyle(), Font::NORMAL);
  EXPECT_EQ(cf.GetFontSize(), 16);
  EXPECT_EQ(cf.GetFontName(), "Arial");
  EXPECT_EQ("arial",
            base::StringToLowerASCII(cf.GetActualFontNameForTesting()));
}

TEST_F(FontTest, LoadArialBold) {
  Font cf("Arial", 16);
  Font bold(cf.Derive(0, Font::BOLD));
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_IOS)
  EXPECT_TRUE(bold.GetNativeFont());
#endif
  EXPECT_EQ(bold.GetStyle(), Font::BOLD);
  EXPECT_EQ("arial",
            base::StringToLowerASCII(cf.GetActualFontNameForTesting()));
}

TEST_F(FontTest, Ascent) {
  Font cf("Arial", 16);
  EXPECT_GT(cf.GetBaseline(), 2);
  EXPECT_LE(cf.GetBaseline(), 22);
}

TEST_F(FontTest, Height) {
  Font cf("Arial", 16);
  EXPECT_GE(cf.GetHeight(), 16);
  // TODO(akalin): Figure out why height is so large on Linux.
  EXPECT_LE(cf.GetHeight(), 26);
}

TEST_F(FontTest, CapHeight) {
  Font cf("Arial", 16);
  EXPECT_GT(cf.GetCapHeight(), 0);
  EXPECT_GT(cf.GetCapHeight(), cf.GetHeight() / 2);
  EXPECT_LT(cf.GetCapHeight(), cf.GetBaseline());
}

TEST_F(FontTest, AvgWidths) {
  Font cf("Arial", 16);
  EXPECT_EQ(cf.GetExpectedTextWidth(0), 0);
  EXPECT_GT(cf.GetExpectedTextWidth(1), cf.GetExpectedTextWidth(0));
  EXPECT_GT(cf.GetExpectedTextWidth(2), cf.GetExpectedTextWidth(1));
  EXPECT_GT(cf.GetExpectedTextWidth(3), cf.GetExpectedTextWidth(2));
}

#if !defined(OS_WIN)
// On Windows, Font::GetActualFontNameForTesting() doesn't work well for now.
// http://crbug.com/327287
//
// Check that fonts used for testing are installed and enabled. On Mac
// fonts may be installed but still need enabling in Font Book.app.
// http://crbug.com/347429
TEST_F(FontTest, GetActualFontNameForTesting) {
  Font arial("Arial", 16);
  EXPECT_EQ("arial",
            base::StringToLowerASCII(arial.GetActualFontNameForTesting()))
      << "********\n"
      << "Your test environment seems to be missing Arial font, which is "
      << "needed for unittests.  Check if Arial font is installed.\n"
      << "********";
  Font symbol("Symbol", 16);
  EXPECT_EQ("symbol",
            base::StringToLowerASCII(symbol.GetActualFontNameForTesting()))
      << "********\n"
      << "Your test environment seems to be missing Symbol font, which is "
      << "needed for unittests.  Check if Symbol font is installed.\n"
      << "********";

  const char* const invalid_font_name = "no_such_font_name";
  Font fallback_font(invalid_font_name, 16);
  EXPECT_NE(invalid_font_name,
            base::StringToLowerASCII(
                fallback_font.GetActualFontNameForTesting()));
}
#endif

#if defined(OS_WIN)
TEST_F(FontTest, DeriveResizesIfSizeTooSmall) {
  Font cf("Arial", 8);
  // The minimum font size is set to 5 in browser_main.cc.
  ScopedMinimumFontSizeCallback minimum_size(5);

  Font derived_font = cf.Derive(-4, cf.GetStyle());
  EXPECT_EQ(5, derived_font.GetFontSize());
}

TEST_F(FontTest, DeriveKeepsOriginalSizeIfHeightOk) {
  Font cf("Arial", 8);
  // The minimum font size is set to 5 in browser_main.cc.
  ScopedMinimumFontSizeCallback minimum_size(5);

  Font derived_font = cf.Derive(-2, cf.GetStyle());
  EXPECT_EQ(6, derived_font.GetFontSize());
}
#endif  // defined(OS_WIN)

}  // namespace
}  // namespace gfx
