// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_H_
#define MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_H_

#include <cras_types.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_manager_base.h"

namespace media {

class MEDIA_EXPORT AudioManagerCras : public AudioManagerBase {
 public:
  AudioManagerCras(AudioLogFactory* audio_log_factory);

  // AudioManager implementation.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void ShowAudioInputSettings() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  void SetHasKeyboardMic() override;

  // AudioManagerBase implementation.
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

  static snd_pcm_format_t BitsToFormat(int bits_per_sample);

 protected:
  ~AudioManagerCras() override;

  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  // Called by MakeLinearOutputStream and MakeLowLatencyOutputStream.
  AudioOutputStream* MakeOutputStream(const AudioParameters& params);

  // Called by MakeLinearInputStream and MakeLowLatencyInputStream.
  AudioInputStream* MakeInputStream(const AudioParameters& params,
                                    const std::string& device_id);

  bool has_keyboard_mic_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerCras);
};

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_H_
