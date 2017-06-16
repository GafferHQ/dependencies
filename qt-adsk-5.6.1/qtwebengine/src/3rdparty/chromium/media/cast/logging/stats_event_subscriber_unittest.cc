// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/stats_event_subscriber.h"
#include "media/cast/test/fake_receiver_time_offset_estimator.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kReceiverOffsetSecs = 100;
}

namespace media {
namespace cast {

class StatsEventSubscriberTest : public ::testing::Test {
 protected:
  StatsEventSubscriberTest()
      : sender_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(sender_clock_)),
        cast_environment_(new CastEnvironment(
            scoped_ptr<base::TickClock>(sender_clock_).Pass(),
            task_runner_,
            task_runner_,
            task_runner_)),
        fake_offset_estimator_(
            base::TimeDelta::FromSeconds(kReceiverOffsetSecs)) {
    receiver_clock_.Advance(base::TimeDelta::FromSeconds(kReceiverOffsetSecs));
    cast_environment_->Logging()->AddRawEventSubscriber(
        &fake_offset_estimator_);
  }

  ~StatsEventSubscriberTest() override {
    if (subscriber_.get())
      cast_environment_->Logging()->RemoveRawEventSubscriber(subscriber_.get());
    cast_environment_->Logging()->RemoveRawEventSubscriber(
        &fake_offset_estimator_);
  }

  void AdvanceClocks(base::TimeDelta delta) {
    sender_clock_->Advance(delta);
    receiver_clock_.Advance(delta);
  }

  void Init(EventMediaType event_media_type) {
    DCHECK(!subscriber_.get());
    subscriber_.reset(new StatsEventSubscriber(
        event_media_type, cast_environment_->Clock(), &fake_offset_estimator_));
    cast_environment_->Logging()->AddRawEventSubscriber(subscriber_.get());
  }

  base::SimpleTestTickClock* sender_clock_;  // Owned by CastEnvironment.
  base::SimpleTestTickClock receiver_clock_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  test::FakeReceiverTimeOffsetEstimator fake_offset_estimator_;
  scoped_ptr<StatsEventSubscriber> subscriber_;
};

TEST_F(StatsEventSubscriberTest, CaptureEncode) {
  Init(VIDEO_EVENT);

  uint32 rtp_timestamp = 0;
  uint32 frame_id = 0;
  int extra_frames = 50;
  // Only the first |extra_frames| frames logged will be taken into account
  // when computing dropped frames.
  int num_frames = StatsEventSubscriber::kMaxFrameInfoMapSize + 50;
  int dropped_frames = 0;
  base::TimeTicks start_time = sender_clock_->NowTicks();
  // Drop half the frames during the encode step.
  for (int i = 0; i < num_frames; i++) {
    cast_environment_->Logging()->InsertFrameEvent(sender_clock_->NowTicks(),
                                                   FRAME_CAPTURE_BEGIN,
                                                   VIDEO_EVENT,
                                                   rtp_timestamp,
                                                   frame_id);
    AdvanceClocks(base::TimeDelta::FromMicroseconds(10));
    cast_environment_->Logging()->InsertFrameEvent(sender_clock_->NowTicks(),
                                                   FRAME_CAPTURE_END,
                                                   VIDEO_EVENT,
                                                   rtp_timestamp,
                                                   frame_id);
    if (i % 2 == 0) {
      AdvanceClocks(base::TimeDelta::FromMicroseconds(10));
      cast_environment_->Logging()->InsertEncodedFrameEvent(
          sender_clock_->NowTicks(),
          FRAME_ENCODED,
          VIDEO_EVENT,
          rtp_timestamp,
          frame_id,
          1024,
          true,
          5678,
          9.10,
          11.12);
    } else if (i < extra_frames) {
      dropped_frames++;
    }
    AdvanceClocks(base::TimeDelta::FromMicroseconds(34567));
    rtp_timestamp += 90;
    frame_id++;
  }

  base::TimeTicks end_time = sender_clock_->NowTicks();

  StatsEventSubscriber::StatsMap stats_map;
  subscriber_->GetStatsInternal(&stats_map);

  StatsEventSubscriber::StatsMap::iterator it =
      stats_map.find(StatsEventSubscriber::CAPTURE_FPS);
  ASSERT_TRUE(it != stats_map.end());

  base::TimeDelta duration = end_time - start_time;
  EXPECT_DOUBLE_EQ(
      it->second,
      static_cast<double>(num_frames) / duration.InMillisecondsF() * 1000);

  it = stats_map.find(StatsEventSubscriber::NUM_FRAMES_CAPTURED);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, static_cast<double>(num_frames));

  it = stats_map.find(StatsEventSubscriber::NUM_FRAMES_DROPPED_BY_ENCODER);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, static_cast<double>(dropped_frames));

  it = stats_map.find(StatsEventSubscriber::AVG_CAPTURE_LATENCY_MS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, static_cast<double>(0.01));
}

TEST_F(StatsEventSubscriberTest, Encode) {
  Init(VIDEO_EVENT);

  uint32 rtp_timestamp = 0;
  uint32 frame_id = 0;
  int num_frames = 10;
  base::TimeTicks start_time = sender_clock_->NowTicks();
  AdvanceClocks(base::TimeDelta::FromMicroseconds(35678));
  base::TimeTicks first_event_time = sender_clock_->NowTicks();
  base::TimeTicks last_event_time;
  int total_size = 0;
  for (int i = 0; i < num_frames; i++) {
    int size = 1000 + base::RandInt(-100, 100);
    total_size += size;
    cast_environment_->Logging()->InsertEncodedFrameEvent(
        sender_clock_->NowTicks(),
        FRAME_ENCODED, VIDEO_EVENT,
        rtp_timestamp,
        frame_id,
        size,
        true,
        5678,
        9.10,
        11.12);
    last_event_time = sender_clock_->NowTicks();

    AdvanceClocks(base::TimeDelta::FromMicroseconds(35678));
    rtp_timestamp += 90;
    frame_id++;
  }

  base::TimeTicks end_time = sender_clock_->NowTicks();

  StatsEventSubscriber::StatsMap stats_map;
  subscriber_->GetStatsInternal(&stats_map);

  StatsEventSubscriber::StatsMap::iterator it =
      stats_map.find(StatsEventSubscriber::ENCODE_FPS);
  ASSERT_TRUE(it != stats_map.end());

  base::TimeDelta duration = end_time - start_time;
  EXPECT_DOUBLE_EQ(
      it->second,
      static_cast<double>(num_frames) / duration.InMillisecondsF() * 1000);

  it = stats_map.find(StatsEventSubscriber::ENCODE_KBPS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second,
              static_cast<double>(total_size) / duration.InMillisecondsF() * 8);

  it = stats_map.find(StatsEventSubscriber::FIRST_EVENT_TIME_MS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(
      it->second,
      (first_event_time - base::TimeTicks::UnixEpoch()).InMillisecondsF());

  it = stats_map.find(StatsEventSubscriber::LAST_EVENT_TIME_MS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(
      it->second,
      (last_event_time - base::TimeTicks::UnixEpoch()).InMillisecondsF());
}

TEST_F(StatsEventSubscriberTest, Decode) {
  Init(VIDEO_EVENT);

  uint32 rtp_timestamp = 0;
  uint32 frame_id = 0;
  int num_frames = 10;
  base::TimeTicks start_time = sender_clock_->NowTicks();
  for (int i = 0; i < num_frames; i++) {
    cast_environment_->Logging()->InsertFrameEvent(receiver_clock_.NowTicks(),
                                                   FRAME_DECODED, VIDEO_EVENT,
                                                   rtp_timestamp,
                                                   frame_id);

    AdvanceClocks(base::TimeDelta::FromMicroseconds(36789));
    rtp_timestamp += 90;
    frame_id++;
  }

  base::TimeTicks end_time = sender_clock_->NowTicks();

  StatsEventSubscriber::StatsMap stats_map;
  subscriber_->GetStatsInternal(&stats_map);

  StatsEventSubscriber::StatsMap::iterator it =
      stats_map.find(StatsEventSubscriber::DECODE_FPS);
  ASSERT_TRUE(it != stats_map.end());

  base::TimeDelta duration = end_time - start_time;
  EXPECT_DOUBLE_EQ(
      it->second,
      static_cast<double>(num_frames) / duration.InMillisecondsF() * 1000);
}

TEST_F(StatsEventSubscriberTest, PlayoutDelay) {
  Init(VIDEO_EVENT);

  uint32 rtp_timestamp = 0;
  uint32 frame_id = 0;
  int num_frames = 10;
  int late_frames = 0;
  for (int i = 0, delay_ms = -50; i < num_frames; i++, delay_ms += 10) {
    base::TimeDelta delay = base::TimeDelta::FromMilliseconds(delay_ms);
    if (delay_ms > 0)
      late_frames++;
    cast_environment_->Logging()->InsertFrameEventWithDelay(
        receiver_clock_.NowTicks(),
        FRAME_PLAYOUT,
        VIDEO_EVENT,
        rtp_timestamp,
        frame_id,
        delay);

    AdvanceClocks(base::TimeDelta::FromMicroseconds(37890));
    rtp_timestamp += 90;
    frame_id++;
  }

  StatsEventSubscriber::StatsMap stats_map;
  subscriber_->GetStatsInternal(&stats_map);

  StatsEventSubscriber::StatsMap::iterator it = stats_map.find(
      StatsEventSubscriber::NUM_FRAMES_LATE);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, late_frames);
}

TEST_F(StatsEventSubscriberTest, E2ELatency) {
  Init(VIDEO_EVENT);

  uint32 rtp_timestamp = 0;
  uint32 frame_id = 0;
  int num_frames = 10;
  base::TimeDelta total_latency;
  for (int i = 0; i < num_frames; i++) {
    cast_environment_->Logging()->InsertFrameEvent(sender_clock_->NowTicks(),
                                                   FRAME_CAPTURE_BEGIN,
                                                   VIDEO_EVENT,
                                                   rtp_timestamp,
                                                   frame_id);

    int latency_micros = 100000 + base::RandInt(-5000, 50000);
    base::TimeDelta latency = base::TimeDelta::FromMicroseconds(latency_micros);
    AdvanceClocks(latency);

    int delay_micros = base::RandInt(-50000, 50000);
    base::TimeDelta delay = base::TimeDelta::FromMilliseconds(delay_micros);
    total_latency += latency;

    cast_environment_->Logging()->InsertFrameEventWithDelay(
        receiver_clock_.NowTicks(),
        FRAME_PLAYOUT,
        VIDEO_EVENT,
        rtp_timestamp,
        frame_id,
        delay);

    rtp_timestamp += 90;
    frame_id++;
  }

  StatsEventSubscriber::StatsMap stats_map;
  subscriber_->GetStatsInternal(&stats_map);

  StatsEventSubscriber::StatsMap::iterator it =
      stats_map.find(StatsEventSubscriber::AVG_E2E_LATENCY_MS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(
      it->second, total_latency.InMillisecondsF() / num_frames);
}

TEST_F(StatsEventSubscriberTest, Packets) {
  Init(VIDEO_EVENT);

  uint32 rtp_timestamp = 0;
  int num_packets = 10;
  int num_latency_recorded_packets = 0;
  base::TimeTicks start_time = sender_clock_->NowTicks();
  int total_size = 0;
  int retransmit_total_size = 0;
  base::TimeDelta total_network_latency;
  base::TimeDelta total_queueing_latency;
  base::TimeDelta total_packet_latency;
  int num_packets_transmitted = 0;
  int num_packets_received = 0;
  int num_packets_retransmitted = 0;
  int num_packets_rtx_rejected = 0;

  base::TimeTicks sender_encoded_time = sender_clock_->NowTicks();
  base::TimeTicks receiver_encoded_time = receiver_clock_.NowTicks();
  cast_environment_->Logging()->InsertFrameEvent(sender_encoded_time,
                                                 FRAME_ENCODED,
                                                 VIDEO_EVENT,
                                                 rtp_timestamp,
                                                 0);

  // Every 2nd packet will be retransmitted once.
  // Every 4th packet will be retransmitted twice.
  // Every 8th packet will be retransmitted 3 times + 1 rejected retransmission.
  for (int i = 0; i < num_packets; i++) {
    int size = 1000 + base::RandInt(-100, 100);
    total_size += size;

    cast_environment_->Logging()->InsertPacketEvent(sender_clock_->NowTicks(),
                                                    PACKET_SENT_TO_NETWORK,
                                                    VIDEO_EVENT,
                                                    rtp_timestamp,
                                                    0,
                                                    i,
                                                    num_packets - 1,
                                                    size);
    num_packets_transmitted++;
    total_queueing_latency += sender_clock_->NowTicks() - sender_encoded_time;

    int latency_micros = 20000 + base::RandInt(-10000, 10000);
    base::TimeDelta latency = base::TimeDelta::FromMicroseconds(latency_micros);
    // Latency is only recorded for packets that aren't retransmitted.
    if (i % 2 != 0) {
      total_network_latency += latency;
      total_packet_latency +=
          receiver_clock_.NowTicks() - receiver_encoded_time + latency;
      num_latency_recorded_packets++;
    }

    AdvanceClocks(latency);

    base::TimeTicks received_time = receiver_clock_.NowTicks();

    // Retransmission 1.
    AdvanceClocks(base::TimeDelta::FromMicroseconds(12345));
    if (i % 2 == 0) {
      cast_environment_->Logging()->InsertPacketEvent(
          receiver_clock_.NowTicks(),
          PACKET_RETRANSMITTED,
          VIDEO_EVENT,
          rtp_timestamp,
          0,
          i,
          num_packets - 1,
          size);
      retransmit_total_size += size;
      num_packets_transmitted++;
      num_packets_retransmitted++;
    }

    // Retransmission 2.
    AdvanceClocks(base::TimeDelta::FromMicroseconds(13456));
    if (i % 4 == 0) {
      cast_environment_->Logging()->InsertPacketEvent(
          receiver_clock_.NowTicks(),
          PACKET_RETRANSMITTED,
          VIDEO_EVENT,
          rtp_timestamp,
          0,
          i,
          num_packets - 1,
          size);
      retransmit_total_size += size;
      num_packets_transmitted++;
      num_packets_retransmitted++;
    }

    // Retransmission 3.
    AdvanceClocks(base::TimeDelta::FromMicroseconds(14567));
    if (i % 8 == 0) {
      cast_environment_->Logging()->InsertPacketEvent(
          receiver_clock_.NowTicks(),
          PACKET_RETRANSMITTED,
          VIDEO_EVENT,
          rtp_timestamp,
          0,
          i,
          num_packets - 1,
          size);
      cast_environment_->Logging()->InsertPacketEvent(
          receiver_clock_.NowTicks(),
          PACKET_RTX_REJECTED,
          VIDEO_EVENT,
          rtp_timestamp,
          0,
          i,
          num_packets - 1,
          size);
      retransmit_total_size += size;
      num_packets_transmitted++;
      num_packets_retransmitted++;
      num_packets_rtx_rejected++;
    }

    cast_environment_->Logging()->InsertPacketEvent(received_time,
                                                    PACKET_RECEIVED,
                                                    VIDEO_EVENT,
                                                    rtp_timestamp,
                                                    0,
                                                    i,
                                                    num_packets - 1,
                                                    size);
    num_packets_received++;
  }

  base::TimeTicks end_time = sender_clock_->NowTicks();
  base::TimeDelta duration = end_time - start_time;

  StatsEventSubscriber::StatsMap stats_map;
  subscriber_->GetStatsInternal(&stats_map);

  // Measure AVG_NETWORK_LATENCY_MS, TRANSMISSION_KBPS, RETRANSMISSION_KBPS.
  StatsEventSubscriber::StatsMap::iterator it =
      stats_map.find(StatsEventSubscriber::AVG_NETWORK_LATENCY_MS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(
      it->second,
      total_network_latency.InMillisecondsF() / num_latency_recorded_packets);

  it = stats_map.find(StatsEventSubscriber::AVG_QUEUEING_LATENCY_MS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(
      it->second,
      total_queueing_latency.InMillisecondsF() / num_packets);

  it = stats_map.find(StatsEventSubscriber::AVG_PACKET_LATENCY_MS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(
      it->second,
      total_packet_latency.InMillisecondsF() / num_latency_recorded_packets);

  it = stats_map.find(StatsEventSubscriber::TRANSMISSION_KBPS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second,
              static_cast<double>(total_size) / duration.InMillisecondsF() * 8);

  it = stats_map.find(StatsEventSubscriber::RETRANSMISSION_KBPS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second,
              static_cast<double>(retransmit_total_size) /
                  duration.InMillisecondsF() * 8);

  it = stats_map.find(StatsEventSubscriber::NUM_PACKETS_SENT);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, static_cast<double>(num_packets));

  it = stats_map.find(StatsEventSubscriber::NUM_PACKETS_RECEIVED);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, static_cast<double>(num_packets_received));

  it = stats_map.find(StatsEventSubscriber::NUM_PACKETS_RETRANSMITTED);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, static_cast<double>(num_packets_retransmitted));

  it = stats_map.find(StatsEventSubscriber::NUM_PACKETS_RTX_REJECTED);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, static_cast<double>(num_packets_rtx_rejected));
}

bool CheckHistogramHasValue(base::ListValue* values,
                            const std::string& bucket, int expected_count) {
  for (size_t i = 0; i < values->GetSize(); ++i) {
    const base::DictionaryValue* dict = NULL;
    values->GetDictionary(i, &dict);
    if (!dict->HasKey(bucket))
      continue;
    int bucket_count = 0;
    if (!dict->GetInteger(bucket, &bucket_count))
      return false;
    return bucket_count == expected_count;
  }
  return false;
}

TEST_F(StatsEventSubscriberTest, Histograms) {
  Init(VIDEO_EVENT);
  AdvanceClocks(base::TimeDelta::FromMilliseconds(123));

  uint32 rtp_timestamp = 123;
  uint32 frame_id = 0;

  // 10 Frames with capture latency in the bucket of "10-14"ms.
  // 10 Frames with encode time in the bucket of "15-19"ms.
  for (int i = 0; i < 10; ++i) {
    ++frame_id;
    ++rtp_timestamp;
    cast_environment_->Logging()->InsertFrameEvent(sender_clock_->NowTicks(),
                                                   FRAME_CAPTURE_BEGIN,
                                                   VIDEO_EVENT,
                                                   rtp_timestamp,
                                                   frame_id);
    AdvanceClocks(base::TimeDelta::FromMilliseconds(10));
    cast_environment_->Logging()->InsertFrameEvent(sender_clock_->NowTicks(),
                                                   FRAME_CAPTURE_END,
                                                   VIDEO_EVENT,
                                                   rtp_timestamp,
                                                   frame_id);
    AdvanceClocks(base::TimeDelta::FromMilliseconds(15));
    cast_environment_->Logging()->InsertEncodedFrameEvent(
        sender_clock_->NowTicks(),
        FRAME_ENCODED,
        VIDEO_EVENT,
        rtp_timestamp,
        frame_id,
        1024,
        true,
        5678,
        9.10,
        11.12);
  }

  // Send 3 packets for the last frame.
  // Queueing latencies are 100ms, 200ms and 300ms.
  for (int i = 0; i < 3; ++i) {
    AdvanceClocks(base::TimeDelta::FromMilliseconds(100));
    cast_environment_->Logging()->InsertPacketEvent(sender_clock_->NowTicks(),
                                                    PACKET_SENT_TO_NETWORK,
                                                    VIDEO_EVENT,
                                                    rtp_timestamp,
                                                    0,
                                                    i,
                                                    2,
                                                    123);
  }

  // Receive 3 packets for the last frame.
  // Network latencies are 100ms, 200ms and 300ms.
  // Packet latencies are 400ms.
  AdvanceClocks(base::TimeDelta::FromMilliseconds(100));
  for (int i = 0; i < 3; ++i) {
    cast_environment_->Logging()->InsertPacketEvent(receiver_clock_.NowTicks(),
                                                    PACKET_RECEIVED,
                                                    VIDEO_EVENT,
                                                    rtp_timestamp,
                                                    0,
                                                    i,
                                                    2,
                                                    123);
  }

  cast_environment_->Logging()->InsertFrameEventWithDelay(
      receiver_clock_.NowTicks(),
      FRAME_PLAYOUT,
      VIDEO_EVENT,
      rtp_timestamp,
      frame_id,
      base::TimeDelta::FromMilliseconds(100));

  StatsEventSubscriber::SimpleHistogram* histogram;
  scoped_ptr<base::ListValue> values;

  histogram = subscriber_->GetHistogramForTesting(
      StatsEventSubscriber::CAPTURE_LATENCY_MS_HISTO);
  ASSERT_TRUE(histogram);
  values = histogram->GetHistogram().Pass();
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "10-14", 10));

  histogram = subscriber_->GetHistogramForTesting(
      StatsEventSubscriber::ENCODE_TIME_MS_HISTO);
  ASSERT_TRUE(histogram);
  values = histogram->GetHistogram().Pass();
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "15-19", 10));

  histogram = subscriber_->GetHistogramForTesting(
      StatsEventSubscriber::QUEUEING_LATENCY_MS_HISTO);
  ASSERT_TRUE(histogram);
  values = histogram->GetHistogram().Pass();
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "100-119", 1));
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "200-219", 1));
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "300-319", 1));

  histogram = subscriber_->GetHistogramForTesting(
      StatsEventSubscriber::NETWORK_LATENCY_MS_HISTO);
  ASSERT_TRUE(histogram);
  values = histogram->GetHistogram().Pass();
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "100-119", 1));
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "200-219", 1));
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "300-319", 1));

  histogram = subscriber_->GetHistogramForTesting(
      StatsEventSubscriber::PACKET_LATENCY_MS_HISTO);
  ASSERT_TRUE(histogram);
  values = histogram->GetHistogram().Pass();
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "400-419", 3));

  histogram = subscriber_->GetHistogramForTesting(
      StatsEventSubscriber::LATE_FRAME_MS_HISTO);
  ASSERT_TRUE(histogram);
  values = histogram->GetHistogram().Pass();
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "100-119", 1));
}

}  // namespace cast
}  // namespace media
