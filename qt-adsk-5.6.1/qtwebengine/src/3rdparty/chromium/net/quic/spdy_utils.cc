// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/spdy_utils.h"

#include "base/memory/scoped_ptr.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"

using std::string;

namespace net {

// static
SpdyMajorVersion SpdyUtils::GetSpdyVersionForQuicVersion(
    QuicVersion quic_version) {
  if (quic_version > QUIC_VERSION_24) {
    return HTTP2;
  }
  return SPDY3;
}

// static
SpdyHeaderBlock SpdyUtils::ConvertSpdy3ResponseHeadersToSpdy4(
    SpdyHeaderBlock response_headers) {
  // SPDY/4 headers include neither the version field nor the response details.
  response_headers.erase(":version");
  size_t end_of_code = response_headers[":status"].find(' ');
  if (end_of_code != string::npos) {
    response_headers[":status"].erase(end_of_code);
  }
  return response_headers;
}

// static
string SpdyUtils::SerializeUncompressedHeaders(const SpdyHeaderBlock& headers,
                                               QuicVersion quic_version) {
  SpdyMajorVersion spdy_version = GetSpdyVersionForQuicVersion(quic_version);

  size_t length = SpdyFramer::GetSerializedLength(spdy_version, &headers);
  SpdyFrameBuilder builder(length, spdy_version);
  SpdyFramer::WriteHeaderBlock(&builder, spdy_version, &headers);
  scoped_ptr<SpdyFrame> block(builder.take());
  return string(block->data(), length);
}

}  // namespace net
