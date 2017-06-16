// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_ACCELERATED_VIDEO_DECODER_H_
#define CONTENT_COMMON_GPU_MEDIA_ACCELERATED_VIDEO_DECODER_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/size.h"

namespace content {

// An AcceleratedVideoDecoder is a video decoder that requires support from an
// external accelerator (typically a hardware accelerator) to partially offload
// the decode process after parsing stream headers, and performing reference
// frame and state management.
class CONTENT_EXPORT AcceleratedVideoDecoder {
 public:
  AcceleratedVideoDecoder() {}
  virtual ~AcceleratedVideoDecoder() {}

  virtual void SetStream(const uint8_t* ptr, size_t size) = 0;

  // Have the decoder flush its state and trigger output of all previously
  // decoded surfaces. Return false on failure.
  virtual bool Flush() WARN_UNUSED_RESULT = 0;

  // Stop (pause) decoding, discarding all remaining inputs and outputs,
  // but do not flush decoder state, so that playback can be resumed later,
  // possibly from a different location.
  // To be called during decoding.
  virtual void Reset() = 0;

  enum DecodeResult {
    kDecodeError,  // Error while decoding.
    // TODO(posciak): unsupported streams are currently treated as error
    // in decoding; in future it could perhaps be possible to fall back
    // to software decoding instead.
    // kStreamError,  // Error in stream.
    kAllocateNewSurfaces,  // Need a new set of surfaces to be allocated.
    kRanOutOfStreamData,   // Need more stream data to proceed.
    kRanOutOfSurfaces,     // Waiting for the client to free up output surfaces.
  };

  // Try to decode more of the stream, returning decoded frames asynchronously.
  // Return when more stream is needed, when we run out of free surfaces, when
  // we need a new set of them, or when an error occurs.
  virtual DecodeResult Decode() WARN_UNUSED_RESULT = 0;

  // Return dimensions/required number of output surfaces that client should
  // be ready to provide for the decoder to function properly.
  // To be used after Decode() returns kNeedNewSurfaces.
  virtual gfx::Size GetPicSize() const = 0;
  virtual size_t GetRequiredNumOfPictures() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceleratedVideoDecoder);
};

}  //  namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_ACCELERATED_VIDEO_DECODER_H_
