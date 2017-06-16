// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_POOL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_POOL_H_

#include <map>

#include "base/basictypes.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_frame.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace content {

// A thread-safe class that does the bookkeeping and lifetime management for a
// pool of pixel buffers cycled between an in-process producer (e.g. a
// VideoCaptureDevice) and a set of out-of-process consumers. The pool is
// intended to be orchestrated by a VideoCaptureDevice::Client, but is designed
// to outlive the controller if necessary. The pixel buffers may be backed by a
// SharedMemory, but this is not compulsory.
//
// Producers get a buffer by calling ReserveForProducer(), and may pass on their
// ownership to the consumer by calling HoldForConsumers(), or drop the buffer
// (without further processing) by calling RelinquishProducerReservation().
// Consumers signal that they are done with the buffer by calling
// RelinquishConsumerHold().
//
// Buffers are allocated on demand, but there will never be more than |count|
// buffers in existence at any time. Buffers are identified by an int value
// called |buffer_id|. -1 (kInvalidId) is never a valid ID, and is returned by
// some methods to indicate failure. The active set of buffer ids may change
// over the lifetime of the buffer pool, as existing buffers are freed and
// reallocated at larger size. When reallocation occurs, new buffer IDs will
// circulate.
class CONTENT_EXPORT VideoCaptureBufferPool
    : public base::RefCountedThreadSafe<VideoCaptureBufferPool> {
 public:
  static const int kInvalidId;

  // Abstraction of a pool's buffer data buffer and size for clients.
  class BufferHandle {
   public:
    virtual ~BufferHandle() {}
    virtual size_t size() const = 0;
    virtual void* data() = 0;
    virtual ClientBuffer AsClientBuffer() = 0;
#if defined(OS_POSIX)
    virtual base::FileDescriptor AsPlatformFile() = 0;
#endif
  };

  explicit VideoCaptureBufferPool(int count);

  // One-time (per client/per-buffer) initialization to share a particular
  // buffer to a process. The size of the allocation is returned as
  // |memory_size|.
  base::SharedMemoryHandle ShareToProcess(int buffer_id,
                                          base::ProcessHandle process_handle,
                                          size_t* memory_size);

  // Try and obtain a BufferHandle for |buffer_id|.
  scoped_ptr<BufferHandle> GetBufferHandle(int buffer_id);

  // Reserve or allocate a buffer to support a packed frame of |dimensions| of
  // pixel |format| and return its id. This will fail (returning kInvalidId) if
  // the pool already is at its |count| limit of the number of allocations, and
  // all allocated buffers are in use by the producer and/or consumers.
  //
  // If successful, the reserved buffer remains reserved (and writable by the
  // producer) until ownership is transferred either to the consumer via
  // HoldForConsumers(), or back to the pool with
  // RelinquishProducerReservation().
  //
  // On occasion, this call will decide to free an old buffer to make room for a
  // new allocation at a larger size. If so, the ID of the destroyed buffer is
  // returned via |buffer_id_to_drop|.
  int ReserveForProducer(media::VideoPixelFormat format,
                         media::VideoPixelStorage storage,
                         const gfx::Size& dimensions,
                         int* buffer_id_to_drop);

  // Indicate that a buffer held for the producer should be returned back to the
  // pool without passing on to the consumer. This effectively is the opposite
  // of ReserveForProducer().
  void RelinquishProducerReservation(int buffer_id);

  // Transfer a buffer from producer to consumer ownership.
  // |buffer_id| must be a buffer index previously returned by
  // ReserveForProducer(), and not already passed to HoldForConsumers().
  void HoldForConsumers(int buffer_id, int num_clients);

  // Indicate that one or more consumers are done with a particular buffer. This
  // effectively is the opposite of HoldForConsumers(). Once the consumers are
  // done, a buffer is returned to the pool for reuse.
  void RelinquishConsumerHold(int buffer_id, int num_clients);

  // Returns a snapshot of the current number of buffers in-use divided by the
  // maximum |count_|.
  double GetBufferPoolUtilization() const;

 private:
  class GpuMemoryBufferTracker;
  class SharedMemTracker;
  // Generic class to keep track of the state of a given mappable resource. Each
  // Tracker carries indication of pixel format and storage type.
  class Tracker {
   public:
    static scoped_ptr<Tracker> CreateTracker(bool use_gmb);

    Tracker()
        : pixel_count_(0), held_by_producer_(false), consumer_hold_count_(0) {}
    virtual bool Init(media::VideoPixelFormat format,
                      media::VideoPixelStorage storage_type,
                      const gfx::Size& dimensions) = 0;
    virtual ~Tracker();

    size_t pixel_count() const { return pixel_count_; }
    void set_pixel_count(size_t count) { pixel_count_ = count; }
    media::VideoPixelFormat pixel_format() const { return pixel_format_; }
    void set_pixel_format(media::VideoPixelFormat format) {
      pixel_format_ = format;
    }
    media::VideoPixelStorage storage_type() const { return storage_type_; }
    void set_storage_type(media::VideoPixelStorage storage_type) {
      storage_type_ = storage_type;
    }
    bool held_by_producer() const { return held_by_producer_; }
    void set_held_by_producer(bool value) { held_by_producer_ = value; }
    int consumer_hold_count() const { return consumer_hold_count_; }
    void set_consumer_hold_count(int value) { consumer_hold_count_ = value; }

    // Returns a handle to the underlying storage, be that a block of Shared
    // Memory, or a GpuMemoryBuffer.
    virtual scoped_ptr<BufferHandle> GetBufferHandle() = 0;
    // The actual size of the underlying backing resource.
    virtual size_t mapped_size() const = 0;

    virtual bool ShareToProcess(base::ProcessHandle process_handle,
                                base::SharedMemoryHandle* new_handle) = 0;

   private:
    size_t pixel_count_;
    media::VideoPixelFormat pixel_format_;
    media::VideoPixelStorage storage_type_;
    // Indicates whether this Tracker is currently referenced by the producer.
    bool held_by_producer_;
    // Number of consumer processes which hold this Tracker.
    int consumer_hold_count_;
  };

  friend class base::RefCountedThreadSafe<VideoCaptureBufferPool>;
  virtual ~VideoCaptureBufferPool();

  int ReserveForProducerInternal(media::VideoPixelFormat format,
                                 media::VideoPixelStorage storage,
                                 const gfx::Size& dimensions,
                                 int* tracker_id_to_drop);

  Tracker* GetTracker(int buffer_id);

  // The max number of buffers that the pool is allowed to have at any moment.
  const int count_;

  // Protects everything below it.
  mutable base::Lock lock_;

  // The ID of the next buffer.
  int next_buffer_id_;

  // The buffers, indexed by the first parameter, a buffer id.
  using TrackerMap = std::map<int, Tracker*>;
  TrackerMap trackers_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureBufferPool);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_POOL_H_
