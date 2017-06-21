// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/AudioHardware.h>
#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/mac/audio_device_listener_mac.h"

namespace media {

class AUAudioInputStream;
class AUHALStream;

// Mac OS X implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class MEDIA_EXPORT AudioManagerMac : public AudioManagerBase {
 public:
  AudioManagerMac(AudioLogFactory* audio_log_factory);

  // Implementation of AudioManager.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) override;

  // Implementation of AudioManagerBase.
  AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) override;
  AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id) override;
  AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id) override;
  AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id) override;
  std::string GetDefaultOutputDeviceID() override;

  // Used to track destruction of input and output streams.
  void ReleaseOutputStream(AudioOutputStream* stream) override;
  void ReleaseInputStream(AudioInputStream* stream) override;

  static bool GetDefaultInputDevice(AudioDeviceID* device);
  static bool GetDefaultOutputDevice(AudioDeviceID* device);
  static bool GetDefaultDevice(AudioDeviceID* device, bool input);

  static bool GetDefaultOutputChannels(int* channels);

  static bool GetDeviceChannels(AudioDeviceID device,
                                AudioObjectPropertyScope scope,
                                int* channels);

  static int HardwareSampleRateForDevice(AudioDeviceID device_id);
  static int HardwareSampleRate();

  // OSX has issues with starting streams as the sytem goes into suspend and
  // immediately after it wakes up from resume.  See http://crbug.com/160920.
  // As a workaround we delay Start() when it occurs after suspend and for a
  // small amount of time after resume.
  //
  // Streams should consult ShouldDeferStreamStart() and if true check the value
  // again after |kStartDelayInSecsForPowerEvents| has elapsed. If false, the
  // stream may be started immediately.
  enum { kStartDelayInSecsForPowerEvents = 1 };
  bool ShouldDeferStreamStart();

  // Changes the buffer size for |device_id| if there are no active input or
  // output streams on the device or |desired_buffer_size| is lower than the
  // current device buffer size.
  //
  // Returns false if an error occurred. There is no indication if the buffer
  // size was changed or not.
  // |element| is 0 for output streams and 1 for input streams.
  // TODO(dalecurtis): we could change the the last parameter to an input/output
  // pointer so it can be updated if the buffer size is not changed.
  // See http://crbug.com/428706 for details.
  bool MaybeChangeBufferSize(AudioDeviceID device_id,
                             AudioUnit audio_unit,
                             AudioUnitElement element,
                             size_t desired_buffer_size);

 protected:
  ~AudioManagerMac() override;

  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  void InitializeOnAudioThread();
  void ShutdownOnAudioThread();

  int ChooseBufferSize(bool is_input, int sample_rate);

  // Notify streams of a device change if the default output device or its
  // sample rate has changed, otherwise does nothing.
  void HandleDeviceChanges();

  scoped_ptr<AudioDeviceListenerMac> output_device_listener_;

  // Track the output sample-rate and the default output device
  // so we can intelligently handle device notifications only when necessary.
  int current_sample_rate_;
  AudioDeviceID current_output_device_;

  // Helper class which monitors power events to determine if output streams
  // should defer Start() calls.  Required to workaround an OSX bug.  See
  // http://crbug.com/160920 for more details.
  class AudioPowerObserver;
  scoped_ptr<AudioPowerObserver> power_observer_;

  // Tracks all constructed input and output streams so they can be stopped at
  // shutdown.  See ShutdownOnAudioThread() for more details.
  std::list<AudioInputStream*> basic_input_streams_;
  std::list<AUAudioInputStream*> low_latency_input_streams_;
  std::list<AUHALStream*> output_streams_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerMac);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
