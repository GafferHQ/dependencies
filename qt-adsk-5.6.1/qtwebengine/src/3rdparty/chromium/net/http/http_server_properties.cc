// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_config.h"

namespace net {

const char kAlternateProtocolHeader[] = "Alternate-Protocol";

namespace {

// The order of these strings much match the order of the enum definition
// for AlternateProtocol.
const char* const kAlternateProtocolStrings[] = {
    "npn-spdy/2",
    "npn-spdy/3",
    "npn-spdy/3.1",
    "npn-h2-14",  // HTTP/2 draft-14. Called SPDY4 internally.
    "npn-h2",
    "quic"};

static_assert(arraysize(kAlternateProtocolStrings) ==
                  NUM_VALID_ALTERNATE_PROTOCOLS,
              "kAlternateProtocolStrings has incorrect size");

}  // namespace

void HistogramAlternateProtocolUsage(AlternateProtocolUsage usage) {
  UMA_HISTOGRAM_ENUMERATION("Net.AlternateProtocolUsage", usage,
                            ALTERNATE_PROTOCOL_USAGE_MAX);
}

void HistogramBrokenAlternateProtocolLocation(
    BrokenAlternateProtocolLocation location){
  UMA_HISTOGRAM_ENUMERATION("Net.AlternateProtocolBrokenLocation", location,
                            BROKEN_ALTERNATE_PROTOCOL_LOCATION_MAX);
}

bool IsAlternateProtocolValid(AlternateProtocol protocol) {
  return protocol >= ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION &&
      protocol <= ALTERNATE_PROTOCOL_MAXIMUM_VALID_VERSION;
}

const char* AlternateProtocolToString(AlternateProtocol protocol) {
  switch (protocol) {
    case DEPRECATED_NPN_SPDY_2:
    case NPN_SPDY_3:
    case NPN_SPDY_3_1:
    case NPN_HTTP_2_14:
    case NPN_HTTP_2:
    case QUIC:
      DCHECK(IsAlternateProtocolValid(protocol));
      return kAlternateProtocolStrings[
          protocol - ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION];
    case UNINITIALIZED_ALTERNATE_PROTOCOL:
      return "Uninitialized";
  }
  NOTREACHED();
  return "";
}

AlternateProtocol AlternateProtocolFromString(const std::string& str) {
  for (int i = ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION;
       i <= ALTERNATE_PROTOCOL_MAXIMUM_VALID_VERSION; ++i) {
    AlternateProtocol protocol = static_cast<AlternateProtocol>(i);
    if (str == AlternateProtocolToString(protocol))
      return protocol;
  }
  return UNINITIALIZED_ALTERNATE_PROTOCOL;
}

AlternateProtocol AlternateProtocolFromNextProto(NextProto next_proto) {
  switch (next_proto) {
    case kProtoDeprecatedSPDY2:
      return DEPRECATED_NPN_SPDY_2;
    case kProtoSPDY3:
      return NPN_SPDY_3;
    case kProtoSPDY31:
      return NPN_SPDY_3_1;
    case kProtoHTTP2_14:
      return NPN_HTTP_2_14;
    case kProtoHTTP2:
      return NPN_HTTP_2;
    case kProtoQUIC1SPDY3:
      return QUIC;

    case kProtoUnknown:
    case kProtoHTTP11:
      break;
  }

  NOTREACHED() << "Invalid NextProto: " << next_proto;
  return UNINITIALIZED_ALTERNATE_PROTOCOL;
}

std::string AlternativeService::ToString() const {
  return base::StringPrintf("%s %s:%d", AlternateProtocolToString(protocol),
                            host.c_str(), port);
}

std::string AlternativeServiceInfo::ToString() const {
  return base::StringPrintf("%s, p=%f", alternative_service.ToString().c_str(),
                            probability);
}

// static
void HttpServerProperties::ForceHTTP11(SSLConfig* ssl_config) {
  ssl_config->next_protos.clear();
  ssl_config->next_protos.push_back(kProtoHTTP11);
}

}  // namespace net
