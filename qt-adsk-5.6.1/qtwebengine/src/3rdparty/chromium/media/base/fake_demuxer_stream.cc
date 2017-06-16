// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/fake_demuxer_stream.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

const int kStartTimestampMs = 0;
const int kDurationMs = 30;
const int kStartWidth = 320;
const int kStartHeight = 240;
const int kWidthDelta = 4;
const int kHeightDelta = 3;
const uint8 kKeyId[] = { 0x00, 0x01, 0x02, 0x03 };
const uint8 kIv[] = {
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

FakeDemuxerStream::FakeDemuxerStream(int num_configs,
                                     int num_buffers_in_one_config,
                                     bool is_encrypted)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      num_configs_(num_configs),
      num_buffers_in_one_config_(num_buffers_in_one_config),
      config_changes_(num_configs > 1),
      is_encrypted_(is_encrypted),
      read_to_hold_(-1) {
  DCHECK_GT(num_configs, 0);
  DCHECK_GT(num_buffers_in_one_config, 0);
  Initialize();
  UpdateVideoDecoderConfig();
}

FakeDemuxerStream::~FakeDemuxerStream() {}

void FakeDemuxerStream::Initialize() {
  DCHECK_EQ(-1, read_to_hold_);
  num_configs_left_ = num_configs_;
  num_buffers_left_in_current_config_ = num_buffers_in_one_config_;
  num_buffers_returned_ = 0;
  current_timestamp_ = base::TimeDelta::FromMilliseconds(kStartTimestampMs);
  duration_ = base::TimeDelta::FromMilliseconds(kDurationMs);
  splice_timestamp_ = kNoTimestamp();
  next_coded_size_ = gfx::Size(kStartWidth, kStartHeight);
  next_read_num_ = 0;
}

void FakeDemuxerStream::Read(const ReadCB& read_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(read_cb_.is_null());

  read_cb_ = BindToCurrentLoop(read_cb);

  if (read_to_hold_ == next_read_num_)
    return;

  DCHECK(read_to_hold_ == -1 || read_to_hold_ > next_read_num_);
  DoRead();
}

AudioDecoderConfig FakeDemuxerStream::audio_decoder_config() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  NOTREACHED();
  return AudioDecoderConfig();
}

VideoDecoderConfig FakeDemuxerStream::video_decoder_config() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return video_decoder_config_;
}

// TODO(xhwang): Support audio if needed.
DemuxerStream::Type FakeDemuxerStream::type() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return VIDEO;
}

bool FakeDemuxerStream::SupportsConfigChanges() {
  return config_changes_;
}

VideoRotation FakeDemuxerStream::video_rotation() {
  return VIDEO_ROTATION_0;
}

void FakeDemuxerStream::HoldNextRead() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  read_to_hold_ = next_read_num_;
}

void FakeDemuxerStream::HoldNextConfigChangeRead() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // Set |read_to_hold_| to be the next config change read.
  read_to_hold_ = next_read_num_ + num_buffers_in_one_config_ -
                  next_read_num_ % (num_buffers_in_one_config_ + 1);
}

void FakeDemuxerStream::SatisfyRead() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(read_to_hold_, next_read_num_);
  DCHECK(!read_cb_.is_null());

  read_to_hold_ = -1;
  DoRead();
}

void FakeDemuxerStream::SatisfyReadAndHoldNext() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(read_to_hold_, next_read_num_);
  DCHECK(!read_cb_.is_null());

  ++read_to_hold_;
  DoRead();
}

void FakeDemuxerStream::Reset() {
  read_to_hold_ = -1;

  if (!read_cb_.is_null())
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
}

void FakeDemuxerStream::SeekToStart() {
  Reset();
  Initialize();
}

void FakeDemuxerStream::UpdateVideoDecoderConfig() {
  const gfx::Rect kVisibleRect(kStartWidth, kStartHeight);
  video_decoder_config_.Initialize(
      kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN, VideoFrame::YV12,
      VideoFrame::COLOR_SPACE_UNSPECIFIED, next_coded_size_, kVisibleRect,
      next_coded_size_, NULL, 0, is_encrypted_, false);
  next_coded_size_.Enlarge(kWidthDelta, kHeightDelta);
}

void FakeDemuxerStream::DoRead() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!read_cb_.is_null());

  next_read_num_++;

  if (num_buffers_left_in_current_config_ == 0) {
    // End of stream.
    if (num_configs_left_ == 0) {
      base::ResetAndReturn(&read_cb_).Run(kOk,
                                          DecoderBuffer::CreateEOSBuffer());
      return;
    }

    // Config change.
    num_buffers_left_in_current_config_ = num_buffers_in_one_config_;
    UpdateVideoDecoderConfig();
    base::ResetAndReturn(&read_cb_).Run(kConfigChanged, NULL);
    return;
  }

  scoped_refptr<DecoderBuffer> buffer = CreateFakeVideoBufferForTest(
      video_decoder_config_, current_timestamp_, duration_);

  // TODO(xhwang): Output out-of-order buffers if needed.
  if (is_encrypted_) {
    buffer->set_decrypt_config(scoped_ptr<DecryptConfig>(
        new DecryptConfig(std::string(kKeyId, kKeyId + arraysize(kKeyId)),
                          std::string(kIv, kIv + arraysize(kIv)),
                          std::vector<SubsampleEntry>())));
  }
  buffer->set_timestamp(current_timestamp_);
  buffer->set_duration(duration_);
  buffer->set_splice_timestamp(splice_timestamp_);
  current_timestamp_ += duration_;

  num_buffers_left_in_current_config_--;
  if (num_buffers_left_in_current_config_ == 0)
    num_configs_left_--;

  num_buffers_returned_++;
  base::ResetAndReturn(&read_cb_).Run(kOk, buffer);
}

FakeDemuxerStreamProvider::FakeDemuxerStreamProvider(
    int num_video_configs,
    int num_video_buffers_in_one_config,
    bool is_video_encrypted)
    : fake_video_stream_(num_video_configs,
                         num_video_buffers_in_one_config,
                         is_video_encrypted) {
}

FakeDemuxerStreamProvider::~FakeDemuxerStreamProvider() {
}

DemuxerStream* FakeDemuxerStreamProvider::GetStream(DemuxerStream::Type type) {
  if (type == DemuxerStream::Type::AUDIO)
    return nullptr;
  return &fake_video_stream_;
};

}  // namespace media
