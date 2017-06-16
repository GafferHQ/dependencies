// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/mach/child_port_server.h"

#include <string.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/mach/mach_extensions.h"

namespace crashpad {
namespace test {
namespace {

using testing::Eq;
using testing::Pointee;
using testing::Return;

// Fake Mach ports. These aren’t used as ports in these tests, they’re just used
// as cookies to make sure that the correct values get passed to the correct
// places.
const mach_port_t kServerLocalPort = 0x05050505;
const mach_port_t kCheckInPort = 0x06060606;

// Other fake values.
const mach_msg_type_name_t kCheckInPortRightType = MACH_MSG_TYPE_PORT_SEND;
const child_port_token_t kCheckInToken = 0xfedcba9876543210;

// The definition of the request structure from child_port.h isn’t available
// here. It needs custom initialization code, so duplicate the expected
// definition of the structure from child_port.h here in this file, and provide
// the initialization code as a method in true object-oriented fashion.

struct __attribute__((packed, aligned(4))) ChildPortCheckInRequest {
  ChildPortCheckInRequest() {
    memset(this, 0xa5, sizeof(*this));
    Head.msgh_bits =
        MACH_MSGH_BITS(0, MACH_MSG_TYPE_PORT_SEND) | MACH_MSGH_BITS_COMPLEX;
    Head.msgh_size = sizeof(*this) - sizeof(trailer);
    Head.msgh_remote_port = MACH_PORT_NULL;
    Head.msgh_local_port = kServerLocalPort;
    Head.msgh_id = 10011;
    msgh_body.msgh_descriptor_count = 1;
    port.name = kCheckInPort;
    port.disposition = kCheckInPortRightType;
    port.type = MACH_MSG_PORT_DESCRIPTOR;
    NDR = NDR_record;
    token = kCheckInToken;
  }

  mach_msg_header_t Head;
  mach_msg_body_t msgh_body;
  mach_msg_port_descriptor_t port;
  NDR_record_t NDR;
  child_port_token_t token;
  mach_msg_trailer_t trailer;
};

struct MIGReply : public mig_reply_error_t {
  MIGReply() {
    memset(this, 0x5a, sizeof(*this));
    RetCode = KERN_FAILURE;
  }

  void Verify() {
    EXPECT_EQ(implicit_cast<mach_msg_bits_t>(MACH_MSGH_BITS(0, 0)),
              Head.msgh_bits);
    EXPECT_EQ(sizeof(*this), Head.msgh_size);
    EXPECT_EQ(kMachPortNull, Head.msgh_remote_port);
    EXPECT_EQ(kMachPortNull, Head.msgh_local_port);
    EXPECT_EQ(10111, Head.msgh_id);
    EXPECT_EQ(0, memcmp(&NDR, &NDR_record, sizeof(NDR)));
    EXPECT_EQ(MIG_NO_REPLY, RetCode);
  }
};

class MockChildPortServerInterface : public ChildPortServer::Interface {
 public:
  MOCK_METHOD6(HandleChildPortCheckIn,
               kern_return_t(child_port_server_t server,
                             const child_port_token_t token,
                             mach_port_t port,
                             mach_msg_type_name_t right_type,
                             const mach_msg_trailer_t* trailer,
                             bool* destroy_request));
};

TEST(ChildPortServer, MockChildPortCheckIn) {
  MockChildPortServerInterface server_interface;
  ChildPortServer server(&server_interface);

  std::set<mach_msg_id_t> expect_request_ids;
  expect_request_ids.insert(10011);  // There is no constant for this.
  EXPECT_EQ(expect_request_ids, server.MachMessageServerRequestIDs());

  ChildPortCheckInRequest request;
  EXPECT_EQ(request.Head.msgh_size, server.MachMessageServerRequestSize());

  MIGReply reply;
  EXPECT_EQ(sizeof(reply), server.MachMessageServerReplySize());

  EXPECT_CALL(server_interface,
              HandleChildPortCheckIn(kServerLocalPort,
                                     kCheckInToken,
                                     kCheckInPort,
                                     kCheckInPortRightType,
                                     Eq(&request.trailer),
                                     Pointee(Eq(false))))
      .WillOnce(Return(MIG_NO_REPLY))
      .RetiresOnSaturation();

  bool destroy_request = false;
  EXPECT_TRUE(server.MachMessageServerFunction(
      reinterpret_cast<mach_msg_header_t*>(&request),
      reinterpret_cast<mach_msg_header_t*>(&reply),
      &destroy_request));
  EXPECT_FALSE(destroy_request);

  reply.Verify();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
