// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_COMMON_TYPE_CONVERTERS_H_
#define MOJO_COMMON_COMMON_TYPE_CONVERTERS_H_

#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "mojo/common/mojo_common_export.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/string.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

template <>
struct MOJO_COMMON_EXPORT TypeConverter<String, base::StringPiece> {
  static String Convert(const base::StringPiece& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<base::StringPiece, String> {
  static base::StringPiece Convert(const String& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<String, base::string16> {
  static String Convert(const base::string16& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<base::string16, String> {
  static base::string16 Convert(const String& input);
};

// TODO(erg): In the very long term, we will want to remove conversion between
// std::strings and arrays of unsigned bytes. However, there is too much code
// across chrome which uses std::string as a bag of bytes that we probably
// don't want to roll this function at each callsite.
template <>
struct MOJO_COMMON_EXPORT TypeConverter<std::string, Array<uint8_t> > {
  static std::string Convert(const Array<uint8_t>& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<Array<uint8_t>, std::string> {
  static Array<uint8_t> Convert(const std::string& input);
};

}  // namespace mojo

#endif  // MOJO_COMMON_COMMON_TYPE_CONVERTERS_H_
