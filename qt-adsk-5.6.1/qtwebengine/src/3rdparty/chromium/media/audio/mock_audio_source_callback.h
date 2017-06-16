// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MOCK_AUDIO_SOURCE_CALLBACK_H_
#define MEDIA_AUDIO_MOCK_AUDIO_SOURCE_CALLBACK_H_

#include "media/audio/audio_io.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockAudioSourceCallback : public AudioOutputStream::AudioSourceCallback {
 public:
  MockAudioSourceCallback();
  virtual ~MockAudioSourceCallback();

  MOCK_METHOD2(OnMoreData, int(AudioBus* audio_bus,
                               uint32 total_bytes_delay));
  MOCK_METHOD1(OnError, void(AudioOutputStream* stream));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioSourceCallback);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MOCK_AUDIO_SOURCE_CALLBACK_H_
