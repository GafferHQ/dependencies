// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_socket_address_coder.h"

#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace net {
namespace test {

TEST(QuicSocketAddressCoderTest, EncodeIPv4) {
  IPAddressNumber ip;
  ASSERT_TRUE(ParseIPLiteralToNumber("4.31.198.44", &ip));
  QuicSocketAddressCoder coder(IPEndPoint(ip, 0x1234));
  string serialized = coder.Encode();
  string expected("\x02\x00\x04\x1f\xc6\x2c\x34\x12", 8);
  EXPECT_EQ(expected, serialized);
}

TEST(QuicSocketAddressCoderTest, EncodeIPv6) {
  IPAddressNumber ip;
  ASSERT_TRUE(ParseIPLiteralToNumber("2001:700:300:1800::f", &ip));
  QuicSocketAddressCoder coder(IPEndPoint(ip, 0x5678));
  string serialized = coder.Encode();
  string expected("\x0a\x00"
                  "\x20\x01\x07\x00\x03\x00\x18\x00"
                  "\x00\x00\x00\x00\x00\x00\x00\x0f"
                  "\x78\x56", 20);
  EXPECT_EQ(expected, serialized);
}

TEST(QuicSocketAddressCoderTest, DecodeIPv4) {
  string serialized("\x02\x00\x04\x1f\xc6\x2c\x34\x12", 8);
  QuicSocketAddressCoder coder;
  ASSERT_TRUE(coder.Decode(serialized.data(), serialized.length()));
  EXPECT_EQ(AF_INET, ConvertAddressFamily(GetAddressFamily(coder.ip())));
  string expected_addr("\x04\x1f\xc6\x2c", 4);
  EXPECT_EQ(expected_addr, IPAddressToPackedString(coder.ip()));
  EXPECT_EQ(0x1234, coder.port());
}

TEST(QuicSocketAddressCoderTest, DecodeIPv6) {
  string serialized("\x0a\x00"
                    "\x20\x01\x07\x00\x03\x00\x18\x00"
                    "\x00\x00\x00\x00\x00\x00\x00\x0f"
                    "\x78\x56", 20);
  QuicSocketAddressCoder coder;
  ASSERT_TRUE(coder.Decode(serialized.data(), serialized.length()));
  EXPECT_EQ(AF_INET6, ConvertAddressFamily(GetAddressFamily(coder.ip())));
  string expected_addr("\x20\x01\x07\x00\x03\x00\x18\x00"
                       "\x00\x00\x00\x00\x00\x00\x00\x0f", 16);
  EXPECT_EQ(expected_addr, IPAddressToPackedString(coder.ip()));
  EXPECT_EQ(0x5678, coder.port());
}

TEST(QuicSocketAddressCoderTest, DecodeBad) {
  string serialized("\x0a\x00"
                    "\x20\x01\x07\x00\x03\x00\x18\x00"
                    "\x00\x00\x00\x00\x00\x00\x00\x0f"
                    "\x78\x56", 20);
  QuicSocketAddressCoder coder;
  EXPECT_TRUE(coder.Decode(serialized.data(), serialized.length()));
  // Append junk.
  serialized.push_back('\0');
  EXPECT_FALSE(coder.Decode(serialized.data(), serialized.length()));
  // Undo.
  serialized.resize(20);
  EXPECT_TRUE(coder.Decode(serialized.data(), serialized.length()));

  // Set an unknown address family.
  serialized[0] = '\x03';
  EXPECT_FALSE(coder.Decode(serialized.data(), serialized.length()));
  // Undo.
  serialized[0] = '\x0a';
  EXPECT_TRUE(coder.Decode(serialized.data(), serialized.length()));

  // Truncate.
  size_t len = serialized.length();
  for (size_t i = 0; i < len; i++) {
    ASSERT_FALSE(serialized.empty());
    serialized.erase(serialized.length() - 1);
    EXPECT_FALSE(coder.Decode(serialized.data(), serialized.length()));
  }
  EXPECT_TRUE(serialized.empty());
}

TEST(QuicSocketAddressCoderTest, EncodeAndDecode) {
  struct {
    const char* ip_literal;
    uint16 port;
  } test_case[] = {
    { "93.184.216.119", 0x1234 },
    { "199.204.44.194", 80 },
    { "149.20.4.69", 443 },
    { "127.0.0.1", 8080 },
    { "2001:700:300:1800::", 0x5678 },
    { "::1", 65534 },
  };

  for (size_t i = 0; i < arraysize(test_case); i++) {
    IPAddressNumber ip;
    ASSERT_TRUE(ParseIPLiteralToNumber(test_case[i].ip_literal, &ip));
    QuicSocketAddressCoder encoder(IPEndPoint(ip, test_case[i].port));
    string serialized = encoder.Encode();

    QuicSocketAddressCoder decoder;
    ASSERT_TRUE(decoder.Decode(serialized.data(), serialized.length()));
    EXPECT_EQ(encoder.ip(), decoder.ip());
    EXPECT_EQ(encoder.port(), decoder.port());
  }
}

}  // namespace test
}  // namespace net
