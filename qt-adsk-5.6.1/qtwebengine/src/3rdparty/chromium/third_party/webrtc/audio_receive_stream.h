/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_RECEIVE_STREAM_H_
#define WEBRTC_AUDIO_RECEIVE_STREAM_H_

#include <map>
#include <string>
#include <vector>

#include "webrtc/config.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class AudioDecoder;

class AudioReceiveStream {
 public:
  struct Stats {};

  struct Config {
    std::string ToString() const;

    // Receive-stream specific RTP settings.
    struct Rtp {
      std::string ToString() const;

      // Synchronization source (stream identifier) to be received.
      uint32_t remote_ssrc = 0;

      // Sender SSRC used for sending RTCP (such as receiver reports).
      uint32_t local_ssrc = 0;

      // RTP header extensions used for the received stream.
      std::vector<RtpExtension> extensions;
    } rtp;

    // Decoders for every payload that we can receive. Call owns the
    // AudioDecoder instances once the Config is submitted to
    // Call::CreateReceiveStream().
    // TODO(solenberg): Use unique_ptr<> once our std lib fully supports C++11.
    std::map<uint8_t, AudioDecoder*> decoder_map;
  };

  virtual Stats GetStats() const = 0;

 protected:
  virtual ~AudioReceiveStream() {}
};
}  // namespace webrtc

#endif  // WEBRTC_AUDIO_RECEIVE_STREAM_H_
