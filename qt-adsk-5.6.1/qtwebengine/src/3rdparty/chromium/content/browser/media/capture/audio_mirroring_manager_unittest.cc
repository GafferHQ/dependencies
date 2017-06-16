// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/audio_mirroring_manager.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "content/browser/browser_thread_impl.h"
#include "media/audio/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::AudioOutputStream;
using media::AudioParameters;
using testing::_;
using testing::Invoke;
using testing::NotNull;
using testing::Ref;
using testing::Return;
using testing::ReturnRef;

namespace content {

namespace {

class MockDiverter : public AudioMirroringManager::Diverter {
 public:
  MOCK_METHOD0(GetAudioParameters, const AudioParameters&());
  MOCK_METHOD1(StartDiverting, void(AudioOutputStream*));
  MOCK_METHOD0(StopDiverting, void());
};

class MockMirroringDestination
    : public AudioMirroringManager::MirroringDestination {
 public:
  typedef AudioMirroringManager::SourceFrameRef SourceFrameRef;

  MockMirroringDestination(int render_process_id, int render_frame_id)
      : render_process_id_(render_process_id),
        render_frame_id_(render_frame_id),
        query_count_(0) {}

  MOCK_METHOD2(QueryForMatches,
               void(const std::set<SourceFrameRef>& candidates,
                    const MatchesCallback& results_callback));
  MOCK_METHOD1(AddInput,
               media::AudioOutputStream*(const media::AudioParameters& params));

  void SimulateQuery(const std::set<SourceFrameRef>& candidates,
                     const MatchesCallback& results_callback) {
    ++query_count_;

    std::set<SourceFrameRef> result;
    if (candidates.find(SourceFrameRef(render_process_id_, render_frame_id_)) !=
            candidates.end()) {
      result.insert(SourceFrameRef(render_process_id_, render_frame_id_));
    }
    results_callback.Run(result);
  }

  media::AudioOutputStream* SimulateAddInput(
      const media::AudioParameters& params) {
    static AudioOutputStream* const kNonNullPointer =
        reinterpret_cast<AudioOutputStream*>(0x11111110);
    return kNonNullPointer;
  }

  int query_count() const {
    return query_count_;
  }

 private:
  const int render_process_id_;
  const int render_frame_id_;
  int query_count_;
};

}  // namespace

class AudioMirroringManagerTest : public testing::Test {
 public:
  typedef AudioMirroringManager::Diverter Diverter;
  typedef AudioMirroringManager::MirroringDestination MirroringDestination;
  typedef AudioMirroringManager::StreamRoutes StreamRoutes;

  AudioMirroringManagerTest()
      : io_thread_(BrowserThread::IO, &message_loop_),
        params_(AudioParameters::AUDIO_FAKE, media::CHANNEL_LAYOUT_STEREO,
                AudioParameters::kAudioCDSampleRate, 16,
                AudioParameters::kAudioCDSampleRate / 10) {}

  MockDiverter* CreateStream(
      int render_process_id, int render_frame_id, int expected_times_diverted) {
    MockDiverter* const diverter = new MockDiverter();
    if (expected_times_diverted > 0) {
      EXPECT_CALL(*diverter, GetAudioParameters())
          .Times(expected_times_diverted)
          .WillRepeatedly(ReturnRef(params_));
      EXPECT_CALL(*diverter, StartDiverting(NotNull()))
          .Times(expected_times_diverted);
      EXPECT_CALL(*diverter, StopDiverting())
          .Times(expected_times_diverted);
    }

    mirroring_manager_.AddDiverter(
        render_process_id, render_frame_id, diverter);

    return diverter;
  }

  void KillStream(MockDiverter* diverter) {
    mirroring_manager_.RemoveDiverter(diverter);
    delete diverter;
  }

  void StartMirroringTo(const scoped_ptr<MockMirroringDestination>& dest,
                        int expected_inputs_added) {
    EXPECT_CALL(*dest, QueryForMatches(_, _))
        .WillRepeatedly(Invoke(dest.get(),
                               &MockMirroringDestination::SimulateQuery));
    if (expected_inputs_added > 0) {
      EXPECT_CALL(*dest, AddInput(Ref(params_)))
          .Times(expected_inputs_added)
          .WillRepeatedly(Invoke(dest.get(),
                                 &MockMirroringDestination::SimulateAddInput))
          .RetiresOnSaturation();
    }

    mirroring_manager_.StartMirroring(dest.get());
  }

  void StopMirroringTo(const scoped_ptr<MockMirroringDestination>& dest) {
    mirroring_manager_.StopMirroring(dest.get());
  }

  int CountStreamsDivertedTo(
      const scoped_ptr<MockMirroringDestination>& dest) const {
    int count = 0;
    for (StreamRoutes::const_iterator it = mirroring_manager_.routes_.begin();
         it != mirroring_manager_.routes_.end(); ++it) {
      if (it->destination == dest.get())
        ++count;
    }
    return count;
  }

  void ExpectNoLongerManagingAnything() const {
    EXPECT_TRUE(mirroring_manager_.routes_.empty());
    EXPECT_TRUE(mirroring_manager_.sessions_.empty());
  }

 private:
  base::MessageLoopForIO message_loop_;
  BrowserThreadImpl io_thread_;
  AudioParameters params_;
  AudioMirroringManager mirroring_manager_;

  DISALLOW_COPY_AND_ASSIGN(AudioMirroringManagerTest);
};

namespace {
const int kRenderProcessId = 123;
const int kRenderFrameId = 456;
const int kAnotherRenderProcessId = 789;
const int kAnotherRenderFrameId = 1234;
const int kYetAnotherRenderProcessId = 4560;
const int kYetAnotherRenderFrameId = 7890;
}

TEST_F(AudioMirroringManagerTest, MirroringSessionOfNothing) {
  const scoped_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId));
  StartMirroringTo(destination, 0);
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  StopMirroringTo(destination);
  EXPECT_EQ(0, destination->query_count());

  ExpectNoLongerManagingAnything();
}

TEST_F(AudioMirroringManagerTest, TwoMirroringSessionsOfNothing) {
  const scoped_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId));
  StartMirroringTo(destination, 0);
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  StopMirroringTo(destination);
  EXPECT_EQ(0, destination->query_count());

  const scoped_ptr<MockMirroringDestination> another_destination(
      new MockMirroringDestination(kAnotherRenderProcessId,
                                   kAnotherRenderFrameId));
  StartMirroringTo(another_destination, 0);
  EXPECT_EQ(0, CountStreamsDivertedTo(another_destination));

  StopMirroringTo(another_destination);
  EXPECT_EQ(0, another_destination->query_count());

  ExpectNoLongerManagingAnything();
}

// Tests that a mirroring session starts after, and ends before, a stream that
// will be diverted to it.
TEST_F(AudioMirroringManagerTest, StreamLifetimeAroundMirroringSession) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 1);
  const scoped_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId));
  StartMirroringTo(destination, 1);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  StopMirroringTo(destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  KillStream(stream);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  ExpectNoLongerManagingAnything();
}

// Tests that a mirroring session starts before, and ends after, a stream that
// will be diverted to it.
TEST_F(AudioMirroringManagerTest, StreamLifetimeWithinMirroringSession) {
  const scoped_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId));
  StartMirroringTo(destination, 1);
  EXPECT_EQ(0, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 1);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  KillStream(stream);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  StopMirroringTo(destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  ExpectNoLongerManagingAnything();
}

// Tests that a stream is diverted correctly as two mirroring sessions come and
// go.
TEST_F(AudioMirroringManagerTest, StreamLifetimeAcrossTwoMirroringSessions) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 2);

  const scoped_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId));
  StartMirroringTo(destination, 1);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  StopMirroringTo(destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  const scoped_ptr<MockMirroringDestination> second_destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId));
  StartMirroringTo(second_destination, 1);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, second_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(second_destination));

  StopMirroringTo(second_destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, second_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(second_destination));

  KillStream(stream);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, second_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(second_destination));

  ExpectNoLongerManagingAnything();
}

// Tests that a stream does not flip-flop between two destinations that are a
// match for it.
TEST_F(AudioMirroringManagerTest, StreamDivertingStickyToOneDestination_1) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 2);

  const scoped_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId));
  StartMirroringTo(destination, 1);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  const scoped_ptr<MockMirroringDestination> replacement_destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId));
  StartMirroringTo(replacement_destination, 1);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));
  EXPECT_EQ(0, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  StopMirroringTo(destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, replacement_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(replacement_destination));

  StopMirroringTo(replacement_destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  KillStream(stream);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  ExpectNoLongerManagingAnything();
}

// Same as StreamDivertingStickyToOneDestination_1, with a different order of
// operations that should have the same effects.
TEST_F(AudioMirroringManagerTest, StreamDivertingStickyToOneDestination_2) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 2);

  const scoped_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId));
  StartMirroringTo(destination, 1);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  const scoped_ptr<MockMirroringDestination> replacement_destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId));
  StartMirroringTo(replacement_destination, 1);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));
  EXPECT_EQ(0, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  StopMirroringTo(destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, replacement_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(replacement_destination));

  KillStream(stream);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  StopMirroringTo(replacement_destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  ExpectNoLongerManagingAnything();
}

// Same as StreamDivertingStickyToOneDestination_1, except that the stream is
// killed before the first destination is stopped.  Therefore, the second
// destination should never see the stream.
TEST_F(AudioMirroringManagerTest, StreamDivertingStickyToOneDestination_3) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 1);

  const scoped_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId));
  StartMirroringTo(destination, 1);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  const scoped_ptr<MockMirroringDestination> replacement_destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId));
  StartMirroringTo(replacement_destination, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));
  EXPECT_EQ(0, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  KillStream(stream);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(0, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  StopMirroringTo(destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(0, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  StopMirroringTo(replacement_destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(0, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  ExpectNoLongerManagingAnything();
}

// Tests that multiple streams are diverted/mixed to one destination.
TEST_F(AudioMirroringManagerTest, MultipleStreamsInOneMirroringSession) {
  MockDiverter* const stream1 =
      CreateStream(kRenderProcessId, kRenderFrameId, 1);

  const scoped_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId));
  StartMirroringTo(destination, 3);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  MockDiverter* const stream2 =
      CreateStream(kRenderProcessId, kRenderFrameId, 1);
  EXPECT_EQ(2, destination->query_count());
  EXPECT_EQ(2, CountStreamsDivertedTo(destination));

  MockDiverter* const stream3 =
      CreateStream(kRenderProcessId, kRenderFrameId, 1);
  EXPECT_EQ(3, destination->query_count());
  EXPECT_EQ(3, CountStreamsDivertedTo(destination));

  KillStream(stream2);
  EXPECT_EQ(3, destination->query_count());
  EXPECT_EQ(2, CountStreamsDivertedTo(destination));

  StopMirroringTo(destination);
  EXPECT_EQ(3, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  KillStream(stream3);
  EXPECT_EQ(3, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  KillStream(stream1);
  EXPECT_EQ(3, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  ExpectNoLongerManagingAnything();
}

// A random interleaving of operations for three separate targets, each of which
// has one stream mirrored to one destination.
TEST_F(AudioMirroringManagerTest, ThreeSeparateMirroringSessions) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 1);

  const scoped_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId));
  StartMirroringTo(destination, 1);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  const scoped_ptr<MockMirroringDestination> another_destination(
      new MockMirroringDestination(kAnotherRenderProcessId,
                                   kAnotherRenderFrameId));
  StartMirroringTo(another_destination, 1);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));
  EXPECT_EQ(0, another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(another_destination));

  MockDiverter* const another_stream =
      CreateStream(kAnotherRenderProcessId, kAnotherRenderFrameId, 1);
  EXPECT_EQ(2, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, another_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(another_destination));

  KillStream(stream);
  EXPECT_EQ(2, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, another_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(another_destination));

  MockDiverter* const yet_another_stream =
      CreateStream(kYetAnotherRenderProcessId, kYetAnotherRenderFrameId, 1);
  EXPECT_EQ(3, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, another_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(another_destination));

  const scoped_ptr<MockMirroringDestination> yet_another_destination(
      new MockMirroringDestination(kYetAnotherRenderProcessId,
                                   kYetAnotherRenderFrameId));
  StartMirroringTo(yet_another_destination, 1);
  EXPECT_EQ(3, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, another_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(another_destination));
  EXPECT_EQ(1, yet_another_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(yet_another_destination));

  StopMirroringTo(another_destination);
  EXPECT_EQ(4, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(another_destination));
  EXPECT_EQ(2, yet_another_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(yet_another_destination));

  StopMirroringTo(yet_another_destination);
  EXPECT_EQ(5, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(another_destination));
  EXPECT_EQ(2, yet_another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(yet_another_destination));

  StopMirroringTo(destination);
  EXPECT_EQ(5, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(another_destination));
  EXPECT_EQ(2, yet_another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(yet_another_destination));

  KillStream(another_stream);
  EXPECT_EQ(5, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(another_destination));
  EXPECT_EQ(2, yet_another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(yet_another_destination));

  KillStream(yet_another_stream);
  EXPECT_EQ(5, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(another_destination));
  EXPECT_EQ(2, yet_another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(yet_another_destination));

  ExpectNoLongerManagingAnything();
}

}  // namespace content
