// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_received_packet_manager.h"

#include <algorithm>
#include <vector>

#include "net/quic/quic_connection_stats.h"
#include "net/quic/test_tools/quic_received_packet_manager_peer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::pair;
using std::vector;

namespace net {
namespace test {

class EntropyTrackerPeer {
 public:
  static QuicPacketSequenceNumber first_gap(
      const QuicReceivedPacketManager::EntropyTracker& tracker) {
    return tracker.first_gap_;
  }
  static QuicPacketSequenceNumber largest_observed(
      const QuicReceivedPacketManager::EntropyTracker& tracker) {
    return tracker.largest_observed_;
  }
  static int packets_entropy_size(
      const QuicReceivedPacketManager::EntropyTracker& tracker) {
    return tracker.packets_entropy_.size();
  }
  static bool IsTrackingPacket(
      const QuicReceivedPacketManager::EntropyTracker& tracker,
      QuicPacketSequenceNumber sequence_number) {
    return sequence_number >= tracker.first_gap_ &&
        sequence_number <
            (tracker.first_gap_ + tracker.packets_entropy_.size()) &&
        tracker.packets_entropy_[sequence_number - tracker.first_gap_].second;
  }
};

namespace {

// Entropy of individual packets is not tracked if there are no gaps.
TEST(EntropyTrackerTest, NoGaps) {
  QuicReceivedPacketManager::EntropyTracker tracker;

  tracker.RecordPacketEntropyHash(1, 23);
  tracker.RecordPacketEntropyHash(2, 42);

  EXPECT_EQ(23 ^ 42, tracker.EntropyHash(2));
  EXPECT_EQ(3u, EntropyTrackerPeer::first_gap(tracker));

  EXPECT_EQ(2u, EntropyTrackerPeer::largest_observed(tracker));
  EXPECT_EQ(0, EntropyTrackerPeer::packets_entropy_size(tracker));
  EXPECT_FALSE(EntropyTrackerPeer::IsTrackingPacket(tracker, 1));
  EXPECT_FALSE(EntropyTrackerPeer::IsTrackingPacket(tracker, 2));
}

// Entropy of individual packets is tracked as long as there are gaps.
// Filling the first gap results in entropy getting garbage collected.
TEST(EntropyTrackerTest, FillGaps) {
  QuicReceivedPacketManager::EntropyTracker tracker;

  tracker.RecordPacketEntropyHash(2, 5);
  tracker.RecordPacketEntropyHash(5, 17);
  tracker.RecordPacketEntropyHash(6, 23);
  tracker.RecordPacketEntropyHash(9, 42);

  EXPECT_EQ(1u, EntropyTrackerPeer::first_gap(tracker));
  EXPECT_EQ(9u, EntropyTrackerPeer::largest_observed(tracker));
  EXPECT_EQ(9, EntropyTrackerPeer::packets_entropy_size(tracker));

  EXPECT_EQ(5, tracker.EntropyHash(2));
  EXPECT_EQ(5 ^ 17, tracker.EntropyHash(5));
  EXPECT_EQ(5 ^ 17 ^ 23, tracker.EntropyHash(6));
  EXPECT_EQ(5 ^ 17 ^ 23 ^ 42, tracker.EntropyHash(9));

  EXPECT_FALSE(EntropyTrackerPeer::IsTrackingPacket(tracker, 1));
  EXPECT_TRUE(EntropyTrackerPeer::IsTrackingPacket(tracker, 2));
  EXPECT_TRUE(EntropyTrackerPeer::IsTrackingPacket(tracker, 5));
  EXPECT_TRUE(EntropyTrackerPeer::IsTrackingPacket(tracker, 6));
  EXPECT_TRUE(EntropyTrackerPeer::IsTrackingPacket(tracker, 9));

  // Fill the gap at 1.
  tracker.RecordPacketEntropyHash(1, 2);

  EXPECT_EQ(3u, EntropyTrackerPeer::first_gap(tracker));
  EXPECT_EQ(9u, EntropyTrackerPeer::largest_observed(tracker));
  EXPECT_EQ(7, EntropyTrackerPeer::packets_entropy_size(tracker));

  EXPECT_EQ(2 ^ 5 ^ 17, tracker.EntropyHash(5));
  EXPECT_EQ(2 ^ 5 ^ 17 ^ 23, tracker.EntropyHash(6));
  EXPECT_EQ(2 ^ 5 ^ 17 ^ 23 ^ 42, tracker.EntropyHash(9));

  EXPECT_FALSE(EntropyTrackerPeer::IsTrackingPacket(tracker, 1));
  EXPECT_FALSE(EntropyTrackerPeer::IsTrackingPacket(tracker, 2));
  EXPECT_TRUE(EntropyTrackerPeer::IsTrackingPacket(tracker, 5));
  EXPECT_TRUE(EntropyTrackerPeer::IsTrackingPacket(tracker, 6));
  EXPECT_TRUE(EntropyTrackerPeer::IsTrackingPacket(tracker, 9));

  // Fill the gap at 4.
  tracker.RecordPacketEntropyHash(4, 2);

  EXPECT_EQ(3u, EntropyTrackerPeer::first_gap(tracker));
  EXPECT_EQ(9u, EntropyTrackerPeer::largest_observed(tracker));
  EXPECT_EQ(7, EntropyTrackerPeer::packets_entropy_size(tracker));

  EXPECT_EQ(5, tracker.EntropyHash(4));
  EXPECT_EQ(5 ^ 17, tracker.EntropyHash(5));
  EXPECT_EQ(5 ^ 17 ^ 23, tracker.EntropyHash(6));
  EXPECT_EQ(5 ^ 17 ^ 23 ^ 42, tracker.EntropyHash(9));

  EXPECT_FALSE(EntropyTrackerPeer::IsTrackingPacket(tracker, 3));
  EXPECT_TRUE(EntropyTrackerPeer::IsTrackingPacket(tracker, 4));
  EXPECT_TRUE(EntropyTrackerPeer::IsTrackingPacket(tracker, 5));
  EXPECT_TRUE(EntropyTrackerPeer::IsTrackingPacket(tracker, 6));
  EXPECT_TRUE(EntropyTrackerPeer::IsTrackingPacket(tracker, 9));

  // Fill the gap at 3.  Entropy for packets 3 to 6 are forgotten.
  tracker.RecordPacketEntropyHash(3, 2);

  EXPECT_EQ(7u, EntropyTrackerPeer::first_gap(tracker));
  EXPECT_EQ(9u, EntropyTrackerPeer::largest_observed(tracker));
  EXPECT_EQ(3, EntropyTrackerPeer::packets_entropy_size(tracker));

  EXPECT_EQ(2 ^ 5 ^ 17 ^ 23 ^ 42, tracker.EntropyHash(9));

  EXPECT_FALSE(EntropyTrackerPeer::IsTrackingPacket(tracker, 3));
  EXPECT_FALSE(EntropyTrackerPeer::IsTrackingPacket(tracker, 4));
  EXPECT_FALSE(EntropyTrackerPeer::IsTrackingPacket(tracker, 5));
  EXPECT_FALSE(EntropyTrackerPeer::IsTrackingPacket(tracker, 6));
  EXPECT_TRUE(EntropyTrackerPeer::IsTrackingPacket(tracker, 9));

  // Fill in the rest.
  tracker.RecordPacketEntropyHash(7, 2);
  tracker.RecordPacketEntropyHash(8, 2);

  EXPECT_EQ(10u, EntropyTrackerPeer::first_gap(tracker));
  EXPECT_EQ(9u, EntropyTrackerPeer::largest_observed(tracker));
  EXPECT_EQ(0, EntropyTrackerPeer::packets_entropy_size(tracker));

  EXPECT_EQ(2 ^ 5 ^ 17 ^ 23 ^ 42, tracker.EntropyHash(9));
}

TEST(EntropyTrackerTest, SetCumulativeEntropyUpTo) {
  QuicReceivedPacketManager::EntropyTracker tracker;

  tracker.RecordPacketEntropyHash(2, 5);
  tracker.RecordPacketEntropyHash(5, 17);
  tracker.RecordPacketEntropyHash(6, 23);
  tracker.RecordPacketEntropyHash(9, 42);

  EXPECT_EQ(1u, EntropyTrackerPeer::first_gap(tracker));
  EXPECT_EQ(9u, EntropyTrackerPeer::largest_observed(tracker));
  EXPECT_EQ(9, EntropyTrackerPeer::packets_entropy_size(tracker));

  // Inform the tracker about value of the hash at a gap.
  tracker.SetCumulativeEntropyUpTo(3, 7);
  EXPECT_EQ(3u, EntropyTrackerPeer::first_gap(tracker));
  EXPECT_EQ(9u, EntropyTrackerPeer::largest_observed(tracker));
  EXPECT_EQ(7, EntropyTrackerPeer::packets_entropy_size(tracker));

  EXPECT_EQ(7 ^ 17, tracker.EntropyHash(5));
  EXPECT_EQ(7 ^ 17 ^ 23, tracker.EntropyHash(6));
  EXPECT_EQ(7 ^ 17 ^ 23 ^ 42, tracker.EntropyHash(9));

  // Inform the tracker about value of the hash at a known location.
  tracker.SetCumulativeEntropyUpTo(6, 1);
  EXPECT_EQ(7u, EntropyTrackerPeer::first_gap(tracker));
  EXPECT_EQ(9u, EntropyTrackerPeer::largest_observed(tracker));
  EXPECT_EQ(3, EntropyTrackerPeer::packets_entropy_size(tracker));

  EXPECT_EQ(1 ^ 23 ^ 42, tracker.EntropyHash(9));

  // Inform the tracker about value of the hash at the last location.
  tracker.SetCumulativeEntropyUpTo(9, 21);
  EXPECT_EQ(10u, EntropyTrackerPeer::first_gap(tracker));
  EXPECT_EQ(9u, EntropyTrackerPeer::largest_observed(tracker));
  EXPECT_EQ(0, EntropyTrackerPeer::packets_entropy_size(tracker));

  EXPECT_EQ(42 ^ 21, tracker.EntropyHash(9));
}

class QuicReceivedPacketManagerTest : public ::testing::Test {
 protected:
  QuicReceivedPacketManagerTest() : received_manager_(&stats_) {}

  void RecordPacketReceipt(QuicPacketSequenceNumber sequence_number,
                           QuicPacketEntropyHash entropy_hash) {
    RecordPacketReceipt(sequence_number, entropy_hash, QuicTime::Zero());
  }

  void RecordPacketReceipt(QuicPacketSequenceNumber sequence_number,
                           QuicPacketEntropyHash entropy_hash,
                           QuicTime receipt_time) {
    QuicPacketHeader header;
    header.packet_sequence_number = sequence_number;
    header.entropy_hash = entropy_hash;
    received_manager_.RecordPacketReceived(0u, header, receipt_time);
  }

  void RecordPacketRevived(QuicPacketSequenceNumber sequence_number) {
    received_manager_.RecordPacketRevived(sequence_number);
  }

  QuicConnectionStats stats_;
  QuicReceivedPacketManager received_manager_;
};

TEST_F(QuicReceivedPacketManagerTest, ReceivedPacketEntropyHash) {
  vector<pair<QuicPacketSequenceNumber, QuicPacketEntropyHash> > entropies;
  entropies.push_back(std::make_pair(1, 12));
  entropies.push_back(std::make_pair(7, 1));
  entropies.push_back(std::make_pair(2, 33));
  entropies.push_back(std::make_pair(5, 3));
  entropies.push_back(std::make_pair(8, 34));

  for (size_t i = 0; i < entropies.size(); ++i) {
    RecordPacketReceipt(entropies[i].first, entropies[i].second);
  }

  std::sort(entropies.begin(), entropies.end());

  QuicPacketEntropyHash hash = 0;
  size_t index = 0;
  for (size_t i = 1; i <= (*entropies.rbegin()).first; ++i) {
    if (entropies[index].first == i) {
      hash ^= entropies[index].second;
      ++index;
    }
    if (i < 3) continue;
    EXPECT_EQ(hash, received_manager_.EntropyHash(i));
  }
  // Reorder by 5 when 2 is received after 7.
  EXPECT_EQ(5u, stats_.max_sequence_reordering);
  EXPECT_EQ(0, stats_.max_time_reordering_us);
  EXPECT_EQ(2u, stats_.packets_reordered);
}

TEST_F(QuicReceivedPacketManagerTest, EntropyHashBelowLeastObserved) {
  EXPECT_EQ(0, received_manager_.EntropyHash(0));
  RecordPacketReceipt(4, 5);
  EXPECT_EQ(0, received_manager_.EntropyHash(3));
}

TEST_F(QuicReceivedPacketManagerTest, EntropyHashAboveLargestObserved) {
  EXPECT_EQ(0, received_manager_.EntropyHash(0));
  RecordPacketReceipt(4, 5);
  EXPECT_EQ(0, received_manager_.EntropyHash(3));
}

TEST_F(QuicReceivedPacketManagerTest, SetCumulativeEntropyUpTo) {
  vector<pair<QuicPacketSequenceNumber, QuicPacketEntropyHash> > entropies;
  entropies.push_back(std::make_pair(1, 12));
  entropies.push_back(std::make_pair(2, 1));
  entropies.push_back(std::make_pair(3, 33));
  entropies.push_back(std::make_pair(4, 3));
  entropies.push_back(std::make_pair(6, 34));
  entropies.push_back(std::make_pair(7, 29));

  QuicPacketEntropyHash entropy_hash = 0;
  for (size_t i = 0; i < entropies.size(); ++i) {
    RecordPacketReceipt(entropies[i].first, entropies[i].second);
    entropy_hash ^= entropies[i].second;
  }
  EXPECT_EQ(entropy_hash, received_manager_.EntropyHash(7));

  // Now set the entropy hash up to 5 to be 100.
  entropy_hash ^= 100;
  for (size_t i = 0; i < 4; ++i) {
    entropy_hash ^= entropies[i].second;
  }
  QuicReceivedPacketManagerPeer::SetCumulativeEntropyUpTo(
      &received_manager_, 5, 100);
  EXPECT_EQ(entropy_hash, received_manager_.EntropyHash(7));

  QuicReceivedPacketManagerPeer::SetCumulativeEntropyUpTo(
      &received_manager_, 1, 50);
  EXPECT_EQ(entropy_hash, received_manager_.EntropyHash(7));

  // No reordering.
  EXPECT_EQ(0u, stats_.max_sequence_reordering);
  EXPECT_EQ(0, stats_.max_time_reordering_us);
  EXPECT_EQ(0u, stats_.packets_reordered);
}

TEST_F(QuicReceivedPacketManagerTest, DontWaitForPacketsBefore) {
  QuicPacketHeader header;
  header.packet_sequence_number = 2u;
  received_manager_.RecordPacketReceived(0u, header, QuicTime::Zero());
  header.packet_sequence_number = 7u;
  received_manager_.RecordPacketReceived(0u, header, QuicTime::Zero());
  EXPECT_TRUE(received_manager_.IsAwaitingPacket(3u));
  EXPECT_TRUE(received_manager_.IsAwaitingPacket(6u));
  EXPECT_TRUE(QuicReceivedPacketManagerPeer::DontWaitForPacketsBefore(
      &received_manager_, 4));
  EXPECT_FALSE(received_manager_.IsAwaitingPacket(3u));
  EXPECT_TRUE(received_manager_.IsAwaitingPacket(6u));
}

TEST_F(QuicReceivedPacketManagerTest, UpdateReceivedPacketInfo) {
  QuicPacketHeader header;
  header.packet_sequence_number = 2u;
  QuicTime two_ms = QuicTime::Zero().Add(QuicTime::Delta::FromMilliseconds(2));
  received_manager_.RecordPacketReceived(0u, header, two_ms);

  QuicAckFrame ack;
  received_manager_.UpdateReceivedPacketInfo(&ack, QuicTime::Zero());
  // When UpdateReceivedPacketInfo with a time earlier than the time of the
  // largest observed packet, make sure that the delta is 0, not negative.
  EXPECT_EQ(QuicTime::Delta::Zero(), ack.delta_time_largest_observed);
  EXPECT_FALSE(ack.received_packet_times.empty());

  QuicTime four_ms = QuicTime::Zero().Add(QuicTime::Delta::FromMilliseconds(4));
  received_manager_.UpdateReceivedPacketInfo(&ack, four_ms);
  // When UpdateReceivedPacketInfo after not having received a new packet,
  // the delta should still be accurate.
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(2),
            ack.delta_time_largest_observed);
}

TEST_F(QuicReceivedPacketManagerTest, UpdateReceivedConnectionStats) {
  RecordPacketReceipt(1, 0);
  RecordPacketReceipt(6, 0);
  RecordPacketReceipt(
      2, 0, QuicTime::Zero().Add(QuicTime::Delta::FromMilliseconds(1)));

  EXPECT_EQ(4u, stats_.max_sequence_reordering);
  EXPECT_EQ(1000, stats_.max_time_reordering_us);
  EXPECT_EQ(1u, stats_.packets_reordered);
}

TEST_F(QuicReceivedPacketManagerTest, RevivedPacket) {
  RecordPacketReceipt(1, 0);
  RecordPacketReceipt(3, 0);
  RecordPacketRevived(2);

  QuicAckFrame ack;
  received_manager_.UpdateReceivedPacketInfo(&ack, QuicTime::Zero());
  EXPECT_EQ(1u, ack.missing_packets.size());
  EXPECT_EQ(2u, *ack.missing_packets.begin());
  EXPECT_EQ(1u, ack.revived_packets.size());
  EXPECT_EQ(2u, *ack.missing_packets.begin());
}

TEST_F(QuicReceivedPacketManagerTest, PacketRevivedThenReceived) {
  RecordPacketReceipt(1, 0);
  RecordPacketReceipt(3, 0);
  RecordPacketRevived(2);
  RecordPacketReceipt(2, 0);

  QuicAckFrame ack;
  received_manager_.UpdateReceivedPacketInfo(&ack, QuicTime::Zero());
  EXPECT_TRUE(ack.missing_packets.empty());
  EXPECT_TRUE(ack.revived_packets.empty());
}


}  // namespace
}  // namespace test
}  // namespace net
