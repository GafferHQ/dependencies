// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FAKE_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FAKE_VIDEO_DECODER_H_

#include <list>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/callback_holder.h"
#include "media/base/decoder_buffer.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "ui/gfx/geometry/size.h"

using base::ResetAndReturn;

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

typedef base::Callback<void(int)> BytesDecodedCB;

class FakeVideoDecoder : public VideoDecoder {
 public:
  // Constructs an object with a decoding delay of |decoding_delay| frames.
  // |bytes_decoded_cb| is called after each decode. The sum of the byte
  // count over all calls will be equal to total_bytes_decoded().
  FakeVideoDecoder(int decoding_delay,
                   int max_parallel_decoding_requests,
                   const BytesDecodedCB& bytes_decoded_cb);
  ~FakeVideoDecoder() override;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) override;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;
  int GetMaxDecodeRequests() const override;

  // Holds the next init/decode/reset callback from firing.
  void HoldNextInit();
  void HoldDecode();
  void HoldNextReset();

  // Satisfies the pending init/decode/reset callback, which must be ready to
  // fire when these methods are called.
  void SatisfyInit();
  void SatisfyDecode();
  void SatisfyReset();

  // Satisfies single  decode request.
  void SatisfySingleDecode();

  void SimulateError();
  // Fail with status DECODER_ERROR_NOT_SUPPORTED when Initialize() is called.
  void SimulateFailureToInit();

  int total_bytes_decoded() const { return total_bytes_decoded_; }

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_NORMAL,
    STATE_END_OF_STREAM,
    STATE_ERROR,
  };

  // Callback for updating |total_bytes_decoded_|.
  void OnFrameDecoded(int buffer_size,
                      const DecodeCB& decode_cb,
                      Status status);

  // Runs |decode_cb| or puts it to |held_decode_callbacks_| depending on
  // current value of |hold_decode_|.
  void RunOrHoldDecode(const DecodeCB& decode_cb);

  // Runs |decode_cb| with a frame from |decoded_frames_|.
  void RunDecodeCallback(const DecodeCB& decode_cb);

  void DoReset();

  base::ThreadChecker thread_checker_;

  const size_t decoding_delay_;
  const int max_parallel_decoding_requests_;
  BytesDecodedCB bytes_decoded_cb_;

  State state_;

  CallbackHolder<InitCB> init_cb_;
  CallbackHolder<base::Closure> reset_cb_;

  OutputCB output_cb_;

  bool hold_decode_;
  std::list<DecodeCB> held_decode_callbacks_;

  VideoDecoderConfig current_config_;

  std::list<scoped_refptr<VideoFrame> > decoded_frames_;

  int total_bytes_decoded_;

  bool fail_to_initialize_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<FakeVideoDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FAKE_VIDEO_DECODER_H_
