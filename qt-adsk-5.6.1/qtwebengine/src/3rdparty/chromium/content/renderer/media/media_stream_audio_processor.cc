// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_processor.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/trace_event/trace_event.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_fifo.h"
#include "media/base/channel_layout.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.h"
#include "third_party/webrtc/modules/audio_processing/typing_detection.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#endif

namespace content {

namespace {

using webrtc::AudioProcessing;
using webrtc::NoiseSuppression;

const int kAudioProcessingNumberOfChannels = 1;

AudioProcessing::ChannelLayout MapLayout(media::ChannelLayout media_layout) {
  switch (media_layout) {
    case media::CHANNEL_LAYOUT_MONO:
      return AudioProcessing::kMono;
    case media::CHANNEL_LAYOUT_STEREO:
      return AudioProcessing::kStereo;
    case media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC:
      return AudioProcessing::kStereoAndKeyboard;
    default:
      NOTREACHED() << "Layout not supported: " << media_layout;
      return AudioProcessing::kMono;
  }
}

// This is only used for playout data where only max two channels is supported.
AudioProcessing::ChannelLayout ChannelsToLayout(int num_channels) {
  switch (num_channels) {
    case 1:
      return AudioProcessing::kMono;
    case 2:
      return AudioProcessing::kStereo;
    default:
      NOTREACHED() << "Channels not supported: " << num_channels;
      return AudioProcessing::kMono;
  }
}

// Used by UMA histograms and entries shouldn't be re-ordered or removed.
enum AudioTrackProcessingStates {
  AUDIO_PROCESSING_ENABLED = 0,
  AUDIO_PROCESSING_DISABLED,
  AUDIO_PROCESSING_IN_WEBRTC,
  AUDIO_PROCESSING_MAX
};

void RecordProcessingState(AudioTrackProcessingStates state) {
  UMA_HISTOGRAM_ENUMERATION("Media.AudioTrackProcessingStates",
                            state, AUDIO_PROCESSING_MAX);
}

bool IsDelayAgnosticAecEnabled() {
  // Note: It's important to query the field trial state first, to ensure that
  // UMA reports the correct group.
  const std::string group_name =
      base::FieldTrialList::FindFullName("UseDelayAgnosticAEC");
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableDelayAgnosticAec))
    return true;
  if (command_line->HasSwitch(switches::kDisableDelayAgnosticAec))
    return false;

  return (group_name == "Enabled" || group_name == "DefaultEnabled");
}

bool IsBeamformingEnabled(const MediaAudioConstraints& audio_constraints) {
  return base::FieldTrialList::FindFullName("ChromebookBeamforming") ==
             "Enabled" ||
         audio_constraints.GetProperty(MediaAudioConstraints::kGoogBeamforming);
}

}  // namespace

// Wraps AudioBus to provide access to the array of channel pointers, since this
// is the type webrtc::AudioProcessing deals in. The array is refreshed on every
// channel_ptrs() call, and will be valid until the underlying AudioBus pointers
// are changed, e.g. through calls to SetChannelData() or SwapChannels().
//
// All methods are called on one of the capture or render audio threads
// exclusively.
class MediaStreamAudioBus {
 public:
  MediaStreamAudioBus(int channels, int frames)
      : bus_(media::AudioBus::Create(channels, frames)),
        channel_ptrs_(new float*[channels]) {
    // May be created in the main render thread and used in the audio threads.
    thread_checker_.DetachFromThread();
  }

  media::AudioBus* bus() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return bus_.get();
  }

  float* const* channel_ptrs() {
    DCHECK(thread_checker_.CalledOnValidThread());
    for (int i = 0; i < bus_->channels(); ++i) {
      channel_ptrs_[i] = bus_->channel(i);
    }
    return channel_ptrs_.get();
  }

 private:
  base::ThreadChecker thread_checker_;
  scoped_ptr<media::AudioBus> bus_;
  scoped_ptr<float*[]> channel_ptrs_;
};

// Wraps AudioFifo to provide a cleaner interface to MediaStreamAudioProcessor.
// It avoids the FIFO when the source and destination frames match. All methods
// are called on one of the capture or render audio threads exclusively. If
// |source_channels| is larger than |destination_channels|, only the first
// |destination_channels| are kept from the source.
class MediaStreamAudioFifo {
 public:
  MediaStreamAudioFifo(int source_channels,
                       int destination_channels,
                       int source_frames,
                       int destination_frames,
                       int sample_rate)
     : source_channels_(source_channels),
       source_frames_(source_frames),
       sample_rate_(sample_rate),
       destination_(
           new MediaStreamAudioBus(destination_channels, destination_frames)),
       data_available_(false) {
    DCHECK_GE(source_channels, destination_channels);
    DCHECK_GT(sample_rate_, 0);

    if (source_channels > destination_channels) {
      audio_source_intermediate_ =
          media::AudioBus::CreateWrapper(destination_channels);
    }

    if (source_frames != destination_frames) {
      // Since we require every Push to be followed by as many Consumes as
      // possible, twice the larger of the two is a (probably) loose upper bound
      // on the FIFO size.
      const int fifo_frames = 2 * std::max(source_frames, destination_frames);
      fifo_.reset(new media::AudioFifo(destination_channels, fifo_frames));
    }

    // May be created in the main render thread and used in the audio threads.
    thread_checker_.DetachFromThread();
  }

  void Push(const media::AudioBus& source, base::TimeDelta audio_delay) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK_EQ(source.channels(), source_channels_);
    DCHECK_EQ(source.frames(), source_frames_);

    const media::AudioBus* source_to_push = &source;

    if (audio_source_intermediate_) {
      for (int i = 0; i < destination_->bus()->channels(); ++i) {
        audio_source_intermediate_->SetChannelData(
            i,
            const_cast<float*>(source.channel(i)));
      }
      audio_source_intermediate_->set_frames(source.frames());
      source_to_push = audio_source_intermediate_.get();
    }

    if (fifo_) {
      next_audio_delay_ = audio_delay +
          fifo_->frames() * base::TimeDelta::FromSeconds(1) / sample_rate_;
      fifo_->Push(source_to_push);
    } else {
      source_to_push->CopyTo(destination_->bus());
      next_audio_delay_ = audio_delay;
      data_available_ = true;
    }
  }

  // Returns true if there are destination_frames() of data available to be
  // consumed, and otherwise false.
  bool Consume(MediaStreamAudioBus** destination,
               base::TimeDelta* audio_delay) {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (fifo_) {
      if (fifo_->frames() < destination_->bus()->frames())
        return false;

      fifo_->Consume(destination_->bus(), 0, destination_->bus()->frames());
      *audio_delay = next_audio_delay_;
      next_audio_delay_ -=
          destination_->bus()->frames() * base::TimeDelta::FromSeconds(1) /
              sample_rate_;
    } else {
      if (!data_available_)
        return false;
      *audio_delay = next_audio_delay_;
      // The data was already copied to |destination_| in this case.
      data_available_ = false;
    }

    *destination = destination_.get();
    return true;
  }

 private:
  base::ThreadChecker thread_checker_;
  const int source_channels_;  // For a DCHECK.
  const int source_frames_;  // For a DCHECK.
  const int sample_rate_;
  scoped_ptr<media::AudioBus> audio_source_intermediate_;
  scoped_ptr<MediaStreamAudioBus> destination_;
  scoped_ptr<media::AudioFifo> fifo_;

  // When using |fifo_|, this is the audio delay of the first sample to be
  // consumed next from the FIFO.  When not using |fifo_|, this is the audio
  // delay of the first sample in |destination_|.
  base::TimeDelta next_audio_delay_;

  // True when |destination_| contains the data to be returned by the next call
  // to Consume().  Only used when the FIFO is disabled.
  bool data_available_;
};

MediaStreamAudioProcessor::MediaStreamAudioProcessor(
    const blink::WebMediaConstraints& constraints,
    int effects,
    WebRtcPlayoutDataSource* playout_data_source)
    : render_delay_ms_(0),
      playout_data_source_(playout_data_source),
      audio_mirroring_(false),
      typing_detected_(false),
      stopped_(false) {
  capture_thread_checker_.DetachFromThread();
  render_thread_checker_.DetachFromThread();
  InitializeAudioProcessingModule(constraints, effects);

  aec_dump_message_filter_ = AecDumpMessageFilter::Get();
  // In unit tests not creating a message filter, |aec_dump_message_filter_|
  // will be NULL. We can just ignore that. Other unit tests and browser tests
  // ensure that we do get the filter when we should.
  if (aec_dump_message_filter_.get())
    aec_dump_message_filter_->AddDelegate(this);
}

MediaStreamAudioProcessor::~MediaStreamAudioProcessor() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  Stop();
}

void MediaStreamAudioProcessor::OnCaptureFormatChanged(
    const media::AudioParameters& input_format) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // There is no need to hold a lock here since the caller guarantees that
  // there is no more PushCaptureData() and ProcessAndConsumeData() callbacks
  // on the capture thread.
  InitializeCaptureFifo(input_format);

  // Reset the |capture_thread_checker_| since the capture data will come from
  // a new capture thread.
  capture_thread_checker_.DetachFromThread();
}

void MediaStreamAudioProcessor::PushCaptureData(
    const media::AudioBus& audio_source,
    base::TimeDelta capture_delay) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());

  capture_fifo_->Push(audio_source, capture_delay);
}

bool MediaStreamAudioProcessor::ProcessAndConsumeData(
    int volume,
    bool key_pressed,
    media::AudioBus** processed_data,
    base::TimeDelta* capture_delay,
    int* new_volume) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  DCHECK(processed_data);
  DCHECK(capture_delay);
  DCHECK(new_volume);

  TRACE_EVENT0("audio", "MediaStreamAudioProcessor::ProcessAndConsumeData");

  MediaStreamAudioBus* process_bus;
  if (!capture_fifo_->Consume(&process_bus, capture_delay))
    return false;

  // Use the process bus directly if audio processing is disabled.
  MediaStreamAudioBus* output_bus = process_bus;
  *new_volume = 0;
  if (audio_processing_) {
    output_bus = output_bus_.get();
    *new_volume = ProcessData(process_bus->channel_ptrs(),
                              process_bus->bus()->frames(), *capture_delay,
                              volume, key_pressed, output_bus->channel_ptrs());
  }

  // Swap channels before interleaving the data.
  if (audio_mirroring_ &&
      output_format_.channel_layout() == media::CHANNEL_LAYOUT_STEREO) {
    // Swap the first and second channels.
    output_bus->bus()->SwapChannels(0, 1);
  }

  *processed_data = output_bus->bus();

  return true;
}

void MediaStreamAudioProcessor::Stop() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (stopped_)
    return;

  stopped_ = true;

  if (aec_dump_message_filter_.get()) {
    aec_dump_message_filter_->RemoveDelegate(this);
    aec_dump_message_filter_ = NULL;
  }

  if (!audio_processing_.get())
    return;

  audio_processing_.get()->UpdateHistogramsOnCallEnd();
  StopEchoCancellationDump(audio_processing_.get());

  if (playout_data_source_) {
    playout_data_source_->RemovePlayoutSink(this);
    playout_data_source_ = NULL;
  }
}

const media::AudioParameters& MediaStreamAudioProcessor::InputFormat() const {
  return input_format_;
}

const media::AudioParameters& MediaStreamAudioProcessor::OutputFormat() const {
  return output_format_;
}

void MediaStreamAudioProcessor::OnAecDumpFile(
    const IPC::PlatformFileForTransit& file_handle) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  base::File file = IPC::PlatformFileForTransitToFile(file_handle);
  DCHECK(file.IsValid());

  if (audio_processing_)
    StartEchoCancellationDump(audio_processing_.get(), file.Pass());
  else
    file.Close();
}

void MediaStreamAudioProcessor::OnDisableAecDump() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (audio_processing_)
    StopEchoCancellationDump(audio_processing_.get());
}

void MediaStreamAudioProcessor::OnIpcClosing() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  aec_dump_message_filter_ = NULL;
}

void MediaStreamAudioProcessor::OnPlayoutData(media::AudioBus* audio_bus,
                                              int sample_rate,
                                              int audio_delay_milliseconds) {
  DCHECK(render_thread_checker_.CalledOnValidThread());
  DCHECK(audio_processing_->echo_control_mobile()->is_enabled() ^
         audio_processing_->echo_cancellation()->is_enabled());

  TRACE_EVENT0("audio", "MediaStreamAudioProcessor::OnPlayoutData");
  DCHECK_LT(audio_delay_milliseconds,
            std::numeric_limits<base::subtle::Atomic32>::max());
  base::subtle::Release_Store(&render_delay_ms_, audio_delay_milliseconds);

  InitializeRenderFifoIfNeeded(sample_rate, audio_bus->channels(),
                               audio_bus->frames());

  render_fifo_->Push(
      *audio_bus, base::TimeDelta::FromMilliseconds(audio_delay_milliseconds));
  MediaStreamAudioBus* analysis_bus;
  base::TimeDelta audio_delay;
  while (render_fifo_->Consume(&analysis_bus, &audio_delay)) {
    // TODO(ajm): Should AnalyzeReverseStream() account for the |audio_delay|?
    audio_processing_->AnalyzeReverseStream(
        analysis_bus->channel_ptrs(),
        analysis_bus->bus()->frames(),
        sample_rate,
        ChannelsToLayout(audio_bus->channels()));
  }
}

void MediaStreamAudioProcessor::OnPlayoutDataSourceChanged() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // There is no need to hold a lock here since the caller guarantees that
  // there is no more OnPlayoutData() callback on the render thread.
  render_thread_checker_.DetachFromThread();
  render_fifo_.reset();
}

void MediaStreamAudioProcessor::GetStats(AudioProcessorStats* stats) {
  stats->typing_noise_detected =
      (base::subtle::Acquire_Load(&typing_detected_) != false);
  GetAecStats(audio_processing_.get()->echo_cancellation(), stats);
}

void MediaStreamAudioProcessor::InitializeAudioProcessingModule(
    const blink::WebMediaConstraints& constraints, int effects) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(!audio_processing_);

  MediaAudioConstraints audio_constraints(constraints, effects);

  // Audio mirroring can be enabled even though audio processing is otherwise
  // disabled.
  audio_mirroring_ = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogAudioMirroring);

#if defined(OS_IOS)
  // On iOS, VPIO provides built-in AGC and AEC.
  const bool echo_cancellation = false;
  const bool goog_agc = false;
#else
  const bool echo_cancellation =
      audio_constraints.GetEchoCancellationProperty();
  const bool goog_agc = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogAutoGainControl);
#endif

#if defined(OS_IOS) || defined(OS_ANDROID)
  const bool goog_experimental_aec = false;
  const bool goog_typing_detection = false;
#else
  const bool goog_experimental_aec = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogExperimentalEchoCancellation);
  const bool goog_typing_detection = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogTypingNoiseDetection);
#endif

  const bool goog_ns = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogNoiseSuppression);
  const bool goog_experimental_ns = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogExperimentalNoiseSuppression);
  const bool goog_beamforming = IsBeamformingEnabled(audio_constraints);
  const bool goog_high_pass_filter = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogHighpassFilter);
  // Return immediately if no goog constraint is enabled.
  if (!echo_cancellation && !goog_experimental_aec && !goog_ns &&
      !goog_high_pass_filter && !goog_typing_detection &&
      !goog_agc && !goog_experimental_ns && !goog_beamforming) {
    RecordProcessingState(AUDIO_PROCESSING_DISABLED);
    return;
  }

  // Experimental options provided at creation.
  webrtc::Config config;
  if (goog_experimental_aec)
    config.Set<webrtc::ExtendedFilter>(new webrtc::ExtendedFilter(true));
  if (goog_experimental_ns)
    config.Set<webrtc::ExperimentalNs>(new webrtc::ExperimentalNs(true));
  if (IsDelayAgnosticAecEnabled())
    config.Set<webrtc::DelayAgnostic>(new webrtc::DelayAgnostic(true));
  if (goog_beamforming) {
    ConfigureBeamforming(&config, audio_constraints.GetPropertyAsString(
        MediaAudioConstraints::kGoogArrayGeometry));
  }

  // Create and configure the webrtc::AudioProcessing.
  audio_processing_.reset(webrtc::AudioProcessing::Create(config));

  // Enable the audio processing components.
  if (echo_cancellation) {
    EnableEchoCancellation(audio_processing_.get());

    if (playout_data_source_)
      playout_data_source_->AddPlayoutSink(this);

    // Prepare for logging echo information. If there are data remaining in
    // |echo_information_| we simply discard it.
    echo_information_.reset(new EchoInformation());
  }

  if (goog_ns) {
    // The beamforming postfilter is effective at suppressing stationary noise,
    // so reduce the single-channel NS aggressiveness when enabled.
    const NoiseSuppression::Level ns_level =
        config.Get<webrtc::Beamforming>().enabled ? NoiseSuppression::kLow
                                                  : NoiseSuppression::kHigh;

    EnableNoiseSuppression(audio_processing_.get(), ns_level);
  }

  if (goog_high_pass_filter)
    EnableHighPassFilter(audio_processing_.get());

  if (goog_typing_detection) {
    // TODO(xians): Remove this |typing_detector_| after the typing suppression
    // is enabled by default.
    typing_detector_.reset(new webrtc::TypingDetection());
    EnableTypingDetection(audio_processing_.get(), typing_detector_.get());
  }

  if (goog_agc)
    EnableAutomaticGainControl(audio_processing_.get());

  RecordProcessingState(AUDIO_PROCESSING_ENABLED);
}

void MediaStreamAudioProcessor::ConfigureBeamforming(
    webrtc::Config* config,
    const std::string& geometry_str) const {
  std::vector<webrtc::Point> geometry = ParseArrayGeometry(geometry_str);
#if defined(OS_CHROMEOS)
  if (geometry.size() == 0) {
    const std::string board = base::SysInfo::GetLsbReleaseBoard();
    if (board.find("nyan_kitty") != std::string::npos) {
      geometry.push_back(webrtc::Point(-0.03f, 0.f, 0.f));
      geometry.push_back(webrtc::Point(0.03f, 0.f, 0.f));
    } else if (board.find("peach_pi") != std::string::npos) {
      geometry.push_back(webrtc::Point(-0.025f, 0.f, 0.f));
      geometry.push_back(webrtc::Point(0.025f, 0.f, 0.f));
    } else if (board.find("samus") != std::string::npos) {
      geometry.push_back(webrtc::Point(-0.032f, 0.f, 0.f));
      geometry.push_back(webrtc::Point(0.032f, 0.f, 0.f));
    } else if (board.find("swanky") != std::string::npos) {
      geometry.push_back(webrtc::Point(-0.026f, 0.f, 0.f));
      geometry.push_back(webrtc::Point(0.026f, 0.f, 0.f));
    }
  }
#endif
  config->Set<webrtc::Beamforming>(new webrtc::Beamforming(geometry.size() > 1,
                                                           geometry));
}

std::vector<webrtc::Point> MediaStreamAudioProcessor::ParseArrayGeometry(
    const std::string& geometry_str) const {
  std::vector<webrtc::Point> result;
  std::vector<float> values;
  std::istringstream str(geometry_str);
  std::copy(std::istream_iterator<float>(str),
            std::istream_iterator<float>(),
            std::back_inserter(values));
  if (values.size() % 3 == 0) {
    for (size_t i = 0; i < values.size(); i += 3) {
      result.push_back(webrtc::Point(values[i + 0],
                                     values[i + 1],
                                     values[i + 2]));
    }
  }
  return result;
}

void MediaStreamAudioProcessor::InitializeCaptureFifo(
    const media::AudioParameters& input_format) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(input_format.IsValid());
  input_format_ = input_format;

  // TODO(ajm): For now, we assume fixed parameters for the output when audio
  // processing is enabled, to match the previous behavior. We should either
  // use the input parameters (in which case, audio processing will convert
  // at output) or ideally, have a backchannel from the sink to know what
  // format it would prefer.
#if defined(OS_ANDROID)
  int audio_processing_sample_rate = AudioProcessing::kSampleRate16kHz;
#else
  int audio_processing_sample_rate = AudioProcessing::kSampleRate48kHz;
#endif
  const int output_sample_rate = audio_processing_ ?
                                 audio_processing_sample_rate :
                                 input_format.sample_rate();
  media::ChannelLayout output_channel_layout = audio_processing_ ?
      media::GuessChannelLayout(kAudioProcessingNumberOfChannels) :
      input_format.channel_layout();

  // The output channels from the fifo is normally the same as input.
  int fifo_output_channels = input_format.channels();

  // Special case for if we have a keyboard mic channel on the input and no
  // audio processing is used. We will then have the fifo strip away that
  // channel. So we use stereo as output layout, and also change the output
  // channels for the fifo.
  if (input_format.channel_layout() ==
          media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC &&
      !audio_processing_) {
    output_channel_layout = media::CHANNEL_LAYOUT_STEREO;
    fifo_output_channels = ChannelLayoutToChannelCount(output_channel_layout);
  }

  // webrtc::AudioProcessing requires a 10 ms chunk size. We use this native
  // size when processing is enabled. When disabled we use the same size as
  // the source if less than 10 ms.
  //
  // TODO(ajm): This conditional buffer size appears to be assuming knowledge of
  // the sink based on the source parameters. PeerConnection sinks seem to want
  // 10 ms chunks regardless, while WebAudio sinks want less, and we're assuming
  // we can identify WebAudio sinks by the input chunk size. Less fragile would
  // be to have the sink actually tell us how much it wants (as in the above
  // TODO).
  int processing_frames = input_format.sample_rate() / 100;
  int output_frames = output_sample_rate / 100;
  if (!audio_processing_ && input_format.frames_per_buffer() < output_frames) {
    processing_frames = input_format.frames_per_buffer();
    output_frames = processing_frames;
  }

  output_format_ = media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      output_channel_layout,
      output_sample_rate,
      16,
      output_frames);

  capture_fifo_.reset(
      new MediaStreamAudioFifo(input_format.channels(),
                               fifo_output_channels,
                               input_format.frames_per_buffer(),
                               processing_frames,
                               input_format.sample_rate()));

  if (audio_processing_) {
    output_bus_.reset(new MediaStreamAudioBus(output_format_.channels(),
                                              output_frames));
  }
}

void MediaStreamAudioProcessor::InitializeRenderFifoIfNeeded(
    int sample_rate, int number_of_channels, int frames_per_buffer) {
  DCHECK(render_thread_checker_.CalledOnValidThread());
  if (render_fifo_.get() &&
      render_format_.sample_rate() == sample_rate &&
      render_format_.channels() == number_of_channels &&
      render_format_.frames_per_buffer() == frames_per_buffer) {
    // Do nothing if the |render_fifo_| has been setup properly.
    return;
  }

  render_format_ = media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::GuessChannelLayout(number_of_channels),
      sample_rate,
      16,
      frames_per_buffer);

  const int analysis_frames = sample_rate / 100;  // 10 ms chunks.
  render_fifo_.reset(
      new MediaStreamAudioFifo(number_of_channels,
                               number_of_channels,
                               frames_per_buffer,
                               analysis_frames,
                               sample_rate));
}

int MediaStreamAudioProcessor::ProcessData(const float* const* process_ptrs,
                                           int process_frames,
                                           base::TimeDelta capture_delay,
                                           int volume,
                                           bool key_pressed,
                                           float* const* output_ptrs) {
  DCHECK(audio_processing_);
  DCHECK(capture_thread_checker_.CalledOnValidThread());

  TRACE_EVENT0("audio", "MediaStreamAudioProcessor::ProcessData");

  base::subtle::Atomic32 render_delay_ms =
      base::subtle::Acquire_Load(&render_delay_ms_);
  int64 capture_delay_ms = capture_delay.InMilliseconds();
  DCHECK_LT(capture_delay_ms,
            std::numeric_limits<base::subtle::Atomic32>::max());
  int total_delay_ms =  capture_delay_ms + render_delay_ms;
  if (total_delay_ms > 300) {
    LOG(WARNING) << "Large audio delay, capture delay: " << capture_delay_ms
                 << "ms; render delay: " << render_delay_ms << "ms";
  }

  webrtc::AudioProcessing* ap = audio_processing_.get();
  ap->set_stream_delay_ms(total_delay_ms);

  DCHECK_LE(volume, WebRtcAudioDeviceImpl::kMaxVolumeLevel);
  webrtc::GainControl* agc = ap->gain_control();
  int err = agc->set_stream_analog_level(volume);
  DCHECK_EQ(err, 0) << "set_stream_analog_level() error: " << err;

  ap->set_stream_key_pressed(key_pressed);

  err = ap->ProcessStream(process_ptrs,
                          process_frames,
                          input_format_.sample_rate(),
                          MapLayout(input_format_.channel_layout()),
                          output_format_.sample_rate(),
                          MapLayout(output_format_.channel_layout()),
                          output_ptrs);
  DCHECK_EQ(err, 0) << "ProcessStream() error: " << err;

  if (typing_detector_) {
    webrtc::VoiceDetection* vad = ap->voice_detection();
    DCHECK(vad->is_enabled());
    bool detected = typing_detector_->Process(key_pressed,
                                              vad->stream_has_voice());
    base::subtle::Release_Store(&typing_detected_, detected);
  }

  if (echo_information_) {
    echo_information_.get()->UpdateAecDelayStats(ap->echo_cancellation());
  }

  // Return 0 if the volume hasn't been changed, and otherwise the new volume.
  return (agc->stream_analog_level() == volume) ?
      0 : agc->stream_analog_level();
}

}  // namespace content
