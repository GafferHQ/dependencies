// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_DISCARDABLE_SHARED_MEMORY_MANAGER_H_
#define CONTENT_CHILD_CHILD_DISCARDABLE_SHARED_MEMORY_MANAGER_H_

#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_provider.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/content_export.h"
#include "content/common/discardable_shared_memory_heap.h"
#include "content/common/host_discardable_shared_memory_manager.h"

namespace content {

// Implementation of DiscardableMemoryAllocator that allocates
// discardable memory segments through the browser process.
class CONTENT_EXPORT ChildDiscardableSharedMemoryManager
    : public base::DiscardableMemoryAllocator,
      public base::trace_event::MemoryDumpProvider {
 public:
  explicit ChildDiscardableSharedMemoryManager(ThreadSafeSender* sender);
  ~ChildDiscardableSharedMemoryManager() override;

  // Overridden from base::DiscardableMemoryAllocator:
  scoped_ptr<base::DiscardableMemory> AllocateLockedDiscardableMemory(
      size_t size) override;

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;

  // Release memory and associated resources that have been purged.
  void ReleaseFreeMemory();

  bool LockSpan(DiscardableSharedMemoryHeap::Span* span);
  void UnlockSpan(DiscardableSharedMemoryHeap::Span* span);
  void ReleaseSpan(scoped_ptr<DiscardableSharedMemoryHeap::Span> span);

 private:
  scoped_ptr<base::DiscardableSharedMemory>
  AllocateLockedDiscardableSharedMemory(size_t size,
                                        DiscardableSharedMemoryId id);
  void MemoryUsageChanged(size_t new_bytes_allocated,
                          size_t new_bytes_free) const;

  mutable base::Lock lock_;
  DiscardableSharedMemoryHeap heap_;
  scoped_refptr<ThreadSafeSender> sender_;

  DISALLOW_COPY_AND_ASSIGN(ChildDiscardableSharedMemoryManager);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_DISCARDABLE_SHARED_MEMORY_MANAGER_H_
