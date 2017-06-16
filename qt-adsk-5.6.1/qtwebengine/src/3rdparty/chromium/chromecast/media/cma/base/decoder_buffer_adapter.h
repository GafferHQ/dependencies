// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_DECODER_BUFFER_ADAPTER_H_
#define CHROMECAST_MEDIA_CMA_BASE_DECODER_BUFFER_ADAPTER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"

namespace media {
class DecoderBuffer;
}

namespace chromecast {
namespace media {

// DecoderBufferAdapter wraps a ::media::DecoderBuffer
// into a DecoderBufferBase.
class DecoderBufferAdapter : public DecoderBufferBase {
 public:
  // Using explicit constructor without providing stream Id will set it to
  // kPrimary by default.
  explicit DecoderBufferAdapter(
      const scoped_refptr<::media::DecoderBuffer>& buffer);
  DecoderBufferAdapter(
      StreamId stream_id, const scoped_refptr<::media::DecoderBuffer>& buffer);

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
  ~DecoderBufferAdapter() override;

  StreamId stream_id_;
  scoped_refptr<::media::DecoderBuffer> const buffer_;

  DISALLOW_COPY_AND_ASSIGN(DecoderBufferAdapter);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_DECODER_BUFFER_ADAPTER_H_
