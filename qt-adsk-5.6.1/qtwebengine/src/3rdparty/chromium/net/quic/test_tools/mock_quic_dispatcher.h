// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_MOCK_QUIC_DISPATCHER_H_
#define NET_QUIC_TEST_TOOLS_MOCK_QUIC_DISPATCHER_H_

#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/quic/quic_dispatcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {
namespace test {

class MockQuicDispatcher : public tools::QuicDispatcher {
 public:
  MockQuicDispatcher(const QuicConfig& config,
                     const QuicCryptoServerConfig* crypto_config,
                     PacketWriterFactory* packet_writer_factory,
                     QuicConnectionHelperInterface* helper);

  ~MockQuicDispatcher() override;

  MOCK_METHOD3(ProcessPacket, void(const IPEndPoint& server_address,
                                   const IPEndPoint& client_address,
                                   const QuicEncryptedPacket& packet));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockQuicDispatcher);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_MOCK_QUIC_DISPATCHER_H_
