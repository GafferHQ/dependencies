// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_renderer.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_logging.h"
#include "content/renderer/render_frame_impl.h"
#include "media/audio/audio_output_device.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/sample_rates.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "third_party/libjingle/source/talk/media/base/audiorenderer.h"


#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "media/audio/win/core_audio_util_win.h"
#endif

namespace content {

namespace {

// We add a UMA histogram measuring the execution time of the Render() method
// every |kNumCallbacksBetweenRenderTimeHistograms| callback. Assuming 10ms
// between each callback leads to one UMA update each 100ms.
const int kNumCallbacksBetweenRenderTimeHistograms = 10;

// This is a simple wrapper class that's handed out to users of a shared
// WebRtcAudioRenderer instance.  This class maintains the per-user 'playing'
// and 'started' states to avoid problems related to incorrect usage which
// might violate the implementation assumptions inside WebRtcAudioRenderer
// (see the play reference count).
class SharedAudioRenderer : public MediaStreamAudioRenderer {
 public:
  // Callback definition for a callback that is called when when Play(), Pause()
  // or SetVolume are called (whenever the internal |playing_state_| changes).
  typedef base::Callback<
      void(const scoped_refptr<webrtc::MediaStreamInterface>&,
           WebRtcAudioRenderer::PlayingState*)> OnPlayStateChanged;

  SharedAudioRenderer(
      const scoped_refptr<MediaStreamAudioRenderer>& delegate,
      const scoped_refptr<webrtc::MediaStreamInterface>& media_stream,
      const OnPlayStateChanged& on_play_state_changed)
      : delegate_(delegate), media_stream_(media_stream), started_(false),
        on_play_state_changed_(on_play_state_changed) {
    DCHECK(!on_play_state_changed_.is_null());
    DCHECK(media_stream_.get());
  }

 protected:
  ~SharedAudioRenderer() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DVLOG(1) << __FUNCTION__;
    Stop();
  }

  void Start() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (started_)
      return;
    started_ = true;
    delegate_->Start();
  }

  void Play() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(started_);
    if (playing_state_.playing())
      return;
    playing_state_.set_playing(true);
    on_play_state_changed_.Run(media_stream_, &playing_state_);
  }

  void Pause() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(started_);
    if (!playing_state_.playing())
      return;
    playing_state_.set_playing(false);
    on_play_state_changed_.Run(media_stream_, &playing_state_);
  }

  void Stop() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!started_)
      return;
    Pause();
    started_ = false;
    delegate_->Stop();
  }

  void SetVolume(float volume) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(volume >= 0.0f && volume <= 1.0f);
    playing_state_.set_volume(volume);
    on_play_state_changed_.Run(media_stream_, &playing_state_);
  }

  void SwitchOutputDevice(
      const std::string& device_id,
      const GURL& security_origin,
      const media::SwitchOutputDeviceCB& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DVLOG(1) << __FUNCTION__
             << "(" << device_id << ", " << security_origin << ")";
    delegate_->SwitchOutputDevice(device_id, security_origin, callback);
  }

  base::TimeDelta GetCurrentRenderTime() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return delegate_->GetCurrentRenderTime();
  }

  bool IsLocalRenderer() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return delegate_->IsLocalRenderer();
  }

 private:
  base::ThreadChecker thread_checker_;
  const scoped_refptr<MediaStreamAudioRenderer> delegate_;
  const scoped_refptr<webrtc::MediaStreamInterface> media_stream_;
  bool started_;
  WebRtcAudioRenderer::PlayingState playing_state_;
  OnPlayStateChanged on_play_state_changed_;
};

// Returns either AudioParameters::NO_EFFECTS or AudioParameters::DUCKING
// depending on whether or not an input element is currently open with
// ducking enabled.
int GetCurrentDuckingFlag(int render_frame_id) {
  RenderFrameImpl* const frame =
      RenderFrameImpl::FromRoutingID(render_frame_id);
  MediaStreamDispatcher* const dispatcher = frame ?
      frame->GetMediaStreamDispatcher() : NULL;
  if (dispatcher && dispatcher->IsAudioDuckingActive()) {
    return media::AudioParameters::DUCKING;
  }

  return media::AudioParameters::NO_EFFECTS;
}

}  // namespace

int WebRtcAudioRenderer::GetOptimalBufferSize(int sample_rate,
                                              int hardware_buffer_size) {
  // Use native hardware buffer size as default. On Windows, we strive to open
  // up using this native hardware buffer size to achieve best
  // possible performance and to ensure that no FIFO is needed on the browser
  // side to match the client request. That is why there is no #if case for
  // Windows below.
  int frames_per_buffer = hardware_buffer_size;

#if defined(OS_LINUX) || defined(OS_MACOSX)
  // On Linux and MacOS, the low level IO implementations on the browser side
  // supports all buffer size the clients want. We use the native peer
  // connection buffer size (10ms) to achieve best possible performance.
  frames_per_buffer = sample_rate / 100;
#elif defined(OS_ANDROID)
  // TODO(henrika): Keep tuning this scheme and espcicially for low-latency
  // cases. Might not be possible to come up with the perfect solution using
  // the render side only.
  int frames_per_10ms = sample_rate / 100;
  if (frames_per_buffer < 2 * frames_per_10ms) {
    // Examples of low-latency frame sizes and the resulting |buffer_size|:
    //  Nexus 7     : 240 audio frames => 2*480 = 960
    //  Nexus 10    : 256              => 2*441 = 882
    //  Galaxy Nexus: 144              => 2*441 = 882
    frames_per_buffer = 2 * frames_per_10ms;
    DVLOG(1) << "Low-latency output detected on Android";
  }
#endif

  DVLOG(1) << "Using sink output buffer size: " << frames_per_buffer;
  return frames_per_buffer;
}

WebRtcAudioRenderer::WebRtcAudioRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& signaling_thread,
    const scoped_refptr<webrtc::MediaStreamInterface>& media_stream,
    int source_render_frame_id,
    int session_id,
    int sample_rate,
    int frames_per_buffer)
    : state_(UNINITIALIZED),
      source_render_frame_id_(source_render_frame_id),
      session_id_(session_id),
      signaling_thread_(signaling_thread),
      media_stream_(media_stream),
      source_(NULL),
      play_ref_count_(0),
      start_ref_count_(0),
      audio_delay_milliseconds_(0),
      fifo_delay_milliseconds_(0),
      sink_params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                   media::CHANNEL_LAYOUT_STEREO, sample_rate, 16,
                   frames_per_buffer,
                   GetCurrentDuckingFlag(source_render_frame_id)),
      render_callback_count_(0) {
  WebRtcLogMessage(base::StringPrintf(
      "WAR::WAR. source_render_frame_id=%d"
      ", session_id=%d, sample_rate=%d, frames_per_buffer=%d, effects=%i",
      source_render_frame_id, session_id, sample_rate, frames_per_buffer,
      sink_params_.effects()));
}

WebRtcAudioRenderer::~WebRtcAudioRenderer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, UNINITIALIZED);
}

bool WebRtcAudioRenderer::Initialize(WebRtcAudioRendererSource* source) {
  DVLOG(1) << "WebRtcAudioRenderer::Initialize()";
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, UNINITIALIZED);
  DCHECK(source);
  DCHECK(!sink_.get());
  DCHECK(!source_);

  // WebRTC does not yet support higher rates than 96000 on the client side
  // and 48000 is the preferred sample rate. Therefore, if 192000 is detected,
  // we change the rate to 48000 instead. The consequence is that the native
  // layer will be opened up at 192kHz but WebRTC will provide data at 48kHz
  // which will then be resampled by the audio converted on the browser side
  // to match the native audio layer.
  int sample_rate = sink_params_.sample_rate();
  DVLOG(1) << "Audio output hardware sample rate: " << sample_rate;
  if (sample_rate == 192000) {
    DVLOG(1) << "Resampling from 48000 to 192000 is required";
    sample_rate = 48000;
  }
  media::AudioSampleRate asr;
  if (media::ToAudioSampleRate(sample_rate, &asr)) {
    UMA_HISTOGRAM_ENUMERATION(
        "WebRTC.AudioOutputSampleRate", asr, media::kAudioSampleRateMax + 1);
  } else {
    UMA_HISTOGRAM_COUNTS("WebRTC.AudioOutputSampleRateUnexpected",
                         sample_rate);
  }

  // Set up audio parameters for the source, i.e., the WebRTC client.

  // The WebRTC client only supports multiples of 10ms as buffer size where
  // 10ms is preferred for lowest possible delay.
  media::AudioParameters source_params;
  const int frames_per_10ms = (sample_rate / 100);
  DVLOG(1) << "Using WebRTC output buffer size: " << frames_per_10ms;

  source_params.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      sink_params_.channel_layout(), sink_params_.channels(),
                      sample_rate, 16, frames_per_10ms);

  const int frames_per_buffer =
      GetOptimalBufferSize(sample_rate, sink_params_.frames_per_buffer());

  sink_params_.Reset(sink_params_.format(), sink_params_.channel_layout(),
                     sink_params_.channels(), sample_rate, 16,
                     frames_per_buffer);

  // Create a FIFO if re-buffering is required to match the source input with
  // the sink request. The source acts as provider here and the sink as
  // consumer.
  fifo_delay_milliseconds_ = 0;
  if (source_params.frames_per_buffer() != sink_params_.frames_per_buffer()) {
    DVLOG(1) << "Rebuffering from " << source_params.frames_per_buffer()
             << " to " << sink_params_.frames_per_buffer();
    audio_fifo_.reset(new media::AudioPullFifo(
        source_params.channels(),
        source_params.frames_per_buffer(),
        base::Bind(
            &WebRtcAudioRenderer::SourceCallback,
            base::Unretained(this))));

    if (sink_params_.frames_per_buffer() > source_params.frames_per_buffer()) {
      int frame_duration_milliseconds = base::Time::kMillisecondsPerSecond /
          static_cast<double>(source_params.sample_rate());
      fifo_delay_milliseconds_ = (sink_params_.frames_per_buffer() -
        source_params.frames_per_buffer()) * frame_duration_milliseconds;
    }
  }

  source_ = source;

  // Configure the audio rendering client and start rendering.
  sink_ = AudioDeviceFactory::NewOutputDevice(source_render_frame_id_);

  DCHECK_GE(session_id_, 0);
  sink_->InitializeWithSessionId(sink_params_, this, session_id_);

  sink_->Start();

  // User must call Play() before any audio can be heard.
  state_ = PAUSED;

  return true;
}

scoped_refptr<MediaStreamAudioRenderer>
WebRtcAudioRenderer::CreateSharedAudioRendererProxy(
    const scoped_refptr<webrtc::MediaStreamInterface>& media_stream) {
  content::SharedAudioRenderer::OnPlayStateChanged on_play_state_changed =
      base::Bind(&WebRtcAudioRenderer::OnPlayStateChanged, this);
  return new SharedAudioRenderer(this, media_stream, on_play_state_changed);
}

bool WebRtcAudioRenderer::IsStarted() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return start_ref_count_ != 0;
}

void WebRtcAudioRenderer::Start() {
  DVLOG(1) << "WebRtcAudioRenderer::Start()";
  DCHECK(thread_checker_.CalledOnValidThread());
  ++start_ref_count_;
}

void WebRtcAudioRenderer::Play() {
  DVLOG(1) << "WebRtcAudioRenderer::Play()";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (playing_state_.playing())
    return;

  playing_state_.set_playing(true);
  render_callback_count_ = 0;

  OnPlayStateChanged(media_stream_, &playing_state_);
}

void WebRtcAudioRenderer::EnterPlayState() {
  DVLOG(1) << "WebRtcAudioRenderer::EnterPlayState()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(start_ref_count_, 0) << "Did you forget to call Start()?";
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED)
    return;

  DCHECK(play_ref_count_ == 0 || state_ == PLAYING);
  ++play_ref_count_;

  if (state_ != PLAYING) {
    state_ = PLAYING;

    if (audio_fifo_) {
      audio_delay_milliseconds_ = 0;
      audio_fifo_->Clear();
    }
  }
}

void WebRtcAudioRenderer::Pause() {
  DVLOG(1) << "WebRtcAudioRenderer::Pause()";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!playing_state_.playing())
    return;

  playing_state_.set_playing(false);

  OnPlayStateChanged(media_stream_, &playing_state_);
}

void WebRtcAudioRenderer::EnterPauseState() {
  DVLOG(1) << "WebRtcAudioRenderer::EnterPauseState()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(start_ref_count_, 0) << "Did you forget to call Start()?";
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED)
    return;

  DCHECK_EQ(state_, PLAYING);
  DCHECK_GT(play_ref_count_, 0);
  if (!--play_ref_count_)
    state_ = PAUSED;
}

void WebRtcAudioRenderer::Stop() {
  DVLOG(1) << "WebRtcAudioRenderer::Stop()";
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    base::AutoLock auto_lock(lock_);
    if (state_ == UNINITIALIZED)
      return;

    if (--start_ref_count_)
      return;

    DVLOG(1) << "Calling RemoveAudioRenderer and Stop().";

    source_->RemoveAudioRenderer(this);
    source_ = NULL;
    state_ = UNINITIALIZED;
  }

  // Make sure to stop the sink while _not_ holding the lock since the Render()
  // callback may currently be executing and try to grab the lock while we're
  // stopping the thread on which it runs.
  sink_->Stop();
}

void WebRtcAudioRenderer::SetVolume(float volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(volume >= 0.0f && volume <= 1.0f);

  playing_state_.set_volume(volume);
  OnPlayStateChanged(media_stream_, &playing_state_);
}

void WebRtcAudioRenderer::SwitchOutputDevice(
    const std::string& device_id,
    const GURL& security_origin,
    const media::SwitchOutputDeviceCB& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(sink_);
  DVLOG(1) << __FUNCTION__
           << "(" << device_id << ", " << security_origin << ")";
  sink_->SwitchOutputDevice(device_id, security_origin, callback);
}

base::TimeDelta WebRtcAudioRenderer::GetCurrentRenderTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  return current_time_;
}

bool WebRtcAudioRenderer::IsLocalRenderer() const {
  return false;
}

int WebRtcAudioRenderer::Render(media::AudioBus* audio_bus,
                                int audio_delay_milliseconds) {
  base::AutoLock auto_lock(lock_);
  if (!source_)
    return 0;

  DVLOG(2) << "WebRtcAudioRenderer::Render()";
  DVLOG(2) << "audio_delay_milliseconds: " << audio_delay_milliseconds;

  audio_delay_milliseconds_ = audio_delay_milliseconds;

  if (audio_fifo_)
    audio_fifo_->Consume(audio_bus, audio_bus->frames());
  else
    SourceCallback(0, audio_bus);

  return (state_ == PLAYING) ? audio_bus->frames() : 0;
}

void WebRtcAudioRenderer::OnRenderError() {
  NOTIMPLEMENTED();
  LOG(ERROR) << "OnRenderError()";
}

// Called by AudioPullFifo when more data is necessary.
void WebRtcAudioRenderer::SourceCallback(
    int fifo_frame_delay, media::AudioBus* audio_bus) {
  base::TimeTicks start_time = base::TimeTicks::Now() ;
  DVLOG(2) << "WebRtcAudioRenderer::SourceCallback("
           << fifo_frame_delay << ", "
           << audio_bus->frames() << ")";

  int output_delay_milliseconds = audio_delay_milliseconds_;
  output_delay_milliseconds += fifo_delay_milliseconds_;
  DVLOG(2) << "output_delay_milliseconds: " << output_delay_milliseconds;

  // We need to keep render data for the |source_| regardless of |state_|,
  // otherwise the data will be buffered up inside |source_|.
  source_->RenderData(audio_bus, sink_params_.sample_rate(),
                      output_delay_milliseconds,
                      &current_time_);

  // Avoid filling up the audio bus if we are not playing; instead
  // return here and ensure that the returned value in Render() is 0.
  if (state_ != PLAYING)
    audio_bus->Zero();

  if (++render_callback_count_ == kNumCallbacksBetweenRenderTimeHistograms) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
    render_callback_count_ = 0;
    UMA_HISTOGRAM_TIMES("WebRTC.AudioRenderTimes", elapsed);
  }
}

void WebRtcAudioRenderer::UpdateSourceVolume(
    webrtc::AudioSourceInterface* source) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Note: If there are no playing audio renderers, then the volume will be
  // set to 0.0.
  float volume = 0.0f;

  SourcePlayingStates::iterator entry = source_playing_states_.find(source);
  if (entry != source_playing_states_.end()) {
    PlayingStates& states = entry->second;
    for (PlayingStates::const_iterator it = states.begin();
         it != states.end(); ++it) {
      if ((*it)->playing())
        volume += (*it)->volume();
    }
  }

  // The valid range for volume scaling of a remote webrtc source is
  // 0.0-10.0 where 1.0 is no attenuation/boost.
  DCHECK(volume >= 0.0f);
  if (volume > 10.0f)
    volume = 10.0f;

  DVLOG(1) << "Setting remote source volume: " << volume;
  if (!signaling_thread_->BelongsToCurrentThread()) {
    // Libjingle hands out proxy objects in most cases, but the audio source
    // object is an exception (bug?).  So, to work around that, we need to make
    // sure we call SetVolume on the signaling thread.
    signaling_thread_->PostTask(FROM_HERE,
        base::Bind(&webrtc::AudioSourceInterface::SetVolume, source, volume));
  } else {
    source->SetVolume(volume);
  }
}

bool WebRtcAudioRenderer::AddPlayingState(
    webrtc::AudioSourceInterface* source,
    PlayingState* state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(state->playing());
  // Look up or add the |source| to the map.
  PlayingStates& array = source_playing_states_[source];
  if (std::find(array.begin(), array.end(), state) != array.end())
    return false;

  array.push_back(state);

  return true;
}

bool WebRtcAudioRenderer::RemovePlayingState(
    webrtc::AudioSourceInterface* source,
    PlayingState* state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!state->playing());
  SourcePlayingStates::iterator found = source_playing_states_.find(source);
  if (found == source_playing_states_.end())
    return false;

  PlayingStates& array = found->second;
  PlayingStates::iterator state_it =
      std::find(array.begin(), array.end(), state);
  if (state_it == array.end())
    return false;

  array.erase(state_it);

  if (array.empty())
    source_playing_states_.erase(found);

  return true;
}

void WebRtcAudioRenderer::OnPlayStateChanged(
    const scoped_refptr<webrtc::MediaStreamInterface>& media_stream,
    PlayingState* state) {
  webrtc::AudioTrackVector tracks(media_stream->GetAudioTracks());
  for (webrtc::AudioTrackVector::iterator it = tracks.begin();
       it != tracks.end(); ++it) {
    webrtc::AudioSourceInterface* source = (*it)->GetSource();
    DCHECK(source);
    if (!state->playing()) {
      if (RemovePlayingState(source, state))
        EnterPauseState();
    } else if (AddPlayingState(source, state)) {
      EnterPlayState();
    }
    UpdateSourceVolume(source);
  }
}

}  // namespace content
