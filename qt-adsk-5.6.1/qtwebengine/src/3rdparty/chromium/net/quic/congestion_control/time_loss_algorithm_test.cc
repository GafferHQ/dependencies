// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/time_loss_algorithm.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/congestion_control/rtt_stats.h"
#include "net/quic/quic_ack_notifier_manager.h"
#include "net/quic/quic_unacked_packet_map.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::vector;

namespace net {
namespace test {
namespace {

// Default packet length.
const uint32 kDefaultLength = 1000;

class TimeLossAlgorithmTest : public ::testing::Test {
 protected:
  TimeLossAlgorithmTest() : unacked_packets_(&ack_notifier_manager_) {
    rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                         QuicTime::Delta::Zero(),
                         clock_.Now());
  }

  ~TimeLossAlgorithmTest() override {
    STLDeleteElements(&packets_);
  }

  void SendDataPacket(QuicPacketSequenceNumber sequence_number) {
    packets_.push_back(new QuicEncryptedPacket(nullptr, kDefaultLength));
    SerializedPacket packet(sequence_number, PACKET_1BYTE_SEQUENCE_NUMBER,
                            packets_.back(), 0,
                            new RetransmittableFrames(ENCRYPTION_NONE));
    unacked_packets_.AddSentPacket(packet, 0, NOT_RETRANSMISSION, clock_.Now(),
                                   1000, true);
  }

  void VerifyLosses(QuicPacketSequenceNumber largest_observed,
                    QuicPacketSequenceNumber* losses_expected,
                    size_t num_losses) {
    SequenceNumberSet lost_packets =
        loss_algorithm_.DetectLostPackets(
            unacked_packets_, clock_.Now(), largest_observed, rtt_stats_);
    EXPECT_EQ(num_losses, lost_packets.size());
    for (size_t i = 0; i < num_losses; ++i) {
      EXPECT_TRUE(ContainsKey(lost_packets, losses_expected[i]));
    }
  }

  vector<QuicEncryptedPacket*> packets_;
  AckNotifierManager ack_notifier_manager_;
  QuicUnackedPacketMap unacked_packets_;
  TimeLossAlgorithm loss_algorithm_;
  RttStats rtt_stats_;
  MockClock clock_;
};

TEST_F(TimeLossAlgorithmTest, NoLossFor500Nacks) {
  const size_t kNumSentPackets = 5;
  // Transmit 5 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }
  unacked_packets_.RemoveFromInFlight(2);
  for (size_t i = 1; i < 500; ++i) {
    unacked_packets_.NackPacket(1, i);
    VerifyLosses(2, nullptr, 0);
  }
  EXPECT_EQ(rtt_stats_.smoothed_rtt().Multiply(1.25),
            loss_algorithm_.GetLossTimeout().Subtract(clock_.Now()));
}

TEST_F(TimeLossAlgorithmTest, NoLossUntilTimeout) {
  const size_t kNumSentPackets = 10;
  // Transmit 10 packets at 1/10th an RTT interval.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
    clock_.AdvanceTime(rtt_stats_.smoothed_rtt().Multiply(0.1));
  }
  // Expect the timer to not be set.
  EXPECT_EQ(QuicTime::Zero(), loss_algorithm_.GetLossTimeout());
  // The packet should not be lost until 1.25 RTTs pass.
  unacked_packets_.NackPacket(1, 1);
  unacked_packets_.RemoveFromInFlight(2);
  VerifyLosses(2, nullptr, 0);
  // Expect the timer to be set to 0.25 RTT's in the future.
  EXPECT_EQ(rtt_stats_.smoothed_rtt().Multiply(0.25),
            loss_algorithm_.GetLossTimeout().Subtract(clock_.Now()));
  unacked_packets_.NackPacket(1, 5);
  VerifyLosses(2, nullptr, 0);
  clock_.AdvanceTime(rtt_stats_.smoothed_rtt().Multiply(0.25));
  QuicPacketSequenceNumber lost[] = { 1 };
  VerifyLosses(2, lost, arraysize(lost));
  EXPECT_EQ(QuicTime::Zero(), loss_algorithm_.GetLossTimeout());
}

TEST_F(TimeLossAlgorithmTest, NoLossWithoutNack) {
  const size_t kNumSentPackets = 10;
  // Transmit 10 packets at 1/10th an RTT interval.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
    clock_.AdvanceTime(rtt_stats_.smoothed_rtt().Multiply(0.1));
  }
  // Expect the timer to not be set.
  EXPECT_EQ(QuicTime::Zero(), loss_algorithm_.GetLossTimeout());
  // The packet should not be lost without a nack.
  unacked_packets_.RemoveFromInFlight(1);
  VerifyLosses(1, nullptr, 0);
  // The timer should still not be set.
  EXPECT_EQ(QuicTime::Zero(), loss_algorithm_.GetLossTimeout());
  clock_.AdvanceTime(rtt_stats_.smoothed_rtt().Multiply(0.25));
  VerifyLosses(1, nullptr, 0);
  clock_.AdvanceTime(rtt_stats_.smoothed_rtt());
  VerifyLosses(1, nullptr, 0);

  EXPECT_EQ(QuicTime::Zero(), loss_algorithm_.GetLossTimeout());
}

TEST_F(TimeLossAlgorithmTest, MultipleLossesAtOnce) {
  const size_t kNumSentPackets = 10;
  // Transmit 10 packets at once and then go forward an RTT.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }
  clock_.AdvanceTime(rtt_stats_.smoothed_rtt());
  // Expect the timer to not be set.
  EXPECT_EQ(QuicTime::Zero(), loss_algorithm_.GetLossTimeout());
  // The packet should not be lost until 1.25 RTTs pass.
  for (size_t i = 1; i < kNumSentPackets; ++i) {
    unacked_packets_.NackPacket(i, 1);
  }
  unacked_packets_.RemoveFromInFlight(10);
  VerifyLosses(10, nullptr, 0);
  // Expect the timer to be set to 0.25 RTT's in the future.
  EXPECT_EQ(rtt_stats_.smoothed_rtt().Multiply(0.25),
            loss_algorithm_.GetLossTimeout().Subtract(clock_.Now()));
  clock_.AdvanceTime(rtt_stats_.smoothed_rtt().Multiply(0.25));
  QuicPacketSequenceNumber lost[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  VerifyLosses(10, lost, arraysize(lost));
  EXPECT_EQ(QuicTime::Zero(), loss_algorithm_.GetLossTimeout());
}

}  // namespace
}  // namespace test
}  // namespace net
