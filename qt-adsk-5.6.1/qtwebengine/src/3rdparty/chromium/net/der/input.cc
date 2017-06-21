// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/logging.h"
#include "net/der/input.h"

namespace net {

namespace der {

Input::Input() : data_(nullptr), len_(0) {
}

Input::Input(const uint8_t* data, size_t len) : data_(data), len_(len) {
}

bool Input::Equals(const Input& other) const {
  if (len_ != other.len_)
    return false;
  return memcmp(data_, other.data_, len_) == 0;
}

ByteReader::ByteReader(const Input& in)
    : data_(in.UnsafeData()), len_(in.Length()) {
}

bool ByteReader::ReadByte(uint8_t* byte_p) {
  if (!HasMore())
    return false;
  *byte_p = *data_;
  Advance(1);
  return true;
}

bool ByteReader::ReadBytes(size_t len, Input* out) {
  if (len > len_)
    return false;
  *out = Input(data_, len);
  Advance(len);
  return true;
}

// Returns whether there is any more data to be read.
bool ByteReader::HasMore() {
  return len_ > 0;
}

Mark ByteReader::NewMark() {
  return Mark(data_);
}

bool ByteReader::AdvanceToMark(Mark mark) {
  if (mark.ptr_ < data_)
    return false;
  // mark.ptr_ >= data_, so no concern of integer underflow here.
  size_t advance_len = mark.ptr_ - data_;
  if (advance_len > len_)
    return false;
  Advance(advance_len);
  return true;
}

bool ByteReader::ReadToMark(Mark mark, Input* out) {
  if (mark.ptr_ < data_)
    return false;
  // mark.ptr_ >= data_, so no concern of integer underflow here.
  size_t len = mark.ptr_ - data_;
  return ReadBytes(len, out);
}

void ByteReader::Advance(size_t len) {
  CHECK_LE(len, len_);
  data_ += len;
  len_ -= len;
}

Mark Mark::NullMark() {
  return Mark();
}

bool Mark::IsEmpty() {
  return ptr_ == nullptr;
}

Mark::Mark(const uint8_t* ptr) : ptr_(ptr) {
}

Mark::Mark() : ptr_(nullptr) {
}

}  // namespace der

}  // namespace net
