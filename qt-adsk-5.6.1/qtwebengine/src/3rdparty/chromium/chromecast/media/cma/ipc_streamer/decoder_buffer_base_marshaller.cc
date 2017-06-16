// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/ipc_streamer/decoder_buffer_base_marshaller.h"

#include "base/logging.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/ipc/media_message.h"
#include "chromecast/media/cma/ipc/media_message_type.h"
#include "chromecast/media/cma/ipc_streamer/decrypt_config_marshaller.h"
#include "media/base/decrypt_config.h"

namespace chromecast {
namespace media {

namespace {
const size_t kMaxFrameSize = 4 * 1024 * 1024;

class DecoderBufferFromMsg : public DecoderBufferBase {
 public:
  explicit DecoderBufferFromMsg(scoped_ptr<MediaMessage> msg);

  void Initialize();

  // DecoderBufferBase implementation.
  StreamId stream_id() const override;
  base::TimeDelta timestamp() const override;
  void set_timestamp(const base::TimeDelta& timestamp) override;
  const uint8* data() const override;
  uint8* writable_data() const override;
  size_t data_size() const override;
  const ::media::DecryptConfig* decrypt_config() const override;
  bool end_of_stream() const override;

 private:
  ~DecoderBufferFromMsg() override;

  // Indicates whether this is an end of stream frame.
  bool is_eos_;

  // Stream Id this decoder buffer belongs to.
  StreamId stream_id_;

  // Frame timestamp.
  base::TimeDelta pts_;

  // CENC parameters.
  scoped_ptr< ::media::DecryptConfig> decrypt_config_;

  // Size of the frame.
  size_t data_size_;

  // Keeps the message since frame data is not copied.
  scoped_ptr<MediaMessage> msg_;
  uint8* data_;

  DISALLOW_COPY_AND_ASSIGN(DecoderBufferFromMsg);
};

DecoderBufferFromMsg::DecoderBufferFromMsg(
    scoped_ptr<MediaMessage> msg)
    : is_eos_(true),
      stream_id_(kPrimary),
      msg_(msg.Pass()),
      data_(NULL) {
  CHECK(msg_);
}

DecoderBufferFromMsg::~DecoderBufferFromMsg() {
}

void DecoderBufferFromMsg::Initialize() {
  CHECK_EQ(msg_->type(), FrameMediaMsg);

  CHECK(msg_->ReadPod(&is_eos_));
  if (is_eos_)
    return;

  CHECK(msg_->ReadPod(&stream_id_));

  int64 pts_internal = 0;
  CHECK(msg_->ReadPod(&pts_internal));
  pts_ = base::TimeDelta::FromInternalValue(pts_internal);

  bool has_decrypt_config = false;
  CHECK(msg_->ReadPod(&has_decrypt_config));
  if (has_decrypt_config)
    decrypt_config_.reset(DecryptConfigMarshaller::Read(msg_.get()).release());

  CHECK(msg_->ReadPod(&data_size_));
  CHECK_GT(data_size_, 0u);
  CHECK_LT(data_size_, kMaxFrameSize);

  // Get a pointer to the frame data inside the message.
  // Avoid copying the frame data here.
  data_ = static_cast<uint8*>(msg_->GetWritableBuffer(data_size_));
  CHECK(data_);

  if (decrypt_config_) {
    uint32 subsample_total_size = 0;
    for (size_t k = 0; k < decrypt_config_->subsamples().size(); k++) {
      subsample_total_size += decrypt_config_->subsamples()[k].clear_bytes;
      subsample_total_size += decrypt_config_->subsamples()[k].cypher_bytes;
    }
    CHECK_EQ(subsample_total_size, data_size_);
  }
}

StreamId DecoderBufferFromMsg::stream_id() const {
  return stream_id_;
}

base::TimeDelta DecoderBufferFromMsg::timestamp() const {
  return pts_;
}

void DecoderBufferFromMsg::set_timestamp(const base::TimeDelta& timestamp) {
  pts_ = timestamp;
}

const uint8* DecoderBufferFromMsg::data() const {
  CHECK(msg_->IsSerializedMsgAvailable());
  return data_;
}

uint8* DecoderBufferFromMsg::writable_data() const {
  CHECK(msg_->IsSerializedMsgAvailable());
  return data_;
}

size_t DecoderBufferFromMsg::data_size() const {
  return data_size_;
}

const ::media::DecryptConfig* DecoderBufferFromMsg::decrypt_config() const {
  return decrypt_config_.get();
}

bool DecoderBufferFromMsg::end_of_stream() const {
  return is_eos_;
}

}  // namespace

// static
void DecoderBufferBaseMarshaller::Write(
    const scoped_refptr<DecoderBufferBase>& buffer,
    MediaMessage* msg) {
  CHECK(msg->WritePod(buffer->end_of_stream()));
  if (buffer->end_of_stream())
    return;

  CHECK(msg->WritePod(buffer->stream_id()));
  CHECK(msg->WritePod(buffer->timestamp().ToInternalValue()));

  bool has_decrypt_config =
      (buffer->decrypt_config() != NULL &&
       buffer->decrypt_config()->iv().size() > 0);
  CHECK(msg->WritePod(has_decrypt_config));

  if (has_decrypt_config) {
    // DecryptConfig may contain 0 subsamples if all content is encrypted.
    // Map this case to a single fully-encrypted "subsample" for more consistent
    // backend handling.
    if (buffer->decrypt_config()->subsamples().empty()) {
      std::vector< ::media::SubsampleEntry> encrypted_subsample_list(1);
      encrypted_subsample_list[0].clear_bytes = 0;
      encrypted_subsample_list[0].cypher_bytes = buffer->data_size();
      ::media::DecryptConfig full_sample_config(
          buffer->decrypt_config()->key_id(),
          buffer->decrypt_config()->iv(),
          encrypted_subsample_list);
      DecryptConfigMarshaller::Write(full_sample_config, msg);
    } else {
      DecryptConfigMarshaller::Write(*buffer->decrypt_config(), msg);
    }
  }

  CHECK(msg->WritePod(buffer->data_size()));
  CHECK(msg->WriteBuffer(buffer->data(), buffer->data_size()));
}

// static
scoped_refptr<DecoderBufferBase> DecoderBufferBaseMarshaller::Read(
    scoped_ptr<MediaMessage> msg) {
  scoped_refptr<DecoderBufferFromMsg> buffer(
      new DecoderBufferFromMsg(msg.Pass()));
  buffer->Initialize();
  return buffer;
}

}  // namespace media
}  // namespace chromecast
