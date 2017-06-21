// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/size.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_IOS)
#include <CoreGraphics/CoreGraphics.h>
#elif defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif

#include "base/strings/stringprintf.h"

namespace gfx {

#if defined(OS_MACOSX)
Size::Size(const CGSize& s)
    : width_(s.width < 0 ? 0 : s.width),
      height_(s.height < 0 ? 0 : s.height) {
}

Size& Size::operator=(const CGSize& s) {
  set_width(s.width);
  set_height(s.height);
  return *this;
}
#endif

#if defined(OS_WIN)
SIZE Size::ToSIZE() const {
  SIZE s;
  s.cx = width();
  s.cy = height();
  return s;
}
#elif defined(OS_MACOSX)
CGSize Size::ToCGSize() const {
  return CGSizeMake(width(), height());
}
#endif

int Size::GetArea() const {
  return width() * height();
}

void Size::Enlarge(int grow_width, int grow_height) {
  SetSize(width() + grow_width, height() + grow_height);
}

void Size::SetToMin(const Size& other) {
  width_ = width() <= other.width() ? width() : other.width();
  height_ = height() <= other.height() ? height() : other.height();
}

void Size::SetToMax(const Size& other) {
  width_ = width() >= other.width() ? width() : other.width();
  height_ = height() >= other.height() ? height() : other.height();
}

std::string Size::ToString() const {
  return base::StringPrintf("%dx%d", width(), height());
}

}  // namespace gfx
