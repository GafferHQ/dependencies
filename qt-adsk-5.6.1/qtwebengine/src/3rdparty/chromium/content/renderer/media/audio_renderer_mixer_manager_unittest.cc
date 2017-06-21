// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/renderer/media/audio_renderer_mixer_manager.h"
#include "ipc/ipc_message.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/fake_audio_render_callback.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

static const int kBitsPerChannel = 16;
static const int kSampleRate = 48000;
static const int kBufferSize = 8192;
static const media::ChannelLayout kChannelLayout = media::CHANNEL_LAYOUT_STEREO;

static const int kRenderFrameId = 124;
static const int kAnotherRenderFrameId = 678;

using media::AudioParameters;

class AudioRendererMixerManagerTest : public testing::Test {
 public:
  AudioRendererMixerManagerTest()
      : fake_config_(AudioParameters(), AudioParameters()) {
    AudioParameters output_params(
        AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::CHANNEL_LAYOUT_STEREO,
        kSampleRate,
        16,
        kBufferSize);
    fake_config_.UpdateOutputConfig(output_params);

    manager_.reset(new AudioRendererMixerManager(&fake_config_));

    // We don't want to deal with instantiating a real AudioOutputDevice since
    // it's not important to our testing, so we inject a mock.
    mock_sink_ = new media::MockAudioRendererSink();
    manager_->SetAudioRendererSinkForTesting(mock_sink_.get());
  }

  media::AudioRendererMixer* GetMixer(int source_render_frame_id,
                                      const media::AudioParameters& params) {
    return manager_->GetMixer(source_render_frame_id, params);
  }

  void RemoveMixer(int source_render_frame_id,
                   const media::AudioParameters& params) {
    return manager_->RemoveMixer(source_render_frame_id, params);
  }

  // Number of instantiated mixers.
  int mixer_count() {
    return manager_->mixers_.size();
  }

 protected:
  media::AudioHardwareConfig fake_config_;
  scoped_ptr<AudioRendererMixerManager> manager_;
  scoped_refptr<media::MockAudioRendererSink> mock_sink_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerManagerTest);
};

// Verify GetMixer() and RemoveMixer() both work as expected; particularly with
// respect to the explicit ref counting done.
TEST_F(AudioRendererMixerManagerTest, GetRemoveMixer) {
  // Since we're testing two different sets of parameters, we expect
  // AudioRendererMixerManager to call Start and Stop on our mock twice.
  EXPECT_CALL(*mock_sink_.get(), Start()).Times(2);
  EXPECT_CALL(*mock_sink_.get(), Stop()).Times(2);

  // There should be no mixers outstanding to start with.
  EXPECT_EQ(mixer_count(), 0);

  media::AudioParameters params1(
      AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate,
      kBitsPerChannel, kBufferSize);

  media::AudioRendererMixer* mixer1 = GetMixer(kRenderFrameId, params1);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(mixer_count(), 1);

  // The same parameters should return the same mixer1.
  EXPECT_EQ(mixer1, GetMixer(kRenderFrameId, params1));
  EXPECT_EQ(mixer_count(), 1);

  // Remove the extra mixer we just acquired.
  RemoveMixer(kRenderFrameId, params1);
  EXPECT_EQ(mixer_count(), 1);

  media::AudioParameters params2(
      AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate * 2,
      kBitsPerChannel, kBufferSize * 2);
  media::AudioRendererMixer* mixer2 = GetMixer(kRenderFrameId, params2);
  ASSERT_TRUE(mixer2);
  EXPECT_EQ(mixer_count(), 2);

  // Different parameters should result in a different mixer1.
  EXPECT_NE(mixer1, mixer2);

  // Remove both outstanding mixers.
  RemoveMixer(kRenderFrameId, params1);
  EXPECT_EQ(mixer_count(), 1);
  RemoveMixer(kRenderFrameId, params2);
  EXPECT_EQ(mixer_count(), 0);
}

// Verify GetMixer() correctly deduplicates mixer with irrelevant AudioParameter
// differences.
TEST_F(AudioRendererMixerManagerTest, MixerReuse) {
  EXPECT_CALL(*mock_sink_.get(), Start()).Times(2);
  EXPECT_CALL(*mock_sink_.get(), Stop()).Times(2);
  EXPECT_EQ(mixer_count(), 0);

  media::AudioParameters params1(AudioParameters::AUDIO_PCM_LINEAR,
                                 kChannelLayout,
                                 kSampleRate,
                                 kBitsPerChannel,
                                 kBufferSize);
  media::AudioRendererMixer* mixer1 = GetMixer(kRenderFrameId, params1);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(mixer_count(), 1);

  // Different formats, bit depths, and buffer sizes should not result in a
  // different mixer.
  media::AudioParameters params2(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                 kChannelLayout,
                                 kSampleRate,
                                 kBitsPerChannel * 2,
                                 kBufferSize * 2,
                                 AudioParameters::NO_EFFECTS);
  EXPECT_EQ(mixer1, GetMixer(kRenderFrameId, params2));
  EXPECT_EQ(mixer_count(), 1);
  RemoveMixer(kRenderFrameId, params2);
  EXPECT_EQ(mixer_count(), 1);

  // Modify some parameters that do matter.
  media::AudioParameters params3(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                 media::CHANNEL_LAYOUT_MONO,
                                 kSampleRate * 2,
                                 kBitsPerChannel,
                                 kBufferSize,
                                 AudioParameters::NO_EFFECTS);
  ASSERT_NE(params3.channel_layout(), params1.channel_layout());

  EXPECT_NE(mixer1, GetMixer(kRenderFrameId, params3));
  EXPECT_EQ(mixer_count(), 2);
  RemoveMixer(kRenderFrameId, params3);
  EXPECT_EQ(mixer_count(), 1);

  // Remove final mixer.
  RemoveMixer(kRenderFrameId, params1);
  EXPECT_EQ(mixer_count(), 0);
}

// Verify CreateInput() provides AudioRendererMixerInput with the appropriate
// callbacks and they are working as expected.  Also, verify that separate
// mixers are created for separate render views, even though the AudioParameters
// are the same.
TEST_F(AudioRendererMixerManagerTest, CreateInput) {
  // Expect AudioRendererMixerManager to call Start and Stop on our mock twice
  // each.  Note: Under normal conditions, each mixer would get its own sink!
  EXPECT_CALL(*mock_sink_.get(), Start()).Times(2);
  EXPECT_CALL(*mock_sink_.get(), Stop()).Times(2);

  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate,
      kBitsPerChannel, kBufferSize);

  // Create two mixer inputs and ensure this doesn't instantiate any mixers yet.
  EXPECT_EQ(mixer_count(), 0);
  media::FakeAudioRenderCallback callback(0);
  scoped_refptr<media::AudioRendererMixerInput> input(
      manager_->CreateInput(kRenderFrameId));
  input->Initialize(params, &callback);
  EXPECT_EQ(mixer_count(), 0);
  media::FakeAudioRenderCallback another_callback(1);
  scoped_refptr<media::AudioRendererMixerInput> another_input(
      manager_->CreateInput(kAnotherRenderFrameId));
  another_input->Initialize(params, &another_callback);
  EXPECT_EQ(mixer_count(), 0);

  // Implicitly test that AudioRendererMixerInput was provided with the expected
  // callbacks needed to acquire an AudioRendererMixer and remove it.
  input->Start();
  EXPECT_EQ(mixer_count(), 1);
  another_input->Start();
  EXPECT_EQ(mixer_count(), 2);

  // Destroying the inputs should destroy the mixers.
  input->Stop();
  input = NULL;
  EXPECT_EQ(mixer_count(), 1);
  another_input->Stop();
  another_input = NULL;
  EXPECT_EQ(mixer_count(), 0);
}

}  // namespace content
