// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common utilities for Quic tests

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_

#include <string>

#include "base/strings/string_piece.h"
#include "net/quic/proto/cached_network_parameters.pb.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_session.h"
#include "net/spdy/spdy_framer.h"
#include "net/tools/quic/quic_dispatcher.h"
#include "net/tools/quic/quic_per_connection_packet_writer.h"
#include "net/tools/quic/quic_server_session.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

class EpollServer;
class IPEndPoint;

namespace tools {
namespace test {

static const QuicConnectionId kTestConnectionId = 42;
static const uint16 kTestPort = 123;
static const uint32 kInitialStreamFlowControlWindowForTest =
    1024 * 1024;  // 1 MB
static const uint32 kInitialSessionFlowControlWindowForTest =
    1536 * 1024;  // 1.5 MB

// Testing convenience method to construct a QuicAckFrame with |num_nack_ranges|
// nack ranges of width 1 packet, starting from |least_unacked|.
QuicAckFrame MakeAckFrameWithNackRanges(size_t num_nack_ranges,
                                        QuicPacketSequenceNumber least_unacked);

class MockPacketWriter : public QuicPacketWriter {
 public:
  MockPacketWriter();
  virtual ~MockPacketWriter();

  MOCK_METHOD4(WritePacket,
               WriteResult(const char* buffer,
                           size_t buf_len,
                           const IPAddressNumber& self_address,
                           const IPEndPoint& peer_address));
  MOCK_CONST_METHOD0(IsWriteBlockedDataBuffered, bool());
  MOCK_CONST_METHOD0(IsWriteBlocked, bool());
  MOCK_METHOD0(SetWritable, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPacketWriter);
};

class MockQuicServerSessionVisitor : public QuicServerSessionVisitor {
 public:
  MockQuicServerSessionVisitor();
  virtual ~MockQuicServerSessionVisitor();
  MOCK_METHOD2(OnConnectionClosed, void(QuicConnectionId connection_id,
                                        QuicErrorCode error));
  MOCK_METHOD1(OnWriteBlocked,
               void(QuicBlockedWriterInterface* blocked_writer));
  MOCK_METHOD1(OnConnectionAddedToTimeWaitList,
               void(QuicConnectionId connection_id));
  MOCK_METHOD1(OnConnectionRemovedFromTimeWaitList,
               void(QuicConnectionId connection_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockQuicServerSessionVisitor);
};

class MockAckNotifierDelegate : public QuicAckNotifier::DelegateInterface {
 public:
  MockAckNotifierDelegate();

  MOCK_METHOD3(OnAckNotification,
               void(int num_retransmitted_packets,
                    int num_retransmitted_bytes,
                    QuicTime::Delta delta_largest_observed));

 protected:
  // Object is ref counted.
  virtual ~MockAckNotifierDelegate();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAckNotifierDelegate);
};

// Creates per-connection packet writers that register themselves with the
// TestWriterFactory on each write so that TestWriterFactory::OnPacketSent can
// be routed to the appropriate QuicConnection.
class TestWriterFactory : public QuicDispatcher::PacketWriterFactory {
 public:
  TestWriterFactory();
  ~TestWriterFactory() override;

  QuicPacketWriter* Create(QuicPacketWriter* writer,
                           QuicConnection* connection) override;

  // Calls OnPacketSent on the last QuicConnection to write through one of the
  // packet writers created by this factory.
  void OnPacketSent(WriteResult result);

 private:
  class PerConnectionPacketWriter : public QuicPerConnectionPacketWriter {
   public:
    PerConnectionPacketWriter(TestWriterFactory* factory,
                              QuicPacketWriter* writer,
                              QuicConnection* connection);
    ~PerConnectionPacketWriter() override;

    WriteResult WritePacket(const char* buffer,
                            size_t buf_len,
                            const IPAddressNumber& self_address,
                            const IPEndPoint& peer_address) override;

   private:
    TestWriterFactory* factory_;
  };

  // If an asynchronous write is happening and |writer| gets deleted, this
  // clears the pointer to it to prevent use-after-free.
  void Unregister(PerConnectionPacketWriter* writer);

  PerConnectionPacketWriter* current_writer_;
};

class MockTimeWaitListManager : public QuicTimeWaitListManager {
 public:
  MockTimeWaitListManager(QuicPacketWriter* writer,
                          QuicServerSessionVisitor* visitor,
                          QuicConnectionHelperInterface* helper);
  ~MockTimeWaitListManager() override;

  MOCK_METHOD4(AddConnectionIdToTimeWait,
               void(QuicConnectionId connection_id,
                    QuicVersion version,
                    bool connection_rejected_statelessly,
                    QuicEncryptedPacket* close_packet));

  void QuicTimeWaitListManager_AddConnectionIdToTimeWait(
      QuicConnectionId connection_id,
      QuicVersion version,
      bool connection_rejected_statelessly,
      QuicEncryptedPacket* close_packet) {
    QuicTimeWaitListManager::AddConnectionIdToTimeWait(
        connection_id, version, connection_rejected_statelessly, close_packet);
  }

  MOCK_METHOD5(ProcessPacket,
               void(const IPEndPoint& server_address,
                    const IPEndPoint& client_address,
                    QuicConnectionId connection_id,
                    QuicPacketSequenceNumber sequence_number,
                    const QuicEncryptedPacket& packet));
};

}  // namespace test
}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
