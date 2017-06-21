// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_device_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/win/windows_version.h"
#include "content/renderer/media/media_stream_audio_processor.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/sample_rates.h"

using media::AudioParameters;
using media::ChannelLayout;

namespace content {

WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()
    : ref_count_(0),
      audio_transport_callback_(NULL),
      output_delay_ms_(0),
      initialized_(false),
      playing_(false),
      recording_(false),
      microphone_volume_(0) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()";
  // This object can be constructed on either the signaling thread or the main
  // thread, so we need to detach these thread checkers here and have them
  // initialize automatically when the first methods are called.
  signaling_thread_checker_.DetachFromThread();
  main_thread_checker_.DetachFromThread();

  worker_thread_checker_.DetachFromThread();
}

WebRtcAudioDeviceImpl::~WebRtcAudioDeviceImpl() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::~WebRtcAudioDeviceImpl()";
  DCHECK(main_thread_checker_.CalledOnValidThread());
  Terminate();
}

int32_t WebRtcAudioDeviceImpl::AddRef() {
  // We can be AddRefed and released on both the UI thread as well as
  // libjingle's signaling thread.
  return base::subtle::Barrier_AtomicIncrement(&ref_count_, 1);
}

int32_t WebRtcAudioDeviceImpl::Release() {
  // We can be AddRefed and released on both the UI thread as well as
  // libjingle's signaling thread.
  int ret = base::subtle::Barrier_AtomicIncrement(&ref_count_, -1);
  if (ret == 0) {
    delete this;
  }
  return ret;
}

void WebRtcAudioDeviceImpl::RenderData(media::AudioBus* audio_bus,
                                       int sample_rate,
                                       int audio_delay_milliseconds,
                                       base::TimeDelta* current_time) {
  {
    base::AutoLock auto_lock(lock_);
    if (!playing_) {
      // Force silence to AudioBus after stopping playout in case
      // there is lingering audio data in AudioBus.
      audio_bus->Zero();
      return;
    }
    DCHECK(audio_transport_callback_);
    // Store the reported audio delay locally.
    output_delay_ms_ = audio_delay_milliseconds;
  }

  render_buffer_.resize(audio_bus->frames() * audio_bus->channels());
  int frames_per_10_ms = (sample_rate / 100);
  int bytes_per_sample = sizeof(render_buffer_[0]);
  const int bytes_per_10_ms =
      audio_bus->channels() * frames_per_10_ms * bytes_per_sample;
  DCHECK_EQ(audio_bus->frames() % frames_per_10_ms, 0);

  // Get audio frames in blocks of 10 milliseconds from the registered
  // webrtc::AudioTransport source. Keep reading until our internal buffer
  // is full.
  int accumulated_audio_frames = 0;
  int16* audio_data = &render_buffer_[0];
  while (accumulated_audio_frames < audio_bus->frames()) {
    // Get 10ms and append output to temporary byte buffer.
    int64_t elapsed_time_ms = -1;
    int64_t ntp_time_ms = -1;
    static const int kBitsPerByte = 8;
    audio_transport_callback_->PullRenderData(bytes_per_sample * kBitsPerByte,
                                              sample_rate,
                                              audio_bus->channels(),
                                              frames_per_10_ms,
                                              audio_data,
                                              &elapsed_time_ms,
                                              &ntp_time_ms);
    accumulated_audio_frames += frames_per_10_ms;
    if (elapsed_time_ms >= 0) {
      *current_time = base::TimeDelta::FromMilliseconds(elapsed_time_ms);
    }
    audio_data += bytes_per_10_ms;
  }

  // De-interleave each channel and convert to 32-bit floating-point
  // with nominal range -1.0 -> +1.0 to match the callback format.
  audio_bus->FromInterleaved(&render_buffer_[0],
                             audio_bus->frames(),
                             bytes_per_sample);

  // Pass the render data to the playout sinks.
  base::AutoLock auto_lock(lock_);
  for (PlayoutDataSinkList::const_iterator it = playout_sinks_.begin();
       it != playout_sinks_.end(); ++it) {
    (*it)->OnPlayoutData(audio_bus, sample_rate, audio_delay_milliseconds);
  }
}

void WebRtcAudioDeviceImpl::RemoveAudioRenderer(WebRtcAudioRenderer* renderer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(renderer, renderer_.get());
  // Notify the playout sink of the change.
  for (PlayoutDataSinkList::const_iterator it = playout_sinks_.begin();
       it != playout_sinks_.end(); ++it) {
    (*it)->OnPlayoutDataSourceChanged();
  }

  renderer_ = NULL;
}

int32_t WebRtcAudioDeviceImpl::RegisterAudioCallback(
    webrtc::AudioTransport* audio_callback) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::RegisterAudioCallback()";
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  DCHECK_EQ(audio_transport_callback_ == NULL, audio_callback != NULL);
  audio_transport_callback_ = audio_callback;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::Init() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::Init()";
  DCHECK(signaling_thread_checker_.CalledOnValidThread());

  // We need to return a success to continue the initialization of WebRtc VoE
  // because failure on the capturer_ initialization should not prevent WebRTC
  // from working. See issue http://crbug.com/144421 for details.
  initialized_ = true;

  return 0;
}

int32_t WebRtcAudioDeviceImpl::Terminate() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::Terminate()";
  DCHECK(signaling_thread_checker_.CalledOnValidThread());

  // Calling Terminate() multiple times in a row is OK.
  if (!initialized_)
    return 0;

  StopRecording();
  StopPlayout();

  DCHECK(!renderer_.get() || !renderer_->IsStarted())
      << "The shared audio renderer shouldn't be running";

  // Stop all the capturers to ensure no further OnData() and
  // RemoveAudioCapturer() callback.
  // Cache the capturers in a local list since WebRtcAudioCapturer::Stop()
  // will trigger RemoveAudioCapturer() callback.
  CapturerList capturers;
  capturers.swap(capturers_);
  for (CapturerList::const_iterator iter = capturers.begin();
       iter != capturers.end(); ++iter) {
    (*iter)->Stop();
  }

  initialized_ = false;
  return 0;
}

bool WebRtcAudioDeviceImpl::Initialized() const {
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
  return initialized_;
}

int32_t WebRtcAudioDeviceImpl::PlayoutIsAvailable(bool* available) {
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
  *available = initialized_;
  return 0;
}

bool WebRtcAudioDeviceImpl::PlayoutIsInitialized() const {
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
  return initialized_;
}

int32_t WebRtcAudioDeviceImpl::RecordingIsAvailable(bool* available) {
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  *available = (!capturers_.empty());
  return 0;
}

bool WebRtcAudioDeviceImpl::RecordingIsInitialized() const {
  DVLOG(1) << "WebRtcAudioDeviceImpl::RecordingIsInitialized()";
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  return (!capturers_.empty());
}

int32_t WebRtcAudioDeviceImpl::StartPlayout() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StartPlayout()";
  DCHECK(worker_thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  if (!audio_transport_callback_) {
    LOG(ERROR) << "Audio transport is missing";
    return 0;
  }

  // webrtc::VoiceEngine assumes that it is OK to call Start() twice and
  // that the call is ignored the second time.
  playing_ = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopPlayout() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StopPlayout()";
  DCHECK(initialized_);
  // Can be called both from the worker thread (e.g. when called from webrtc)
  // or the signaling thread (e.g. when we call it ourselves internally).
  // The order in this check is important so that we won't incorrectly
  // initialize worker_thread_checker_ on the signaling thread.
  DCHECK(signaling_thread_checker_.CalledOnValidThread() ||
         worker_thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  // webrtc::VoiceEngine assumes that it is OK to call Stop() multiple times.
  playing_ = false;
  return 0;
}

bool WebRtcAudioDeviceImpl::Playing() const {
  DCHECK(worker_thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  return playing_;
}

int32_t WebRtcAudioDeviceImpl::StartRecording() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StartRecording()";
  DCHECK(worker_thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  base::AutoLock auto_lock(lock_);
  if (!audio_transport_callback_) {
    LOG(ERROR) << "Audio transport is missing";
    return -1;
  }

  recording_ = true;

  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopRecording() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StopRecording()";
  DCHECK(initialized_);
  // Can be called both from the worker thread (e.g. when called from webrtc)
  // or the signaling thread (e.g. when we call it ourselves internally).
  // The order in this check is important so that we won't incorrectly
  // initialize worker_thread_checker_ on the signaling thread.
  DCHECK(signaling_thread_checker_.CalledOnValidThread() ||
         worker_thread_checker_.CalledOnValidThread());

  base::AutoLock auto_lock(lock_);
  recording_ = false;
  return 0;
}

bool WebRtcAudioDeviceImpl::Recording() const {
  DCHECK(worker_thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  return recording_;
}

int32_t WebRtcAudioDeviceImpl::SetMicrophoneVolume(uint32_t volume) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::SetMicrophoneVolume(" << volume << ")";
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);

  // Only one microphone is supported at the moment, which is represented by
  // the default capturer.
  scoped_refptr<WebRtcAudioCapturer> capturer(GetDefaultCapturer());
  if (!capturer.get())
    return -1;

  capturer->SetVolume(volume);
  return 0;
}

// TODO(henrika): sort out calling thread once we start using this API.
int32_t WebRtcAudioDeviceImpl::MicrophoneVolume(uint32_t* volume) const {
  DVLOG(1) << "WebRtcAudioDeviceImpl::MicrophoneVolume()";
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
  // We only support one microphone now, which is accessed via the default
  // capturer.
  DCHECK(initialized_);
  scoped_refptr<WebRtcAudioCapturer> capturer(GetDefaultCapturer());
  if (!capturer.get())
    return -1;

  *volume = static_cast<uint32_t>(capturer->Volume());

  return 0;
}

int32_t WebRtcAudioDeviceImpl::MaxMicrophoneVolume(uint32_t* max_volume) const {
  DCHECK(initialized_);
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
  *max_volume = kMaxVolumeLevel;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::MinMicrophoneVolume(uint32_t* min_volume) const {
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
  *min_volume = 0;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StereoPlayoutIsAvailable(bool* available) const {
  DCHECK(initialized_);
  // This method is called during initialization on the signaling thread and
  // then later on the worker thread.  Due to this we cannot DCHECK on what
  // thread we're on since it might incorrectly initialize the
  // worker_thread_checker_.
  base::AutoLock auto_lock(lock_);
  *available = renderer_.get() && renderer_->channels() == 2;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StereoRecordingIsAvailable(
    bool* available) const {
  DCHECK(initialized_);
  // This method is called during initialization on the signaling thread and
  // then later on the worker thread.  Due to this we cannot DCHECK on what
  // thread we're on since it might incorrectly initialize the
  // worker_thread_checker_.

  // TODO(xians): These kind of hardware methods do not make much sense since we
  // support multiple sources. Remove or figure out new APIs for such methods.
  scoped_refptr<WebRtcAudioCapturer> capturer(GetDefaultCapturer());
  if (!capturer.get())
    return -1;

  *available = (capturer->source_audio_parameters().channels() == 2);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::PlayoutDelay(uint16_t* delay_ms) const {
  DCHECK(worker_thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  *delay_ms = static_cast<uint16_t>(output_delay_ms_);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::RecordingDelay(uint16_t* delay_ms) const {
  DCHECK(signaling_thread_checker_.CalledOnValidThread());

  // There is no way to report a correct delay value to WebRTC since there
  // might be multiple WebRtcAudioCapturer instances.
  NOTREACHED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::RecordingSampleRate(
    uint32_t* sample_rate) const {
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
  // We use the default capturer as the recording sample rate.
  scoped_refptr<WebRtcAudioCapturer> capturer(GetDefaultCapturer());
  if (!capturer.get())
    return -1;

  *sample_rate = static_cast<uint32_t>(
      capturer->source_audio_parameters().sample_rate());
  return 0;
}

int32_t WebRtcAudioDeviceImpl::PlayoutSampleRate(
    uint32_t* sample_rate) const {
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
  *sample_rate = renderer_.get() ? renderer_->sample_rate() : 0;
  return 0;
}

bool WebRtcAudioDeviceImpl::SetAudioRenderer(WebRtcAudioRenderer* renderer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(renderer);

  // Here we acquire |lock_| in order to protect the internal state.
  {
    base::AutoLock auto_lock(lock_);
    if (renderer_.get())
      return false;
  }

  // We release |lock_| here because invoking |renderer|->Initialize while
  // holding |lock_| would result in locks taken in the sequence
  // (|this->lock_|,  |renderer->lock_|) while another thread (i.e, the
  // AudioOutputDevice thread) might concurrently invoke a renderer method,
  // which can itself invoke a method from |this|, resulting in locks taken in
  // the sequence (|renderer->lock_|, |this->lock_|) in that thread.
  // This order discrepancy can cause a deadlock (see Issue 433993).
  // However, we do not need to hold |this->lock_| in order to invoke
  // |renderer|->Initialize, since it does not involve any unprotected access to
  // the internal state of |this|.
  if (!renderer->Initialize(this))
    return false;

  // We acquire |lock_| again and assert our precondition, since we are
  // accessing the internal state again.
  base::AutoLock auto_lock(lock_);
  DCHECK(!renderer_.get());
  renderer_ = renderer;
  return true;
}

void WebRtcAudioDeviceImpl::AddAudioCapturer(
    const scoped_refptr<WebRtcAudioCapturer>& capturer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioDeviceImpl::AddAudioCapturer()";
  DCHECK(capturer.get());
  DCHECK(!capturer->device_id().empty());

  base::AutoLock auto_lock(lock_);
  DCHECK(std::find(capturers_.begin(), capturers_.end(), capturer) ==
      capturers_.end());
  capturers_.push_back(capturer);
}

void WebRtcAudioDeviceImpl::RemoveAudioCapturer(
    const scoped_refptr<WebRtcAudioCapturer>& capturer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioDeviceImpl::AddAudioCapturer()";
  DCHECK(capturer.get());
  base::AutoLock auto_lock(lock_);
  capturers_.remove(capturer);
}

scoped_refptr<WebRtcAudioCapturer>
WebRtcAudioDeviceImpl::GetDefaultCapturer() const {
  // Called on the signaling thread (during initialization), worker
  // thread during capture or main thread for a WebAudio source.
  // We can't DCHECK on those three checks here since GetDefaultCapturer
  // may be the first call and therefore could incorrectly initialize the
  // thread checkers.
  DCHECK(initialized_);
  base::AutoLock auto_lock(lock_);
  // Use the last |capturer| which is from the latest getUserMedia call as
  // the default capture device.
  return capturers_.empty() ? NULL : capturers_.back();
}

void WebRtcAudioDeviceImpl::AddPlayoutSink(
    WebRtcPlayoutDataSource::Sink* sink) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(sink);
  base::AutoLock auto_lock(lock_);
  DCHECK(std::find(playout_sinks_.begin(), playout_sinks_.end(), sink) ==
      playout_sinks_.end());
  playout_sinks_.push_back(sink);
}

void WebRtcAudioDeviceImpl::RemovePlayoutSink(
    WebRtcPlayoutDataSource::Sink* sink) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(sink);
  base::AutoLock auto_lock(lock_);
  playout_sinks_.remove(sink);
}

bool WebRtcAudioDeviceImpl::GetAuthorizedDeviceInfoForAudioRenderer(
    int* session_id,
    int* output_sample_rate,
    int* output_frames_per_buffer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  // If there is no capturer or there are more than one open capture devices,
  // return false.
  if (capturers_.size() != 1)
    return false;

  return capturers_.back()->GetPairedOutputParameters(
      session_id, output_sample_rate, output_frames_per_buffer);
}

}  // namespace content
