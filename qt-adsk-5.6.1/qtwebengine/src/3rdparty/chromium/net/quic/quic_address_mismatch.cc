// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_address_mismatch.h"

#include "base/logging.h"
#include "net/base/ip_endpoint.h"

namespace net {

int GetAddressMismatch(const IPEndPoint& first_address,
                       const IPEndPoint& second_address) {
  if (first_address.address().empty() || second_address.address().empty()) {
    return -1;
  }
  IPAddressNumber first_ip_address = first_address.address();
  if (IsIPv4Mapped(first_ip_address)) {
    first_ip_address = ConvertIPv4MappedToIPv4(first_ip_address);
  }
  IPAddressNumber second_ip_address = second_address.address();
  if (IsIPv4Mapped(second_ip_address)) {
    second_ip_address = ConvertIPv4MappedToIPv4(second_ip_address);
  }

  int sample;
  if (first_ip_address != second_ip_address) {
    sample = QUIC_ADDRESS_MISMATCH_BASE;
  } else if (first_address.port() != second_address.port()) {
    sample = QUIC_PORT_MISMATCH_BASE;
  } else {
    sample = QUIC_ADDRESS_AND_PORT_MATCH_BASE;
  }

  // Add an offset to |sample|:
  //   V4_V4: add 0
  //   V6_V6: add 1
  //   V4_V6: add 2
  //   V6_V4: add 3
  bool first_ipv4 = (first_ip_address.size() == kIPv4AddressSize);
  bool second_ipv4 = (second_ip_address.size() == kIPv4AddressSize);
  if (first_ipv4 != second_ipv4) {
    CHECK_EQ(sample, QUIC_ADDRESS_MISMATCH_BASE);
    sample += 2;
  }
  if (!first_ipv4) {
    sample += 1;
  }
  return sample;
}

}  // namespace net
