// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video_capture_oracle.h"

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

base::TimeTicks InitialTestTimeTicks() {
  return base::TimeTicks() + base::TimeDelta::FromSeconds(1);
}

base::TimeDelta Get30HzPeriod() {
  return base::TimeDelta::FromSeconds(1) / 30;
}

gfx::Size Get720pSize() {
  return gfx::Size(1280, 720);
}

}  // namespace

// Tests that VideoCaptureOracle filters out events whose timestamps are
// decreasing.
TEST(VideoCaptureOracleTest, EnforcesEventTimeMonotonicity) {
  const gfx::Rect damage_rect(Get720pSize());
  const base::TimeDelta event_increment = Get30HzPeriod() * 2;

  VideoCaptureOracle oracle(Get30HzPeriod(),
                            Get720pSize(),
                            media::RESOLUTION_POLICY_FIXED_RESOLUTION,
                            false);

  base::TimeTicks t = InitialTestTimeTicks();
  for (int i = 0; i < 10; ++i) {
    t += event_increment;
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        damage_rect, t));
  }

  base::TimeTicks furthest_event_time = t;
  for (int i = 0; i < 10; ++i) {
    t -= event_increment;
    ASSERT_FALSE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        damage_rect, t));
  }

  t = furthest_event_time;
  for (int i = 0; i < 10; ++i) {
    t += event_increment;
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        damage_rect, t));
  }
}

// Tests that VideoCaptureOracle is enforcing the requirement that
// successfully captured frames are delivered in order.  Otherwise, downstream
// consumers could be tripped-up by out-of-order frames or frame timestamps.
TEST(VideoCaptureOracleTest, EnforcesFramesDeliveredInOrder) {
  const gfx::Rect damage_rect(Get720pSize());
  const base::TimeDelta event_increment = Get30HzPeriod() * 2;

  VideoCaptureOracle oracle(Get30HzPeriod(),
                            Get720pSize(),
                            media::RESOLUTION_POLICY_FIXED_RESOLUTION,
                            false);

  // Most basic scenario: Frames delivered one at a time, with no additional
  // captures in-between deliveries.
  base::TimeTicks t = InitialTestTimeTicks();
  int last_frame_number;
  base::TimeTicks ignored;
  for (int i = 0; i < 10; ++i) {
    t += event_increment;
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        damage_rect, t));
    last_frame_number = oracle.RecordCapture(0.0);
    ASSERT_TRUE(oracle.CompleteCapture(last_frame_number, true, &ignored));
  }

  // Basic pipelined scenario: More than one frame in-flight at delivery points.
  for (int i = 0; i < 50; ++i) {
    const int num_in_flight = 1 + i % 3;
    for (int j = 0; j < num_in_flight; ++j) {
      t += event_increment;
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate,
          damage_rect, t));
      last_frame_number = oracle.RecordCapture(0.0);
    }
    for (int j = num_in_flight - 1; j >= 0; --j) {
      ASSERT_TRUE(
          oracle.CompleteCapture(last_frame_number - j, true, &ignored));
    }
  }

  // Pipelined scenario with successful out-of-order delivery attempts
  // rejected.
  for (int i = 0; i < 50; ++i) {
    const int num_in_flight = 1 + i % 3;
    for (int j = 0; j < num_in_flight; ++j) {
      t += event_increment;
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate,
          damage_rect, t));
      last_frame_number = oracle.RecordCapture(0.0);
    }
    ASSERT_TRUE(oracle.CompleteCapture(last_frame_number, true, &ignored));
    for (int j = 1; j < num_in_flight; ++j) {
      ASSERT_FALSE(
          oracle.CompleteCapture(last_frame_number - j, true, &ignored));
    }
  }

  // Pipelined scenario with successful delivery attempts accepted after an
  // unsuccessful out of order delivery attempt.
  for (int i = 0; i < 50; ++i) {
    const int num_in_flight = 1 + i % 3;
    for (int j = 0; j < num_in_flight; ++j) {
      t += event_increment;
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate,
          damage_rect, t));
      last_frame_number = oracle.RecordCapture(0.0);
    }
    // Report the last frame as an out of order failure.
    ASSERT_FALSE(oracle.CompleteCapture(last_frame_number, false, &ignored));
    for (int j = 1; j < num_in_flight - 1; ++j) {
      ASSERT_TRUE(
          oracle.CompleteCapture(last_frame_number - j, true, &ignored));
    }

  }
}

// Tests that VideoCaptureOracle transitions between using its two samplers in a
// way that does not introduce severe jank, pauses, etc.
TEST(VideoCaptureOracleTest, TransitionsSmoothlyBetweenSamplers) {
  const gfx::Rect animation_damage_rect(Get720pSize());
  const base::TimeDelta event_increment = Get30HzPeriod() * 2;

  VideoCaptureOracle oracle(Get30HzPeriod(),
                            Get720pSize(),
                            media::RESOLUTION_POLICY_FIXED_RESOLUTION,
                            false);

  // Run sequences of animation events and non-animation events through the
  // oracle.  As the oracle transitions between each sampler, make sure the
  // frame timestamps won't trip-up downstream consumers.
  base::TimeTicks t = InitialTestTimeTicks();
  base::TimeTicks last_frame_timestamp;
  for (int i = 0; i < 1000; ++i) {
    t += event_increment;

    // For every 100 events, provide 50 that will cause the
    // AnimatedContentSampler to lock-in, followed by 50 that will cause it to
    // lock-out (i.e., the oracle will use the SmoothEventSampler instead).
    const bool provide_animated_content_event =
        (i % 100) >= 25 && (i % 100) < 75;

    // Only the few events that trigger the lock-out transition should be
    // dropped, because the AnimatedContentSampler doesn't yet realize the
    // animation ended.  Otherwise, the oracle should always decide to sample
    // because one of its samplers says to.
    const bool require_oracle_says_sample = (i % 100) < 75 || (i % 100) >= 78;
    const bool oracle_says_sample = oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        provide_animated_content_event ? animation_damage_rect : gfx::Rect(),
        t);
    if (require_oracle_says_sample)
      ASSERT_TRUE(oracle_says_sample);
    if (!oracle_says_sample) {
      ASSERT_EQ(base::TimeDelta(), oracle.estimated_frame_duration());
      continue;
    }
    ASSERT_LT(base::TimeDelta(), oracle.estimated_frame_duration());

    const int frame_number = oracle.RecordCapture(0.0);

    base::TimeTicks frame_timestamp;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &frame_timestamp));
    ASSERT_FALSE(frame_timestamp.is_null());
    if (!last_frame_timestamp.is_null()) {
      const base::TimeDelta delta = frame_timestamp - last_frame_timestamp;
      EXPECT_LE(event_increment.InMicroseconds(), delta.InMicroseconds());
      // Right after the AnimatedContentSampler lock-out transition, there were
      // a few frames dropped, so allow a gap in the timestamps.  Otherwise, the
      // delta between frame timestamps should never be more than 2X the
      // |event_increment|.
      const base::TimeDelta max_acceptable_delta = (i % 100) == 78 ?
          event_increment * 5 : event_increment * 2;
      EXPECT_GE(max_acceptable_delta.InMicroseconds(), delta.InMicroseconds());
    }
    last_frame_timestamp = frame_timestamp;
  }
}

// Tests that VideoCaptureOracle prevents timer polling from initiating
// simultaneous captures.
TEST(VideoCaptureOracleTest, SamplesOnlyOneOverdueFrameAtATime) {
  const base::TimeDelta vsync_interval =
      base::TimeDelta::FromSeconds(1) / 60;
  const base::TimeDelta timer_interval = base::TimeDelta::FromMilliseconds(
      VideoCaptureOracle::kMinTimerPollPeriodMillis);

  VideoCaptureOracle oracle(Get30HzPeriod(),
                            Get720pSize(),
                            media::RESOLUTION_POLICY_FIXED_RESOLUTION,
                            false);

  // Have the oracle observe some compositor events.  Simulate that each capture
  // completes successfully.
  base::TimeTicks t = InitialTestTimeTicks();
  base::TimeTicks ignored;
  bool did_complete_a_capture = false;
  for (int i = 0; i < 10; ++i) {
    t += vsync_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t)) {
      ASSERT_TRUE(
          oracle.CompleteCapture(oracle.RecordCapture(0.0), true, &ignored));
      did_complete_a_capture = true;
    }
  }
  ASSERT_TRUE(did_complete_a_capture);

  // Start one more compositor-based capture, but do not notify of completion
  // yet.
  for (int i = 0; i <= 10; ++i) {
    ASSERT_GT(10, i) << "BUG: Seems like it'll never happen!";
    t += vsync_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t)) {
      break;
    }
  }
  int frame_number = oracle.RecordCapture(0.0);

  // Stop providing the compositor events and start providing timer polling
  // events.  No overdue samplings should be recommended because of the
  // not-yet-complete compositor-based capture.
  for (int i = 0; i < 10; ++i) {
    t += timer_interval;
    ASSERT_FALSE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kTimerPoll, gfx::Rect(), t));
  }

  // Now, complete the oustanding compositor-based capture and continue
  // providing timer polling events.  The oracle should start recommending
  // sampling again.
  ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  did_complete_a_capture = false;
  for (int i = 0; i < 10; ++i) {
    t += timer_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kTimerPoll, gfx::Rect(), t)) {
      ASSERT_TRUE(
          oracle.CompleteCapture(oracle.RecordCapture(0.0), true, &ignored));
      did_complete_a_capture = true;
    }
  }
  ASSERT_TRUE(did_complete_a_capture);

  // Start one more timer-based capture, but do not notify of completion yet.
  for (int i = 0; i <= 10; ++i) {
    ASSERT_GT(10, i) << "BUG: Seems like it'll never happen!";
    t += timer_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kTimerPoll, gfx::Rect(), t)) {
      break;
    }
  }
  frame_number = oracle.RecordCapture(0.0);

  // Confirm that the oracle does not recommend sampling until the outstanding
  // timer-based capture completes.
  for (int i = 0; i < 10; ++i) {
    t += timer_interval;
    ASSERT_FALSE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kTimerPoll, gfx::Rect(), t));
  }
  ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  for (int i = 0; i <= 10; ++i) {
    ASSERT_GT(10, i) << "BUG: Seems like it'll never happen!";
    t += timer_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kTimerPoll, gfx::Rect(), t)) {
      break;
    }
  }
}

// Tests that VideoCaptureOracle does not rapidly change proposed capture sizes,
// to allow both the source content and the rest of the end-to-end system to
// stabilize.
TEST(VideoCaptureOracleTest, DoesNotRapidlyChangeCaptureSize) {
  VideoCaptureOracle oracle(Get30HzPeriod(),
                            Get720pSize(),
                            media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT,
                            true);

  // Run 30 seconds of frame captures without any source size changes.
  base::TimeTicks t = InitialTestTimeTicks();
  const base::TimeDelta event_increment = Get30HzPeriod() * 2;
  base::TimeTicks end_t = t + base::TimeDelta::FromSeconds(30);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(Get720pSize(), oracle.capture_size());
    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(
        oracle.RecordCapture(0.0), true, &ignored));
  }

  // Now run 30 seconds of frame captures with lots of random source size
  // changes.  Check that there was no more than one size change per second.
  gfx::Size source_size = oracle.capture_size();
  base::TimeTicks time_of_last_size_change = InitialTestTimeTicks();
  gfx::Size last_capture_size = oracle.capture_size();
  end_t = t + base::TimeDelta::FromSeconds(30);
  for (; t < end_t; t += event_increment) {
    // Change the source size every frame to a random non-empty size.
    const gfx::Size last_source_size = source_size;
    source_size.SetSize(
        ((last_source_size.width() * 11 + 12345) % 1280) + 1,
        ((last_source_size.height() * 11 + 12345) % 720) + 1);
    ASSERT_NE(last_source_size, source_size);
    oracle.SetSourceSize(source_size);

    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));

    if (oracle.capture_size() != last_capture_size) {
      ASSERT_GE(t - time_of_last_size_change, base::TimeDelta::FromSeconds(1));
      time_of_last_size_change = t;
      last_capture_size = oracle.capture_size();
    }

    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(
        oracle.RecordCapture(0.0), true, &ignored));
  }
}

namespace {

// Tests that VideoCaptureOracle can auto-throttle by stepping the capture size
// up or down.  When |is_content_animating| is false, there is more
// aggressiveness expected in the timing of stepping upwards.  If
// |with_consumer_feedback| is false, only buffer pool utilization varies and no
// consumer feedback is provided.  If |with_consumer_feedback| is true, the
// buffer pool utilization is held constant at 25%, and the consumer utilization
// feedback varies.
void RunAutoThrottleTest(bool is_content_animating,
                         bool with_consumer_feedback) {
  SCOPED_TRACE(::testing::Message() << "RunAutoThrottleTest("
               << "(is_content_animating=" << is_content_animating
               << ", with_consumer_feedback=" << with_consumer_feedback << ")");

  VideoCaptureOracle oracle(Get30HzPeriod(),
                            Get720pSize(),
                            media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT,
                            true);

  // Run 10 seconds of frame captures with 90% utilization expect no capture
  // size changes.
  base::TimeTicks t = InitialTestTimeTicks();
  base::TimeTicks time_of_last_size_change = t;
  const base::TimeDelta event_increment = Get30HzPeriod() * 2;
  base::TimeTicks end_t = t + base::TimeDelta::FromSeconds(10);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        is_content_animating ? gfx::Rect(Get720pSize()) : gfx::Rect(),
        t));
    ASSERT_EQ(Get720pSize(), oracle.capture_size());
    const double utilization = 0.9;
    const int frame_number =
        oracle.RecordCapture(with_consumer_feedback ? 0.25 : utilization);
    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    if (with_consumer_feedback)
      oracle.RecordConsumerFeedback(frame_number, utilization);
  }

  // Cause two downward steppings in resolution.  First, indicate overload
  // until the resolution steps down.  Then, indicate a 90% utilization and
  // expect the resolution to remain constant.  Repeat.
  for (int i = 0; i < 2; ++i) {
    const gfx::Size starting_size = oracle.capture_size();
    SCOPED_TRACE(::testing::Message()
                 << "Stepping down from " << starting_size.ToString()
                 << ", i=" << i);

    gfx::Size stepped_down_size;
    end_t = t + base::TimeDelta::FromSeconds(10);
    for (; t < end_t; t += event_increment) {
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        is_content_animating ? gfx::Rect(Get720pSize()) : gfx::Rect(),
        t));

      if (stepped_down_size.IsEmpty()) {
        if (oracle.capture_size() != starting_size) {
          time_of_last_size_change = t;
          stepped_down_size = oracle.capture_size();
          ASSERT_GT(starting_size.width(), stepped_down_size.width());
          ASSERT_GT(starting_size.height(), stepped_down_size.height());
        }
      } else {
        ASSERT_EQ(stepped_down_size, oracle.capture_size());
      }

      const double utilization = stepped_down_size.IsEmpty() ? 1.5 : 0.9;
      const int frame_number =
          oracle.RecordCapture(with_consumer_feedback ? 0.25 : utilization);
      base::TimeTicks ignored;
      ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
      if (with_consumer_feedback)
        oracle.RecordConsumerFeedback(frame_number, utilization);
    }
  }

  // Now, cause two upward steppings in resolution.  First, indicate
  // under-utilization until the resolution steps up.  Then, indicate a 90%
  // utilization and expect the resolution to remain constant.  Repeat.
  for (int i = 0; i < 2; ++i) {
    const gfx::Size starting_size = oracle.capture_size();
    SCOPED_TRACE(::testing::Message()
                 << "Stepping up from " << starting_size.ToString()
                 << ", i=" << i);

    gfx::Size stepped_up_size;
    end_t = t + base::TimeDelta::FromSeconds(is_content_animating ? 90 : 10);
    for (; t < end_t; t += event_increment) {
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate,
          is_content_animating ? gfx::Rect(Get720pSize()) : gfx::Rect(),
          t));

      if (stepped_up_size.IsEmpty()) {
        if (oracle.capture_size() != starting_size) {
          // When content is animating, a much longer amount of time must pass
          // before the capture size will step up.
          ASSERT_LT(
              base::TimeDelta::FromSeconds(is_content_animating ? 15 : 1),
              t - time_of_last_size_change);
          time_of_last_size_change = t;
          stepped_up_size = oracle.capture_size();
          ASSERT_LT(starting_size.width(), stepped_up_size.width());
          ASSERT_LT(starting_size.height(), stepped_up_size.height());
        }
      } else {
        ASSERT_EQ(stepped_up_size, oracle.capture_size());
      }

      const double utilization = stepped_up_size.IsEmpty() ? 0.0 : 0.9;
      const int frame_number =
          oracle.RecordCapture(with_consumer_feedback ? 0.25 : utilization);
      base::TimeTicks ignored;
      ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
      if (with_consumer_feedback)
        oracle.RecordConsumerFeedback(frame_number, utilization);
    }
  }
}

}  // namespace

// Tests that VideoCaptureOracle can auto-throttle by stepping the capture size
// up or down, using utilization feedback signals from either the buffer pool or
// the consumer, and with slightly different behavior depending on whether
// content is animating.
TEST(VideoCaptureOracleTest, AutoThrottlesBasedOnUtilizationFeedback) {
  RunAutoThrottleTest(false, false);
  RunAutoThrottleTest(false, true);
  RunAutoThrottleTest(true, false);
  RunAutoThrottleTest(true, true);
}

// Tests that VideoCaptureOracle does not change the capture size if
// auto-throttling is enabled when using a fixed resolution policy.
TEST(VideoCaptureOracleTest, DoesNotAutoThrottleWhenResolutionIsFixed) {
  VideoCaptureOracle oracle(Get30HzPeriod(),
                            Get720pSize(),
                            media::RESOLUTION_POLICY_FIXED_RESOLUTION,
                            true);

  // Run 10 seconds of frame captures with 90% utilization expect no capture
  // size changes.
  base::TimeTicks t = InitialTestTimeTicks();
  const base::TimeDelta event_increment = Get30HzPeriod() * 2;
  base::TimeTicks end_t = t + base::TimeDelta::FromSeconds(10);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(Get720pSize(), oracle.capture_size());
    base::TimeTicks ignored;
    ASSERT_TRUE(
        oracle.CompleteCapture(oracle.RecordCapture(0.9), true, &ignored));
  }

  // Now run 10 seconds with overload indicated.  Still, expect no capture size
  // changes.
  end_t = t + base::TimeDelta::FromSeconds(10);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(Get720pSize(), oracle.capture_size());
    base::TimeTicks ignored;
    ASSERT_TRUE(
        oracle.CompleteCapture(oracle.RecordCapture(2.0), true, &ignored));
  }
}

}  // namespace media
