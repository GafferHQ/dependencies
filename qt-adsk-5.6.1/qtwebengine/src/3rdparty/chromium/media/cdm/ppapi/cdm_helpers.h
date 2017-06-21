// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_CDM_HELPERS_H_
#define MEDIA_CDM_PPAPI_CDM_HELPERS_H_

#include <map>
#include <utility>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "media/cdm/ppapi/api/content_decryption_module.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/logging.h"

namespace media {

class PpbBufferAllocator;

// cdm::Buffer implementation that provides access to memory owned by a
// pp::Buffer_Dev.
// This class holds a reference to the Buffer_Dev throughout its lifetime.
// TODO(xhwang): Find a better name. It's confusing to have PpbBuffer,
// pp::Buffer_Dev and PPB_Buffer_Dev.
class PpbBuffer : public cdm::Buffer {
 public:
  static PpbBuffer* Create(const pp::Buffer_Dev& buffer, uint32_t buffer_id,
                           PpbBufferAllocator* allocator);

  // cdm::Buffer implementation.
  void Destroy() override;
  uint32_t Capacity() const override;
  uint8_t* Data() override;
  void SetSize(uint32_t size) override;
  uint32_t Size() const override { return size_; }

  // Takes the |buffer_| from this class and returns it.
  // Note: The caller must ensure |allocator->Release()| is called later so that
  // the buffer can be reused by the allocator.
  // Since pp::Buffer_Dev is ref-counted, the caller now holds one reference to
  // the buffer and this class holds no reference. Note that other references
  // may still exist. For example, PpbBufferAllocator always holds a reference
  // to all allocated buffers.
  pp::Buffer_Dev TakeBuffer();

  uint32_t buffer_id() const { return buffer_id_; }

 private:
  PpbBuffer(pp::Buffer_Dev buffer,
            uint32_t buffer_id,
            PpbBufferAllocator* allocator);
  ~PpbBuffer() override;

  pp::Buffer_Dev buffer_;
  uint32_t buffer_id_;
  uint32_t size_;
  PpbBufferAllocator* allocator_;

  DISALLOW_COPY_AND_ASSIGN(PpbBuffer);
};

class PpbBufferAllocator {
 public:
  explicit PpbBufferAllocator(pp::Instance* instance)
      : instance_(instance),
        next_buffer_id_(1) {}
  ~PpbBufferAllocator() {}

  cdm::Buffer* Allocate(uint32_t capacity);

  // Releases the buffer with |buffer_id|. A buffer can be recycled after
  // it is released.
  void Release(uint32_t buffer_id);

 private:
  typedef std::map<uint32_t, pp::Buffer_Dev> AllocatedBufferMap;
  typedef std::multimap<uint32_t, std::pair<uint32_t, pp::Buffer_Dev> >
      FreeBufferMap;

  pp::Buffer_Dev AllocateNewBuffer(uint32_t capacity);

  pp::Instance* const instance_;
  uint32_t next_buffer_id_;
  AllocatedBufferMap allocated_buffers_;
  FreeBufferMap free_buffers_;

  DISALLOW_COPY_AND_ASSIGN(PpbBufferAllocator);
};

class DecryptedBlockImpl : public cdm::DecryptedBlock {
 public:
  DecryptedBlockImpl() : buffer_(NULL), timestamp_(0) {}
  ~DecryptedBlockImpl() override {
    if (buffer_)
      buffer_->Destroy();
  }

  void SetDecryptedBuffer(cdm::Buffer* buffer) override {
    buffer_ = static_cast<PpbBuffer*>(buffer);
  }
  cdm::Buffer* DecryptedBuffer() override { return buffer_; }

  void SetTimestamp(int64_t timestamp) override {
    timestamp_ = timestamp;
  }
  int64_t Timestamp() const override { return timestamp_; }

 private:
  PpbBuffer* buffer_;
  int64_t timestamp_;

  DISALLOW_COPY_AND_ASSIGN(DecryptedBlockImpl);
};

class VideoFrameImpl : public cdm::VideoFrame {
 public:
  VideoFrameImpl();
  ~VideoFrameImpl() override;

  void SetFormat(cdm::VideoFormat format) override {
    format_ = format;
  }
  cdm::VideoFormat Format() const override { return format_; }

  void SetSize(cdm::Size size) override { size_ = size; }
  cdm::Size Size() const override { return size_; }

  void SetFrameBuffer(cdm::Buffer* frame_buffer) override {
    frame_buffer_ = static_cast<PpbBuffer*>(frame_buffer);
  }
  cdm::Buffer* FrameBuffer() override { return frame_buffer_; }

  void SetPlaneOffset(cdm::VideoFrame::VideoPlane plane,
                      uint32_t offset) override {
    PP_DCHECK(plane < kMaxPlanes);
    plane_offsets_[plane] = offset;
  }
  uint32_t PlaneOffset(VideoPlane plane) override {
    PP_DCHECK(plane < kMaxPlanes);
    return plane_offsets_[plane];
  }

  void SetStride(VideoPlane plane, uint32_t stride) override {
    PP_DCHECK(plane < kMaxPlanes);
    strides_[plane] = stride;
  }
  uint32_t Stride(VideoPlane plane) override {
    PP_DCHECK(plane < kMaxPlanes);
    return strides_[plane];
  }

  void SetTimestamp(int64_t timestamp) override {
    timestamp_ = timestamp;
  }
  int64_t Timestamp() const override { return timestamp_; }

 private:
  // The video buffer format.
  cdm::VideoFormat format_;

  // Width and height of the video frame.
  cdm::Size size_;

  // The video frame buffer.
  PpbBuffer* frame_buffer_;

  // Array of data pointers to each plane in the video frame buffer.
  uint32_t plane_offsets_[kMaxPlanes];

  // Array of strides for each plane, typically greater or equal to the width
  // of the surface divided by the horizontal sampling period.  Note that
  // strides can be negative.
  uint32_t strides_[kMaxPlanes];

  // Presentation timestamp in microseconds.
  int64_t timestamp_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameImpl);
};

class AudioFramesImpl : public cdm::AudioFrames {
 public:
  AudioFramesImpl() : buffer_(NULL), format_(cdm::kUnknownAudioFormat) {}
  ~AudioFramesImpl() override {
    if (buffer_)
      buffer_->Destroy();
  }

  // AudioFrames implementation.
  void SetFrameBuffer(cdm::Buffer* buffer) override {
    buffer_ = static_cast<PpbBuffer*>(buffer);
  }
  cdm::Buffer* FrameBuffer() override {
    return buffer_;
  }
  void SetFormat(cdm::AudioFormat format) override {
    format_ = format;
  }
  cdm::AudioFormat Format() const override {
    return format_;
  }

  cdm::Buffer* PassFrameBuffer() {
    PpbBuffer* temp_buffer = buffer_;
    buffer_ = NULL;
    return temp_buffer;
  }

 private:
  PpbBuffer* buffer_;
  cdm::AudioFormat format_;

  DISALLOW_COPY_AND_ASSIGN(AudioFramesImpl);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_CDM_HELPERS_H_
