// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_dispatcher.h"

#include <utility>

#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_utils.h"
#include "net/tools/quic/quic_per_connection_packet_writer.h"
#include "net/tools/quic/quic_time_wait_list_manager.h"

namespace net {

namespace tools {

using std::make_pair;
using base::StringPiece;

// The threshold size for the session map, over which the dispatcher will start
// sending stateless rejects (SREJ), rather than stateful rejects (REJ) to
// clients who support them.  If -1, stateless rejects will not be sent.  If 0,
// the server will only send stateless rejects to clients who support them.
int32 FLAGS_quic_session_map_threshold_for_stateless_rejects = -1;

namespace {

// An alarm that informs the QuicDispatcher to delete old sessions.
class DeleteSessionsAlarm : public QuicAlarm::Delegate {
 public:
  explicit DeleteSessionsAlarm(QuicDispatcher* dispatcher)
      : dispatcher_(dispatcher) {
  }

  QuicTime OnAlarm() override {
    dispatcher_->DeleteSessions();
    // Let the dispatcher register the alarm at appropriate time.
    return QuicTime::Zero();
  }

 private:
  // Not owned.
  QuicDispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(DeleteSessionsAlarm);
};

}  // namespace

class QuicDispatcher::QuicFramerVisitor : public QuicFramerVisitorInterface {
 public:
  explicit QuicFramerVisitor(QuicDispatcher* dispatcher)
      : dispatcher_(dispatcher),
        connection_id_(0) {}

  // QuicFramerVisitorInterface implementation
  void OnPacket() override {}
  bool OnUnauthenticatedPublicHeader(
      const QuicPacketPublicHeader& header) override {
    connection_id_ = header.connection_id;
    return dispatcher_->OnUnauthenticatedPublicHeader(header);
  }
  bool OnUnauthenticatedHeader(const QuicPacketHeader& header) override {
    dispatcher_->OnUnauthenticatedHeader(header);
    return false;
  }
  void OnError(QuicFramer* framer) override {
    QuicErrorCode error = framer->error();
    dispatcher_->SetLastError(error);
    DVLOG(1) << QuicUtils::ErrorToString(error);
  }

  bool OnProtocolVersionMismatch(QuicVersion /*received_version*/) override {
    DVLOG(1) << "Version mismatch, connection ID " << connection_id_;
    // Keep processing after protocol mismatch - this will be dealt with by the
    // time wait list or connection that we will create.
    return true;
  }

  // The following methods should never get called because
  // OnUnauthenticatedPublicHeader() or OnUnauthenticatedHeader() (whichever was
  // called last), will return false and prevent a subsequent invocation of
  // these methods.  Thus, the payload of the packet is never processed in the
  // dispatcher.
  void OnPublicResetPacket(const QuicPublicResetPacket& /*packet*/) override {
    DCHECK(false);
  }
  void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& /*packet*/) override {
    DCHECK(false);
  }
  void OnDecryptedPacket(EncryptionLevel level) override { DCHECK(false); }
  bool OnPacketHeader(const QuicPacketHeader& /*header*/) override {
    DCHECK(false);
    return false;
  }
  void OnRevivedPacket() override { DCHECK(false); }
  void OnFecProtectedPayload(StringPiece /*payload*/) override {
    DCHECK(false);
  }
  bool OnStreamFrame(const QuicStreamFrame& /*frame*/) override {
    DCHECK(false);
    return false;
  }
  bool OnAckFrame(const QuicAckFrame& /*frame*/) override {
    DCHECK(false);
    return false;
  }
  bool OnStopWaitingFrame(const QuicStopWaitingFrame& /*frame*/) override {
    DCHECK(false);
    return false;
  }
  bool OnPingFrame(const QuicPingFrame& /*frame*/) override {
    DCHECK(false);
    return false;
  }
  bool OnRstStreamFrame(const QuicRstStreamFrame& /*frame*/) override {
    DCHECK(false);
    return false;
  }
  bool OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& /*frame*/) override {
    DCHECK(false);
    return false;
  }
  bool OnGoAwayFrame(const QuicGoAwayFrame& /*frame*/) override {
    DCHECK(false);
    return false;
  }
  bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& /*frame*/) override {
    DCHECK(false);
    return false;
  }
  bool OnBlockedFrame(const QuicBlockedFrame& frame) override {
    DCHECK(false);
    return false;
  }
  void OnFecData(const QuicFecData& /*fec*/) override { DCHECK(false); }
  void OnPacketComplete() override { DCHECK(false); }

 private:
  QuicDispatcher* dispatcher_;

  // Latched in OnUnauthenticatedPublicHeader for use later.
  QuicConnectionId connection_id_;
};

QuicPacketWriter* QuicDispatcher::DefaultPacketWriterFactory::Create(
    QuicPacketWriter* writer,
    QuicConnection* connection) {
  return new QuicPerConnectionPacketWriter(writer, connection);
}

QuicDispatcher::PacketWriterFactoryAdapter::PacketWriterFactoryAdapter(
    QuicDispatcher* dispatcher)
    : dispatcher_(dispatcher) {}

QuicDispatcher::PacketWriterFactoryAdapter::~PacketWriterFactoryAdapter() {}

QuicPacketWriter* QuicDispatcher::PacketWriterFactoryAdapter::Create(
    QuicConnection* connection) const {
  return dispatcher_->packet_writer_factory_->Create(
      dispatcher_->writer_.get(),
      connection);
}

QuicDispatcher::QuicDispatcher(const QuicConfig& config,
                               const QuicCryptoServerConfig* crypto_config,
                               const QuicVersionVector& supported_versions,
                               PacketWriterFactory* packet_writer_factory,
                               QuicConnectionHelperInterface* helper)
    : config_(config),
      crypto_config_(crypto_config),
      helper_(helper),
      delete_sessions_alarm_(
          helper_->CreateAlarm(new DeleteSessionsAlarm(this))),
      packet_writer_factory_(packet_writer_factory),
      connection_writer_factory_(this),
      supported_versions_(supported_versions),
      current_packet_(nullptr),
      framer_(supported_versions,
              /*unused*/ QuicTime::Zero(),
              Perspective::IS_SERVER),
      framer_visitor_(new QuicFramerVisitor(this)),
      last_error_(QUIC_NO_ERROR) {
  framer_.set_visitor(framer_visitor_.get());
}

QuicDispatcher::~QuicDispatcher() {
  STLDeleteValues(&session_map_);
  STLDeleteElements(&closed_session_list_);
}

void QuicDispatcher::InitializeWithWriter(QuicPacketWriter* writer) {
  DCHECK(writer_ == nullptr);
  writer_.reset(writer);
  time_wait_list_manager_.reset(CreateQuicTimeWaitListManager());
}

void QuicDispatcher::ProcessPacket(const IPEndPoint& server_address,
                                   const IPEndPoint& client_address,
                                   const QuicEncryptedPacket& packet) {
  current_server_address_ = server_address;
  current_client_address_ = client_address;
  current_packet_ = &packet;
  // ProcessPacket will cause the packet to be dispatched in
  // OnUnauthenticatedPublicHeader, or sent to the time wait list manager
  // in OnAuthenticatedHeader.
  framer_.ProcessPacket(packet);
  // TODO(rjshade): Return a status describing if/why a packet was dropped,
  //                and log somehow.  Maybe expose as a varz.
}

bool QuicDispatcher::OnUnauthenticatedPublicHeader(
    const QuicPacketPublicHeader& header) {
  // Port zero is only allowed for unidirectional UDP, so is disallowed by QUIC.
  // Given that we can't even send a reply rejecting the packet, just drop the
  // packet.
  if (current_client_address_.port() == 0) {
    return false;
  }

  // Stopgap test: The code does not construct full-length connection IDs
  // correctly from truncated connection ID fields.  Prevent this from causing
  // the connection ID lookup to error by dropping any packet with a short
  // connection ID.
  if (header.connection_id_length != PACKET_8BYTE_CONNECTION_ID) {
    return false;
  }

  // Packets with connection IDs for active connections are processed
  // immediately.
  QuicConnectionId connection_id = header.connection_id;
  SessionMap::iterator it = session_map_.find(connection_id);
  if (it != session_map_.end()) {
    it->second->connection()->ProcessUdpPacket(
        current_server_address_, current_client_address_, *current_packet_);
    return false;
  }

  // If the packet is a public reset for a connection ID that is not active,
  // there is nothing we must do or can do.
  if (header.reset_flag) {
    return false;
  }

  if (time_wait_list_manager_->IsConnectionIdInTimeWait(connection_id)) {
    // Set the framer's version based on the recorded version for this
    // connection and continue processing for non-public-reset packets.
    return HandlePacketForTimeWait(header);
  }

  // The packet has an unknown connection ID.

  // Unless the packet provides a version, assume that we can continue
  // processing using our preferred version.
  QuicVersion version = supported_versions_.front();
  if (header.version_flag) {
    QuicVersion packet_version = header.versions.front();
    if (framer_.IsSupportedVersion(packet_version)) {
      version = packet_version;
    } else {
      // Packets set to be processed but having an unsupported version will
      // cause a connection to be created.  The connection will handle
      // sending a version negotiation packet.
      // TODO(ianswett): This will malfunction if the full header of the packet
      // causes a parsing error when parsed using the server's preferred
      // version.
    }
  }
  // Set the framer's version and continue processing.
  framer_.set_version(version);
  return true;
}

void QuicDispatcher::OnUnauthenticatedHeader(const QuicPacketHeader& header) {
  QuicConnectionId connection_id = header.public_header.connection_id;

  if (time_wait_list_manager_->IsConnectionIdInTimeWait(
          header.public_header.connection_id)) {
    // This connection ID is already in time-wait state.
    time_wait_list_manager_->ProcessPacket(
        current_server_address_, current_client_address_,
        header.public_header.connection_id, header.packet_sequence_number,
        *current_packet_);
    return;
  }

  // Packet's connection ID is unknown.
  // Apply the validity checks.
  QuicPacketFate fate = ValidityChecks(header);
  switch (fate) {
    case kFateProcess: {
      // Create a session and process the packet.
      QuicServerSession* session = CreateQuicSession(
          connection_id, current_server_address_, current_client_address_);
      DVLOG(1) << "Created new session for " << connection_id;
      session_map_.insert(make_pair(connection_id, session));
      session->connection()->ProcessUdpPacket(
          current_server_address_, current_client_address_, *current_packet_);

      if (FLAGS_enable_quic_stateless_reject_support &&
          session->UsingStatelessRejectsIfPeerSupported() &&
          session->PeerSupportsStatelessRejects() &&
          !session->IsCryptoHandshakeConfirmed()) {
        DVLOG(1) << "Removing new session for " << connection_id
                 << " because the session is in stateless reject mode and"
                 << " encryption has not been established.";
        session->connection()->CloseConnection(
            QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT, /* from_peer */ false);
      }
      break;
    }
    case kFateTimeWait:
      // Add this connection_id to the time-wait state, to safely reject
      // future packets.
      DVLOG(1) << "Adding connection ID " << connection_id
               << "to time-wait list.";
      time_wait_list_manager_->AddConnectionIdToTimeWait(
          connection_id, framer_.version(),
          /*connection_rejected_statelessly=*/false, nullptr);
      DCHECK(time_wait_list_manager_->IsConnectionIdInTimeWait(
          header.public_header.connection_id));
      time_wait_list_manager_->ProcessPacket(
          current_server_address_, current_client_address_,
          header.public_header.connection_id, header.packet_sequence_number,
          *current_packet_);
      break;
    case kFateDrop:
      // Do nothing with the packet.
      break;
  }
}

QuicDispatcher::QuicPacketFate QuicDispatcher::ValidityChecks(
    const QuicPacketHeader& header) {
  // To have all the checks work properly without tears, insert any new check
  // into the framework of this method in the section for checks that return the
  // check's fate value.  The sections for checks must be ordered with the
  // highest priority fate first.

  // Checks that return kFateDrop.

  // Checks that return kFateTimeWait.

  // All packets within a connection sent by a client before receiving a
  // response from the server are required to have the version negotiation flag
  // set.  Since this may be a client continuing a connection we lost track of
  // via server restart, send a rejection to fast-fail the connection.
  if (!header.public_header.version_flag) {
    DVLOG(1) << "Packet without version arrived for unknown connection ID "
             << header.public_header.connection_id;
    return kFateTimeWait;
  }

  // Check that the sequence numer is within the range that the client is
  // expected to send before receiving a response from the server.
  if (header.packet_sequence_number == kInvalidPacketSequenceNumber ||
      header.packet_sequence_number > kMaxReasonableInitialSequenceNumber) {
    return kFateTimeWait;
  }

  return kFateProcess;
}

void QuicDispatcher::CleanUpSession(SessionMap::iterator it,
                                    bool should_close_statelessly) {
  QuicConnection* connection = it->second->connection();
  QuicEncryptedPacket* connection_close_packet =
      connection->ReleaseConnectionClosePacket();
  write_blocked_list_.erase(connection);
  DCHECK(!should_close_statelessly || !connection_close_packet);
  time_wait_list_manager_->AddConnectionIdToTimeWait(
      it->first, connection->version(), should_close_statelessly,
      connection_close_packet);
  session_map_.erase(it);
}

void QuicDispatcher::DeleteSessions() {
  STLDeleteElements(&closed_session_list_);
}

void QuicDispatcher::OnCanWrite() {
  // The socket is now writable.
  writer_->SetWritable();

  // Give all the blocked writers one chance to write, until we're blocked again
  // or there's no work left.
  while (!write_blocked_list_.empty() && !writer_->IsWriteBlocked()) {
    QuicBlockedWriterInterface* blocked_writer =
        write_blocked_list_.begin()->first;
    write_blocked_list_.erase(write_blocked_list_.begin());
    blocked_writer->OnCanWrite();
  }
}

bool QuicDispatcher::HasPendingWrites() const {
  return !write_blocked_list_.empty();
}

void QuicDispatcher::Shutdown() {
  while (!session_map_.empty()) {
    QuicServerSession* session = session_map_.begin()->second;
    session->connection()->SendConnectionClose(QUIC_PEER_GOING_AWAY);
    // Validate that the session removes itself from the session map on close.
    DCHECK(session_map_.empty() || session_map_.begin()->second != session);
  }
  DeleteSessions();
}

void QuicDispatcher::OnConnectionClosed(QuicConnectionId connection_id,
                                        QuicErrorCode error) {
  SessionMap::iterator it = session_map_.find(connection_id);
  if (it == session_map_.end()) {
    LOG(DFATAL) << "ConnectionId " << connection_id
                << " does not exist in the session map.  "
                << "Error: " << QuicUtils::ErrorToString(error);
    LOG(DFATAL) << base::debug::StackTrace().ToString();
    return;
  }

  DVLOG_IF(1, error != QUIC_NO_ERROR) << "Closing connection ("
                                      << connection_id
                                      << ") due to error: "
                                      << QuicUtils::ErrorToString(error);

  if (closed_session_list_.empty()) {
    delete_sessions_alarm_->Cancel();
    delete_sessions_alarm_->Set(helper()->GetClock()->ApproximateNow());
  }
  closed_session_list_.push_back(it->second);
  const bool should_close_statelessly =
      (error == QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT);
  CleanUpSession(it, should_close_statelessly);
}

void QuicDispatcher::OnWriteBlocked(
    QuicBlockedWriterInterface* blocked_writer) {
  if (!writer_->IsWriteBlocked()) {
    LOG(DFATAL) <<
        "QuicDispatcher::OnWriteBlocked called when the writer is not blocked.";
    // Return without adding the connection to the blocked list, to avoid
    // infinite loops in OnCanWrite.
    return;
  }
  write_blocked_list_.insert(std::make_pair(blocked_writer, true));
}

void QuicDispatcher::OnConnectionAddedToTimeWaitList(
    QuicConnectionId connection_id) {
  DVLOG(1) << "Connection " << connection_id << " added to time wait list.";
}

void QuicDispatcher::OnConnectionRemovedFromTimeWaitList(
    QuicConnectionId connection_id) {
  DVLOG(1) << "Connection " << connection_id << " removed from time wait list.";
}

QuicServerSession* QuicDispatcher::CreateQuicSession(
    QuicConnectionId connection_id,
    const IPEndPoint& server_address,
    const IPEndPoint& client_address) {
  // The QuicServerSession takes ownership of |connection| below.
  QuicConnection* connection = new QuicConnection(
      connection_id, client_address, helper_.get(), connection_writer_factory_,
      /* owns_writer= */ true, Perspective::IS_SERVER,
      crypto_config_->HasProofSource(), supported_versions_);

  QuicServerSession* session =
      new QuicServerSession(config_, connection, this, crypto_config_);
  session->Initialize();
  if (FLAGS_quic_session_map_threshold_for_stateless_rejects != -1 &&
      session_map_.size() >=
          static_cast<size_t>(
              FLAGS_quic_session_map_threshold_for_stateless_rejects)) {
    session->set_use_stateless_rejects_if_peer_supported(true);
  }
  return session;
}

QuicTimeWaitListManager* QuicDispatcher::CreateQuicTimeWaitListManager() {
  // TODO(rjshade): The QuicTimeWaitListManager should take ownership of the
  // per-connection packet writer.
  time_wait_list_writer_.reset(
      packet_writer_factory_->Create(writer_.get(), nullptr));
  return new QuicTimeWaitListManager(time_wait_list_writer_.get(), this,
                                     helper_.get(), supported_versions());
}

bool QuicDispatcher::HandlePacketForTimeWait(
    const QuicPacketPublicHeader& header) {
  if (header.reset_flag) {
    // Public reset packets do not have sequence numbers, so ignore the packet.
    return false;
  }

  // Switch the framer to the correct version, so that the sequence number can
  // be parsed correctly.
  framer_.set_version(time_wait_list_manager_->GetQuicVersionFromConnectionId(
      header.connection_id));

  // Continue parsing the packet to extract the sequence number.  Then
  // send it to the time wait manager in OnUnathenticatedHeader.
  return true;
}

void QuicDispatcher::SetLastError(QuicErrorCode error) {
  last_error_ = error;
}

}  // namespace tools
}  // namespace net
