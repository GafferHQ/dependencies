// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_SPDY_BALSA_UTILS_H_
#define NET_TOOLS_QUIC_SPDY_BALSA_UTILS_H_

#include <string>

#include "net/quic/quic_protocol.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_protocol.h"
#include "net/tools/balsa/balsa_headers.h"

namespace net {
namespace tools {

class SpdyBalsaUtils {
 public:
  static std::string SerializeResponseHeaders(
      const BalsaHeaders& response_headers,
      QuicVersion quic_version);

  static SpdyHeaderBlock RequestHeadersToSpdyHeaders(
      const BalsaHeaders& request_headers,
      QuicVersion quic_version);

  static SpdyHeaderBlock ResponseHeadersToSpdyHeaders(
      const BalsaHeaders& response_headers,
      QuicVersion quic_version);

  static void SpdyHeadersToResponseHeaders(const SpdyHeaderBlock& block,
                                           BalsaHeaders* headers,
                                           QuicVersion quic_version);

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyBalsaUtils);
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_SPDY_BALSA_UTILS_H_
