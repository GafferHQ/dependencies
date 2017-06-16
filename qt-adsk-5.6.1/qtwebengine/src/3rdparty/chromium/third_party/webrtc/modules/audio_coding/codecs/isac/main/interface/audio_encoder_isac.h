/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_INTERFACE_AUDIO_ENCODER_ISAC_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_INTERFACE_AUDIO_ENCODER_ISAC_H_

#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_coding/codecs/audio_encoder_mutable_impl.h"
#include "webrtc/modules/audio_coding/codecs/isac/audio_encoder_isac_t.h"
#include "webrtc/modules/audio_coding/codecs/isac/main/interface/isac.h"

namespace webrtc {

struct IsacFloat {
  typedef ISACStruct instance_type;
  static const bool has_swb = true;
  static inline int16_t Control(instance_type* inst,
                                int32_t rate,
                                int framesize) {
    return WebRtcIsac_Control(inst, rate, framesize);
  }
  static inline int16_t ControlBwe(instance_type* inst,
                                   int32_t rate_bps,
                                   int frame_size_ms,
                                   int16_t enforce_frame_size) {
    return WebRtcIsac_ControlBwe(inst, rate_bps, frame_size_ms,
                                 enforce_frame_size);
  }
  static inline int16_t Create(instance_type** inst) {
    return WebRtcIsac_Create(inst);
  }
  static inline int DecodeInternal(instance_type* inst,
                                   const uint8_t* encoded,
                                   int16_t len,
                                   int16_t* decoded,
                                   int16_t* speech_type) {
    return WebRtcIsac_Decode(inst, encoded, len, decoded, speech_type);
  }
  static inline int16_t DecodePlc(instance_type* inst,
                                  int16_t* decoded,
                                  int16_t num_lost_frames) {
    return WebRtcIsac_DecodePlc(inst, decoded, num_lost_frames);
  }

  static inline int16_t DecoderInit(instance_type* inst) {
    return WebRtcIsac_DecoderInit(inst);
  }
  static inline int Encode(instance_type* inst,
                           const int16_t* speech_in,
                           uint8_t* encoded) {
    return WebRtcIsac_Encode(inst, speech_in, encoded);
  }
  static inline int16_t EncoderInit(instance_type* inst, int16_t coding_mode) {
    return WebRtcIsac_EncoderInit(inst, coding_mode);
  }
  static inline uint16_t EncSampRate(instance_type* inst) {
    return WebRtcIsac_EncSampRate(inst);
  }

  static inline int16_t Free(instance_type* inst) {
    return WebRtcIsac_Free(inst);
  }
  static inline void GetBandwidthInfo(instance_type* inst,
                                      IsacBandwidthInfo* bwinfo) {
    WebRtcIsac_GetBandwidthInfo(inst, bwinfo);
  }
  static inline int16_t GetErrorCode(instance_type* inst) {
    return WebRtcIsac_GetErrorCode(inst);
  }

  static inline int16_t GetNewFrameLen(instance_type* inst) {
    return WebRtcIsac_GetNewFrameLen(inst);
  }
  static inline void SetBandwidthInfo(instance_type* inst,
                                      const IsacBandwidthInfo* bwinfo) {
    WebRtcIsac_SetBandwidthInfo(inst, bwinfo);
  }
  static inline int16_t SetDecSampRate(instance_type* inst,
                                       uint16_t sample_rate_hz) {
    return WebRtcIsac_SetDecSampRate(inst, sample_rate_hz);
  }
  static inline int16_t SetEncSampRate(instance_type* inst,
                                       uint16_t sample_rate_hz) {
    return WebRtcIsac_SetEncSampRate(inst, sample_rate_hz);
  }
  static inline int16_t UpdateBwEstimate(instance_type* inst,
                                         const uint8_t* encoded,
                                         int32_t packet_size,
                                         uint16_t rtp_seq_number,
                                         uint32_t send_ts,
                                         uint32_t arr_ts) {
    return WebRtcIsac_UpdateBwEstimate(inst, encoded, packet_size,
                                       rtp_seq_number, send_ts, arr_ts);
  }
  static inline int16_t SetMaxPayloadSize(instance_type* inst,
                                          int16_t max_payload_size_bytes) {
    return WebRtcIsac_SetMaxPayloadSize(inst, max_payload_size_bytes);
  }
  static inline int16_t SetMaxRate(instance_type* inst, int32_t max_bit_rate) {
    return WebRtcIsac_SetMaxRate(inst, max_bit_rate);
  }
};

typedef AudioEncoderDecoderIsacT<IsacFloat> AudioEncoderDecoderIsac;

struct CodecInst;

class AudioEncoderDecoderMutableIsacFloat
    : public AudioEncoderMutableImpl<AudioEncoderDecoderIsac,
                                     AudioEncoderDecoderMutableIsac> {
 public:
  explicit AudioEncoderDecoderMutableIsacFloat(const CodecInst& codec_inst);
  void UpdateSettings(const CodecInst& codec_inst) override;
  void SetMaxPayloadSize(int max_payload_size_bytes) override;
  void SetMaxRate(int max_rate_bps) override;

  // From AudioDecoder.
  int Decode(const uint8_t* encoded,
             size_t encoded_len,
             int sample_rate_hz,
             size_t max_decoded_bytes,
             int16_t* decoded,
             SpeechType* speech_type) override;
  int DecodeRedundant(const uint8_t* encoded,
                      size_t encoded_len,
                      int sample_rate_hz,
                      size_t max_decoded_bytes,
                      int16_t* decoded,
                      SpeechType* speech_type) override;
  bool HasDecodePlc() const override;
  int DecodePlc(int num_frames, int16_t* decoded) override;
  int Init() override;
  int IncomingPacket(const uint8_t* payload,
                     size_t payload_len,
                     uint16_t rtp_sequence_number,
                     uint32_t rtp_timestamp,
                     uint32_t arrival_timestamp) override;
  int ErrorCode() override;
  int PacketDuration(const uint8_t* encoded, size_t encoded_len) const override;
  int PacketDurationRedundant(const uint8_t* encoded,
                              size_t encoded_len) const override;
  bool PacketHasFec(const uint8_t* encoded, size_t encoded_len) const override;
  size_t Channels() const override;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_INTERFACE_AUDIO_ENCODER_ISAC_H_
