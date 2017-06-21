// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_TRACK_RUN_ITERATOR_H_
#define MEDIA_FORMATS_MP4_TRACK_RUN_ITERATOR_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/stream_parser_buffer.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/cenc.h"

namespace media {

class DecryptConfig;

namespace mp4 {

base::TimeDelta MEDIA_EXPORT TimeDeltaFromRational(int64 numer, int64 denom);
DecodeTimestamp MEDIA_EXPORT DecodeTimestampFromRational(int64 numer,
                                                         int64 denom);

struct SampleInfo;
struct TrackRunInfo;

class MEDIA_EXPORT TrackRunIterator {
 public:
  // Create a new TrackRunIterator. A reference to |moov| will be retained for
  // the lifetime of this object.
  TrackRunIterator(const Movie* moov, const LogCB& log_cb);
  ~TrackRunIterator();

  // Sets up the iterator to handle all the runs from the current fragment.
  bool Init(const MovieFragment& moof);

  // Returns true if the properties of the current run or sample are valid.
  bool IsRunValid() const;
  bool IsSampleValid() const;

  // Advance the properties to refer to the next run or sample. Requires that
  // the current sample be valid.
  void AdvanceRun();
  void AdvanceSample();

  // Returns true if this track run has auxiliary information and has not yet
  // been cached. Only valid if IsRunValid().
  bool AuxInfoNeedsToBeCached();

  // Caches the CENC data from the given buffer. |buf| must be a buffer starting
  // at the offset given by cenc_offset(), with a |size| of at least
  // cenc_size(). Returns true on success, false on error.
  bool CacheAuxInfo(const uint8* buf, int size);

  // Returns the maximum buffer location at which no data earlier in the stream
  // will be required in order to read the current or any subsequent sample. You
  // may clear all data up to this offset before reading the current sample
  // safely. Result is in the same units as offset() (for Media Source this is
  // in bytes past the the head of the MOOF box).
  int64 GetMaxClearOffset();

  // Property of the current run. Only valid if IsRunValid().
  uint32 track_id() const;
  int64 aux_info_offset() const;
  int aux_info_size() const;
  bool is_encrypted() const;
  bool is_audio() const;
  // Only one is valid, based on the value of is_audio().
  const AudioSampleEntry& audio_description() const;
  const VideoSampleEntry& video_description() const;

  // Properties of the current sample. Only valid if IsSampleValid().
  int64 sample_offset() const;
  int sample_size() const;
  DecodeTimestamp dts() const;
  base::TimeDelta cts() const;
  base::TimeDelta duration() const;
  bool is_keyframe() const;
  bool is_random_access_point() const;

  // Only call when is_encrypted() is true and AuxInfoNeedsToBeCached() is
  // false. Result is owned by caller.
  scoped_ptr<DecryptConfig> GetDecryptConfig();

 private:
  void ResetRun();
  const TrackEncryption& track_encryption() const;

  uint32 GetGroupDescriptionIndex(uint32 sample_index) const;
  const CencSampleEncryptionInfoEntry& GetSampleEncryptionInfoEntry(
      uint32 group_description_index) const;

  // Sample encryption information.
  bool IsSampleEncrypted(size_t sample_index) const;
  uint8 GetIvSize(size_t sample_index) const;
  const std::vector<uint8>& GetKeyId(size_t sample_index) const;

  const Movie* moov_;
  LogCB log_cb_;

  std::vector<TrackRunInfo> runs_;
  std::vector<TrackRunInfo>::const_iterator run_itr_;
  std::vector<SampleInfo>::const_iterator sample_itr_;

  std::vector<FrameCENCInfo> cenc_info_;

  int64 sample_dts_;
  int64 sample_offset_;

  DISALLOW_COPY_AND_ASSIGN(TrackRunIterator);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_FORMATS_MP4_TRACK_RUN_ITERATOR_H_
