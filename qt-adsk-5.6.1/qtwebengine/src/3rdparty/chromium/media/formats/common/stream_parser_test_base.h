// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_COMMON_STREAM_PARSER_TEST_BASE_H_
#define MEDIA_FORMATS_COMMON_STREAM_PARSER_TEST_BASE_H_

#include "base/memory/scoped_ptr.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder_config.h"

namespace media {

// Test helper for verifying StreamParser behavior.
class StreamParserTestBase {
 public:
  explicit StreamParserTestBase(scoped_ptr<StreamParser> stream_parser);
  virtual ~StreamParserTestBase();

 protected:
  // Chunks a given parser appropriate file.  Appends |append_bytes| at a time
  // until the file is exhausted.  Returns a coded string representing the
  // segments and timestamps of the extracted frames.
  //
  // The start of each media segment is designated by "NewSegment", similarly
  // the end of each segment by "EndOfSegment".  Segments end when one or more
  // frames are parsed from an append.  If the append contains a partial frame
  // the segment will continue into the next append.
  //
  // Parsed frame(s) are represented as "{ xxK yyK zzK }"  Where xx, yy, and zz
  // are the timestamps in milliseconds of each parsed frame.  For example:
  //
  //     "NewSegment{ 0K 23K 46K }EndOfSegment"
  //     "NewSegment{ 0K }{ 23K }{ 46K }EndOfSegment"
  //     "NewSegment{ 0K }{ 23K }EndOfSegmentNewSegment{ 46K }EndOfSegment"
  //
  std::string ParseFile(const std::string& filename, int append_bytes);

  // Similar to ParseFile() except parses the given |data| in a single append of
  // size |length|.
  std::string ParseData(const uint8* data, size_t length);

  // The last AudioDecoderConfig handed to OnNewConfig().
  const AudioDecoderConfig& last_audio_config() const {
    return last_audio_config_;
  }

 private:
  bool AppendDataInPieces(const uint8* data, size_t length, size_t piece_size);
  void OnInitDone(const StreamParser::InitParameters& params);
  bool OnNewConfig(const AudioDecoderConfig& audio_config,
                   const VideoDecoderConfig& video_config,
                   const StreamParser::TextTrackConfigMap& text_config);
  bool OnNewBuffers(const StreamParser::BufferQueue& audio_buffers,
                    const StreamParser::BufferQueue& video_buffers,
                    const StreamParser::TextBufferQueueMap& text_map);
  void OnKeyNeeded(EmeInitDataType type, const std::vector<uint8>& init_data);
  void OnNewSegment();
  void OnEndOfSegment();

  scoped_ptr<StreamParser> parser_;
  std::stringstream results_stream_;
  AudioDecoderConfig last_audio_config_;

  DISALLOW_COPY_AND_ASSIGN(StreamParserTestBase);
};

}  // namespace media

#endif  // MEDIA_FORMATS_COMMON_STREAM_PARSER_TEST_BASE_H_
