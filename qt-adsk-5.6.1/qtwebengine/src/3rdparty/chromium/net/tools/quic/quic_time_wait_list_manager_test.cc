// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_time_wait_list_manager.h"

#include <errno.h>

#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_data_reader.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/quic/quic_epoll_connection_helper.h"
#include "net/tools/quic/test_tools/mock_epoll_server.h"
#include "net/tools/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::BuildUnsizedDataPacket;
using net::test::NoOpFramerVisitor;
using net::test::QuicVersionMax;
using net::test::QuicVersionMin;
using net::test::ValueRestore;
using testing::Args;
using testing::Assign;
using testing::DoAll;
using testing::Matcher;
using testing::MatcherInterface;
using testing::NiceMock;
using testing::Return;
using testing::ReturnPointee;
using testing::SetArgPointee;
using testing::StrictMock;
using testing::Truly;
using testing::_;

namespace net {
namespace tools {
namespace test {

class FramerVisitorCapturingPublicReset : public NoOpFramerVisitor {
 public:
  FramerVisitorCapturingPublicReset() {}
  ~FramerVisitorCapturingPublicReset() override {}

  void OnPublicResetPacket(const QuicPublicResetPacket& public_reset) override {
    public_reset_packet_ = public_reset;
  }

  const QuicPublicResetPacket public_reset_packet() {
    return public_reset_packet_;
  }

 private:
  QuicPublicResetPacket public_reset_packet_;
};

class QuicTimeWaitListManagerPeer {
 public:
  static bool ShouldSendResponse(QuicTimeWaitListManager* manager,
                                 int received_packet_count) {
    return manager->ShouldSendResponse(received_packet_count);
  }

  static QuicTime::Delta time_wait_period(QuicTimeWaitListManager* manager) {
    return manager->time_wait_period_;
  }

  static QuicVersion GetQuicVersionFromConnectionId(
      QuicTimeWaitListManager* manager,
      QuicConnectionId connection_id) {
    return manager->GetQuicVersionFromConnectionId(connection_id);
  }
};

namespace {

class MockFakeTimeEpollServer : public FakeTimeEpollServer {
 public:
  MOCK_METHOD2(RegisterAlarm, void(int64 timeout_in_us,
                                   EpollAlarmCallbackInterface* alarm));
};

class QuicTimeWaitListManagerTest : public ::testing::Test {
 protected:
  QuicTimeWaitListManagerTest()
      : helper_(&epoll_server_),
        time_wait_list_manager_(&writer_,
                                &visitor_,
                                &helper_,
                                QuicSupportedVersions()),
        framer_(QuicSupportedVersions(),
                QuicTime::Zero(),
                Perspective::IS_SERVER),
        connection_id_(45),
        client_address_(net::test::TestPeerIPAddress(), kTestPort),
        writer_is_blocked_(false) {}

  ~QuicTimeWaitListManagerTest() override {}

  void SetUp() override {
    EXPECT_CALL(writer_, IsWriteBlocked())
        .WillRepeatedly(ReturnPointee(&writer_is_blocked_));
    EXPECT_CALL(writer_, IsWriteBlockedDataBuffered())
        .WillRepeatedly(Return(false));
  }

  void AddConnectionId(QuicConnectionId connection_id) {
    AddConnectionId(connection_id, QuicVersionMax(),
                    /*connection_rejected_statelessly=*/false, nullptr);
  }

  void AddStatelessConnectionId(QuicConnectionId connection_id) {
    time_wait_list_manager_.AddConnectionIdToTimeWait(
        connection_id, QuicVersionMax(),
        /*connection_rejected_statelessly=*/true, nullptr);
  }

  void AddConnectionId(QuicConnectionId connection_id,
                       QuicVersion version,
                       bool connection_rejected_statelessly,
                       QuicEncryptedPacket* packet) {
    time_wait_list_manager_.AddConnectionIdToTimeWait(
        connection_id, version, connection_rejected_statelessly, packet);
  }

  bool IsConnectionIdInTimeWait(QuicConnectionId connection_id) {
    return time_wait_list_manager_.IsConnectionIdInTimeWait(connection_id);
  }

  void ProcessPacket(QuicConnectionId connection_id,
                     QuicPacketSequenceNumber sequence_number) {
    QuicEncryptedPacket packet(nullptr, 0);
    time_wait_list_manager_.ProcessPacket(server_address_,
                                          client_address_,
                                          connection_id,
                                          sequence_number,
                                          packet);
  }

  QuicEncryptedPacket* ConstructEncryptedPacket(
      EncryptionLevel level,
      QuicConnectionId connection_id,
      QuicPacketSequenceNumber sequence_number) {
    QuicPacketHeader header;
    header.public_header.connection_id = connection_id;
    header.public_header.connection_id_length = PACKET_8BYTE_CONNECTION_ID;
    header.public_header.version_flag = false;
    header.public_header.reset_flag = false;
    header.public_header.sequence_number_length = PACKET_6BYTE_SEQUENCE_NUMBER;
    header.packet_sequence_number = sequence_number;
    header.entropy_flag = false;
    header.entropy_hash = 0;
    header.fec_flag = false;
    header.is_in_fec_group = NOT_IN_FEC_GROUP;
    header.fec_group = 0;
    QuicStreamFrame stream_frame(1, false, 0, StringPiece("data"));
    QuicFrame frame(&stream_frame);
    QuicFrames frames;
    frames.push_back(frame);
    scoped_ptr<QuicPacket> packet(
        BuildUnsizedDataPacket(&framer_, header, frames));
    EXPECT_TRUE(packet != nullptr);
    char buffer[kMaxPacketSize];
    scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPayload(
        ENCRYPTION_NONE, sequence_number, *packet, buffer, kMaxPacketSize));
    EXPECT_TRUE(encrypted != nullptr);
    return encrypted->Clone();
  }

  NiceMock<MockFakeTimeEpollServer> epoll_server_;
  QuicEpollConnectionHelper helper_;
  StrictMock<MockPacketWriter> writer_;
  StrictMock<MockQuicServerSessionVisitor> visitor_;
  QuicTimeWaitListManager time_wait_list_manager_;
  QuicFramer framer_;
  QuicConnectionId connection_id_;
  IPEndPoint server_address_;
  IPEndPoint client_address_;
  bool writer_is_blocked_;
};

class ValidatePublicResetPacketPredicate
    : public MatcherInterface<const std::tr1::tuple<const char*, int> > {
 public:
  explicit ValidatePublicResetPacketPredicate(QuicConnectionId connection_id,
                                              QuicPacketSequenceNumber number)
      : connection_id_(connection_id), sequence_number_(number) {
  }

  bool MatchAndExplain(
      const std::tr1::tuple<const char*, int> packet_buffer,
      testing::MatchResultListener* /* listener */) const override {
    FramerVisitorCapturingPublicReset visitor;
    QuicFramer framer(QuicSupportedVersions(), QuicTime::Zero(),
                      Perspective::IS_CLIENT);
    framer.set_visitor(&visitor);
    QuicEncryptedPacket encrypted(std::tr1::get<0>(packet_buffer),
                                  std::tr1::get<1>(packet_buffer));
    framer.ProcessPacket(encrypted);
    QuicPublicResetPacket packet = visitor.public_reset_packet();
    return connection_id_ == packet.public_header.connection_id &&
        packet.public_header.reset_flag && !packet.public_header.version_flag &&
        sequence_number_ == packet.rejected_sequence_number &&
        net::test::TestPeerIPAddress() == packet.client_address.address() &&
        kTestPort == packet.client_address.port();
  }

  void DescribeTo(::std::ostream* os) const override {}

  void DescribeNegationTo(::std::ostream* os) const override {}

 private:
  QuicConnectionId connection_id_;
  QuicPacketSequenceNumber sequence_number_;
};


Matcher<const std::tr1::tuple<const char*, int> > PublicResetPacketEq(
    QuicConnectionId connection_id,
    QuicPacketSequenceNumber sequence_number) {
  return MakeMatcher(new ValidatePublicResetPacketPredicate(connection_id,
                                                            sequence_number));
}

TEST_F(QuicTimeWaitListManagerTest, CheckConnectionIdInTimeWait) {
  EXPECT_FALSE(IsConnectionIdInTimeWait(connection_id_));
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  AddConnectionId(connection_id_);
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id_));
}

TEST_F(QuicTimeWaitListManagerTest, CheckStatelessConnectionIdInTimeWait) {
  EXPECT_FALSE(IsConnectionIdInTimeWait(connection_id_));
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  AddStatelessConnectionId(connection_id_);
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id_));
}

TEST_F(QuicTimeWaitListManagerTest, SendConnectionClose) {
  const size_t kConnectionCloseLength = 100;
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  AddConnectionId(connection_id_, QuicVersionMax(),
                  /*connection_rejected_statelessly=*/false,
                  new QuicEncryptedPacket(new char[kConnectionCloseLength],
                                          kConnectionCloseLength, true));
  const int kRandomSequenceNumber = 1;
  EXPECT_CALL(writer_, WritePacket(_, kConnectionCloseLength,
                                   server_address_.address(),
                                   client_address_))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  ProcessPacket(connection_id_, kRandomSequenceNumber);
}

TEST_F(QuicTimeWaitListManagerTest, SendPublicReset) {
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  AddConnectionId(connection_id_);
  const int kRandomSequenceNumber = 1;
  EXPECT_CALL(writer_, WritePacket(_, _,
                                   server_address_.address(),
                                   client_address_))
      .With(Args<0, 1>(PublicResetPacketEq(connection_id_,
                                           kRandomSequenceNumber)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));

  ProcessPacket(connection_id_, kRandomSequenceNumber);
}

TEST_F(QuicTimeWaitListManagerTest, SendPublicResetWithExponentialBackOff) {
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  AddConnectionId(connection_id_);
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());
  for (int sequence_number = 1; sequence_number < 101; ++sequence_number) {
    if ((sequence_number & (sequence_number - 1)) == 0) {
      EXPECT_CALL(writer_, WritePacket(_, _, _, _))
          .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));
    }
    ProcessPacket(connection_id_, sequence_number);
    // Send public reset with exponential back off.
    if ((sequence_number & (sequence_number - 1)) == 0) {
      EXPECT_TRUE(QuicTimeWaitListManagerPeer::ShouldSendResponse(
                      &time_wait_list_manager_, sequence_number));
    } else {
      EXPECT_FALSE(QuicTimeWaitListManagerPeer::ShouldSendResponse(
                       &time_wait_list_manager_, sequence_number));
    }
  }
}

TEST_F(QuicTimeWaitListManagerTest, NoPublicResetForStatelessConnections) {
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  AddStatelessConnectionId(connection_id_);
  const int kRandomSequenceNumber = 1;
  ProcessPacket(connection_id_, kRandomSequenceNumber);
}

TEST_F(QuicTimeWaitListManagerTest, CleanUpOldConnectionIds) {
  const size_t kConnectionIdCount = 100;
  const size_t kOldConnectionIdCount = 31;

  // Add connection_ids such that their expiry time is time_wait_period_.
  epoll_server_.set_now_in_usec(0);
  for (size_t connection_id = 1; connection_id <= kOldConnectionIdCount;
       ++connection_id) {
    EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id));
    AddConnectionId(connection_id);
  }
  EXPECT_EQ(kOldConnectionIdCount, time_wait_list_manager_.num_connections());

  // Add remaining connection_ids such that their add time is
  // 2 * time_wait_period_.
  const QuicTime::Delta time_wait_period =
      QuicTimeWaitListManagerPeer::time_wait_period(&time_wait_list_manager_);
  epoll_server_.set_now_in_usec(time_wait_period.ToMicroseconds());
  for (size_t connection_id = kOldConnectionIdCount + 1;
       connection_id <= kConnectionIdCount; ++connection_id) {
    EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id));
    AddConnectionId(connection_id);
  }
  EXPECT_EQ(kConnectionIdCount, time_wait_list_manager_.num_connections());

  QuicTime::Delta offset = QuicTime::Delta::FromMicroseconds(39);
  // Now set the current time as time_wait_period + offset usecs.
  epoll_server_.set_now_in_usec(time_wait_period.Add(offset).ToMicroseconds());
  // After all the old connection_ids are cleaned up, check the next alarm
  // interval.
  int64 next_alarm_time = epoll_server_.ApproximateNowInUsec() +
      time_wait_period.Subtract(offset).ToMicroseconds();
  EXPECT_CALL(epoll_server_, RegisterAlarm(next_alarm_time, _));

  for (size_t connection_id = 1; connection_id <= kConnectionIdCount;
       ++connection_id) {
    if (connection_id <= kOldConnectionIdCount) {
      EXPECT_CALL(visitor_, OnConnectionRemovedFromTimeWaitList(connection_id));
    }
  }
  time_wait_list_manager_.CleanUpOldConnectionIds();
  for (size_t connection_id = 1; connection_id <= kConnectionIdCount;
       ++connection_id) {
    EXPECT_EQ(connection_id > kOldConnectionIdCount,
              IsConnectionIdInTimeWait(connection_id))
        << "kOldConnectionIdCount: " << kOldConnectionIdCount
        << " connection_id: " <<  connection_id;
  }
  EXPECT_EQ(kConnectionIdCount - kOldConnectionIdCount,
            time_wait_list_manager_.num_connections());
}

TEST_F(QuicTimeWaitListManagerTest, SendQueuedPackets) {
  QuicConnectionId connection_id = 1;
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id));
  AddConnectionId(connection_id);
  QuicPacketSequenceNumber sequence_number = 234;
  scoped_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
      ENCRYPTION_NONE, connection_id, sequence_number));
  // Let first write through.
  EXPECT_CALL(writer_, WritePacket(_, _,
                                   server_address_.address(),
                                   client_address_))
      .With(Args<0, 1>(PublicResetPacketEq(connection_id,
                                           sequence_number)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, packet->length())));
  ProcessPacket(connection_id, sequence_number);

  // write block for the next packet.
  EXPECT_CALL(writer_, WritePacket(_, _,
                                   server_address_.address(),
                                   client_address_))
      .With(Args<0, 1>(PublicResetPacketEq(connection_id,
                                           sequence_number)))
      .WillOnce(DoAll(
          Assign(&writer_is_blocked_, true),
          Return(WriteResult(WRITE_STATUS_BLOCKED, EAGAIN))));
  EXPECT_CALL(visitor_, OnWriteBlocked(&time_wait_list_manager_));
  ProcessPacket(connection_id, sequence_number);
  // 3rd packet. No public reset should be sent;
  ProcessPacket(connection_id, sequence_number);

  // write packet should not be called since we are write blocked but the
  // should be queued.
  QuicConnectionId other_connection_id = 2;
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(other_connection_id));
  AddConnectionId(other_connection_id);
  QuicPacketSequenceNumber other_sequence_number = 23423;
  scoped_ptr<QuicEncryptedPacket> other_packet(
      ConstructEncryptedPacket(
          ENCRYPTION_NONE, other_connection_id, other_sequence_number));
  EXPECT_CALL(writer_, WritePacket(_, _, _, _))
      .Times(0);
  EXPECT_CALL(visitor_, OnWriteBlocked(&time_wait_list_manager_));
  ProcessPacket(other_connection_id, other_sequence_number);
  EXPECT_EQ(2u, time_wait_list_manager_.num_connections());

  // Now expect all the write blocked public reset packets to be sent again.
  writer_is_blocked_ = false;
  EXPECT_CALL(writer_, WritePacket(_, _,
                                   server_address_.address(),
                                   client_address_))
      .With(Args<0, 1>(PublicResetPacketEq(connection_id,
                                           sequence_number)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, packet->length())));
  EXPECT_CALL(writer_, WritePacket(_, _,
                                   server_address_.address(),
                                   client_address_))
      .With(Args<0, 1>(PublicResetPacketEq(other_connection_id,
                                           other_sequence_number)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK,
                                   other_packet->length())));
  time_wait_list_manager_.OnCanWrite();
}

TEST_F(QuicTimeWaitListManagerTest, GetQuicVersionFromMap) {
  const int kConnectionId1 = 123;
  const int kConnectionId2 = 456;
  const int kConnectionId3 = 789;

  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(kConnectionId1));
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(kConnectionId2));
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(kConnectionId3));
  AddConnectionId(kConnectionId1, QuicVersionMin(),
                  /*connection_rejected_statelessly=*/false, nullptr);
  AddConnectionId(kConnectionId2, QuicVersionMax(),
                  /*connection_rejected_statelessly=*/false, nullptr);
  AddConnectionId(kConnectionId3, QuicVersionMax(),
                  /*connection_rejected_statelessly=*/false, nullptr);

  EXPECT_EQ(QuicVersionMin(),
            QuicTimeWaitListManagerPeer::GetQuicVersionFromConnectionId(
                &time_wait_list_manager_, kConnectionId1));
  EXPECT_EQ(QuicVersionMax(),
            QuicTimeWaitListManagerPeer::GetQuicVersionFromConnectionId(
                &time_wait_list_manager_, kConnectionId2));
  EXPECT_EQ(QuicVersionMax(),
            QuicTimeWaitListManagerPeer::GetQuicVersionFromConnectionId(
                &time_wait_list_manager_, kConnectionId3));
}

TEST_F(QuicTimeWaitListManagerTest, AddConnectionIdTwice) {
  // Add connection_ids such that their expiry time is time_wait_period_.
  epoll_server_.set_now_in_usec(0);
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  AddConnectionId(connection_id_);
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id_));
  const size_t kConnectionCloseLength = 100;
  AddConnectionId(connection_id_, QuicVersionMax(),
                  /*connection_rejected_statelessly=*/false,
                  new QuicEncryptedPacket(new char[kConnectionCloseLength],
                                          kConnectionCloseLength, true));
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id_));
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());

  EXPECT_CALL(writer_, WritePacket(_,
                                   kConnectionCloseLength,
                                   server_address_.address(),
                                   client_address_))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  const int kRandomSequenceNumber = 1;
  ProcessPacket(connection_id_, kRandomSequenceNumber);

  const QuicTime::Delta time_wait_period =
      QuicTimeWaitListManagerPeer::time_wait_period(&time_wait_list_manager_);

  QuicTime::Delta offset = QuicTime::Delta::FromMicroseconds(39);
  // Now set the current time as time_wait_period + offset usecs.
  epoll_server_.set_now_in_usec(time_wait_period.Add(offset).ToMicroseconds());
  // After the connection_ids are cleaned up, check the next alarm interval.
  int64 next_alarm_time = epoll_server_.ApproximateNowInUsec() +
      time_wait_period.ToMicroseconds();

  EXPECT_CALL(epoll_server_, RegisterAlarm(next_alarm_time, _));
  EXPECT_CALL(visitor_, OnConnectionRemovedFromTimeWaitList(connection_id_));
  time_wait_list_manager_.CleanUpOldConnectionIds();
  EXPECT_FALSE(IsConnectionIdInTimeWait(connection_id_));
  EXPECT_EQ(0u, time_wait_list_manager_.num_connections());
}

TEST_F(QuicTimeWaitListManagerTest, ConnectionIdsOrderedByTime) {
  // Simple randomization: the values of connection_ids are swapped based on the
  // current seconds on the clock. If the container is broken, the test will be
  // 50% flaky.
  int odd_second = static_cast<int>(epoll_server_.ApproximateNowInUsec()) % 2;
  EXPECT_TRUE(odd_second == 0 || odd_second == 1);
  const QuicConnectionId connection_id1 = odd_second;
  const QuicConnectionId connection_id2 = 1 - odd_second;

  // 1 will hash lower than 2, but we add it later. They should come out in the
  // add order, not hash order.
  epoll_server_.set_now_in_usec(0);
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id1));
  AddConnectionId(connection_id1);
  epoll_server_.set_now_in_usec(10);
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id2));
  AddConnectionId(connection_id2);
  EXPECT_EQ(2u, time_wait_list_manager_.num_connections());

  const QuicTime::Delta time_wait_period =
      QuicTimeWaitListManagerPeer::time_wait_period(&time_wait_list_manager_);
  epoll_server_.set_now_in_usec(time_wait_period.ToMicroseconds() + 1);

  EXPECT_CALL(epoll_server_, RegisterAlarm(_, _));

  EXPECT_CALL(visitor_, OnConnectionRemovedFromTimeWaitList(connection_id1));
  time_wait_list_manager_.CleanUpOldConnectionIds();
  EXPECT_FALSE(IsConnectionIdInTimeWait(connection_id1));
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id2));
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());
}

TEST_F(QuicTimeWaitListManagerTest, MaxConnectionsTest) {
  // Basically, shut off time-based eviction.
  FLAGS_quic_time_wait_list_seconds = 10000000000;
  FLAGS_quic_time_wait_list_max_connections = 5;

  QuicConnectionId current_connection_id = 0;
  // Add exactly the maximum number of connections
  for (int64 i = 0; i < FLAGS_quic_time_wait_list_max_connections; ++i) {
    ++current_connection_id;
    EXPECT_FALSE(IsConnectionIdInTimeWait(current_connection_id));
    EXPECT_CALL(visitor_,
                OnConnectionAddedToTimeWaitList(current_connection_id));
    AddConnectionId(current_connection_id);
    EXPECT_EQ(current_connection_id, time_wait_list_manager_.num_connections());
    EXPECT_TRUE(IsConnectionIdInTimeWait(current_connection_id));
  }

  // Now keep adding.  Since we're already at the max, every new connection-id
  // will evict the oldest one.
  for (int64 i = 0; i < FLAGS_quic_time_wait_list_max_connections; ++i) {
    ++current_connection_id;
    const QuicConnectionId id_to_evict =
        current_connection_id - FLAGS_quic_time_wait_list_max_connections;
    EXPECT_TRUE(IsConnectionIdInTimeWait(id_to_evict));
    EXPECT_FALSE(IsConnectionIdInTimeWait(current_connection_id));
    EXPECT_CALL(visitor_, OnConnectionRemovedFromTimeWaitList(id_to_evict));
    EXPECT_CALL(visitor_,
                OnConnectionAddedToTimeWaitList(current_connection_id));
    AddConnectionId(current_connection_id);
    EXPECT_EQ(static_cast<size_t>(FLAGS_quic_time_wait_list_max_connections),
              time_wait_list_manager_.num_connections());
    EXPECT_FALSE(IsConnectionIdInTimeWait(id_to_evict));
    EXPECT_TRUE(IsConnectionIdInTimeWait(current_connection_id));
  }
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
