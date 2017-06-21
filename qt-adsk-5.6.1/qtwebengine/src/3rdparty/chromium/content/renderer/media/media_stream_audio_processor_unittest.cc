// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "content/public/common/media_stream_request.h"
#include "content/renderer/media/media_stream_audio_processor.h"
#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/mock_media_constraint_factory.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Return;

namespace content {

namespace {

#if defined(ANDROID)
const int kAudioProcessingSampleRate = 16000;
#else
const int kAudioProcessingSampleRate = 48000;
#endif
const int kAudioProcessingNumberOfChannel = 1;

// The number of packers used for testing.
const int kNumberOfPacketsForTest = 100;

const int kMaxNumberOfPlayoutDataChannels = 2;

void ReadDataFromSpeechFile(char* data, int length) {
  base::FilePath file;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file));
  file = file.Append(FILE_PATH_LITERAL("media"))
             .Append(FILE_PATH_LITERAL("test"))
             .Append(FILE_PATH_LITERAL("data"))
             .Append(FILE_PATH_LITERAL("speech_16b_stereo_48kHz.raw"));
  DCHECK(base::PathExists(file));
  int64 data_file_size64 = 0;
  DCHECK(base::GetFileSize(file, &data_file_size64));
  EXPECT_EQ(length, base::ReadFile(file, data, length));
  DCHECK(data_file_size64 > length);
}

}  // namespace

class MediaStreamAudioProcessorTest : public ::testing::Test {
 public:
  MediaStreamAudioProcessorTest()
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO, 48000, 16, 512) {
  }

 protected:
  // Helper method to save duplicated code.
  void ProcessDataAndVerifyFormat(MediaStreamAudioProcessor* audio_processor,
                                  int expected_output_sample_rate,
                                  int expected_output_channels,
                                  int expected_output_buffer_size) {
    // Read the audio data from a file.
    const media::AudioParameters& params = audio_processor->InputFormat();
    const int packet_size =
        params.frames_per_buffer() * 2 * params.channels();
    const size_t length = packet_size * kNumberOfPacketsForTest;
    scoped_ptr<char[]> capture_data(new char[length]);
    ReadDataFromSpeechFile(capture_data.get(), length);
    const int16* data_ptr = reinterpret_cast<const int16*>(capture_data.get());
    scoped_ptr<media::AudioBus> data_bus = media::AudioBus::Create(
        params.channels(), params.frames_per_buffer());

    // |data_bus_playout| is used if the number of capture channels is larger
    // that max allowed playout channels. |data_bus_playout_to_use| points to
    // the AudioBus to use, either |data_bus| or |data_bus_playout|.
    scoped_ptr<media::AudioBus> data_bus_playout;
    media::AudioBus* data_bus_playout_to_use = data_bus.get();
    if (params.channels() > kMaxNumberOfPlayoutDataChannels) {
      data_bus_playout =
          media::AudioBus::CreateWrapper(kMaxNumberOfPlayoutDataChannels);
      data_bus_playout->set_frames(params.frames_per_buffer());
      data_bus_playout_to_use = data_bus_playout.get();
    }

    const base::TimeDelta input_capture_delay =
        base::TimeDelta::FromMilliseconds(20);
    const base::TimeDelta output_buffer_duration =
        expected_output_buffer_size * base::TimeDelta::FromSeconds(1) /
            expected_output_sample_rate;
    for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
      data_bus->FromInterleaved(data_ptr, data_bus->frames(), 2);
      audio_processor->PushCaptureData(*data_bus, input_capture_delay);

      // |audio_processor| does nothing when the audio processing is off in
      // the processor.
      webrtc::AudioProcessing* ap = audio_processor->audio_processing_.get();
#if defined(OS_ANDROID) || defined(OS_IOS)
      const bool is_aec_enabled = ap && ap->echo_control_mobile()->is_enabled();
      // AEC should be turned off for mobiles.
      DCHECK(!ap || !ap->echo_cancellation()->is_enabled());
#else
      const bool is_aec_enabled = ap && ap->echo_cancellation()->is_enabled();
#endif
      if (is_aec_enabled) {
        if (params.channels() > kMaxNumberOfPlayoutDataChannels) {
          for (int i = 0; i < kMaxNumberOfPlayoutDataChannels; ++i) {
            data_bus_playout->SetChannelData(
                i, const_cast<float*>(data_bus->channel(i)));
          }
        }
        audio_processor->OnPlayoutData(data_bus_playout_to_use,
                                       params.sample_rate(), 10);
      }

      media::AudioBus* processed_data = nullptr;
      base::TimeDelta capture_delay;
      int new_volume = 0;
      while (audio_processor->ProcessAndConsumeData(
                 255, false, &processed_data, &capture_delay, &new_volume)) {
        EXPECT_TRUE(processed_data);
        EXPECT_NEAR(input_capture_delay.InMillisecondsF(),
                    capture_delay.InMillisecondsF(),
                    output_buffer_duration.InMillisecondsF());
        EXPECT_EQ(audio_processor->OutputFormat().sample_rate(),
                  expected_output_sample_rate);
        EXPECT_EQ(audio_processor->OutputFormat().channels(),
                  expected_output_channels);
        EXPECT_EQ(audio_processor->OutputFormat().frames_per_buffer(),
                  expected_output_buffer_size);
      }

      data_ptr += params.frames_per_buffer() * params.channels();
    }
  }

  void VerifyDefaultComponents(MediaStreamAudioProcessor* audio_processor) {
    webrtc::AudioProcessing* audio_processing =
        audio_processor->audio_processing_.get();
#if defined(OS_ANDROID)
    EXPECT_TRUE(audio_processing->echo_control_mobile()->is_enabled());
    EXPECT_TRUE(audio_processing->echo_control_mobile()->routing_mode() ==
        webrtc::EchoControlMobile::kSpeakerphone);
    EXPECT_FALSE(audio_processing->echo_cancellation()->is_enabled());
#elif !defined(OS_IOS)
    EXPECT_TRUE(audio_processing->echo_cancellation()->is_enabled());
    EXPECT_TRUE(audio_processing->echo_cancellation()->suppression_level() ==
        webrtc::EchoCancellation::kHighSuppression);
    EXPECT_TRUE(audio_processing->echo_cancellation()->are_metrics_enabled());
    EXPECT_TRUE(
        audio_processing->echo_cancellation()->is_delay_logging_enabled());
#endif

    EXPECT_TRUE(audio_processing->noise_suppression()->is_enabled());
    EXPECT_TRUE(audio_processing->noise_suppression()->level() ==
        webrtc::NoiseSuppression::kHigh);
    EXPECT_TRUE(audio_processing->high_pass_filter()->is_enabled());
    EXPECT_TRUE(audio_processing->gain_control()->is_enabled());
#if defined(OS_ANDROID) || defined(OS_IOS)
    EXPECT_TRUE(audio_processing->gain_control()->mode() ==
        webrtc::GainControl::kFixedDigital);
    EXPECT_FALSE(audio_processing->voice_detection()->is_enabled());
#else
    EXPECT_TRUE(audio_processing->gain_control()->mode() ==
        webrtc::GainControl::kAdaptiveAnalog);
    EXPECT_TRUE(audio_processing->voice_detection()->is_enabled());
    EXPECT_TRUE(audio_processing->voice_detection()->likelihood() ==
        webrtc::VoiceDetection::kVeryLowLikelihood);
#endif
  }

  media::AudioParameters params_;
};

// Test crashing with ASAN on Android. crbug.com/468762
#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_WithAudioProcessing DISABLED_WithAudioProcessing
#else
#define MAYBE_WithAudioProcessing WithAudioProcessing
#endif
TEST_F(MediaStreamAudioProcessorTest, MAYBE_WithAudioProcessing) {
  MockMediaConstraintFactory constraint_factory;
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          constraint_factory.CreateWebMediaConstraints(), 0,
          webrtc_audio_device.get()));
  EXPECT_TRUE(audio_processor->has_audio_processing());
  audio_processor->OnCaptureFormatChanged(params_);
  VerifyDefaultComponents(audio_processor.get());

  ProcessDataAndVerifyFormat(audio_processor.get(),
                             kAudioProcessingSampleRate,
                             kAudioProcessingNumberOfChannel,
                             kAudioProcessingSampleRate / 100);
  // Set |audio_processor| to NULL to make sure |webrtc_audio_device| outlives
  // |audio_processor|.
  audio_processor = NULL;
}

TEST_F(MediaStreamAudioProcessorTest, VerifyTabCaptureWithoutAudioProcessing) {
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  // Create MediaStreamAudioProcessor instance for kMediaStreamSourceTab source.
  MockMediaConstraintFactory tab_constraint_factory;
  const std::string tab_string = kMediaStreamSourceTab;
  tab_constraint_factory.AddMandatory(kMediaStreamSource,
                                      tab_string);
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          tab_constraint_factory.CreateWebMediaConstraints(), 0,
          webrtc_audio_device.get()));
  EXPECT_FALSE(audio_processor->has_audio_processing());
  audio_processor->OnCaptureFormatChanged(params_);

  ProcessDataAndVerifyFormat(audio_processor.get(),
                             params_.sample_rate(),
                             params_.channels(),
                             params_.sample_rate() / 100);

  // Create MediaStreamAudioProcessor instance for kMediaStreamSourceSystem
  // source.
  MockMediaConstraintFactory system_constraint_factory;
  const std::string system_string = kMediaStreamSourceSystem;
  system_constraint_factory.AddMandatory(kMediaStreamSource,
                                         system_string);
  audio_processor = new rtc::RefCountedObject<MediaStreamAudioProcessor>(
      system_constraint_factory.CreateWebMediaConstraints(), 0,
      webrtc_audio_device.get());
  EXPECT_FALSE(audio_processor->has_audio_processing());

  // Set |audio_processor| to NULL to make sure |webrtc_audio_device| outlives
  // |audio_processor|.
  audio_processor = NULL;
}

TEST_F(MediaStreamAudioProcessorTest, TurnOffDefaultConstraints) {
  // Turn off the default constraints and pass it to MediaStreamAudioProcessor.
  MockMediaConstraintFactory constraint_factory;
  constraint_factory.DisableDefaultAudioConstraints();
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          constraint_factory.CreateWebMediaConstraints(), 0,
          webrtc_audio_device.get()));
  EXPECT_FALSE(audio_processor->has_audio_processing());
  audio_processor->OnCaptureFormatChanged(params_);

  ProcessDataAndVerifyFormat(audio_processor.get(),
                             params_.sample_rate(),
                             params_.channels(),
                             params_.sample_rate() / 100);
  // Set |audio_processor| to NULL to make sure |webrtc_audio_device| outlives
  // |audio_processor|.
  audio_processor = NULL;
}

TEST_F(MediaStreamAudioProcessorTest, VerifyConstraints) {
  static const char* kDefaultAudioConstraints[] = {
    MediaAudioConstraints::kEchoCancellation,
    MediaAudioConstraints::kGoogAudioMirroring,
    MediaAudioConstraints::kGoogAutoGainControl,
    MediaAudioConstraints::kGoogEchoCancellation,
    MediaAudioConstraints::kGoogExperimentalEchoCancellation,
    MediaAudioConstraints::kGoogExperimentalAutoGainControl,
    MediaAudioConstraints::kGoogExperimentalNoiseSuppression,
    MediaAudioConstraints::kGoogHighpassFilter,
    MediaAudioConstraints::kGoogNoiseSuppression,
    MediaAudioConstraints::kGoogTypingNoiseDetection,
    kMediaStreamAudioHotword
  };

  // Verify mandatory constraints.
  for (size_t i = 0; i < arraysize(kDefaultAudioConstraints); ++i) {
    MockMediaConstraintFactory constraint_factory;
    constraint_factory.AddMandatory(kDefaultAudioConstraints[i], false);
    blink::WebMediaConstraints constraints =
        constraint_factory.CreateWebMediaConstraints();
    MediaAudioConstraints audio_constraints(constraints, 0);
    EXPECT_FALSE(audio_constraints.GetProperty(kDefaultAudioConstraints[i]));
  }

  // Verify optional constraints.
  for (size_t i = 0; i < arraysize(kDefaultAudioConstraints); ++i) {
    MockMediaConstraintFactory constraint_factory;
    constraint_factory.AddOptional(kDefaultAudioConstraints[i], false);
    blink::WebMediaConstraints constraints =
        constraint_factory.CreateWebMediaConstraints();
    MediaAudioConstraints audio_constraints(constraints, 0);
    EXPECT_FALSE(audio_constraints.GetProperty(kDefaultAudioConstraints[i]));
  }

  {
    // Verify echo cancellation is off when platform aec effect is on.
    MockMediaConstraintFactory constraint_factory;
    MediaAudioConstraints audio_constraints(
        constraint_factory.CreateWebMediaConstraints(),
        media::AudioParameters::ECHO_CANCELLER);
    EXPECT_FALSE(audio_constraints.GetEchoCancellationProperty());
  }

  {
    // Verify |kEchoCancellation| overwrite |kGoogEchoCancellation|.
    MockMediaConstraintFactory constraint_factory_1;
    constraint_factory_1.AddOptional(MediaAudioConstraints::kEchoCancellation,
                                   true);
    constraint_factory_1.AddOptional(
        MediaAudioConstraints::kGoogEchoCancellation, false);
    blink::WebMediaConstraints constraints_1 =
        constraint_factory_1.CreateWebMediaConstraints();
    MediaAudioConstraints audio_constraints_1(constraints_1, 0);
    EXPECT_TRUE(audio_constraints_1.GetEchoCancellationProperty());

    MockMediaConstraintFactory constraint_factory_2;
    constraint_factory_2.AddOptional(MediaAudioConstraints::kEchoCancellation,
                                     false);
    constraint_factory_2.AddOptional(
        MediaAudioConstraints::kGoogEchoCancellation, true);
    blink::WebMediaConstraints constraints_2 =
        constraint_factory_2.CreateWebMediaConstraints();
    MediaAudioConstraints audio_constraints_2(constraints_2, 0);
    EXPECT_FALSE(audio_constraints_2.GetEchoCancellationProperty());
  }

  {
    // When |kEchoCancellation| is explicitly set to false, the default values
    // for all the constraints except |kMediaStreamAudioDucking| are false.
    MockMediaConstraintFactory constraint_factory;
    constraint_factory.AddOptional(MediaAudioConstraints::kEchoCancellation,
                                   false);
    blink::WebMediaConstraints constraints =
        constraint_factory.CreateWebMediaConstraints();
    MediaAudioConstraints audio_constraints(constraints, 0);
    for (size_t i = 0; i < arraysize(kDefaultAudioConstraints); ++i) {
      EXPECT_FALSE(audio_constraints.GetProperty(kDefaultAudioConstraints[i]));
    }
#if defined(OS_WIN)
    EXPECT_TRUE(audio_constraints.GetProperty(kMediaStreamAudioDucking));
#else
    EXPECT_FALSE(audio_constraints.GetProperty(kMediaStreamAudioDucking));
#endif
  }

  {
    // |kMediaStreamAudioHotword| is always off by default.
    MockMediaConstraintFactory constraint_factory;
    MediaAudioConstraints audio_constraints(
        constraint_factory.CreateWebMediaConstraints(), 0);
    EXPECT_FALSE(audio_constraints.GetProperty(kMediaStreamAudioHotword));
  }
}

TEST_F(MediaStreamAudioProcessorTest, ValidateConstraints) {
  MockMediaConstraintFactory constraint_factory;
  const std::string dummy_constraint = "dummy";
  constraint_factory.AddMandatory(dummy_constraint, true);
  MediaAudioConstraints audio_constraints(
      constraint_factory.CreateWebMediaConstraints(), 0);
  EXPECT_FALSE(audio_constraints.IsValid());
}

// Test crashing with ASAN on Android. crbug.com/468762
#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_TestAllSampleRates DISABLED_TestAllSampleRates
#else
#define MAYBE_TestAllSampleRates TestAllSampleRates
#endif
TEST_F(MediaStreamAudioProcessorTest, MAYBE_TestAllSampleRates) {
  MockMediaConstraintFactory constraint_factory;
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          constraint_factory.CreateWebMediaConstraints(), 0,
          webrtc_audio_device.get()));
  EXPECT_TRUE(audio_processor->has_audio_processing());

  static const int kSupportedSampleRates[] =
      { 8000, 16000, 22050, 32000, 44100, 48000, 88200, 96000 };
  for (size_t i = 0; i < arraysize(kSupportedSampleRates); ++i) {
    int buffer_size = (kSupportedSampleRates[i] / 100)  < 128 ?
        kSupportedSampleRates[i] / 100 : 128;
    media::AudioParameters params(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::CHANNEL_LAYOUT_STEREO, kSupportedSampleRates[i], 16,
        buffer_size);
    audio_processor->OnCaptureFormatChanged(params);
    VerifyDefaultComponents(audio_processor.get());

    ProcessDataAndVerifyFormat(audio_processor.get(),
                               kAudioProcessingSampleRate,
                               kAudioProcessingNumberOfChannel,
                               kAudioProcessingSampleRate / 100);
  }

  // Set |audio_processor| to NULL to make sure |webrtc_audio_device|
  // outlives |audio_processor|.
  audio_processor = NULL;
}

// Test that if we have an AEC dump message filter created, we are getting it
// correctly in MSAP. Any IPC messages will be deleted since no sender in the
// filter will be created.
TEST_F(MediaStreamAudioProcessorTest, GetAecDumpMessageFilter) {
  base::MessageLoopForUI message_loop;
  scoped_refptr<AecDumpMessageFilter> aec_dump_message_filter_(
      new AecDumpMessageFilter(message_loop.task_runner(),
                               message_loop.task_runner()));

  MockMediaConstraintFactory constraint_factory;
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          constraint_factory.CreateWebMediaConstraints(), 0,
          webrtc_audio_device.get()));

  EXPECT_TRUE(audio_processor->aec_dump_message_filter_.get());

  audio_processor = NULL;
}

TEST_F(MediaStreamAudioProcessorTest, TestStereoAudio) {
  // Set up the correct constraints to turn off the audio processing and turn
  // on the stereo channels mirroring.
  MockMediaConstraintFactory constraint_factory;
  constraint_factory.AddMandatory(MediaAudioConstraints::kEchoCancellation,
                                  false);
  constraint_factory.AddMandatory(MediaAudioConstraints::kGoogAudioMirroring,
                                  true);
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          constraint_factory.CreateWebMediaConstraints(), 0,
          webrtc_audio_device.get()));
  EXPECT_FALSE(audio_processor->has_audio_processing());
  const media::AudioParameters source_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_STEREO, 48000, 16, 480);
  audio_processor->OnCaptureFormatChanged(source_params);
  EXPECT_EQ(audio_processor->OutputFormat().channels(), 2);

  // Construct left and right channels, and assign different values to the
  // first data of the left channel and right channel.
  const int size = media::AudioBus::CalculateMemorySize(source_params);
  scoped_ptr<float, base::AlignedFreeDeleter> left_channel(
      static_cast<float*>(base::AlignedAlloc(size, 32)));
  scoped_ptr<float, base::AlignedFreeDeleter> right_channel(
      static_cast<float*>(base::AlignedAlloc(size, 32)));
  scoped_ptr<media::AudioBus> wrapper = media::AudioBus::CreateWrapper(
      source_params.channels());
  wrapper->set_frames(source_params.frames_per_buffer());
  wrapper->SetChannelData(0, left_channel.get());
  wrapper->SetChannelData(1, right_channel.get());
  wrapper->Zero();
  float* left_channel_ptr = left_channel.get();
  left_channel_ptr[0] = 1.0f;

  // Run the test consecutively to make sure the stereo channels are not
  // flipped back and forth.
  static const int kNumberOfPacketsForTest = 100;
  const base::TimeDelta pushed_capture_delay =
      base::TimeDelta::FromMilliseconds(42);
  for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
    audio_processor->PushCaptureData(*wrapper, pushed_capture_delay);

    media::AudioBus* processed_data = nullptr;
    base::TimeDelta capture_delay;
    int new_volume = 0;
    EXPECT_TRUE(audio_processor->ProcessAndConsumeData(
        0, false, &processed_data, &capture_delay, &new_volume));
    EXPECT_TRUE(processed_data);
    EXPECT_EQ(processed_data->channel(0)[0], 0);
    EXPECT_NE(processed_data->channel(1)[0], 0);
    EXPECT_EQ(pushed_capture_delay, capture_delay);
  }

  // Set |audio_processor| to NULL to make sure |webrtc_audio_device| outlives
  // |audio_processor|.
  audio_processor = NULL;
}

// Disabled on android clang builds due to crbug.com/470499
#if defined(__clang__) && defined(OS_ANDROID)
#define MAYBE_TestWithKeyboardMicChannel DISABLED_TestWithKeyboardMicChannel
#else
#define MAYBE_TestWithKeyboardMicChannel TestWithKeyboardMicChannel
#endif

TEST_F(MediaStreamAudioProcessorTest, MAYBE_TestWithKeyboardMicChannel) {
  MockMediaConstraintFactory constraint_factory;
  constraint_factory.AddMandatory(
      MediaAudioConstraints::kGoogExperimentalNoiseSuppression, true);
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          constraint_factory.CreateWebMediaConstraints(), 0,
          webrtc_audio_device.get()));
  EXPECT_TRUE(audio_processor->has_audio_processing());

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC,
                                48000, 16, 512);
  audio_processor->OnCaptureFormatChanged(params);

  ProcessDataAndVerifyFormat(audio_processor.get(),
                             kAudioProcessingSampleRate,
                             kAudioProcessingNumberOfChannel,
                             kAudioProcessingSampleRate / 100);
  // Set |audio_processor| to NULL to make sure |webrtc_audio_device| outlives
  // |audio_processor|.
  audio_processor = NULL;
}

}  // namespace content
