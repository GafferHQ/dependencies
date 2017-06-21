// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/stdlib/string_number_conversion.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <limits>

#include "base/logging.h"
#include "util/stdlib/cxx.h"

// CONSTEXPR_STATIC_ASSERT will be a normal static_assert if the C++ library is
// the C++11 library. If using an older C++ library, the
// std::numeric_limits<>::min() and max() functions will not be marked as
// constexpr, and thus won’t be usable with static_assert(). In that case, a
// run-time CHECK() will have to do.
#if CXX_LIBRARY_HAS_CONSTEXPR
#define CONSTEXPR_STATIC_ASSERT(condition, message) \
  static_assert(condition, message)
#else
#define CONSTEXPR_STATIC_ASSERT(condition, message) CHECK(condition) << message
#endif

namespace {

template <typename TIntType, typename TLongType>
struct StringToIntegerTraits {
  using IntType = TIntType;
  using LongType = TLongType;
  static void TypeCheck() {
    static_assert(std::numeric_limits<TIntType>::is_integer &&
                      std::numeric_limits<TLongType>::is_integer,
                  "IntType and LongType must be integer");
    static_assert(std::numeric_limits<TIntType>::is_signed ==
                      std::numeric_limits<TLongType>::is_signed,
                  "IntType and LongType signedness must agree");
    CONSTEXPR_STATIC_ASSERT(std::numeric_limits<TIntType>::min() >=
                                    std::numeric_limits<TLongType>::min() &&
                                std::numeric_limits<TIntType>::min() <
                                    std::numeric_limits<TLongType>::max(),
                            "IntType min must be in LongType range");
    CONSTEXPR_STATIC_ASSERT(std::numeric_limits<TIntType>::max() >
                                    std::numeric_limits<TLongType>::min() &&
                                std::numeric_limits<TIntType>::max() <=
                                    std::numeric_limits<TLongType>::max(),
                            "IntType max must be in LongType range");
  }
};

template <typename TIntType, typename TLongType>
struct StringToSignedIntegerTraits
    : public StringToIntegerTraits<TIntType, TLongType> {
  static void TypeCheck() {
    static_assert(std::numeric_limits<TIntType>::is_signed,
                  "StringToSignedTraits IntType must be signed");
    return super::TypeCheck();
  }
  static bool IsNegativeOverflow(TLongType value) {
    return value < std::numeric_limits<TIntType>::min();
  }

 private:
  using super = StringToIntegerTraits<TIntType, TLongType>;
};

template <typename TIntType, typename TLongType>
struct StringToUnsignedIntegerTraits
    : public StringToIntegerTraits<TIntType, TLongType> {
  static void TypeCheck() {
    static_assert(!std::numeric_limits<TIntType>::is_signed,
                  "StringToUnsignedTraits IntType must be unsigned");
    return super::TypeCheck();
  }
  static bool IsNegativeOverflow(TLongType value) { return false; }

 private:
  using super = StringToIntegerTraits<TIntType, TLongType>;
};

struct StringToIntTraits : public StringToSignedIntegerTraits<int, long> {
  static LongType Convert(const char* str, char** end, int base) {
    return strtol(str, end, base);
  }
};

struct StringToUnsignedIntTraits
    : public StringToUnsignedIntegerTraits<unsigned int, unsigned long> {
  static LongType Convert(const char* str, char** end, int base) {
    if (str[0] == '-') {
      return 0;
    }
    return strtoul(str, end, base);
  }
};

template <typename Traits>
bool StringToIntegerInternal(const base::StringPiece& string,
                             typename Traits::IntType* number) {
  using IntType = typename Traits::IntType;
  using LongType = typename Traits::LongType;

  Traits::TypeCheck();

  if (string.empty() || isspace(string[0])) {
    return false;
  }

  if (string[string.length()] != '\0') {
    // The implementations use the C standard library’s conversion routines,
    // which rely on the strings having a trailing NUL character. std::string
    // will NUL-terminate.
    std::string terminated_string(string.data(), string.length());
    return StringToIntegerInternal<Traits>(terminated_string, number);
  }

  errno = 0;
  char* end;
  LongType result = Traits::Convert(string.data(), &end, 0);
  if (Traits::IsNegativeOverflow(result) ||
      result > std::numeric_limits<IntType>::max() ||
      errno == ERANGE ||
      end != string.data() + string.length()) {
    return false;
  }
  *number = result;
  return true;
}

}  // namespace

namespace crashpad {

bool StringToNumber(const base::StringPiece& string, int* number) {
  return StringToIntegerInternal<StringToIntTraits>(string, number);
}

bool StringToNumber(const base::StringPiece& string, unsigned int* number) {
  return StringToIntegerInternal<StringToUnsignedIntTraits>(string, number);
}

}  // namespace crashpad
