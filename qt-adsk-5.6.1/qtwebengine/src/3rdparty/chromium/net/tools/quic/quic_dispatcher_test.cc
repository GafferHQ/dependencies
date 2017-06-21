// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_dispatcher.h"

#include <ostream>
#include <string>

#include "base/strings/string_piece.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_epoll_connection_helper.h"
#include "net/tools/quic/quic_packet_writer_wrapper.h"
#include "net/tools/quic/quic_time_wait_list_manager.h"
#include "net/tools/quic/test_tools/quic_dispatcher_peer.h"
#include "net/tools/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using net::EpollServer;
using net::test::ConstructEncryptedPacket;
using net::test::MockConnection;
using net::test::ValueRestore;
using std::string;
using std::vector;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::WithoutArgs;
using testing::_;

namespace net {
namespace tools {
namespace test {
namespace {

class TestQuicSpdyServerSession : public QuicServerSession {
 public:
  TestQuicSpdyServerSession(const QuicConfig& config,
                            QuicConnection* connection,
                            const QuicCryptoServerConfig* crypto_config)
      : QuicServerSession(config, connection, nullptr, crypto_config),
        crypto_stream_(QuicServerSession::GetCryptoStream()) {}
  ~TestQuicSpdyServerSession() override{};

  MOCK_METHOD2(OnConnectionClosed, void(QuicErrorCode error, bool from_peer));
  MOCK_METHOD1(CreateIncomingDynamicStream, QuicDataStream*(QuicStreamId id));
  MOCK_METHOD0(CreateOutgoingDynamicStream, QuicDataStream*());

  void SetCryptoStream(QuicCryptoServerStream* crypto_stream) {
    crypto_stream_ = crypto_stream;
  }

  QuicCryptoServerStream* GetCryptoStream() override { return crypto_stream_; }

 private:
  QuicCryptoServerStream* crypto_stream_;

  DISALLOW_COPY_AND_ASSIGN(TestQuicSpdyServerSession);
};

class TestDispatcher : public QuicDispatcher {
 public:
  TestDispatcher(const QuicConfig& config,
                 const QuicCryptoServerConfig* crypto_config,
                 EpollServer* eps)
      : QuicDispatcher(config,
                       crypto_config,
                       QuicSupportedVersions(),
                       new QuicDispatcher::DefaultPacketWriterFactory(),
                       new QuicEpollConnectionHelper(eps)) {}

  MOCK_METHOD3(CreateQuicSession,
               QuicServerSession*(QuicConnectionId connection_id,
                                  const IPEndPoint& server_address,
                                  const IPEndPoint& client_address));

  using QuicDispatcher::current_server_address;
  using QuicDispatcher::current_client_address;
};

// A Connection class which unregisters the session from the dispatcher when
// sending connection close.
// It'd be slightly more realistic to do this from the Session but it would
// involve a lot more mocking.
class MockServerConnection : public MockConnection {
 public:
  MockServerConnection(QuicConnectionId connection_id,
                       QuicDispatcher* dispatcher)
      : MockConnection(connection_id, Perspective::IS_SERVER),
        dispatcher_(dispatcher) {}

  void UnregisterOnConnectionClosed() {
    LOG(ERROR) << "Unregistering " << connection_id();
    dispatcher_->OnConnectionClosed(connection_id(), QUIC_NO_ERROR);
  }

 private:
  QuicDispatcher* dispatcher_;
};

QuicServerSession* CreateSession(QuicDispatcher* dispatcher,
                                 const QuicConfig& config,
                                 QuicConnectionId connection_id,
                                 const IPEndPoint& client_address,
                                 const QuicCryptoServerConfig* crypto_config,
                                 TestQuicSpdyServerSession** session) {
  MockServerConnection* connection =
      new MockServerConnection(connection_id, dispatcher);
  *session = new TestQuicSpdyServerSession(config, connection, crypto_config);
  connection->set_visitor(*session);
  ON_CALL(*connection, SendConnectionClose(_)).WillByDefault(
      WithoutArgs(Invoke(
          connection, &MockServerConnection::UnregisterOnConnectionClosed)));
  EXPECT_CALL(*reinterpret_cast<MockConnection*>((*session)->connection()),
              ProcessUdpPacket(_, client_address, _));

  return *session;
}

class QuicDispatcherTest : public ::testing::Test {
 public:
  QuicDispatcherTest()
      : helper_(&eps_),
        crypto_config_(QuicCryptoServerConfig::TESTING,
                       QuicRandom::GetInstance()),
        dispatcher_(config_, &crypto_config_, &eps_),
        time_wait_list_manager_(nullptr),
        session1_(nullptr),
        session2_(nullptr) {
    dispatcher_.InitializeWithWriter(new QuicDefaultPacketWriter(1));
  }

  ~QuicDispatcherTest() override {}

  MockConnection* connection1() {
    return reinterpret_cast<MockConnection*>(session1_->connection());
  }

  MockConnection* connection2() {
    return reinterpret_cast<MockConnection*>(session2_->connection());
  }

  void ProcessPacket(IPEndPoint client_address,
                     QuicConnectionId connection_id,
                     bool has_version_flag,
                     const string& data) {
    ProcessPacket(client_address, connection_id, has_version_flag, data,
                  PACKET_8BYTE_CONNECTION_ID, PACKET_6BYTE_SEQUENCE_NUMBER);
  }

  void ProcessPacket(IPEndPoint client_address,
                     QuicConnectionId connection_id,
                     bool has_version_flag,
                     const string& data,
                     QuicConnectionIdLength connection_id_length,
                     QuicSequenceNumberLength sequence_number_length) {
    ProcessPacket(client_address, connection_id, has_version_flag, data,
                  connection_id_length, sequence_number_length, 1);
  }

  void ProcessPacket(IPEndPoint client_address,
                     QuicConnectionId connection_id,
                     bool has_version_flag,
                     const string& data,
                     QuicConnectionIdLength connection_id_length,
                     QuicSequenceNumberLength sequence_number_length,
                     QuicPacketSequenceNumber sequence_number) {
    scoped_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
        connection_id, has_version_flag, false, sequence_number, data,
        connection_id_length, sequence_number_length));
    data_ = string(packet->data(), packet->length());
    dispatcher_.ProcessPacket(server_address_, client_address, *packet);
  }

  void ValidatePacket(const QuicEncryptedPacket& packet) {
    EXPECT_EQ(data_.length(), packet.AsStringPiece().length());
    EXPECT_EQ(data_, packet.AsStringPiece());
  }

  void CreateTimeWaitListManager() {
    time_wait_list_manager_ = new MockTimeWaitListManager(
        QuicDispatcherPeer::GetWriter(&dispatcher_), &dispatcher_, &helper_);
    // dispatcher_ takes the ownership of time_wait_list_manager_.
    QuicDispatcherPeer::SetTimeWaitListManager(&dispatcher_,
                                               time_wait_list_manager_);
  }

  EpollServer eps_;
  QuicEpollConnectionHelper helper_;
  QuicConfig config_;
  QuicCryptoServerConfig crypto_config_;
  IPEndPoint server_address_;
  TestDispatcher dispatcher_;
  MockTimeWaitListManager* time_wait_list_manager_;
  TestQuicSpdyServerSession* session1_;
  TestQuicSpdyServerSession* session2_;
  string data_;
};

TEST_F(QuicDispatcherTest, ProcessPackets) {
  IPEndPoint client_address(net::test::Loopback4(), 1);
  server_address_ = IPEndPoint(net::test::Any4(), 5);

  EXPECT_CALL(dispatcher_, CreateQuicSession(1, _, client_address))
      .WillOnce(testing::Return(CreateSession(&dispatcher_, config_, 1,
                                              client_address, &crypto_config_,
                                              &session1_)));
  ProcessPacket(client_address, 1, true, "foo");
  EXPECT_EQ(client_address, dispatcher_.current_client_address());
  EXPECT_EQ(server_address_, dispatcher_.current_server_address());

  EXPECT_CALL(dispatcher_, CreateQuicSession(2, _, client_address))
      .WillOnce(testing::Return(CreateSession(&dispatcher_, config_, 2,
                                              client_address, &crypto_config_,
                                              &session2_)));
  ProcessPacket(client_address, 2, true, "bar");

  EXPECT_CALL(*reinterpret_cast<MockConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _)).Times(1).
      WillOnce(testing::WithArgs<2>(Invoke(
          this, &QuicDispatcherTest::ValidatePacket)));
  ProcessPacket(client_address, 1, false, "eep");
}

TEST_F(QuicDispatcherTest, Shutdown) {
  IPEndPoint client_address(net::test::Loopback4(), 1);

  EXPECT_CALL(dispatcher_, CreateQuicSession(_, _, client_address))
      .WillOnce(testing::Return(CreateSession(&dispatcher_, config_, 1,
                                              client_address, &crypto_config_,
                                              &session1_)));

  ProcessPacket(client_address, 1, true, "foo");

  EXPECT_CALL(*reinterpret_cast<MockConnection*>(session1_->connection()),
              SendConnectionClose(QUIC_PEER_GOING_AWAY));

  dispatcher_.Shutdown();
}

TEST_F(QuicDispatcherTest, TimeWaitListManager) {
  CreateTimeWaitListManager();

  // Create a new session.
  IPEndPoint client_address(net::test::Loopback4(), 1);
  QuicConnectionId connection_id = 1;
  EXPECT_CALL(dispatcher_, CreateQuicSession(connection_id, _, client_address))
      .WillOnce(testing::Return(CreateSession(&dispatcher_, config_,
                                              connection_id, client_address,
                                              &crypto_config_, &session1_)));
  ProcessPacket(client_address, connection_id, true, "foo");

  // Close the connection by sending public reset packet.
  QuicPublicResetPacket packet;
  packet.public_header.connection_id = connection_id;
  packet.public_header.reset_flag = true;
  packet.public_header.version_flag = false;
  packet.rejected_sequence_number = 19191;
  packet.nonce_proof = 132232;
  scoped_ptr<QuicEncryptedPacket> encrypted(
      QuicFramer::BuildPublicResetPacket(packet));
  EXPECT_CALL(*session1_, OnConnectionClosed(QUIC_PUBLIC_RESET, true)).Times(1)
      .WillOnce(WithoutArgs(Invoke(
          reinterpret_cast<MockServerConnection*>(session1_->connection()),
          &MockServerConnection::UnregisterOnConnectionClosed)));
  EXPECT_CALL(*reinterpret_cast<MockConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(Invoke(
          reinterpret_cast<MockConnection*>(session1_->connection()),
          &MockConnection::ReallyProcessUdpPacket));
  dispatcher_.ProcessPacket(IPEndPoint(), client_address, *encrypted);
  EXPECT_TRUE(time_wait_list_manager_->IsConnectionIdInTimeWait(connection_id));

  // Dispatcher forwards subsequent packets for this connection_id to the time
  // wait list manager.
  EXPECT_CALL(*time_wait_list_manager_,
              ProcessPacket(_, _, connection_id, _, _)).Times(1);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _, _, _))
      .Times(0);
  ProcessPacket(client_address, connection_id, true, "foo");
}

TEST_F(QuicDispatcherTest, NoVersionPacketToTimeWaitListManager) {
  CreateTimeWaitListManager();

  IPEndPoint client_address(net::test::Loopback4(), 1);
  QuicConnectionId connection_id = 1;
  // Dispatcher forwards all packets for this connection_id to the time wait
  // list manager.
  EXPECT_CALL(dispatcher_, CreateQuicSession(_, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              ProcessPacket(_, _, connection_id, _, _)).Times(1);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _, _, _))
      .Times(1);
  ProcessPacket(client_address, connection_id, false, "data");
}

// Enables mocking of the handshake-confirmation for stateless rejects.
class MockQuicCryptoServerStream : public QuicCryptoServerStream {
 public:
  MockQuicCryptoServerStream(const QuicCryptoServerConfig& crypto_config,
                             QuicSession* session)
      : QuicCryptoServerStream(&crypto_config, session) {}
  void set_handshake_confirmed_for_testing(bool handshake_confirmed) {
    handshake_confirmed_ = handshake_confirmed;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockQuicCryptoServerStream);
};

struct StatelessRejectTestParams {
  StatelessRejectTestParams(bool enable_stateless_rejects_via_flag,
                            bool use_stateless_rejects_if_peer_supported,
                            bool client_supports_statelesss_rejects,
                            bool crypto_handshake_successful)
      : enable_stateless_rejects_via_flag(enable_stateless_rejects_via_flag),
        use_stateless_rejects_if_peer_supported(
            use_stateless_rejects_if_peer_supported),
        client_supports_statelesss_rejects(client_supports_statelesss_rejects),
        crypto_handshake_successful(crypto_handshake_successful) {}

  friend std::ostream& operator<<(std::ostream& os,
                                  const StatelessRejectTestParams& p) {
    os << "  enable_stateless_rejects_via_flag: "
       << p.enable_stateless_rejects_via_flag << std::endl;
    os << "{ use_stateless_rejects_if_peer_supported: "
       << p.use_stateless_rejects_if_peer_supported << std::endl;
    os << "{ client_supports_statelesss_rejects: "
       << p.client_supports_statelesss_rejects << std::endl;
    os << "  crypto_handshake_successful: " << p.crypto_handshake_successful
       << " }";
    return os;
  }

  // This only enables the stateless reject feature via the feature-flag.
  // It does not force the crypto server to emit stateless rejects.
  bool enable_stateless_rejects_via_flag;
  // If true, this forces the server to send a stateless reject when rejecting
  // messages.  This should be a no-op if enable_stateless_rejects_via_flag is
  // false or the peer does not support them.
  bool use_stateless_rejects_if_peer_supported;
  // Whether or not the client supports stateless rejects.
  bool client_supports_statelesss_rejects;
  // Should the initial crypto handshake succeed or not.
  bool crypto_handshake_successful;
};

// Constructs various test permutations for stateless rejects.
vector<StatelessRejectTestParams> GetStatelessRejectTestParams() {
  vector<StatelessRejectTestParams> params;
  for (bool enable_stateless_rejects_via_flag : {true, false}) {
    for (bool use_stateless_rejects_if_peer_supported : {true, false}) {
      for (bool client_supports_statelesss_rejects : {true, false}) {
        for (bool crypto_handshake_successful : {true, false}) {
          params.push_back(StatelessRejectTestParams(
              enable_stateless_rejects_via_flag,
              use_stateless_rejects_if_peer_supported,
              client_supports_statelesss_rejects, crypto_handshake_successful));
        }
      }
    }
  }
  return params;
}

class QuicDispatcherStatelessRejectTest
    : public QuicDispatcherTest,
      public ::testing::WithParamInterface<StatelessRejectTestParams> {
 public:
  QuicDispatcherStatelessRejectTest() : crypto_stream1_(nullptr) {}

  ~QuicDispatcherStatelessRejectTest() override {
    if (crypto_stream1_) {
      delete crypto_stream1_;
    }
  }

  // This test setup assumes that all testing will be done using
  // crypto_stream1_.
  void SetUp() override {
    FLAGS_enable_quic_stateless_reject_support =
        GetParam().enable_stateless_rejects_via_flag;
  }

  // Returns true or false, depending on whether the server will emit
  // a stateless reject, depending upon the parameters of the test.
  bool ExpectStatelessReject() {
    return GetParam().enable_stateless_rejects_via_flag &&
           GetParam().use_stateless_rejects_if_peer_supported &&
           !GetParam().crypto_handshake_successful &&
           GetParam().client_supports_statelesss_rejects;
  }

  // Sets up dispatcher_, sesession1_, and crypto_stream1_ based on
  // the test parameters.
  QuicServerSession* CreateSessionBasedOnTestParams(
      QuicConnectionId connection_id,
      const IPEndPoint& client_address) {
    CreateSession(&dispatcher_, config_, connection_id, client_address,
                  &crypto_config_, &session1_);

    crypto_stream1_ = new MockQuicCryptoServerStream(crypto_config_, session1_);
    session1_->SetCryptoStream(crypto_stream1_);
    crypto_stream1_->set_use_stateless_rejects_if_peer_supported(
        GetParam().use_stateless_rejects_if_peer_supported);
    crypto_stream1_->set_handshake_confirmed_for_testing(
        GetParam().crypto_handshake_successful);
    crypto_stream1_->set_peer_supports_stateless_rejects(
        GetParam().client_supports_statelesss_rejects);
    return session1_;
  }

  MockQuicCryptoServerStream* crypto_stream1_;
};

TEST_F(QuicDispatcherTest, ProcessPacketWithZeroPort) {
  CreateTimeWaitListManager();

  IPEndPoint client_address(net::test::Loopback4(), 0);
  server_address_ = IPEndPoint(net::test::Any4(), 5);

  // dispatcher_ should drop this packet.
  EXPECT_CALL(dispatcher_, CreateQuicSession(1, _, client_address)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _, _, _))
      .Times(0);
  ProcessPacket(client_address, 1, true, "foo");
}

TEST_F(QuicDispatcherTest, OKSeqNoPacketProcessed) {
  IPEndPoint client_address(net::test::Loopback4(), 1);
  QuicConnectionId connection_id = 1;
  server_address_ = IPEndPoint(net::test::Any4(), 5);

  EXPECT_CALL(dispatcher_, CreateQuicSession(1, _, client_address))
      .WillOnce(testing::Return(CreateSession(&dispatcher_, config_, 1,
                                              client_address, &crypto_config_,
                                              &session1_)));
  // A packet whose sequence number is the largest that is allowed to start a
  // connection.
  ProcessPacket(client_address, connection_id, true, "data",
                PACKET_8BYTE_CONNECTION_ID, PACKET_6BYTE_SEQUENCE_NUMBER,
                QuicDispatcher::kMaxReasonableInitialSequenceNumber);
  EXPECT_EQ(client_address, dispatcher_.current_client_address());
  EXPECT_EQ(server_address_, dispatcher_.current_server_address());
}

TEST_F(QuicDispatcherTest, TooBigSeqNoPacketToTimeWaitListManager) {
  CreateTimeWaitListManager();

  IPEndPoint client_address(net::test::Loopback4(), 1);
  QuicConnectionId connection_id = 1;
  // Dispatcher forwards this packet for this connection_id to the time wait
  // list manager.
  EXPECT_CALL(dispatcher_, CreateQuicSession(_, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              ProcessPacket(_, _, connection_id, _, _)).Times(1);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _, _, _))
      .Times(1);
  // A packet whose sequence number is one to large to be allowed to start a
  // connection.
  ProcessPacket(client_address, connection_id, true, "data",
                PACKET_8BYTE_CONNECTION_ID, PACKET_6BYTE_SEQUENCE_NUMBER,
                QuicDispatcher::kMaxReasonableInitialSequenceNumber + 1);
}

INSTANTIATE_TEST_CASE_P(QuicDispatcherStatelessRejectTests,
                        QuicDispatcherStatelessRejectTest,
                        ::testing::ValuesIn(GetStatelessRejectTestParams()));

// Parameterized test for stateless rejects.  Should test all
// combinations of enabling/disabling, reject/no-reject for stateless
// rejects.
TEST_P(QuicDispatcherStatelessRejectTest, ParameterizedBasicTest) {
  CreateTimeWaitListManager();

  IPEndPoint client_address(net::test::Loopback4(), 1);
  QuicConnectionId connection_id = 1;
  EXPECT_CALL(dispatcher_, CreateQuicSession(connection_id, _, client_address))
      .WillOnce(testing::Return(
          CreateSessionBasedOnTestParams(connection_id, client_address)));

  // Process the first packet for the connection.
  if (ExpectStatelessReject()) {
    // If this is a stateless reject, we expect the connection to close.
    EXPECT_CALL(*session1_, OnConnectionClosed(_, _))
        .Times(1)
        .WillOnce(WithoutArgs(Invoke(
            reinterpret_cast<MockServerConnection*>(session1_->connection()),
            &MockServerConnection::UnregisterOnConnectionClosed)));
  }
  ProcessPacket(client_address, connection_id, true, "foo");

  // Send a second packet and check the results.  If this is a stateless reject,
  // the existing connection_id will go on the time-wait list.
  EXPECT_EQ(ExpectStatelessReject(),
            time_wait_list_manager_->IsConnectionIdInTimeWait(connection_id));
  if (ExpectStatelessReject()) {
    // The second packet will be processed on the time-wait list.
    EXPECT_CALL(*time_wait_list_manager_,
                ProcessPacket(_, _, connection_id, _, _)).Times(1);
  } else {
    // The second packet will trigger a packet-validation
    EXPECT_CALL(*reinterpret_cast<MockConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .Times(1)
        .WillOnce(testing::WithArgs<2>(
            Invoke(this, &QuicDispatcherTest::ValidatePacket)));
  }
  ProcessPacket(client_address, connection_id, true, "foo");
}

// Verify the stopgap test: Packets with truncated connection IDs should be
// dropped.
class QuicDispatcherTestStrayPacketConnectionId
    : public QuicDispatcherTest,
      public ::testing::WithParamInterface<QuicConnectionIdLength> {};

// Packets with truncated connection IDs should be dropped.
TEST_P(QuicDispatcherTestStrayPacketConnectionId,
       StrayPacketTruncatedConnectionId) {
  const QuicConnectionIdLength connection_id_length = GetParam();

  CreateTimeWaitListManager();

  IPEndPoint client_address(net::test::Loopback4(), 1);
  QuicConnectionId connection_id = 1;
  // Dispatcher drops this packet.
  EXPECT_CALL(dispatcher_, CreateQuicSession(_, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              ProcessPacket(_, _, connection_id, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _, _, _))
      .Times(0);
  ProcessPacket(client_address, connection_id, true, "data",
                connection_id_length, PACKET_6BYTE_SEQUENCE_NUMBER);
}

INSTANTIATE_TEST_CASE_P(ConnectionIdLength,
                        QuicDispatcherTestStrayPacketConnectionId,
                        ::testing::Values(PACKET_0BYTE_CONNECTION_ID,
                                          PACKET_1BYTE_CONNECTION_ID,
                                          PACKET_4BYTE_CONNECTION_ID));

class BlockingWriter : public QuicPacketWriterWrapper {
 public:
  BlockingWriter() : write_blocked_(false) {}

  bool IsWriteBlocked() const override { return write_blocked_; }
  void SetWritable() override { write_blocked_ = false; }

  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const IPAddressNumber& self_client_address,
                          const IPEndPoint& peer_client_address) override {
    // It would be quite possible to actually implement this method here with
    // the fake blocked status, but it would be significantly more work in
    // Chromium, and since it's not called anyway, don't bother.
    LOG(DFATAL) << "Not supported";
    return WriteResult();
  }

  bool write_blocked_;
};

class QuicDispatcherWriteBlockedListTest : public QuicDispatcherTest {
 public:
  void SetUp() override {
    writer_ = new BlockingWriter;
    QuicDispatcherPeer::SetPacketWriterFactory(&dispatcher_,
                                               new TestWriterFactory());
    QuicDispatcherPeer::UseWriter(&dispatcher_, writer_);

    IPEndPoint client_address(net::test::Loopback4(), 1);

    EXPECT_CALL(dispatcher_, CreateQuicSession(_, _, client_address))
        .WillOnce(testing::Return(CreateSession(&dispatcher_, config_, 1,
                                                client_address, &crypto_config_,
                                                &session1_)));
    ProcessPacket(client_address, 1, true, "foo");

    EXPECT_CALL(dispatcher_, CreateQuicSession(_, _, client_address))
        .WillOnce(testing::Return(CreateSession(&dispatcher_, config_, 2,
                                                client_address, &crypto_config_,
                                                &session2_)));
    ProcessPacket(client_address, 2, true, "bar");

    blocked_list_ = QuicDispatcherPeer::GetWriteBlockedList(&dispatcher_);
  }

  void TearDown() override {
    EXPECT_CALL(*connection1(), SendConnectionClose(QUIC_PEER_GOING_AWAY));
    EXPECT_CALL(*connection2(), SendConnectionClose(QUIC_PEER_GOING_AWAY));
    dispatcher_.Shutdown();
  }

  void SetBlocked() {
    writer_->write_blocked_ = true;
  }

  void BlockConnection2() {
    writer_->write_blocked_ = true;
    dispatcher_.OnWriteBlocked(connection2());
  }

 protected:
  BlockingWriter* writer_;
  QuicDispatcher::WriteBlockedList* blocked_list_;
};

TEST_F(QuicDispatcherWriteBlockedListTest, BasicOnCanWrite) {
  // No OnCanWrite calls because no connections are blocked.
  dispatcher_.OnCanWrite();

  // Register connection 1 for events, and make sure it's notified.
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite());
  dispatcher_.OnCanWrite();

  // It should get only one notification.
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_.OnCanWrite();
  EXPECT_FALSE(dispatcher_.HasPendingWrites());
}

TEST_F(QuicDispatcherWriteBlockedListTest, OnCanWriteOrder) {
  // Make sure we handle events in order.
  InSequence s;
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  dispatcher_.OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_.OnCanWrite();

  // Check the other ordering.
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection2());
  dispatcher_.OnWriteBlocked(connection1());
  EXPECT_CALL(*connection2(), OnCanWrite());
  EXPECT_CALL(*connection1(), OnCanWrite());
  dispatcher_.OnCanWrite();
}

TEST_F(QuicDispatcherWriteBlockedListTest, OnCanWriteRemove) {
  // Add and remove one connction.
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_.OnCanWrite();

  // Add and remove one connction and make sure it doesn't affect others.
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  dispatcher_.OnWriteBlocked(connection2());
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_.OnCanWrite();

  // Add it, remove it, and add it back and make sure things are OK.
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  blocked_list_->erase(connection1());
  dispatcher_.OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(1);
  dispatcher_.OnCanWrite();
}

TEST_F(QuicDispatcherWriteBlockedListTest, DoubleAdd) {
  // Make sure a double add does not necessitate a double remove.
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  dispatcher_.OnWriteBlocked(connection1());
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_.OnCanWrite();

  // Make sure a double add does not result in two OnCanWrite calls.
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  dispatcher_.OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(1);
  dispatcher_.OnCanWrite();
}

TEST_F(QuicDispatcherWriteBlockedListTest, OnCanWriteHandleBlock) {
  // Finally make sure if we write block on a write call, we stop calling.
  InSequence s;
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  dispatcher_.OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite()).WillOnce(
      Invoke(this, &QuicDispatcherWriteBlockedListTest::SetBlocked));
  EXPECT_CALL(*connection2(), OnCanWrite()).Times(0);
  dispatcher_.OnCanWrite();

  // And we'll resume where we left off when we get another call.
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_.OnCanWrite();
}

TEST_F(QuicDispatcherWriteBlockedListTest, LimitedWrites) {
  // Make sure we call both writers.  The first will register for more writing
  // but should not be immediately called due to limits.
  InSequence s;
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  dispatcher_.OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite()).WillOnce(
      Invoke(this, &QuicDispatcherWriteBlockedListTest::BlockConnection2));
  dispatcher_.OnCanWrite();
  EXPECT_TRUE(dispatcher_.HasPendingWrites());

  // Now call OnCanWrite again, and connection1 should get its second chance
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_.OnCanWrite();
  EXPECT_FALSE(dispatcher_.HasPendingWrites());
}

TEST_F(QuicDispatcherWriteBlockedListTest, TestWriteLimits) {
  // Finally make sure if we write block on a write call, we stop calling.
  InSequence s;
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  dispatcher_.OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite()).WillOnce(
      Invoke(this, &QuicDispatcherWriteBlockedListTest::SetBlocked));
  EXPECT_CALL(*connection2(), OnCanWrite()).Times(0);
  dispatcher_.OnCanWrite();
  EXPECT_TRUE(dispatcher_.HasPendingWrites());

  // And we'll resume where we left off when we get another call.
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_.OnCanWrite();
  EXPECT_FALSE(dispatcher_.HasPendingWrites());
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
