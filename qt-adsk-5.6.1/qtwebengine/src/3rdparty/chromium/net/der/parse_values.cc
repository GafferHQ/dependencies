// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "net/der/parse_values.h"

namespace net {

namespace der {

namespace {

bool ParseBoolInternal(const Input& in, bool* out, bool relaxed) {
  // According to ITU-T X.690 section 8.2, a bool is encoded as a single octet
  // where the octet of all zeroes is FALSE and a non-zero value for the octet
  // is TRUE.
  if (in.Length() != 1)
    return false;
  ByteReader data(in);
  uint8_t byte;
  if (!data.ReadByte(&byte))
    return false;
  if (byte == 0) {
    *out = false;
    return true;
  }
  // ITU-T X.690 section 11.1 specifies that for DER, the TRUE value must be
  // encoded as an octet of all ones.
  if (byte == 0xff || relaxed) {
    *out = true;
    return true;
  }
  return false;
}

// Reads a positive decimal number with |digits| digits and stores it in
// |*out|. This function does not check that the type of |*out| is large
// enough to hold 10^digits - 1; the caller must choose an appropriate type
// based on the number of digits they wish to parse.
template <typename UINT>
bool DecimalStringToUint(ByteReader& in, size_t digits, UINT* out) {
  UINT value = 0;
  for (size_t i = 0; i < digits; ++i) {
    uint8_t digit;
    if (!in.ReadByte(&digit)) {
      return false;
    }
    if (digit < '0' || digit > '9') {
      return false;
    }
    value = (value * 10) + (digit - '0');
  }
  *out = value;
  return true;
}

// Checks that the values in a GeneralizedTime struct are valid. This involves
// checking that the year is 4 digits, the month is between 1 and 12, the day
// is a day that exists in that month (following current leap year rules),
// hours are between 0 and 23, minutes between 0 and 59, and seconds between
// 0 and 60 (to allow for leap seconds; no validation is done that a leap
// second is on a day that could be a leap second).
bool ValidateGeneralizedTime(const GeneralizedTime& time) {
  CHECK(time.year > 0 && time.year < 9999);
  if (time.month < 1 || time.month > 12)
    return false;
  if (time.day < 1)
    return false;
  if (time.hours < 0 || time.hours > 23)
    return false;
  if (time.minutes < 0 || time.minutes > 59)
    return false;
  // Leap seconds are allowed.
  if (time.seconds < 0 || time.seconds > 60)
    return false;

  // validate upper bound for day of month
  switch (time.month) {
    case 4:
    case 6:
    case 9:
    case 11:
      if (time.day > 30)
        return false;
      break;
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      if (time.day > 31)
        return false;
      break;
    case 2:
      if (time.year % 4 == 0 &&
          (time.year % 100 != 0 || time.year % 400 == 0)) {
        if (time.day > 29)
          return false;
      } else {
        if (time.day > 28)
          return false;
      }
      break;
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

}  // namespace

bool ParseBool(const Input& in, bool* out) {
  return ParseBoolInternal(in, out, false /* relaxed */);
}

// BER interprets any non-zero value as true, while DER requires a bool to
// have either all bits zero (false) or all bits one (true). To support
// malformed certs, we recognized the BER encoding instead of failing to
// parse.
bool ParseBoolRelaxed(const Input& in, bool* out) {
  return ParseBoolInternal(in, out, true /* relaxed */);
}

bool ParseUint64(const Input& in, uint64_t* out) {
  ByteReader reader(in);
  size_t bytes_read = 0;
  uint8_t data;
  uint64_t value = 0;
  // Note that for simplicity, this check only admits integers up to 2^63-1.
  if (in.Length() > sizeof(uint64_t) || in.Length() == 0)
    return false;
  while (reader.ReadByte(&data)) {
    if (bytes_read == 0 && (data & 0x80)) {
      return false;
    }
    value <<= 8;
    value |= data;
    bytes_read++;
  }
  // ITU-T X.690 section 8.3.2 specifies that an integer value must be encoded
  // in the smallest number of octets. If the encoding consists of more than
  // one octet, then the bits of the first octet and the most significant bit
  // of the second octet must not be all zeroes or all ones.
  // Because this function only parses unsigned integers, there's no need to
  // check for the all ones case.
  if (bytes_read > 1) {
    ByteReader first_bytes_reader(in);
    uint8_t first_byte;
    uint8_t second_byte;
    if (!first_bytes_reader.ReadByte(&first_byte) ||
        !first_bytes_reader.ReadByte(&second_byte)) {
      return false;
    }
    if (first_byte == 0 && !(second_byte & 0x80)) {
      return false;
    }
  }
  *out = value;
  return true;
}

bool operator<(const GeneralizedTime& lhs, const GeneralizedTime& rhs) {
  if (lhs.year != rhs.year)
    return lhs.year < rhs.year;
  if (lhs.month != rhs.month)
    return lhs.month < rhs.month;
  if (lhs.day != rhs.day)
    return lhs.day < rhs.day;
  if (lhs.hours != rhs.hours)
    return lhs.hours < rhs.hours;
  if (lhs.minutes != rhs.minutes)
    return lhs.minutes < rhs.minutes;
  return lhs.seconds < rhs.seconds;
}

// A UTC Time in DER encoding should be YYMMDDHHMMSSZ, but some CAs encode
// the time following BER rules, which allows for YYMMDDHHMMZ. If the length
// is 11, assume it's YYMMDDHHMMZ, and in converting it to a GeneralizedTime,
// add in the seconds (set to 0).
bool ParseUTCTimeRelaxed(const Input& in, GeneralizedTime* value) {
  ByteReader reader(in);
  GeneralizedTime time;
  if (!DecimalStringToUint(reader, 2, &time.year) ||
      !DecimalStringToUint(reader, 2, &time.month) ||
      !DecimalStringToUint(reader, 2, &time.day) ||
      !DecimalStringToUint(reader, 2, &time.hours) ||
      !DecimalStringToUint(reader, 2, &time.minutes)) {
    return false;
  }

  // Try to read the 'Z' at the end. If we read something else, then for it to
  // be valid the next bytes should be seconds (and then followed by 'Z').
  uint8_t zulu;
  ByteReader zulu_reader = reader;
  if (!zulu_reader.ReadByte(&zulu))
    return false;
  if (zulu == 'Z' && !zulu_reader.HasMore()) {
    time.seconds = 0;
    *value = time;
    return true;
  }
  if (!DecimalStringToUint(reader, 2, &time.seconds))
    return false;
  if (!reader.ReadByte(&zulu) || zulu != 'Z' || reader.HasMore())
    return false;
  if (!ValidateGeneralizedTime(time))
    return false;
  if (time.year < 50) {
    time.year += 2000;
  } else {
    time.year += 1900;
  }
  *value = time;
  return true;
}

bool ParseUTCTime(const Input& in, GeneralizedTime* value) {
  ByteReader reader(in);
  GeneralizedTime time;
  if (!DecimalStringToUint(reader, 2, &time.year) ||
      !DecimalStringToUint(reader, 2, &time.month) ||
      !DecimalStringToUint(reader, 2, &time.day) ||
      !DecimalStringToUint(reader, 2, &time.hours) ||
      !DecimalStringToUint(reader, 2, &time.minutes) ||
      !DecimalStringToUint(reader, 2, &time.seconds)) {
    return false;
  }
  uint8_t zulu;
  if (!reader.ReadByte(&zulu) || zulu != 'Z' || reader.HasMore())
    return false;
  if (!ValidateGeneralizedTime(time))
    return false;
  if (time.year < 50) {
    time.year += 2000;
  } else {
    time.year += 1900;
  }
  *value = time;
  return true;
}

bool ParseGeneralizedTime(const Input& in, GeneralizedTime* value) {
  ByteReader reader(in);
  GeneralizedTime time;
  if (!DecimalStringToUint(reader, 4, &time.year) ||
      !DecimalStringToUint(reader, 2, &time.month) ||
      !DecimalStringToUint(reader, 2, &time.day) ||
      !DecimalStringToUint(reader, 2, &time.hours) ||
      !DecimalStringToUint(reader, 2, &time.minutes) ||
      !DecimalStringToUint(reader, 2, &time.seconds)) {
    return false;
  }
  uint8_t zulu;
  if (!reader.ReadByte(&zulu) || zulu != 'Z' || reader.HasMore())
    return false;
  if (!ValidateGeneralizedTime(time))
    return false;
  *value = time;
  return true;
}

}  // namespace der

}  // namespace net
