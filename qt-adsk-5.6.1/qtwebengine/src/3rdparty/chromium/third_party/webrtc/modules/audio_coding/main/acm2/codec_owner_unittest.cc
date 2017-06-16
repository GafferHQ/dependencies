/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/modules/audio_coding/codecs/mock/mock_audio_encoder.h"
#include "webrtc/modules/audio_coding/main/acm2/codec_owner.h"

namespace webrtc {
namespace acm2 {

using ::testing::Return;
using ::testing::InSequence;

namespace {
const int kDataLengthSamples = 80;
const int kPacketSizeSamples = 2 * kDataLengthSamples;
const int16_t kZeroData[kDataLengthSamples] = {0};
const CodecInst kDefaultCodecInst =
    {0, "pcmu", 8000, kPacketSizeSamples, 1, 64000};
const int kCngPt = 13;
}  // namespace

class CodecOwnerTest : public ::testing::Test {
 protected:
  CodecOwnerTest() : timestamp_(0) {}

  void CreateCodec() {
    codec_owner_.SetEncoders(kDefaultCodecInst, kCngPt, VADNormal, -1);
  }

  void EncodeAndVerify(size_t expected_out_length,
                       uint32_t expected_timestamp,
                       int expected_payload_type,
                       int expected_send_even_if_empty) {
    uint8_t out[kPacketSizeSamples];
    AudioEncoder::EncodedInfo encoded_info;
    encoded_info = codec_owner_.Encoder()->Encode(
        timestamp_, kZeroData, kDataLengthSamples, kPacketSizeSamples, out);
    timestamp_ += kDataLengthSamples;
    EXPECT_TRUE(encoded_info.redundant.empty());
    EXPECT_EQ(expected_out_length, encoded_info.encoded_bytes);
    EXPECT_EQ(expected_timestamp, encoded_info.encoded_timestamp);
    if (expected_payload_type >= 0)
      EXPECT_EQ(expected_payload_type, encoded_info.payload_type);
    if (expected_send_even_if_empty >= 0)
      EXPECT_EQ(static_cast<bool>(expected_send_even_if_empty),
                encoded_info.send_even_if_empty);
  }

  CodecOwner codec_owner_;
  uint32_t timestamp_;
};

// This test verifies that CNG frames are delivered as expected. Since the frame
// size is set to 20 ms, we expect the first encode call to produce no output
// (which is signaled as 0 bytes output of type kNoEncoding). The next encode
// call should produce one SID frame of 9 bytes. The third call should not
// result in any output (just like the first one). The fourth and final encode
// call should produce an "empty frame", which is like no output, but with
// AudioEncoder::EncodedInfo::send_even_if_empty set to true. (The reason to
// produce an empty frame is to drive sending of DTMF packets in the RTP/RTCP
// module.)
TEST_F(CodecOwnerTest, VerifyCngFrames) {
  CreateCodec();
  uint32_t expected_timestamp = timestamp_;
  // Verify no frame.
  {
    SCOPED_TRACE("First encoding");
    EncodeAndVerify(0, expected_timestamp, -1, -1);
  }

  // Verify SID frame delivered.
  {
    SCOPED_TRACE("Second encoding");
    EncodeAndVerify(9, expected_timestamp, kCngPt, 1);
  }

  // Verify no frame.
  {
    SCOPED_TRACE("Third encoding");
    EncodeAndVerify(0, expected_timestamp, -1, -1);
  }

  // Verify NoEncoding.
  expected_timestamp += 2 * kDataLengthSamples;
  {
    SCOPED_TRACE("Fourth encoding");
    EncodeAndVerify(0, expected_timestamp, kCngPt, 1);
  }
}

TEST_F(CodecOwnerTest, ExternalEncoder) {
  MockAudioEncoderMutable external_encoder;
  codec_owner_.SetEncoders(&external_encoder, -1, VADNormal, -1);
  const int kSampleRateHz = 8000;
  const int kPacketSizeSamples = kSampleRateHz / 100;
  int16_t audio[kPacketSizeSamples] = {0};
  uint8_t encoded[kPacketSizeSamples];
  AudioEncoder::EncodedInfo info;
  EXPECT_CALL(external_encoder, SampleRateHz())
      .WillRepeatedly(Return(kSampleRateHz));

  {
    InSequence s;
    info.encoded_timestamp = 0;
    EXPECT_CALL(external_encoder,
                EncodeInternal(0, audio, arraysize(encoded), encoded))
        .WillOnce(Return(info));
    EXPECT_CALL(external_encoder, Reset());
    EXPECT_CALL(external_encoder, Reset());
    info.encoded_timestamp = 2;
    EXPECT_CALL(external_encoder,
                EncodeInternal(2, audio, arraysize(encoded), encoded))
        .WillOnce(Return(info));
    EXPECT_CALL(external_encoder, Reset());
  }

  info = codec_owner_.Encoder()->Encode(0, audio, arraysize(audio),
                                        arraysize(encoded), encoded);
  EXPECT_EQ(0u, info.encoded_timestamp);
  external_encoder.Reset();  // Dummy call to mark the sequence of expectations.

  // Change to internal encoder.
  CodecInst codec_inst = kDefaultCodecInst;
  codec_inst.pacsize = kPacketSizeSamples;
  codec_owner_.SetEncoders(codec_inst, -1, VADNormal, -1);
  // Don't expect any more calls to the external encoder.
  info = codec_owner_.Encoder()->Encode(1, audio, arraysize(audio),
                                        arraysize(encoded), encoded);
  external_encoder.Reset();  // Dummy call to mark the sequence of expectations.

  // Change back to external encoder again.
  codec_owner_.SetEncoders(&external_encoder, -1, VADNormal, -1);
  info = codec_owner_.Encoder()->Encode(2, audio, arraysize(audio),
                                        arraysize(encoded), encoded);
  EXPECT_EQ(2u, info.encoded_timestamp);
  external_encoder.Reset();  // Dummy call to mark the sequence of expectations.
}

}  // namespace acm2
}  // namespace webrtc
