// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/hash_value.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace net {

namespace {

// CompareSHA1Hashes is a helper function for using bsearch() with an array of
// SHA1 hashes.
int CompareSHA1Hashes(const void* a, const void* b) {
  return memcmp(a, b, base::kSHA1Length);
}

}  // namespace


bool SHA1HashValue::Equals(const SHA1HashValue& other) const {
  return memcmp(data, other.data, sizeof(data)) == 0;
}

bool SHA256HashValue::Equals(const SHA256HashValue& other) const {
  return memcmp(data, other.data, sizeof(data)) == 0;
}

bool HashValue::Equals(const HashValue& other) const {
  if (tag != other.tag)
    return false;
  switch (tag) {
    case HASH_VALUE_SHA1:
      return fingerprint.sha1.Equals(other.fingerprint.sha1);
    case HASH_VALUE_SHA256:
      return fingerprint.sha256.Equals(other.fingerprint.sha256);
    default:
      NOTREACHED() << "Unknown HashValueTag " << tag;
      return false;
  }
}

bool HashValue::FromString(const base::StringPiece value) {
  base::StringPiece base64_str;
  if (value.starts_with("sha1/")) {
    tag = HASH_VALUE_SHA1;
    base64_str = value.substr(5);
  } else if (value.starts_with("sha256/")) {
    tag = HASH_VALUE_SHA256;
    base64_str = value.substr(7);
  } else {
    return false;
  }

  std::string decoded;
  if (!base::Base64Decode(base64_str, &decoded) || decoded.size() != size())
    return false;

  memcpy(data(), decoded.data(), size());
  return true;
}

std::string HashValue::ToString() const {
  std::string base64_str;
  base::Base64Encode(base::StringPiece(reinterpret_cast<const char*>(data()),
                                       size()), &base64_str);
  switch (tag) {
  case HASH_VALUE_SHA1:
    return std::string("sha1/") + base64_str;
  case HASH_VALUE_SHA256:
    return std::string("sha256/") + base64_str;
  default:
    NOTREACHED() << "Unknown HashValueTag " << tag;
    return std::string("unknown/" + base64_str);
  }
}

size_t HashValue::size() const {
  switch (tag) {
    case HASH_VALUE_SHA1:
      return sizeof(fingerprint.sha1.data);
    case HASH_VALUE_SHA256:
      return sizeof(fingerprint.sha256.data);
    default:
      NOTREACHED() << "Unknown HashValueTag " << tag;
      // While an invalid tag should not happen, return a non-zero length
      // to avoid compiler warnings when the result of size() is
      // used with functions like memset.
      return sizeof(fingerprint.sha1.data);
  }
}

unsigned char* HashValue::data() {
  return const_cast<unsigned char*>(const_cast<const HashValue*>(this)->data());
}

const unsigned char* HashValue::data() const {
  switch (tag) {
    case HASH_VALUE_SHA1:
      return fingerprint.sha1.data;
    case HASH_VALUE_SHA256:
      return fingerprint.sha256.data;
    default:
      NOTREACHED() << "Unknown HashValueTag " << tag;
      return NULL;
  }
}

bool IsSHA1HashInSortedArray(const SHA1HashValue& hash,
                             const uint8_t* array,
                             size_t array_byte_len) {
  DCHECK_EQ(0u, array_byte_len % base::kSHA1Length);
  const size_t arraylen = array_byte_len / base::kSHA1Length;
  return NULL != bsearch(hash.data, array, arraylen, base::kSHA1Length,
                         CompareSHA1Hashes);
}

}  // namespace net
