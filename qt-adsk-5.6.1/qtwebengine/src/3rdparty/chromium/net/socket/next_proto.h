// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_NEXT_PROTO_H_
#define NET_SOCKET_NEXT_PROTO_H_

#include <vector>

#include "net/base/net_export.h"

namespace net {

// Next Protocol Negotiation (NPN), if successful, results in agreement on an
// application-level string that specifies the application level protocol to
// use over the TLS connection. NextProto enumerates the application level
// protocols that we recognize.  Do not change or reuse values, because they
// are used to collect statistics on UMA.  Also, values must be in [0,499),
// because of the way TLS protocol negotiation extension information is added to
// UMA histogram.
enum NextProto {
  kProtoUnknown = 0,
  kProtoHTTP11 = 1,
  kProtoMinimumVersion = kProtoHTTP11,

  kProtoDeprecatedSPDY2 = 100,
  kProtoSPDYMinimumVersion = kProtoDeprecatedSPDY2,
  kProtoSPDYHistogramOffset = kProtoDeprecatedSPDY2,
  kProtoSPDY3 = 101,
  kProtoSPDY31 = 102,
  kProtoHTTP2_14 = 103,  // HTTP/2 draft-14, designated implementation draft.
  kProtoHTTP2MinimumVersion = kProtoHTTP2_14,
  // kProtoHTTP2_15 = 104,  // HTTP/2 draft-15
  // kProtoHTTP2_16 = 105,  // HTTP/2 draft-16
  // kProtoHTTP2_17 = 106,  // HTTP/2 draft-17
  kProtoHTTP2 = 107,  // HTTP/2, see https://tools.ietf.org/html/rfc7540.
  kProtoHTTP2MaximumVersion = kProtoHTTP2,
  kProtoSPDYMaximumVersion = kProtoHTTP2MaximumVersion,

  kProtoQUIC1SPDY3 = 200,

  kProtoMaximumVersion = kProtoQUIC1SPDY3,
};

// List of protocols to use for NPN, used for configuring HttpNetworkSessions.
typedef std::vector<NextProto> NextProtoVector;

// Convenience functions to create NextProtoVector.

// Default values, which are subject to change over time.
NET_EXPORT NextProtoVector NextProtosDefaults();

// Enable SPDY/3.1 and QUIC, but not HTTP/2.
NET_EXPORT NextProtoVector NextProtosSpdy31();

// Control SPDY/3.1 and HTTP/2 separately.
NET_EXPORT NextProtoVector NextProtosWithSpdyAndQuic(bool spdy_enabled,
                                                     bool quic_enabled);

// Returns true if |next_proto| is a version of SPDY or HTTP/2.
bool NextProtoIsSPDY(NextProto next_proto);

}  // namespace net

#endif  // NET_SOCKET_NEXT_PROTO_H_
