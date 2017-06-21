// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/clockless_audio_sink.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/simple_thread.h"

namespace media {

// Internal to ClocklessAudioSink. Class is used to call Render() on a seperate
// thread, running as fast as it can read the data.
class ClocklessAudioSinkThread : public base::DelegateSimpleThread::Delegate {
 public:
  explicit ClocklessAudioSinkThread(const AudioParameters& params,
                                    AudioRendererSink::RenderCallback* callback)
      : callback_(callback),
        audio_bus_(AudioBus::Create(params)),
        stop_event_(new base::WaitableEvent(false, false)) {}

  void Start() {
    stop_event_->Reset();
    thread_.reset(new base::DelegateSimpleThread(this, "ClocklessAudioSink"));
    thread_->Start();
  }

  // Generate a signal to stop calling Render().
  base::TimeDelta Stop() {
    stop_event_->Signal();
    thread_->Join();
    return playback_time_;
  }

 private:
   // Call Render() repeatedly, keeping track of the rendering time.
  void Run() override {
     base::TimeTicks start;
     while (!stop_event_->IsSignaled()) {
       int frames_received = callback_->Render(audio_bus_.get(), 0);
       if (frames_received <= 0) {
         // No data received, so let other threads run to provide data.
         base::PlatformThread::YieldCurrentThread();
       } else if (start.is_null()) {
         // First time we processed some audio, so record the starting time.
         start = base::TimeTicks::Now();
       } else {
         // Keep track of the last time data was rendered.
         playback_time_ = base::TimeTicks::Now() - start;
       }
     }
   }

  AudioRendererSink::RenderCallback* callback_;
  scoped_ptr<AudioBus> audio_bus_;
  scoped_ptr<base::WaitableEvent> stop_event_;
  scoped_ptr<base::DelegateSimpleThread> thread_;
  base::TimeDelta playback_time_;
};

ClocklessAudioSink::ClocklessAudioSink()
    : initialized_(false),
      playing_(false) {}

ClocklessAudioSink::~ClocklessAudioSink() {}

void ClocklessAudioSink::Initialize(const AudioParameters& params,
                                    RenderCallback* callback) {
  DCHECK(!initialized_);
  thread_.reset(new ClocklessAudioSinkThread(params, callback));
  initialized_ = true;
}

void ClocklessAudioSink::Start() {
  DCHECK(initialized_);
  DCHECK(!playing_);
}

void ClocklessAudioSink::Stop() {
  if (initialized_)
    Pause();
}

void ClocklessAudioSink::Play() {
  DCHECK(initialized_);

  if (playing_)
    return;

  playing_ = true;
  thread_->Start();
}

void ClocklessAudioSink::Pause() {
  DCHECK(initialized_);

  if (!playing_)
    return;

  playing_ = false;
  playback_time_ = thread_->Stop();
}

bool ClocklessAudioSink::SetVolume(double volume) {
  // Audio is always muted.
  return volume == 0.0;
}

void ClocklessAudioSink::SwitchOutputDevice(
    const std::string& device_id,
    const GURL& security_origin,
    const SwitchOutputDeviceCB& callback) {
  callback.Run(SWITCH_OUTPUT_DEVICE_RESULT_ERROR_NOT_SUPPORTED);
}

}  // namespace media
