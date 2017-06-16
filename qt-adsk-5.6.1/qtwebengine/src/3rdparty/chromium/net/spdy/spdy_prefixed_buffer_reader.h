// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_PREFIXED_BUFFER_READER_H_
#define NET_SPDY_SPDY_PREFIXED_BUFFER_READER_H_

#include "net/base/net_export.h"
#include "net/spdy/spdy_pinnable_buffer_piece.h"

namespace net {

// Reader class which simplifies reading contiguously from
// from a disjoint buffer prefix & suffix.
class NET_EXPORT_PRIVATE SpdyPrefixedBufferReader {
 public:
  SpdyPrefixedBufferReader(const char* prefix, size_t prefix_length,
                           const char* suffix, size_t suffix_length);

  // Returns number of bytes available to be read.
  size_t Available();

  // Reads |count| bytes, copying into |*out|. Returns true on success,
  // false if not enough bytes were available.
  bool ReadN(size_t count, char* out);

  // Reads |count| bytes, returned in |*out|. Returns true on success,
  // false if not enough bytes were available.
  bool ReadN(size_t count, SpdyPinnableBufferPiece* out);

 private:
  const char* prefix_;
  const char* suffix_;

  size_t prefix_length_;
  size_t suffix_length_;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_PREFIXED_BUFFER_READER_H_
