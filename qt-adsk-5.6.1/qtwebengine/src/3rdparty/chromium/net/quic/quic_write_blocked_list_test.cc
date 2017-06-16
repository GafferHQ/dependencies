// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "net/quic/quic_write_blocked_list.h"

#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

TEST(QuicWriteBlockedListTest, PriorityOrder) {
  QuicWriteBlockedList write_blocked_list;

  // Mark streams blocked in roughly reverse priority order, and
  // verify that streams are sorted.
  write_blocked_list.PushBack(40,
                              QuicWriteBlockedList::kLowestPriority);
  write_blocked_list.PushBack(23,
                              QuicWriteBlockedList::kHighestPriority);
  write_blocked_list.PushBack(17,
                              QuicWriteBlockedList::kHighestPriority);
  write_blocked_list.PushBack(kHeadersStreamId,
                              QuicWriteBlockedList::kHighestPriority);
  write_blocked_list.PushBack(kCryptoStreamId,
                              QuicWriteBlockedList::kHighestPriority);

  EXPECT_EQ(5u, write_blocked_list.NumBlockedStreams());
  EXPECT_TRUE(write_blocked_list.HasWriteBlockedCryptoOrHeadersStream());
  EXPECT_TRUE(write_blocked_list.HasWriteBlockedDataStreams());
  // The Crypto stream is highest priority.
  EXPECT_EQ(kCryptoStreamId, write_blocked_list.PopFront());
  // Followed by the Headers stream.
  EXPECT_EQ(kHeadersStreamId, write_blocked_list.PopFront());
  // Streams with same priority are popped in the order they were inserted.
  EXPECT_EQ(23u, write_blocked_list.PopFront());
  EXPECT_EQ(17u, write_blocked_list.PopFront());
  // Low priority stream appears last.
  EXPECT_EQ(40u, write_blocked_list.PopFront());

  EXPECT_EQ(0u, write_blocked_list.NumBlockedStreams());
  EXPECT_FALSE(write_blocked_list.HasWriteBlockedCryptoOrHeadersStream());
  EXPECT_FALSE(write_blocked_list.HasWriteBlockedDataStreams());
}

TEST(QuicWriteBlockedListTest, CryptoStream) {
  QuicWriteBlockedList write_blocked_list;
  write_blocked_list.PushBack(kCryptoStreamId,
                              QuicWriteBlockedList::kHighestPriority);

  EXPECT_EQ(1u, write_blocked_list.NumBlockedStreams());
  EXPECT_TRUE(write_blocked_list.HasWriteBlockedCryptoOrHeadersStream());
  EXPECT_EQ(kCryptoStreamId, write_blocked_list.PopFront());
  EXPECT_EQ(0u, write_blocked_list.NumBlockedStreams());
  EXPECT_FALSE(write_blocked_list.HasWriteBlockedCryptoOrHeadersStream());
}

TEST(QuicWriteBlockedListTest, HeadersStream) {
  QuicWriteBlockedList write_blocked_list;
  write_blocked_list.PushBack(kHeadersStreamId,
                              QuicWriteBlockedList::kHighestPriority);

  EXPECT_EQ(1u, write_blocked_list.NumBlockedStreams());
  EXPECT_TRUE(write_blocked_list.HasWriteBlockedCryptoOrHeadersStream());
  EXPECT_EQ(kHeadersStreamId, write_blocked_list.PopFront());
  EXPECT_EQ(0u, write_blocked_list.NumBlockedStreams());
  EXPECT_FALSE(write_blocked_list.HasWriteBlockedCryptoOrHeadersStream());
}

TEST(QuicWriteBlockedListTest, VerifyHeadersStream) {
  QuicWriteBlockedList write_blocked_list;
  write_blocked_list.PushBack(5,
                              QuicWriteBlockedList::kHighestPriority);
  write_blocked_list.PushBack(kHeadersStreamId,
                              QuicWriteBlockedList::kHighestPriority);

  EXPECT_EQ(2u, write_blocked_list.NumBlockedStreams());
  EXPECT_TRUE(write_blocked_list.HasWriteBlockedCryptoOrHeadersStream());
  EXPECT_TRUE(write_blocked_list.HasWriteBlockedDataStreams());
  // In newer QUIC versions, there is a headers stream which is
  // higher priority than data streams.
  EXPECT_EQ(kHeadersStreamId, write_blocked_list.PopFront());
  EXPECT_EQ(5u, write_blocked_list.PopFront());
  EXPECT_EQ(0u, write_blocked_list.NumBlockedStreams());
  EXPECT_FALSE(write_blocked_list.HasWriteBlockedCryptoOrHeadersStream());
  EXPECT_FALSE(write_blocked_list.HasWriteBlockedDataStreams());
}

TEST(QuicWriteBlockedListTest, NoDuplicateEntries) {
  // Test that QuicWriteBlockedList doesn't allow duplicate entries.
  QuicWriteBlockedList write_blocked_list;

  // Try to add a stream to the write blocked list multiple times at the same
  // priority.
  const QuicStreamId kBlockedId = kClientDataStreamId1;
  write_blocked_list.PushBack(kBlockedId,
                              QuicWriteBlockedList::kHighestPriority);
  write_blocked_list.PushBack(kBlockedId,
                              QuicWriteBlockedList::kHighestPriority);
  write_blocked_list.PushBack(kBlockedId,
                              QuicWriteBlockedList::kHighestPriority);

  // This should only result in one blocked stream being added.
  EXPECT_EQ(1u, write_blocked_list.NumBlockedStreams());
  EXPECT_TRUE(write_blocked_list.HasWriteBlockedDataStreams());

  // There should only be one stream to pop off the front.
  EXPECT_EQ(kBlockedId, write_blocked_list.PopFront());
  EXPECT_EQ(0u, write_blocked_list.NumBlockedStreams());
  EXPECT_FALSE(write_blocked_list.HasWriteBlockedDataStreams());
}

}  // namespace
}  // namespace test
}  // namespace net
