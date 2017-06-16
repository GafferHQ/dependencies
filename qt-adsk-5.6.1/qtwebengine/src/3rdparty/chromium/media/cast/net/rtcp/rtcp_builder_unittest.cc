// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/net/rtcp/rtcp_builder.h"
#include "media/cast/net/rtcp/rtcp_utility.h"
#include "media/cast/net/rtcp/test_rtcp_packet_builder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

namespace {
static const uint32 kSendingSsrc = 0x12345678;
static const uint32 kMediaSsrc = 0x87654321;
static const base::TimeDelta kDefaultDelay =
    base::TimeDelta::FromMilliseconds(100);

RtcpReportBlock GetReportBlock() {
  RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;
  return report_block;
}

}  // namespace


class RtcpBuilderTest : public ::testing::Test {
 protected:
  RtcpBuilderTest()
      : rtcp_builder_(new RtcpBuilder(kSendingSsrc)) {}

  void ExpectPacketEQ(scoped_ptr<Packet> golden_packet,
                      PacketRef packet) {
    int diffs = 0;
    EXPECT_EQ(golden_packet->size(), packet->data.size());
    if (golden_packet->size() == packet->data.size()) {
      for (size_t x = 0; x < golden_packet->size(); x++) {
        EXPECT_EQ((*golden_packet)[x], packet->data[x]) <<
            "x = " << x << " / " << golden_packet->size();
        if ((*golden_packet)[x] != packet->data[x]) {
          if (++diffs > 5)
            break;
        }
      }
    }
  }

  scoped_ptr<RtcpBuilder> rtcp_builder_;

  DISALLOW_COPY_AND_ASSIGN(RtcpBuilderTest);
};

TEST_F(RtcpBuilderTest, RtcpReceiverReport) {
  // Receiver report with report block.
  TestRtcpPacketBuilder p2;
  p2.AddRr(kSendingSsrc, 1);
  p2.AddRb(kMediaSsrc);

  RtcpReportBlock report_block = GetReportBlock();

  ExpectPacketEQ(
      p2.GetPacket().Pass(),
      rtcp_builder_->BuildRtcpFromReceiver(
          &report_block, NULL, NULL, NULL, kDefaultDelay));
}

TEST_F(RtcpBuilderTest, RtcpReceiverReportWithRrtr) {
  // Receiver report with report block.
  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddXrHeader(kSendingSsrc);
  p.AddXrRrtrBlock();

  RtcpReportBlock report_block = GetReportBlock();

  RtcpReceiverReferenceTimeReport rrtr;
  rrtr.ntp_seconds = kNtpHigh;
  rrtr.ntp_fraction = kNtpLow;

  ExpectPacketEQ(p.GetPacket().Pass(),
                 rtcp_builder_->BuildRtcpFromReceiver(
                     &report_block,
                     &rrtr,
                     NULL,
                     NULL,
                     kDefaultDelay));
}

TEST_F(RtcpBuilderTest, RtcpReceiverReportWithCast) {
  // Receiver report with report block.
  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddCast(kSendingSsrc, kMediaSsrc, kDefaultDelay);

  RtcpReportBlock report_block = GetReportBlock();

  RtcpCastMessage cast_message(kMediaSsrc);
  cast_message.ack_frame_id = kAckFrameId;
  PacketIdSet missing_packets;
  cast_message.missing_frames_and_packets[kLostFrameId] = missing_packets;

  missing_packets.insert(kLostPacketId1);
  missing_packets.insert(kLostPacketId2);
  missing_packets.insert(kLostPacketId3);
  cast_message.missing_frames_and_packets[kFrameIdWithLostPackets] =
      missing_packets;

  ExpectPacketEQ(p.GetPacket().Pass(),
                 rtcp_builder_->BuildRtcpFromReceiver(
                     &report_block,
                     NULL,
                     &cast_message,
                     NULL,
                     kDefaultDelay));
}

TEST_F(RtcpBuilderTest, RtcpReceiverReportWithRrtraAndCastMessage) {
  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddXrHeader(kSendingSsrc);
  p.AddXrRrtrBlock();
  p.AddCast(kSendingSsrc, kMediaSsrc, kDefaultDelay);

  RtcpReportBlock report_block = GetReportBlock();

  RtcpReceiverReferenceTimeReport rrtr;
  rrtr.ntp_seconds = kNtpHigh;
  rrtr.ntp_fraction = kNtpLow;

  RtcpCastMessage cast_message(kMediaSsrc);
  cast_message.ack_frame_id = kAckFrameId;
  PacketIdSet missing_packets;
  cast_message.missing_frames_and_packets[kLostFrameId] = missing_packets;

  missing_packets.insert(kLostPacketId1);
  missing_packets.insert(kLostPacketId2);
  missing_packets.insert(kLostPacketId3);
  cast_message.missing_frames_and_packets[kFrameIdWithLostPackets] =
      missing_packets;

  ExpectPacketEQ(p.GetPacket().Pass(),
                 rtcp_builder_->BuildRtcpFromReceiver(
                     &report_block,
                     &rrtr,
                     &cast_message,
                     NULL,
                     kDefaultDelay));
}

TEST_F(RtcpBuilderTest, RtcpReceiverReportWithRrtrCastMessageAndLog) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;

  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddXrHeader(kSendingSsrc);
  p.AddXrRrtrBlock();
  p.AddCast(kSendingSsrc, kMediaSsrc, kDefaultDelay);

  RtcpReportBlock report_block = GetReportBlock();

  RtcpReceiverReferenceTimeReport rrtr;
  rrtr.ntp_seconds = kNtpHigh;
  rrtr.ntp_fraction = kNtpLow;

  RtcpCastMessage cast_message(kMediaSsrc);
  cast_message.ack_frame_id = kAckFrameId;
  PacketIdSet missing_packets;
  cast_message.missing_frames_and_packets[kLostFrameId] = missing_packets;

  missing_packets.insert(kLostPacketId1);
  missing_packets.insert(kLostPacketId2);
  missing_packets.insert(kLostPacketId3);
  cast_message.missing_frames_and_packets[kFrameIdWithLostPackets] =
      missing_packets;

  ReceiverRtcpEventSubscriber event_subscriber(500, VIDEO_EVENT);
  ReceiverRtcpEventSubscriber::RtcpEvents rtcp_events;

  ExpectPacketEQ(p.GetPacket().Pass(),
                 rtcp_builder_->BuildRtcpFromReceiver(
                     &report_block,
                     &rrtr,
                     &cast_message,
                     &rtcp_events,
                     kDefaultDelay));

  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  p.AddReceiverLog(kSendingSsrc);
  p.AddReceiverFrameLog(kRtpTimestamp, 2, kTimeBaseMs);
  p.AddReceiverEventLog(0, FRAME_ACK_SENT, 0);
  p.AddReceiverEventLog(kLostPacketId1, PACKET_RECEIVED, kTimeDelayMs);

  FrameEvent frame_event;
  frame_event.rtp_timestamp = kRtpTimestamp;
  frame_event.type = FRAME_ACK_SENT;
  frame_event.media_type = VIDEO_EVENT;
  frame_event.timestamp = testing_clock.NowTicks();
  event_subscriber.OnReceiveFrameEvent(frame_event);
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));

  PacketEvent packet_event;
  packet_event.rtp_timestamp = kRtpTimestamp;
  packet_event.type = PACKET_RECEIVED;
  packet_event.media_type = VIDEO_EVENT;
  packet_event.timestamp = testing_clock.NowTicks();
  packet_event.packet_id = kLostPacketId1;
  event_subscriber.OnReceivePacketEvent(packet_event);
  event_subscriber.GetRtcpEventsWithRedundancy(&rtcp_events);
  EXPECT_EQ(2u, rtcp_events.size());

  ExpectPacketEQ(
      p.GetPacket().Pass(),
      rtcp_builder_->BuildRtcpFromReceiver(
          &report_block,
          &rrtr,
          &cast_message,
          &rtcp_events,
          kDefaultDelay));
}

TEST_F(RtcpBuilderTest, RtcpReceiverReportWithOversizedFrameLog) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;

  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);

  RtcpReportBlock report_block = GetReportBlock();

  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  p.AddReceiverLog(kSendingSsrc);

  int num_events = kMaxEventsPerRTCP;

  EXPECT_LE(num_events, static_cast<int>(kRtcpMaxReceiverLogMessages));
  p.AddReceiverFrameLog(
      kRtpTimestamp + 2345,
      num_events,
      kTimeBaseMs);
  for (int i = 0; i < num_events; i++) {
    p.AddReceiverEventLog(
        kLostPacketId1,
        PACKET_RECEIVED,
        static_cast<uint16>(kTimeDelayMs * i));
  }


  ReceiverRtcpEventSubscriber event_subscriber(500, VIDEO_EVENT);

  for (size_t i = 0; i < kRtcpMaxReceiverLogMessages; ++i) {
    PacketEvent packet_event;
    packet_event.rtp_timestamp = kRtpTimestamp + 2345;
    packet_event.type = PACKET_RECEIVED;
    packet_event.media_type = VIDEO_EVENT;
    packet_event.timestamp = testing_clock.NowTicks();
    packet_event.packet_id = kLostPacketId1;
    event_subscriber.OnReceivePacketEvent(packet_event);
    testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  }

  ReceiverRtcpEventSubscriber::RtcpEvents rtcp_events;
  event_subscriber.GetRtcpEventsWithRedundancy(&rtcp_events);

  ExpectPacketEQ(p.GetPacket().Pass(),
                 rtcp_builder_->BuildRtcpFromReceiver(
                     &report_block,
                     NULL,
                     NULL,
                     &rtcp_events,
                     kDefaultDelay));
}

TEST_F(RtcpBuilderTest, RtcpReceiverReportWithTooManyLogFrames) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;

  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);

  RtcpReportBlock report_block = GetReportBlock();

  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  p.AddReceiverLog(kSendingSsrc);

  int num_events = kMaxEventsPerRTCP;

  for (int i = 0; i < num_events; i++) {
    p.AddReceiverFrameLog(kRtpTimestamp + i, 1, kTimeBaseMs + i * kTimeDelayMs);
    p.AddReceiverEventLog(0, FRAME_ACK_SENT, 0);
  }

  ReceiverRtcpEventSubscriber event_subscriber(500, VIDEO_EVENT);

  for (size_t i = 0; i < kRtcpMaxReceiverLogMessages; ++i) {
    FrameEvent frame_event;
    frame_event.rtp_timestamp = kRtpTimestamp + static_cast<int>(i);
    frame_event.type = FRAME_ACK_SENT;
    frame_event.media_type = VIDEO_EVENT;
    frame_event.timestamp = testing_clock.NowTicks();
    event_subscriber.OnReceiveFrameEvent(frame_event);
    testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  }

  ReceiverRtcpEventSubscriber::RtcpEvents rtcp_events;
  event_subscriber.GetRtcpEventsWithRedundancy(&rtcp_events);

  ExpectPacketEQ(p.GetPacket().Pass(),
                 rtcp_builder_->BuildRtcpFromReceiver(
                     &report_block,
                     NULL,
                     NULL,
                     &rtcp_events,
                     kDefaultDelay));
}

TEST_F(RtcpBuilderTest, RtcpReceiverReportWithOldLogFrames) {
  static const uint32 kTimeBaseMs = 12345678;

  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);

  RtcpReportBlock report_block = GetReportBlock();

  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  p.AddReceiverLog(kSendingSsrc);

  // Log 11 events for a single frame, each |kTimeBetweenEventsMs| apart.
  // Only last 10 events will be sent because the first event is more than
  // 4095 milliseconds away from latest event.
  const int kTimeBetweenEventsMs = 410;
  p.AddReceiverFrameLog(kRtpTimestamp, 10, kTimeBaseMs + kTimeBetweenEventsMs);
  for (int i = 0; i < 10; ++i) {
    p.AddReceiverEventLog(0, FRAME_ACK_SENT, i * kTimeBetweenEventsMs);
  }

  ReceiverRtcpEventSubscriber event_subscriber(500, VIDEO_EVENT);
  for (int i = 0; i < 11; ++i) {
    FrameEvent frame_event;
    frame_event.rtp_timestamp = kRtpTimestamp;
    frame_event.type = FRAME_ACK_SENT;
    frame_event.media_type = VIDEO_EVENT;
    frame_event.timestamp = testing_clock.NowTicks();
    event_subscriber.OnReceiveFrameEvent(frame_event);
    testing_clock.Advance(
        base::TimeDelta::FromMilliseconds(kTimeBetweenEventsMs));
  }

  ReceiverRtcpEventSubscriber::RtcpEvents rtcp_events;
  event_subscriber.GetRtcpEventsWithRedundancy(&rtcp_events);

  ExpectPacketEQ(p.GetPacket().Pass(),
                 rtcp_builder_->BuildRtcpFromReceiver(
                     &report_block,
                     NULL,
                     NULL,
                     &rtcp_events,
                     kDefaultDelay));
}

TEST_F(RtcpBuilderTest, RtcpReceiverReportRedundancy) {
  uint32 time_base_ms = 12345678;
  int kTimeBetweenEventsMs = 10;

  RtcpReportBlock report_block = GetReportBlock();

  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(time_base_ms));

  ReceiverRtcpEventSubscriber event_subscriber(500, VIDEO_EVENT);
  size_t packet_count = kNumResends * kResendDelay + 10;
  for (size_t i = 0; i < packet_count; i++) {
    TestRtcpPacketBuilder p;
    p.AddRr(kSendingSsrc, 1);
    p.AddRb(kMediaSsrc);

    p.AddReceiverLog(kSendingSsrc);

    int num_events = (i + kResendDelay) / kResendDelay;
    num_events = std::min<int>(num_events, kNumResends);
    p.AddReceiverFrameLog(kRtpTimestamp, num_events,
        time_base_ms - (num_events - 1) * kResendDelay *
        kTimeBetweenEventsMs);
    for (int i = 0; i < num_events; i++) {
      p.AddReceiverEventLog(0, FRAME_ACK_SENT,
        i * kResendDelay * kTimeBetweenEventsMs);
    }

    FrameEvent frame_event;
    frame_event.rtp_timestamp = kRtpTimestamp;
    frame_event.type = FRAME_ACK_SENT;
    frame_event.media_type = VIDEO_EVENT;
    frame_event.timestamp = testing_clock.NowTicks();
    event_subscriber.OnReceiveFrameEvent(frame_event);

    ReceiverRtcpEventSubscriber::RtcpEvents rtcp_events;
    event_subscriber.GetRtcpEventsWithRedundancy(&rtcp_events);

    ExpectPacketEQ(p.GetPacket().Pass(),
                   rtcp_builder_->BuildRtcpFromReceiver(
                       &report_block,
                       NULL,
                       NULL,
                       &rtcp_events,
                       kDefaultDelay));

    testing_clock.Advance(
        base::TimeDelta::FromMilliseconds(kTimeBetweenEventsMs));
    time_base_ms += kTimeBetweenEventsMs;
  }
}

TEST_F(RtcpBuilderTest, RtcpSenderReport) {
  RtcpSenderInfo sender_info;
  sender_info.ntp_seconds = kNtpHigh;
  sender_info.ntp_fraction = kNtpLow;
  sender_info.rtp_timestamp = kRtpTimestamp;
  sender_info.send_packet_count = kSendPacketCount;
  sender_info.send_octet_count = kSendOctetCount;

  // Sender report.
  TestRtcpPacketBuilder p;
  p.AddSr(kSendingSsrc, 0);

  ExpectPacketEQ(p.GetPacket().Pass(),
                 rtcp_builder_->BuildRtcpFromSender(sender_info));
}

}  // namespace cast
}  // namespace media
