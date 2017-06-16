// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/mp4/es_descriptor.h"
#include "media/formats/mp4/mp4_stream_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace media {
namespace mp4 {

class MP4StreamParserTest : public testing::Test {
 public:
  MP4StreamParserTest()
      : configs_received_(false),
        lower_bound_(
            DecodeTimestamp::FromPresentationTime(base::TimeDelta::Max())) {
    std::set<int> audio_object_types;
    audio_object_types.insert(kISO_14496_3);
    parser_.reset(new MP4StreamParser(audio_object_types, false));
  }

 protected:
  scoped_ptr<MP4StreamParser> parser_;
  bool configs_received_;
  AudioDecoderConfig audio_decoder_config_;
  VideoDecoderConfig video_decoder_config_;
  DecodeTimestamp lower_bound_;

  bool AppendData(const uint8* data, size_t length) {
    return parser_->Parse(data, length);
  }

  bool AppendDataInPieces(const uint8* data, size_t length, size_t piece_size) {
    const uint8* start = data;
    const uint8* end = data + length;
    while (start < end) {
      size_t append_size = std::min(piece_size,
                                    static_cast<size_t>(end - start));
      if (!AppendData(start, append_size))
        return false;
      start += append_size;
    }
    return true;
  }

  void InitF(DemuxerStream::Liveness expected_liveness,
             const StreamParser::InitParameters& params) {
    DVLOG(1) << "InitF: dur=" << params.duration.InMilliseconds()
             << ", autoTimestampOffset=" << params.auto_update_timestamp_offset;
    EXPECT_EQ(expected_liveness, params.liveness);
  }

  bool NewConfigF(const AudioDecoderConfig& ac,
                  const VideoDecoderConfig& vc,
                  const StreamParser::TextTrackConfigMap& tc) {
    DVLOG(1) << "NewConfigF: audio=" << ac.IsValidConfig()
             << ", video=" << vc.IsValidConfig();
    configs_received_ = true;
    audio_decoder_config_ = ac;
    video_decoder_config_ = vc;
    return true;
  }

  void DumpBuffers(const std::string& label,
                   const StreamParser::BufferQueue& buffers) {
    DVLOG(2) << "DumpBuffers: " << label << " size " << buffers.size();
    for (StreamParser::BufferQueue::const_iterator buf = buffers.begin();
         buf != buffers.end(); buf++) {
      DVLOG(3) << "  n=" << buf - buffers.begin()
               << ", size=" << (*buf)->data_size()
               << ", dur=" << (*buf)->duration().InMilliseconds();
    }
  }

  bool NewBuffersF(const StreamParser::BufferQueue& audio_buffers,
                   const StreamParser::BufferQueue& video_buffers,
                   const StreamParser::TextBufferQueueMap& text_map) {
    DumpBuffers("audio_buffers", audio_buffers);
    DumpBuffers("video_buffers", video_buffers);

    // TODO(wolenetz/acolwell): Add text track support to more MSE parsers. See
    // http://crbug.com/336926.
    if (!text_map.empty())
      return false;

    // Find the second highest timestamp so that we know what the
    // timestamps on the next set of buffers must be >= than.
    DecodeTimestamp audio = !audio_buffers.empty() ?
        audio_buffers.back()->GetDecodeTimestamp() : kNoDecodeTimestamp();
    DecodeTimestamp video = !video_buffers.empty() ?
        video_buffers.back()->GetDecodeTimestamp() : kNoDecodeTimestamp();
    DecodeTimestamp second_highest_timestamp =
        (audio == kNoDecodeTimestamp() ||
         (video != kNoDecodeTimestamp() && audio > video)) ? video : audio;

    DCHECK(second_highest_timestamp != kNoDecodeTimestamp());

    if (lower_bound_ != kNoDecodeTimestamp() &&
        second_highest_timestamp < lower_bound_) {
      return false;
    }

    lower_bound_ = second_highest_timestamp;
    return true;
  }

  void KeyNeededF(EmeInitDataType type, const std::vector<uint8>& init_data) {
    DVLOG(1) << "KeyNeededF: " << init_data.size();
    EXPECT_EQ(EmeInitDataType::CENC, type);
    EXPECT_FALSE(init_data.empty());
  }

  void NewSegmentF() {
    DVLOG(1) << "NewSegmentF";
    lower_bound_ = kNoDecodeTimestamp();
  }

  void EndOfSegmentF() {
    DVLOG(1) << "EndOfSegmentF()";
    lower_bound_ =
        DecodeTimestamp::FromPresentationTime(base::TimeDelta::Max());
  }

  void InitializeParserAndExpectLiveness(
      DemuxerStream::Liveness expected_liveness) {
    parser_->Init(
        base::Bind(&MP4StreamParserTest::InitF, base::Unretained(this),
                   expected_liveness),
        base::Bind(&MP4StreamParserTest::NewConfigF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::NewBuffersF, base::Unretained(this)),
        true,
        base::Bind(&MP4StreamParserTest::KeyNeededF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::NewSegmentF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::EndOfSegmentF, base::Unretained(this)),
        LogCB());
  }

  void InitializeParser() {
    // Most unencrypted test mp4 files have zero duration and are treated as
    // live streams.
    InitializeParserAndExpectLiveness(DemuxerStream::LIVENESS_LIVE);
  }

  bool ParseMP4File(const std::string& filename, int append_bytes) {
    InitializeParser();

    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                   buffer->data_size(),
                                   append_bytes));
    return true;
  }
};

TEST_F(MP4StreamParserTest, UnalignedAppend) {
  // Test small, non-segment-aligned appends (small enough to exercise
  // incremental append system)
  ParseMP4File("bear-1280x720-av_frag.mp4", 512);
}

TEST_F(MP4StreamParserTest, BytewiseAppend) {
  // Ensure no incremental errors occur when parsing
  ParseMP4File("bear-1280x720-av_frag.mp4", 1);
}

TEST_F(MP4StreamParserTest, MultiFragmentAppend) {
  // Large size ensures multiple fragments are appended in one call (size is
  // larger than this particular test file)
  ParseMP4File("bear-1280x720-av_frag.mp4", 768432);
}

TEST_F(MP4StreamParserTest, Flush) {
  // Flush while reading sample data, then start a new stream.
  InitializeParser();

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->data(), 65536, 512));
  parser_->Flush();
  EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                 buffer->data_size(),
                                 512));
}

TEST_F(MP4StreamParserTest, Reinitialization) {
  InitializeParser();

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                 buffer->data_size(),
                                 512));
  EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                 buffer->data_size(),
                                 512));
}

TEST_F(MP4StreamParserTest, MPEG2_AAC_LC) {
  std::set<int> audio_object_types;
  audio_object_types.insert(kISO_13818_7_AAC_LC);
  parser_.reset(new MP4StreamParser(audio_object_types, false));
  ParseMP4File("bear-mpeg2-aac-only_frag.mp4", 512);
}

// Test that a moov box is not always required after Flush() is called.
TEST_F(MP4StreamParserTest, NoMoovAfterFlush) {
  InitializeParser();

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                 buffer->data_size(),
                                 512));
  parser_->Flush();

  const int kFirstMoofOffset = 1307;
  EXPECT_TRUE(AppendDataInPieces(buffer->data() + kFirstMoofOffset,
                                 buffer->data_size() - kFirstMoofOffset,
                                 512));
}

// Test an invalid file where there are encrypted samples, but
// SampleAuxiliaryInformation{Sizes|Offsets}Box (saiz|saio) are missing.
// The parser should fail instead of crash. See http://crbug.com/361347
TEST_F(MP4StreamParserTest, MissingSampleAuxInfo) {
  // Encrypted test mp4 files have non-zero duration and are treated as
  // recorded streams.
  InitializeParserAndExpectLiveness(DemuxerStream::LIVENESS_RECORDED);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-a_frag-cenc_missing-saiz-saio.mp4");
  EXPECT_FALSE(AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
}

// Test a file where all video samples start with an Access Unit
// Delimiter (AUD) NALU.
TEST_F(MP4StreamParserTest, VideoSamplesStartWithAUDs) {
  ParseMP4File("bear-1280x720-av_with-aud-nalus_frag.mp4", 512);
}

TEST_F(MP4StreamParserTest, CENC) {
  // Encrypted test mp4 files have non-zero duration and are treated as
  // recorded streams.
  InitializeParserAndExpectLiveness(DemuxerStream::LIVENESS_RECORDED);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-v_frag-cenc.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
}

TEST_F(MP4StreamParserTest, NaturalSizeWithoutPASP) {
  InitializeParserAndExpectLiveness(DemuxerStream::LIVENESS_RECORDED);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-640x360-non_square_pixel-without_pasp.mp4");

  EXPECT_TRUE(AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
  EXPECT_EQ(gfx::Size(638, 360), video_decoder_config_.natural_size());
}

TEST_F(MP4StreamParserTest, NaturalSizeWithPASP) {
  InitializeParserAndExpectLiveness(DemuxerStream::LIVENESS_RECORDED);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-640x360-non_square_pixel-with_pasp.mp4");

  EXPECT_TRUE(AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
  EXPECT_EQ(gfx::Size(638, 360), video_decoder_config_.natural_size());
}

}  // namespace mp4
}  // namespace media
