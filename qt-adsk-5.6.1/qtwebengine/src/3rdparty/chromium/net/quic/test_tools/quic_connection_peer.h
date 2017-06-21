// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_CONNECTION_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_CONNECTION_PEER_H_

#include "base/basictypes.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_connection_stats.h"
#include "net/quic/quic_protocol.h"

namespace net {

struct QuicAckFrame;
struct QuicPacketHeader;
class QuicAlarm;
class QuicConnection;
class QuicConnectionHelperInterface;
class QuicConnectionVisitorInterface;
class QuicEncryptedPacket;
class QuicFecGroup;
class QuicFramer;
class QuicPacketCreator;
class QuicPacketGenerator;
class QuicPacketWriter;
class QuicReceivedPacketManager;
class QuicSentPacketManager;
class SendAlgorithmInterface;

namespace test {

// Peer to make public a number of otherwise private QuicConnection methods.
class QuicConnectionPeer {
 public:
  static void SendAck(QuicConnection* connection);

  static void SetSendAlgorithm(QuicConnection* connection,
                               SendAlgorithmInterface* send_algorithm);

  static void PopulateAckFrame(QuicConnection* connection, QuicAckFrame* ack);

  static void PopulateStopWaitingFrame(QuicConnection* connection,
                                       QuicStopWaitingFrame* stop_waiting);

  static QuicConnectionVisitorInterface* GetVisitor(
      QuicConnection* connection);

  static QuicPacketCreator* GetPacketCreator(QuicConnection* connection);

  static QuicPacketGenerator* GetPacketGenerator(QuicConnection* connection);

  static QuicSentPacketManager* GetSentPacketManager(
      QuicConnection* connection);

  static QuicTime::Delta GetNetworkTimeout(QuicConnection* connection);

  static QuicPacketEntropyHash GetSentEntropyHash(
      QuicConnection* connection,
      QuicPacketSequenceNumber sequence_number);

  static QuicPacketEntropyHash PacketEntropy(
      QuicConnection* connection,
      QuicPacketSequenceNumber sequence_number);

  static QuicPacketEntropyHash ReceivedEntropyHash(
      QuicConnection* connection,
      QuicPacketSequenceNumber sequence_number);

  static void SetPerspective(QuicConnection* connection,
                             Perspective perspective);

  static void SetSelfAddress(QuicConnection* connection,
                             const IPEndPoint& self_address);

  static void SetPeerAddress(QuicConnection* connection,
                             const IPEndPoint& peer_address);

  static bool IsSilentCloseEnabled(QuicConnection* connection);

  static void SwapCrypters(QuicConnection* connection, QuicFramer* framer);

  static QuicConnectionHelperInterface* GetHelper(QuicConnection* connection);

  static QuicFramer* GetFramer(QuicConnection* connection);

  // Set last_header_->fec_group = fec_group and return connection->GetFecGroup
  static QuicFecGroup* GetFecGroup(QuicConnection* connection, int fec_group);

  static QuicAlarm* GetAckAlarm(QuicConnection* connection);
  static QuicAlarm* GetPingAlarm(QuicConnection* connection);
  static QuicAlarm* GetFecAlarm(QuicConnection* connection);
  static QuicAlarm* GetResumeWritesAlarm(QuicConnection* connection);
  static QuicAlarm* GetRetransmissionAlarm(QuicConnection* connection);
  static QuicAlarm* GetSendAlarm(QuicConnection* connection);
  static QuicAlarm* GetTimeoutAlarm(QuicConnection* connection);
  static QuicAlarm* GetMtuDiscoveryAlarm(QuicConnection* connection);

  static QuicPacketWriter* GetWriter(QuicConnection* connection);
  // If |owns_writer| is true, takes ownership of |writer|.
  static void SetWriter(QuicConnection* connection,
                        QuicPacketWriter* writer,
                        bool owns_writer);
  static void CloseConnection(QuicConnection* connection);
  static QuicEncryptedPacket* GetConnectionClosePacket(
      QuicConnection* connection);

  static QuicPacketHeader* GetLastHeader(QuicConnection* connection);

  static void SetSequenceNumberOfLastSentPacket(
      QuicConnection* connection, QuicPacketSequenceNumber number);

  static QuicConnectionStats* GetStats(QuicConnection* connection);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicConnectionPeer);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_CONNECTION_PEER_H_
