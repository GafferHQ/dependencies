// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_connection_peer.h"

#include "base/stl_util.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_received_packet_manager.h"
#include "net/quic/test_tools/quic_framer_peer.h"
#include "net/quic/test_tools/quic_packet_generator_peer.h"
#include "net/quic/test_tools/quic_sent_packet_manager_peer.h"

namespace net {
namespace test {

// static
void QuicConnectionPeer::SendAck(QuicConnection* connection) {
  connection->SendAck();
}

// static
void QuicConnectionPeer::SetSendAlgorithm(
    QuicConnection* connection,
    SendAlgorithmInterface* send_algorithm) {
  connection->sent_packet_manager_.send_algorithm_.reset(send_algorithm);
}

// static
void QuicConnectionPeer::PopulateAckFrame(QuicConnection* connection,
                                          QuicAckFrame* ack) {
  connection->PopulateAckFrame(ack);
}

// static
void QuicConnectionPeer::PopulateStopWaitingFrame(
    QuicConnection* connection,
    QuicStopWaitingFrame* stop_waiting) {
  connection->PopulateStopWaitingFrame(stop_waiting);
}

// static
QuicConnectionVisitorInterface* QuicConnectionPeer::GetVisitor(
    QuicConnection* connection) {
  return connection->visitor_;
}

// static
QuicPacketCreator* QuicConnectionPeer::GetPacketCreator(
    QuicConnection* connection) {
  return QuicPacketGeneratorPeer::GetPacketCreator(
      &connection->packet_generator_);
}

// static
QuicPacketGenerator* QuicConnectionPeer::GetPacketGenerator(
    QuicConnection* connection) {
  return &connection->packet_generator_;
}

// static
QuicSentPacketManager* QuicConnectionPeer::GetSentPacketManager(
    QuicConnection* connection) {
  return &connection->sent_packet_manager_;
}

// static
QuicTime::Delta QuicConnectionPeer::GetNetworkTimeout(
    QuicConnection* connection) {
  return connection->idle_network_timeout_;
}

// static
// TODO(ianswett): Create a GetSentEntropyHash which accepts an AckFrame.
QuicPacketEntropyHash QuicConnectionPeer::GetSentEntropyHash(
    QuicConnection* connection,
    QuicPacketSequenceNumber sequence_number) {
  QuicSentEntropyManager::CumulativeEntropy last_entropy_copy =
      connection->sent_entropy_manager_.last_cumulative_entropy_;
  connection->sent_entropy_manager_.UpdateCumulativeEntropy(sequence_number,
                                                            &last_entropy_copy);
  return last_entropy_copy.entropy;
}

// static
QuicPacketEntropyHash QuicConnectionPeer::PacketEntropy(
    QuicConnection* connection,
    QuicPacketSequenceNumber sequence_number) {
  return connection->sent_entropy_manager_.GetPacketEntropy(sequence_number);
}

// static
QuicPacketEntropyHash QuicConnectionPeer::ReceivedEntropyHash(
    QuicConnection* connection,
    QuicPacketSequenceNumber sequence_number) {
  return connection->received_packet_manager_.EntropyHash(
      sequence_number);
}

// static
void QuicConnectionPeer::SetPerspective(QuicConnection* connection,
                                        Perspective perspective) {
  connection->perspective_ = perspective;
  QuicFramerPeer::SetPerspective(&connection->framer_, perspective);
}

// static
void QuicConnectionPeer::SetSelfAddress(QuicConnection* connection,
                                        const IPEndPoint& self_address) {
  connection->self_address_ = self_address;
}

// static
void QuicConnectionPeer::SetPeerAddress(QuicConnection* connection,
                                        const IPEndPoint& peer_address) {
  connection->peer_address_ = peer_address;
}

// static
bool QuicConnectionPeer::IsSilentCloseEnabled(QuicConnection* connection) {
  return connection->silent_close_enabled_;
}

// static
void QuicConnectionPeer::SwapCrypters(QuicConnection* connection,
                                      QuicFramer* framer) {
  QuicFramerPeer::SwapCrypters(framer, &connection->framer_);
}

// static
QuicConnectionHelperInterface* QuicConnectionPeer::GetHelper(
    QuicConnection* connection) {
  return connection->helper_;
}

// static
QuicFramer* QuicConnectionPeer::GetFramer(QuicConnection* connection) {
  return &connection->framer_;
}

// static
QuicFecGroup* QuicConnectionPeer::GetFecGroup(QuicConnection* connection,
                                              int fec_group) {
  connection->last_header_.fec_group = fec_group;
  return connection->GetFecGroup();
}

// static
QuicAlarm* QuicConnectionPeer::GetAckAlarm(QuicConnection* connection) {
  return connection->ack_alarm_.get();
}

// static
QuicAlarm* QuicConnectionPeer::GetPingAlarm(QuicConnection* connection) {
  return connection->ping_alarm_.get();
}

// static
QuicAlarm* QuicConnectionPeer::GetFecAlarm(QuicConnection* connection) {
  return connection->fec_alarm_.get();
}

// static
QuicAlarm* QuicConnectionPeer::GetResumeWritesAlarm(
    QuicConnection* connection) {
  return connection->resume_writes_alarm_.get();
}

// static
QuicAlarm* QuicConnectionPeer::GetRetransmissionAlarm(
    QuicConnection* connection) {
  return connection->retransmission_alarm_.get();
}

// static
QuicAlarm* QuicConnectionPeer::GetSendAlarm(QuicConnection* connection) {
  return connection->send_alarm_.get();
}

// static
QuicAlarm* QuicConnectionPeer::GetTimeoutAlarm(QuicConnection* connection) {
  return connection->timeout_alarm_.get();
}

// static
QuicAlarm* QuicConnectionPeer::GetMtuDiscoveryAlarm(
    QuicConnection* connection) {
  return connection->mtu_discovery_alarm_.get();
}

// static
QuicPacketWriter* QuicConnectionPeer::GetWriter(QuicConnection* connection) {
  return connection->writer_;
}

// static
void QuicConnectionPeer::SetWriter(QuicConnection* connection,
                                   QuicPacketWriter* writer,
                                   bool owns_writer) {
  if (connection->owns_writer_) {
    delete connection->writer_;
  }
  connection->writer_ = writer;
  connection->owns_writer_ = owns_writer;
}

// static
void QuicConnectionPeer::CloseConnection(QuicConnection* connection) {
  connection->connected_ = false;
}

// static
QuicEncryptedPacket* QuicConnectionPeer::GetConnectionClosePacket(
    QuicConnection* connection) {
  return connection->connection_close_packet_.get();
}

// static
QuicPacketHeader* QuicConnectionPeer::GetLastHeader(
    QuicConnection* connection) {
  return &connection->last_header_;
}

// static
void QuicConnectionPeer::SetSequenceNumberOfLastSentPacket(
    QuicConnection* connection, QuicPacketSequenceNumber number) {
  connection->sequence_number_of_last_sent_packet_ = number;
}

// static
QuicConnectionStats* QuicConnectionPeer::GetStats(QuicConnection* connection) {
  return &connection->stats_;
}

}  // namespace test
}  // namespace net
