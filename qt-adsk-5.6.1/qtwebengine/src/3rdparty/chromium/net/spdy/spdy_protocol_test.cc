// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_protocol.h"

#include <limits>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/spdy/spdy_bitmasks.h"
#include "net/spdy/spdy_framer.h"
#include "net/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

enum SpdyProtocolTestTypes {
  SPDY2 = net::SPDY2,
  SPDY3 = net::SPDY3,
};

}  // namespace

namespace net {

class SpdyProtocolTest
    : public ::testing::TestWithParam<SpdyProtocolTestTypes> {
 protected:
  void SetUp() override {
    spdy_version_ = static_cast<SpdyMajorVersion>(GetParam());
  }

  // Version of SPDY protocol to be used.
  SpdyMajorVersion spdy_version_;
};

// All tests are run with two different SPDY versions: SPDY/2 and SPDY/3.
INSTANTIATE_TEST_CASE_P(SpdyProtocolTests,
                        SpdyProtocolTest,
                        ::testing::Values(SPDY2, SPDY3));

class SpdyProtocolDeathTest : public SpdyProtocolTest {};

// All tests are run with two different SPDY versions: SPDY/2 and SPDY/3.
INSTANTIATE_TEST_CASE_P(SpdyProtocolDeathTests,
                        SpdyProtocolDeathTest,
                        ::testing::Values(SPDY2, SPDY3));

TEST_P(SpdyProtocolDeathTest, TestSpdySettingsAndIdOutOfBounds) {
  scoped_ptr<SettingsFlagsAndId> flags_and_id;

  EXPECT_DFATAL(flags_and_id.reset(new SettingsFlagsAndId(1, 0xFFFFFFFF)),
                "SPDY setting ID too large.");
  // Make sure that we get expected values in opt mode.
  if (flags_and_id.get() != NULL) {
    EXPECT_EQ(1, flags_and_id->flags());
    EXPECT_EQ(static_cast<SpdyPingId>(0xffffff), flags_and_id->id());
  }
}

}  // namespace net
