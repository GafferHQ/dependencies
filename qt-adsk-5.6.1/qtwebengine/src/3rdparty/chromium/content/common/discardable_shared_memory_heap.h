// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DISCARDABLE_SHARED_MEMORY_HEAP_H_
#define CONTENT_COMMON_DISCARDABLE_SHARED_MEMORY_HEAP_H_

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/containers/linked_list.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/trace_event/process_memory_dump.h"
#include "content/common/content_export.h"

namespace base {
class DiscardableSharedMemory;
}

namespace content {

// Implements a heap of discardable shared memory. An array of free lists
// is used to keep track of free blocks.
class CONTENT_EXPORT DiscardableSharedMemoryHeap {
 public:
  class CONTENT_EXPORT Span : public base::LinkNode<Span> {
   public:
    ~Span();

    base::DiscardableSharedMemory* shared_memory() { return shared_memory_; }
    size_t start() const { return start_; }
    size_t length() const { return length_; }

   private:
    friend class DiscardableSharedMemoryHeap;

    Span(base::DiscardableSharedMemory* shared_memory,
         size_t start,
         size_t length);

    base::DiscardableSharedMemory* shared_memory_;
    size_t start_;
    size_t length_;

    DISALLOW_COPY_AND_ASSIGN(Span);
  };

  explicit DiscardableSharedMemoryHeap(size_t block_size);
  ~DiscardableSharedMemoryHeap();

  // Grow heap using |shared_memory| and return a span for this new memory.
  // |shared_memory| must be aligned to the block size and |size| must be a
  // multiple of the block size. |deleted_callback| is called when
  // |shared_memory| has been deleted.
  scoped_ptr<Span> Grow(scoped_ptr<base::DiscardableSharedMemory> shared_memory,
                        size_t size,
                        int32_t id,
                        const base::Closure& deleted_callback);

  // Merge |span| into the free lists. This will coalesce |span| with
  // neighboring free spans when possible.
  void MergeIntoFreeLists(scoped_ptr<Span> span);

  // Split an allocated span into two spans, one of length |blocks| followed
  // by another span of length "span->length - blocks" blocks. Modifies |span|
  // to point to the first span of length |blocks|. Return second span.
  scoped_ptr<Span> Split(Span* span, size_t blocks);

  // Search free lists for span that satisfies the request for |blocks| of
  // memory. If found, the span is removed from the free list and returned.
  // |slack| determines the fitness requirement. Only spans that are less
  // or equal to |blocks| + |slack| are considered, worse fitting spans are
  // ignored.
  scoped_ptr<Span> SearchFreeLists(size_t blocks, size_t slack);

  // Release free shared memory segments.
  void ReleaseFreeMemory();

  // Release shared memory segments that have been purged.
  void ReleasePurgedMemory();

  // Returns total bytes of memory in heap.
  size_t GetSize() const;

  // Returns bytes of memory currently in the free lists.
  size_t GetSizeOfFreeLists() const;

  // Dumps memory statistics for chrome://tracing.
  bool OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd);

  // Returns a unique identifier for a given tuple of (process id, segment id)
  // that can be used to match memory dumps across different processes.
  static base::trace_event::MemoryAllocatorDumpGuid GetSegmentGUIDForTracing(
      uint64 tracing_process_id,
      int32 segment_id);

 private:
  class ScopedMemorySegment {
   public:
    ScopedMemorySegment(DiscardableSharedMemoryHeap* heap,
                        scoped_ptr<base::DiscardableSharedMemory> shared_memory,
                        size_t size,
                        int32_t id,
                        const base::Closure& deleted_callback);
    ~ScopedMemorySegment();

    bool IsUsed() const;
    bool IsResident() const;

    // Used for dumping memory statistics from the segment to chrome://tracing.
    void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) const;

   private:
    DiscardableSharedMemoryHeap* const heap_;
    scoped_ptr<base::DiscardableSharedMemory> shared_memory_;
    const size_t size_;
    const int32_t id_;
    const base::Closure deleted_callback_;

    DISALLOW_COPY_AND_ASSIGN(ScopedMemorySegment);
  };

  void InsertIntoFreeList(scoped_ptr<Span> span);
  scoped_ptr<Span> RemoveFromFreeList(Span* span);
  scoped_ptr<Span> Carve(Span* span, size_t blocks);
  void RegisterSpan(Span* span);
  void UnregisterSpan(Span* span);
  bool IsMemoryUsed(const base::DiscardableSharedMemory* shared_memory,
                    size_t size);
  bool IsMemoryResident(const base::DiscardableSharedMemory* shared_memory);
  void ReleaseMemory(const base::DiscardableSharedMemory* shared_memory,
                     size_t size);

  // Dumps memory statistics about a memory segment for chrome://tracing.
  void OnMemoryDump(const base::DiscardableSharedMemory* shared_memory,
                    size_t size,
                    int32_t segment_id,
                    base::trace_event::ProcessMemoryDump* pmd);

  size_t block_size_;
  size_t num_blocks_;
  size_t num_free_blocks_;

  // Vector of memory segments.
  ScopedVector<ScopedMemorySegment> memory_segments_;

  // Mapping from first/last block of span to Span instance.
  typedef base::hash_map<size_t, Span*> SpanMap;
  SpanMap spans_;

  // Array of linked-lists with free discardable memory regions. For i < 256,
  // where the 1st entry is located at index 0 of the array, the kth entry
  // is a free list of runs that consist of k blocks. The 256th entry is a
  // free list of runs that have length >= 256 blocks.
  base::LinkedList<Span> free_spans_[256];

  DISALLOW_COPY_AND_ASSIGN(DiscardableSharedMemoryHeap);
};

}  // namespace content

#endif  // CONTENT_COMMON_DISCARDABLE_SHARED_MEMORY_HEAP_H_
