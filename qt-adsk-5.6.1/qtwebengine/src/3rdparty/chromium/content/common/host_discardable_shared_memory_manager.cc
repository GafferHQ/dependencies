// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/host_discardable_shared_memory_manager.h"

#include <algorithm>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/crash_logging.h"
#include "base/lazy_instance.h"
#include "base/memory/discardable_memory.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "content/common/discardable_shared_memory_heap.h"
#include "content/public/common/child_process_host.h"

namespace content {
namespace {

class DiscardableMemoryImpl : public base::DiscardableMemory {
 public:
  DiscardableMemoryImpl(scoped_ptr<base::DiscardableSharedMemory> shared_memory,
                        const base::Closure& deleted_callback)
      : shared_memory_(shared_memory.Pass()),
        deleted_callback_(deleted_callback),
        is_locked_(true) {}

  ~DiscardableMemoryImpl() override {
    if (is_locked_)
      shared_memory_->Unlock(0, 0);

    deleted_callback_.Run();
  }

  // Overridden from base::DiscardableMemory:
  bool Lock() override {
    DCHECK(!is_locked_);

    if (shared_memory_->Lock(0, 0) != base::DiscardableSharedMemory::SUCCESS)
      return false;

    is_locked_ = true;
    return true;
  }
  void Unlock() override {
    DCHECK(is_locked_);

    shared_memory_->Unlock(0, 0);
    is_locked_ = false;
  }
  void* data() const override {
    DCHECK(is_locked_);
    return shared_memory_->memory();
  }

 private:
  scoped_ptr<base::DiscardableSharedMemory> shared_memory_;
  const base::Closure deleted_callback_;
  bool is_locked_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryImpl);
};

base::LazyInstance<HostDiscardableSharedMemoryManager>
    g_discardable_shared_memory_manager = LAZY_INSTANCE_INITIALIZER;

#if defined(OS_ANDROID)
// Limits the number of FDs used to 32, assuming a 4MB allocation size.
const int64_t kMaxDefaultMemoryLimit = 128 * 1024 * 1024;
#else
const int64_t kMaxDefaultMemoryLimit = 512 * 1024 * 1024;
#endif

const int kEnforceMemoryPolicyDelayMs = 1000;

// Global atomic to generate unique discardable shared memory IDs.
base::StaticAtomicSequenceNumber g_next_discardable_shared_memory_id;

}  // namespace

HostDiscardableSharedMemoryManager::MemorySegment::MemorySegment(
    scoped_ptr<base::DiscardableSharedMemory> memory)
    : memory_(memory.Pass()) {
}

HostDiscardableSharedMemoryManager::MemorySegment::~MemorySegment() {
}

HostDiscardableSharedMemoryManager::HostDiscardableSharedMemoryManager()
    : memory_limit_(
          // Allow 25% of physical memory to be used for discardable memory.
          std::min(base::SysInfo::AmountOfPhysicalMemory() / 4,
                   base::SysInfo::IsLowEndDevice()
                       ?
                       // Use 1/8th of discardable memory on low-end devices.
                       kMaxDefaultMemoryLimit / 8
                       : kMaxDefaultMemoryLimit)),
      bytes_allocated_(0),
      memory_pressure_listener_(new base::MemoryPressureListener(
          base::Bind(&HostDiscardableSharedMemoryManager::OnMemoryPressure,
                     base::Unretained(this)))),
      enforce_memory_policy_pending_(false),
      weak_ptr_factory_(this) {
  DCHECK_NE(memory_limit_, 0u);
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this);
}

HostDiscardableSharedMemoryManager::~HostDiscardableSharedMemoryManager() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

HostDiscardableSharedMemoryManager*
HostDiscardableSharedMemoryManager::current() {
  return g_discardable_shared_memory_manager.Pointer();
}

scoped_ptr<base::DiscardableMemory>
HostDiscardableSharedMemoryManager::AllocateLockedDiscardableMemory(
    size_t size) {
  DiscardableSharedMemoryId new_id =
      g_next_discardable_shared_memory_id.GetNext();
  base::ProcessHandle current_process_handle = base::GetCurrentProcessHandle();

  // Note: Use DiscardableSharedMemoryHeap for in-process allocation
  // of discardable memory if the cost of each allocation is too high.
  base::SharedMemoryHandle handle;
  AllocateLockedDiscardableSharedMemory(current_process_handle,
                                        ChildProcessHost::kInvalidUniqueID,
                                        size, new_id, &handle);
  CHECK(base::SharedMemory::IsHandleValid(handle));
  scoped_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory(handle));
  CHECK(memory->Map(size));
  // Close file descriptor to avoid running out.
  memory->Close();
  return make_scoped_ptr(new DiscardableMemoryImpl(
      memory.Pass(),
      base::Bind(
          &HostDiscardableSharedMemoryManager::DeletedDiscardableSharedMemory,
          base::Unretained(this), new_id, ChildProcessHost::kInvalidUniqueID)));
}

bool HostDiscardableSharedMemoryManager::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(lock_);
  for (const auto& process_entry : processes_) {
    const int child_process_id = process_entry.first;
    const MemorySegmentMap& process_segments = process_entry.second;
    for (const auto& segment_entry : process_segments) {
      const int segment_id = segment_entry.first;
      const MemorySegment* segment = segment_entry.second.get();
      std::string dump_name = base::StringPrintf(
          "discardable/process_%x/segment_%d", child_process_id, segment_id);
      base::trace_event::MemoryAllocatorDump* dump =
          pmd->CreateAllocatorDump(dump_name);
      dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      segment->memory()->mapped_size());

      // Create the cross-process ownership edge. If the child creates a
      // corresponding dump for the same segment, this will avoid to
      // double-count them in tracing. If, instead, no other process will emit a
      // dump with the same guid, the segment will be accounted to the browser.
      const uint64 child_tracing_process_id = base::trace_event::
          MemoryDumpManager::ChildProcessIdToTracingProcessId(child_process_id);
      base::trace_event::MemoryAllocatorDumpGuid shared_segment_guid =
          DiscardableSharedMemoryHeap::GetSegmentGUIDForTracing(
              child_tracing_process_id, segment_id);
      pmd->CreateSharedGlobalAllocatorDump(shared_segment_guid);
      pmd->AddOwnershipEdge(dump->guid(), shared_segment_guid);
    }
  }
  return true;
}

void HostDiscardableSharedMemoryManager::
    AllocateLockedDiscardableSharedMemoryForChild(
        base::ProcessHandle process_handle,
        int child_process_id,
        size_t size,
        DiscardableSharedMemoryId id,
        base::SharedMemoryHandle* shared_memory_handle) {
  AllocateLockedDiscardableSharedMemory(process_handle, child_process_id, size,
                                        id, shared_memory_handle);
}

void HostDiscardableSharedMemoryManager::ChildDeletedDiscardableSharedMemory(
    DiscardableSharedMemoryId id,
    int child_process_id) {
  DeletedDiscardableSharedMemory(id, child_process_id);
}

void HostDiscardableSharedMemoryManager::ProcessRemoved(int child_process_id) {
  base::AutoLock lock(lock_);

  ProcessMap::iterator process_it = processes_.find(child_process_id);
  if (process_it == processes_.end())
    return;

  size_t bytes_allocated_before_releasing_memory = bytes_allocated_;

  for (auto& segment_it : process_it->second)
    ReleaseMemory(segment_it.second->memory());

  processes_.erase(process_it);

  if (bytes_allocated_ != bytes_allocated_before_releasing_memory)
    BytesAllocatedChanged(bytes_allocated_);
}

void HostDiscardableSharedMemoryManager::SetMemoryLimit(size_t limit) {
  base::AutoLock lock(lock_);

  memory_limit_ = limit;
  ReduceMemoryUsageUntilWithinMemoryLimit();
}

void HostDiscardableSharedMemoryManager::EnforceMemoryPolicy() {
  base::AutoLock lock(lock_);

  enforce_memory_policy_pending_ = false;
  ReduceMemoryUsageUntilWithinMemoryLimit();
}

size_t HostDiscardableSharedMemoryManager::GetBytesAllocated() {
  base::AutoLock lock(lock_);

  return bytes_allocated_;
}

void HostDiscardableSharedMemoryManager::AllocateLockedDiscardableSharedMemory(
    base::ProcessHandle process_handle,
    int client_process_id,
    size_t size,
    DiscardableSharedMemoryId id,
    base::SharedMemoryHandle* shared_memory_handle) {
  base::AutoLock lock(lock_);

  // Make sure |id| is not already in use.
  MemorySegmentMap& process_segments = processes_[client_process_id];
  if (process_segments.find(id) != process_segments.end()) {
    LOG(ERROR) << "Invalid discardable shared memory ID";
    *shared_memory_handle = base::SharedMemory::NULLHandle();
    return;
  }

  // Memory usage must be reduced to prevent the addition of |size| from
  // taking usage above the limit. Usage should be reduced to 0 in cases
  // where |size| is greater than the limit.
  size_t limit = 0;
  // Note: the actual mapped size can be larger than requested and cause
  // |bytes_allocated_| to temporarily be larger than |memory_limit_|. The
  // error is minimized by incrementing |bytes_allocated_| with the actual
  // mapped size rather than |size| below.
  if (size < memory_limit_)
    limit = memory_limit_ - size;

  if (bytes_allocated_ > limit)
    ReduceMemoryUsageUntilWithinLimit(limit);

  scoped_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  if (!memory->CreateAndMap(size)) {
    *shared_memory_handle = base::SharedMemory::NULLHandle();
    return;
  }

  if (!memory->ShareToProcess(process_handle, shared_memory_handle)) {
    LOG(ERROR) << "Cannot share discardable memory segment";
    *shared_memory_handle = base::SharedMemory::NULLHandle();
    return;
  }

  base::CheckedNumeric<size_t> checked_bytes_allocated = bytes_allocated_;
  checked_bytes_allocated += memory->mapped_size();
  if (!checked_bytes_allocated.IsValid()) {
    *shared_memory_handle = base::SharedMemory::NULLHandle();
    return;
  }

  bytes_allocated_ = checked_bytes_allocated.ValueOrDie();
  BytesAllocatedChanged(bytes_allocated_);

#if !defined(DISCARDABLE_SHARED_MEMORY_SHRINKING)
  // Close file descriptor to avoid running out.
  memory->Close();
#endif

  scoped_refptr<MemorySegment> segment(new MemorySegment(memory.Pass()));
  process_segments[id] = segment.get();
  segments_.push_back(segment.get());
  std::push_heap(segments_.begin(), segments_.end(), CompareMemoryUsageTime);

  if (bytes_allocated_ > memory_limit_)
    ScheduleEnforceMemoryPolicy();
}

void HostDiscardableSharedMemoryManager::DeletedDiscardableSharedMemory(
    DiscardableSharedMemoryId id,
    int client_process_id) {
  base::AutoLock lock(lock_);

  MemorySegmentMap& process_segments = processes_[client_process_id];

  MemorySegmentMap::iterator segment_it = process_segments.find(id);
  if (segment_it == process_segments.end()) {
    LOG(ERROR) << "Invalid discardable shared memory ID";
    return;
  }

  size_t bytes_allocated_before_releasing_memory = bytes_allocated_;

  ReleaseMemory(segment_it->second->memory());

  process_segments.erase(segment_it);

  if (bytes_allocated_ != bytes_allocated_before_releasing_memory)
    BytesAllocatedChanged(bytes_allocated_);
}

void HostDiscardableSharedMemoryManager::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  base::AutoLock lock(lock_);

  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      // Purge memory until usage is within half of |memory_limit_|.
      ReduceMemoryUsageUntilWithinLimit(memory_limit_ / 2);
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // Purge everything possible when pressure is critical.
      ReduceMemoryUsageUntilWithinLimit(0);
      break;
  }
}

void
HostDiscardableSharedMemoryManager::ReduceMemoryUsageUntilWithinMemoryLimit() {
  lock_.AssertAcquired();

  if (bytes_allocated_ <= memory_limit_)
    return;

  ReduceMemoryUsageUntilWithinLimit(memory_limit_);
  if (bytes_allocated_ > memory_limit_)
    ScheduleEnforceMemoryPolicy();
}

void HostDiscardableSharedMemoryManager::ReduceMemoryUsageUntilWithinLimit(
    size_t limit) {
  TRACE_EVENT1("renderer_host",
               "HostDiscardableSharedMemoryManager::"
               "ReduceMemoryUsageUntilWithinLimit",
               "bytes_allocated",
               bytes_allocated_);

  // Usage time of currently locked segments are updated to this time and
  // we stop eviction attempts as soon as we come across a segment that we've
  // previously tried to evict but was locked.
  base::Time current_time = Now();

  lock_.AssertAcquired();
  size_t bytes_allocated_before_purging = bytes_allocated_;
  while (!segments_.empty()) {
    if (bytes_allocated_ <= limit)
      break;

    // Stop eviction attempts when the LRU segment is currently in use.
    if (segments_.front()->memory()->last_known_usage() >= current_time)
      break;

    std::pop_heap(segments_.begin(), segments_.end(), CompareMemoryUsageTime);
    scoped_refptr<MemorySegment> segment = segments_.back();
    segments_.pop_back();

    // Attempt to purge LRU segment. When successful, released the memory.
    if (segment->memory()->Purge(current_time)) {
#if defined(DISCARDABLE_SHARED_MEMORY_SHRINKING)
      size_t size = segment->memory()->mapped_size();
      DCHECK_GE(bytes_allocated_, size);
      bytes_allocated_ -= size;
      // Shrink memory segment. This will immediately release the memory to
      // the OS.
      segment->memory()->Shrink();
      DCHECK_EQ(segment->memory()->mapped_size(), 0u);
#endif
      ReleaseMemory(segment->memory());
      continue;
    }

    // Add memory segment (with updated usage timestamp) back on heap after
    // failed attempt to purge it.
    segments_.push_back(segment.get());
    std::push_heap(segments_.begin(), segments_.end(), CompareMemoryUsageTime);
  }

  if (bytes_allocated_ != bytes_allocated_before_purging)
    BytesAllocatedChanged(bytes_allocated_);
}

void HostDiscardableSharedMemoryManager::ReleaseMemory(
    base::DiscardableSharedMemory* memory) {
  lock_.AssertAcquired();

  size_t size = memory->mapped_size();
  DCHECK_GE(bytes_allocated_, size);
  bytes_allocated_ -= size;

  // This will unmap the memory segment and drop our reference. The result
  // is that the memory will be released to the OS if the child process is
  // no longer referencing it.
  // Note: We intentionally leave the segment in the |segments| vector to
  // avoid reconstructing the heap. The element will be removed from the heap
  // when its last usage time is older than all other segments.
  memory->Unmap();
  memory->Close();
}

void HostDiscardableSharedMemoryManager::BytesAllocatedChanged(
    size_t new_bytes_allocated) const {
  TRACE_COUNTER1("renderer_host", "TotalDiscardableMemoryUsage",
                 new_bytes_allocated);

  static const char kTotalDiscardableMemoryAllocatedKey[] =
      "total-discardable-memory-allocated";
  base::debug::SetCrashKeyValue(kTotalDiscardableMemoryAllocatedKey,
                                base::Uint64ToString(new_bytes_allocated));
}

base::Time HostDiscardableSharedMemoryManager::Now() const {
  return base::Time::Now();
}

void HostDiscardableSharedMemoryManager::ScheduleEnforceMemoryPolicy() {
  lock_.AssertAcquired();

  if (enforce_memory_policy_pending_)
    return;

  enforce_memory_policy_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&HostDiscardableSharedMemoryManager::EnforceMemoryPolicy,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kEnforceMemoryPolicyDelayMs));
}

}  // namespace content
