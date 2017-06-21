// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/debug/stack_trace.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/data_buffer.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/limits.h"
#include "media/base/mock_filters.h"
#include "media/base/null_video_sink.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/base/wall_clock_time_source.h"
#include "media/renderers/video_renderer_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

MATCHER_P(HasTimestamp, ms, "") {
  *result_listener << "has timestamp " << arg->timestamp().InMilliseconds();
  return arg->timestamp().InMilliseconds() == ms;
}

class VideoRendererImplTest
    : public testing::TestWithParam<bool /* new_video_renderer */> {
 public:
  VideoRendererImplTest()
      : tick_clock_(new base::SimpleTestTickClock()),
        decoder_(new MockVideoDecoder()),
        demuxer_stream_(DemuxerStream::VIDEO) {
    ScopedVector<VideoDecoder> decoders;
    decoders.push_back(decoder_);

    null_video_sink_.reset(new NullVideoSink(
        false, base::TimeDelta::FromSecondsD(1.0 / 60),
        base::Bind(&MockCB::FrameReceived, base::Unretained(&mock_cb_)),
        message_loop_.task_runner()));

    renderer_.reset(new VideoRendererImpl(message_loop_.task_runner(),
                                          null_video_sink_.get(),
                                          decoders.Pass(), true,
                                          nullptr,  // gpu_factories
                                          new MediaLog()));
    if (!GetParam())
      renderer_->disable_new_video_renderer_for_testing();
    renderer_->SetTickClockForTesting(scoped_ptr<base::TickClock>(tick_clock_));
    null_video_sink_->set_tick_clock_for_testing(tick_clock_);
    time_source_.set_tick_clock_for_testing(tick_clock_);

    // Start wallclock time at a non-zero value.
    AdvanceWallclockTimeInMs(12345);

    demuxer_stream_.set_video_decoder_config(TestVideoConfig::Normal());

    // We expect these to be called but we don't care how/when.
    EXPECT_CALL(demuxer_stream_, Read(_)).WillRepeatedly(
        RunCallback<0>(DemuxerStream::kOk,
                       scoped_refptr<DecoderBuffer>(new DecoderBuffer(0))));
  }

  virtual ~VideoRendererImplTest() {}

  void Initialize() {
    InitializeWithLowDelay(false);
  }

  void InitializeWithLowDelay(bool low_delay) {
    // Monitor decodes from the decoder.
    EXPECT_CALL(*decoder_, Decode(_, _))
        .WillRepeatedly(Invoke(this, &VideoRendererImplTest::DecodeRequested));

    EXPECT_CALL(*decoder_, Reset(_))
        .WillRepeatedly(Invoke(this, &VideoRendererImplTest::FlushRequested));

    // Initialize, we shouldn't have any reads.
    InitializeRenderer(low_delay, true);
  }

  void InitializeRenderer(bool low_delay, bool expect_to_success) {
    SCOPED_TRACE(
        base::StringPrintf("InitializeRenderer(%d)", expect_to_success));
    WaitableMessageLoopEvent event;
    CallInitialize(event.GetPipelineStatusCB(), low_delay, expect_to_success);
    event.RunAndWaitForStatus(expect_to_success ? PIPELINE_OK
                                                : DECODER_ERROR_NOT_SUPPORTED);
  }

  void CallInitialize(const PipelineStatusCB& status_cb,
                      bool low_delay,
                      bool expect_to_success) {
    if (low_delay)
      demuxer_stream_.set_liveness(DemuxerStream::LIVENESS_LIVE);
    EXPECT_CALL(*decoder_, Initialize(_, _, _, _))
        .WillOnce(
            DoAll(SaveArg<3>(&output_cb_), RunCallback<2>(expect_to_success)));
    EXPECT_CALL(*this, OnWaitingForDecryptionKey()).Times(0);
    renderer_->Initialize(
        &demuxer_stream_, status_cb, media::SetDecryptorReadyCB(),
        base::Bind(&VideoRendererImplTest::OnStatisticsUpdate,
                   base::Unretained(this)),
        base::Bind(&StrictMock<MockCB>::BufferingStateChange,
                   base::Unretained(&mock_cb_)),
        ended_event_.GetClosure(), error_event_.GetPipelineStatusCB(),
        base::Bind(&WallClockTimeSource::GetWallClockTimes,
                   base::Unretained(&time_source_)),
        base::Bind(&VideoRendererImplTest::OnWaitingForDecryptionKey,
                   base::Unretained(this)));
  }

  void StartPlayingFrom(int milliseconds) {
    SCOPED_TRACE(base::StringPrintf("StartPlayingFrom(%d)", milliseconds));
    const base::TimeDelta media_time =
        base::TimeDelta::FromMilliseconds(milliseconds);
    time_source_.SetMediaTime(media_time);
    renderer_->StartPlayingFrom(media_time);
    message_loop_.RunUntilIdle();
  }

  void Flush() {
    SCOPED_TRACE("Flush()");
    WaitableMessageLoopEvent event;
    renderer_->Flush(event.GetClosure());
    event.RunAndWait();
  }

  void Destroy() {
    SCOPED_TRACE("Destroy()");
    renderer_.reset();
    message_loop_.RunUntilIdle();
  }

  // Parses a string representation of video frames and generates corresponding
  // VideoFrame objects in |decode_results_|.
  //
  // Syntax:
  //   nn - Queue a decoder buffer with timestamp nn * 1000us
  //   abort - Queue an aborted read
  //   error - Queue a decoder error
  //
  // Examples:
  //   A clip that is four frames long: "0 10 20 30"
  //   A clip that has a decode error: "60 70 error"
  void QueueFrames(const std::string& str) {
    std::vector<std::string> tokens;
    base::SplitString(str, ' ', &tokens);
    for (size_t i = 0; i < tokens.size(); ++i) {
      if (tokens[i] == "abort") {
        scoped_refptr<VideoFrame> null_frame;
        decode_results_.push_back(
            std::make_pair(VideoDecoder::kAborted, null_frame));
        continue;
      }

      if (tokens[i] == "error") {
        scoped_refptr<VideoFrame> null_frame;
        decode_results_.push_back(
            std::make_pair(VideoDecoder::kDecodeError, null_frame));
        continue;
      }

      int timestamp_in_ms = 0;
      if (base::StringToInt(tokens[i], &timestamp_in_ms)) {
        gfx::Size natural_size = TestVideoConfig::NormalCodedSize();
        scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
            VideoFrame::YV12,
            natural_size,
            gfx::Rect(natural_size),
            natural_size,
            base::TimeDelta::FromMilliseconds(timestamp_in_ms));
        decode_results_.push_back(std::make_pair(VideoDecoder::kOk, frame));
        continue;
      }

      CHECK(false) << "Unrecognized decoder buffer token: " << tokens[i];
    }
  }

  bool IsReadPending() {
    return !decode_cb_.is_null();
  }

  void WaitForError(PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("WaitForError(%d)", expected));
    error_event_.RunAndWaitForStatus(expected);
  }

  void WaitForEnded() {
    SCOPED_TRACE("WaitForEnded()");
    ended_event_.RunAndWait();
  }

  void WaitForPendingRead() {
    SCOPED_TRACE("WaitForPendingRead()");
    if (!decode_cb_.is_null())
      return;

    DCHECK(wait_for_pending_decode_cb_.is_null());

    WaitableMessageLoopEvent event;
    wait_for_pending_decode_cb_ = event.GetClosure();
    event.RunAndWait();

    DCHECK(!decode_cb_.is_null());
    DCHECK(wait_for_pending_decode_cb_.is_null());
  }

  void SatisfyPendingRead() {
    CHECK(!decode_cb_.is_null());
    CHECK(!decode_results_.empty());

    // Post tasks for OutputCB and DecodeCB.
    scoped_refptr<VideoFrame> frame = decode_results_.front().second;
    if (frame.get())
      message_loop_.PostTask(FROM_HERE, base::Bind(output_cb_, frame));
    message_loop_.PostTask(
        FROM_HERE, base::Bind(base::ResetAndReturn(&decode_cb_),
                              decode_results_.front().first));
    decode_results_.pop_front();
  }

  void SatisfyPendingReadWithEndOfStream() {
    DCHECK(!decode_cb_.is_null());

    // Return EOS buffer to trigger EOS frame.
    EXPECT_CALL(demuxer_stream_, Read(_))
        .WillOnce(RunCallback<0>(DemuxerStream::kOk,
                                 DecoderBuffer::CreateEOSBuffer()));

    // Satify pending |decode_cb_| to trigger a new DemuxerStream::Read().
    message_loop_.PostTask(
        FROM_HERE,
        base::Bind(base::ResetAndReturn(&decode_cb_), VideoDecoder::kOk));

    WaitForPendingRead();

    message_loop_.PostTask(
        FROM_HERE,
        base::Bind(base::ResetAndReturn(&decode_cb_), VideoDecoder::kOk));
  }

  void AdvanceWallclockTimeInMs(int time_ms) {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    base::AutoLock l(lock_);
    tick_clock_->Advance(base::TimeDelta::FromMilliseconds(time_ms));
  }

  void AdvanceTimeInMs(int time_ms) {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    base::AutoLock l(lock_);
    time_ += base::TimeDelta::FromMilliseconds(time_ms);
    time_source_.StopTicking();
    time_source_.SetMediaTime(time_);
    time_source_.StartTicking();
  }

  bool has_ended() const {
    return ended_event_.is_signaled();
  }

 protected:
  // Fixture members.
  scoped_ptr<VideoRendererImpl> renderer_;
  base::SimpleTestTickClock* tick_clock_;  // Owned by |renderer_|.
  MockVideoDecoder* decoder_;  // Owned by |renderer_|.
  NiceMock<MockDemuxerStream> demuxer_stream_;

  // Use StrictMock<T> to catch missing/extra callbacks.
  class MockCB {
   public:
    MOCK_METHOD1(FrameReceived, void(const scoped_refptr<VideoFrame>&));
    MOCK_METHOD1(BufferingStateChange, void(BufferingState));
  };
  StrictMock<MockCB> mock_cb_;

  // Must be destroyed before |renderer_| since they share |tick_clock_|.
  scoped_ptr<NullVideoSink> null_video_sink_;

  PipelineStatistics last_pipeline_statistics_;

  WallClockTimeSource time_source_;

 private:
  void DecodeRequested(const scoped_refptr<DecoderBuffer>& buffer,
                       const VideoDecoder::DecodeCB& decode_cb) {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    CHECK(decode_cb_.is_null());
    decode_cb_ = decode_cb;

    // Wake up WaitForPendingRead() if needed.
    if (!wait_for_pending_decode_cb_.is_null())
      base::ResetAndReturn(&wait_for_pending_decode_cb_).Run();

    if (decode_results_.empty())
      return;

    SatisfyPendingRead();
  }

  void FlushRequested(const base::Closure& callback) {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    decode_results_.clear();
    if (!decode_cb_.is_null()) {
      QueueFrames("abort");
      SatisfyPendingRead();
    }

    message_loop_.PostTask(FROM_HERE, callback);
  }

  void OnStatisticsUpdate(const PipelineStatistics& stats) {
    last_pipeline_statistics_ = stats;
  }

  MOCK_METHOD0(OnWaitingForDecryptionKey, void(void));

  base::MessageLoop message_loop_;

  // Used to protect |time_|.
  base::Lock lock_;
  base::TimeDelta time_;

  // Used for satisfying reads.
  VideoDecoder::OutputCB output_cb_;
  VideoDecoder::DecodeCB decode_cb_;
  base::TimeDelta next_frame_timestamp_;

  WaitableMessageLoopEvent error_event_;
  WaitableMessageLoopEvent ended_event_;

  // Run during DecodeRequested() to unblock WaitForPendingRead().
  base::Closure wait_for_pending_decode_cb_;

  std::deque<std::pair<
      VideoDecoder::Status, scoped_refptr<VideoFrame> > > decode_results_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererImplTest);
};

TEST_P(VideoRendererImplTest, DoNothing) {
  // Test that creation and deletion doesn't depend on calls to Initialize()
  // and/or Destroy().
}

TEST_P(VideoRendererImplTest, DestroyWithoutInitialize) {
  Destroy();
}

TEST_P(VideoRendererImplTest, Initialize) {
  Initialize();
  Destroy();
}

TEST_P(VideoRendererImplTest, InitializeAndStartPlayingFrom) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlayingFrom(0);
  Destroy();
}

TEST_P(VideoRendererImplTest, InitializeAndEndOfStream) {
  Initialize();
  StartPlayingFrom(0);
  WaitForPendingRead();
  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_ENOUGH");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH))
        .WillOnce(RunClosure(event.GetClosure()));
    SatisfyPendingReadWithEndOfStream();
    event.RunAndWait();
  }
  // Firing a time state changed to true should be ignored...
  renderer_->OnTimeStateChanged(true);
  EXPECT_FALSE(null_video_sink_->is_started());
  Destroy();
}

TEST_P(VideoRendererImplTest, DestroyWhileInitializing) {
  CallInitialize(NewExpectedStatusCB(PIPELINE_ERROR_ABORT), false, PIPELINE_OK);
  Destroy();
}

TEST_P(VideoRendererImplTest, DestroyWhileFlushing) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlayingFrom(0);
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_NOTHING));
  renderer_->Flush(NewExpectedClosure());
  Destroy();
}

TEST_P(VideoRendererImplTest, Play) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlayingFrom(0);
  Destroy();
}

TEST_P(VideoRendererImplTest, FlushWithNothingBuffered) {
  Initialize();
  StartPlayingFrom(0);

  // We shouldn't expect a buffering state change since we never reached
  // BUFFERING_HAVE_ENOUGH.
  Flush();
  Destroy();
}

TEST_P(VideoRendererImplTest, DecodeError_Playing) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(_)).Times(testing::AtLeast(1));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlayingFrom(0);
  renderer_->OnTimeStateChanged(true);
  time_source_.StartTicking();
  AdvanceTimeInMs(10);

  QueueFrames("error");
  SatisfyPendingRead();
  WaitForError(PIPELINE_ERROR_DECODE);
  Destroy();
}

TEST_P(VideoRendererImplTest, DecodeError_DuringStartPlayingFrom) {
  Initialize();
  QueueFrames("error");
  StartPlayingFrom(0);
  Destroy();
}

TEST_P(VideoRendererImplTest, StartPlayingFrom_Exact) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(60)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlayingFrom(60);
  Destroy();
}

TEST_P(VideoRendererImplTest, StartPlayingFrom_RightBefore) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(50)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlayingFrom(59);
  Destroy();
}

TEST_P(VideoRendererImplTest, StartPlayingFrom_RightAfter) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(60)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlayingFrom(61);
  Destroy();
}

TEST_P(VideoRendererImplTest, StartPlayingFrom_LowDelay) {
  // In low-delay mode only one frame is required to finish preroll. But frames
  // prior to the start time will not be used.
  InitializeWithLowDelay(true);
  QueueFrames("0 10");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(10)));
  // Expect some amount of have enough/nothing due to only requiring one frame.
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .Times(AnyNumber());
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_NOTHING))
      .Times(AnyNumber());
  StartPlayingFrom(10);

  QueueFrames("20");
  SatisfyPendingRead();

  renderer_->OnTimeStateChanged(true);
  time_source_.StartTicking();

  WaitableMessageLoopEvent event;
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(20)))
      .WillOnce(RunClosure(event.GetClosure()));
  AdvanceTimeInMs(20);
  event.RunAndWait();

  Destroy();
}

// Verify that a late decoder response doesn't break invariants in the renderer.
TEST_P(VideoRendererImplTest, DestroyDuringOutstandingRead) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlayingFrom(0);

  // Check that there is an outstanding Read() request.
  EXPECT_TRUE(IsReadPending());

  Destroy();
}

TEST_P(VideoRendererImplTest, VideoDecoder_InitFailure) {
  InitializeRenderer(false, false);
  Destroy();
}

TEST_P(VideoRendererImplTest, Underflow) {
  Initialize();
  QueueFrames("0 30 60 90");

  {
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
    EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH))
        .WillOnce(RunClosure(event.GetClosure()));
    StartPlayingFrom(0);
    event.RunAndWait();
    Mock::VerifyAndClearExpectations(&mock_cb_);
  }

  renderer_->OnTimeStateChanged(true);

  // Advance time slightly, but enough to exceed the duration of the last frame.
  // Frames should be dropped and we should NOT signal having nothing.
  {
    SCOPED_TRACE("Waiting for frame drops");
    WaitableMessageLoopEvent event;

    // Note: Starting the TimeSource will cause the old VideoRendererImpl to
    // start rendering frames on its own thread, so the first frame may be
    // received.
    time_source_.StartTicking();
    if (GetParam())
      EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(30))).Times(0);
    else
      EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(30))).Times(AnyNumber());

    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(60))).Times(0);
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(90)))
        .WillOnce(RunClosure(event.GetClosure()));
    AdvanceTimeInMs(91);

    event.RunAndWait();
    Mock::VerifyAndClearExpectations(&mock_cb_);
  }

  // Advance time more. Now we should signal having nothing. And put
  // the last frame up for display.
  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_NOTHING");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_NOTHING))
        .WillOnce(RunClosure(event.GetClosure()));
    AdvanceTimeInMs(30);
    // The old rendering path needs wall clock time to increase too.
    if (!GetParam())
      AdvanceWallclockTimeInMs(30);

    event.RunAndWait();
    Mock::VerifyAndClearExpectations(&mock_cb_);
  }

  // Receiving end of stream should signal having enough.
  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_ENOUGH");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH))
        .WillOnce(RunClosure(event.GetClosure()));
    SatisfyPendingReadWithEndOfStream();
    event.RunAndWait();
  }

  WaitForEnded();
  Destroy();
}

// Verifies that the sink is stopped after rendering the first frame if
// playback hasn't started.
TEST_P(VideoRendererImplTest, RenderingStopsAfterFirstFrame) {
  // This test is only for the new rendering path.
  if (!GetParam())
    return;

  InitializeWithLowDelay(true);
  QueueFrames("0");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));

  {
    SCOPED_TRACE("Waiting for sink to stop.");
    WaitableMessageLoopEvent event;

    null_video_sink_->set_background_render(true);
    null_video_sink_->set_stop_cb(event.GetClosure());
    StartPlayingFrom(0);

    EXPECT_TRUE(IsReadPending());
    SatisfyPendingReadWithEndOfStream();

    event.RunAndWait();
  }

  EXPECT_FALSE(has_ended());
  Destroy();
}

// Verifies that the sink is stopped after rendering the first frame if
// playback ha started.
TEST_P(VideoRendererImplTest, RenderingStopsAfterOneFrameWithEOS) {
  // This test is only for the new rendering path.
  if (!GetParam())
    return;

  InitializeWithLowDelay(true);
  QueueFrames("0");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));

  {
    SCOPED_TRACE("Waiting for sink to stop.");
    WaitableMessageLoopEvent event;

    null_video_sink_->set_stop_cb(event.GetClosure());
    StartPlayingFrom(0);
    renderer_->OnTimeStateChanged(true);

    EXPECT_TRUE(IsReadPending());
    SatisfyPendingReadWithEndOfStream();
    WaitForEnded();

    renderer_->OnTimeStateChanged(false);
    event.RunAndWait();
  }

  Destroy();
}

// Tests the case where the video started and received a single Render() call,
// then the video was put into the background.
TEST_P(VideoRendererImplTest, RenderingStartedThenStopped) {
  // This test is only for the new rendering path.
  if (!GetParam())
    return;

  Initialize();
  QueueFrames("0 30 60 90");

  // Start the sink and wait for the first callback.  Set statistics to a non
  // zero value, once we have some decoded frames they should be overwritten.
  last_pipeline_statistics_.video_frames_dropped = 1;
  {
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)))
        .WillOnce(RunClosure(event.GetClosure()));
    StartPlayingFrom(0);
    event.RunAndWait();
    Mock::VerifyAndClearExpectations(&mock_cb_);
    EXPECT_EQ(0u, last_pipeline_statistics_.video_frames_dropped);
  }

  renderer_->OnTimeStateChanged(true);
  time_source_.StartTicking();

  // Suspend all future callbacks and synthetically advance the media time,
  // because this is a background render, we won't underflow by waiting until
  // a pending read is ready.
  null_video_sink_->set_background_render(true);
  AdvanceTimeInMs(91);
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(90)));
  WaitForPendingRead();
  SatisfyPendingReadWithEndOfStream();

  // If this wasn't background rendering mode, this would result in two frames
  // being dropped, but since we set background render to true, none should be
  // reported
  EXPECT_EQ(0u, last_pipeline_statistics_.video_frames_dropped);
  EXPECT_EQ(4u, last_pipeline_statistics_.video_frames_decoded);

  AdvanceTimeInMs(30);
  WaitForEnded();
  Destroy();
}

TEST_P(VideoRendererImplTest, StartPlayingFromThenFlushThenEOS) {
  // This test is only for the new rendering path.
  if (!GetParam())
    return;

  Initialize();
  QueueFrames("0 30 60 90");

  WaitableMessageLoopEvent event;
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .WillOnce(RunClosure(event.GetClosure()));
  StartPlayingFrom(0);
  event.RunAndWait();

  // Cycle ticking so that we get a non-null reference time.
  time_source_.StartTicking();
  time_source_.StopTicking();

  // Flush and simulate a seek past EOS, where some error prevents the decoder
  // from returning any frames.
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_NOTHING));
  Flush();

  StartPlayingFrom(200);
  WaitForPendingRead();
  SatisfyPendingReadWithEndOfStream();
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  WaitForEnded();
  Destroy();
}

INSTANTIATE_TEST_CASE_P(OldVideoRenderer,
                        VideoRendererImplTest,
                        testing::Values(false));
INSTANTIATE_TEST_CASE_P(NewVideoRenderer,
                        VideoRendererImplTest,
                        testing::Values(true));

}  // namespace media
