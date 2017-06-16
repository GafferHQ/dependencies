// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_shared_bitmap_manager.h"

#include "base/debug/alias.h"
#include "base/process/memory.h"
#include "base/process/process_metrics.h"
#include "content/child/child_thread_impl.h"
#include "content/common/child_process_messages.h"
#include "ui/gfx/geometry/size.h"

namespace content {

namespace {

class ChildSharedBitmap : public SharedMemoryBitmap {
 public:
  ChildSharedBitmap(scoped_refptr<ThreadSafeSender> sender,
                    base::SharedMemory* shared_memory,
                    const cc::SharedBitmapId& id)
      : SharedMemoryBitmap(static_cast<uint8*>(shared_memory->memory()),
                           id,
                           shared_memory),
        sender_(sender) {}

  ChildSharedBitmap(scoped_refptr<ThreadSafeSender> sender,
                    scoped_ptr<base::SharedMemory> shared_memory_holder,
                    const cc::SharedBitmapId& id)
      : ChildSharedBitmap(sender, shared_memory_holder.get(), id) {
    shared_memory_holder_ = shared_memory_holder.Pass();
  }

  ~ChildSharedBitmap() override {
    sender_->Send(new ChildProcessHostMsg_DeletedSharedBitmap(id()));
  }

 private:
  scoped_refptr<ThreadSafeSender> sender_;
  scoped_ptr<base::SharedMemory> shared_memory_holder_;
};

// Collect extra information for debugging bitmap creation failures.
void CollectMemoryUsageAndDie(const gfx::Size& size, size_t alloc_size) {
#if defined(OS_WIN)
  int width = size.width();
  int height = size.height();
  DWORD last_error = GetLastError();

  scoped_ptr<base::ProcessMetrics> metrics(
      base::ProcessMetrics::CreateProcessMetrics(
          base::GetCurrentProcessHandle()));

  size_t private_bytes = 0;
  size_t shared_bytes = 0;
  metrics->GetMemoryBytes(&private_bytes, &shared_bytes);

  base::debug::Alias(&width);
  base::debug::Alias(&height);
  base::debug::Alias(&last_error);
  base::debug::Alias(&private_bytes);
  base::debug::Alias(&shared_bytes);
#endif
  base::TerminateBecauseOutOfMemory(alloc_size);
}

}  // namespace

SharedMemoryBitmap::SharedMemoryBitmap(uint8* pixels,
                                       const cc::SharedBitmapId& id,
                                       base::SharedMemory* shared_memory)
    : SharedBitmap(pixels, id), shared_memory_(shared_memory) {
}

ChildSharedBitmapManager::ChildSharedBitmapManager(
    scoped_refptr<ThreadSafeSender> sender)
    : sender_(sender) {
}

ChildSharedBitmapManager::~ChildSharedBitmapManager() {}

scoped_ptr<cc::SharedBitmap> ChildSharedBitmapManager::AllocateSharedBitmap(
    const gfx::Size& size) {
  scoped_ptr<SharedMemoryBitmap> bitmap = AllocateSharedMemoryBitmap(size);
#if defined(OS_POSIX)
  // Close file descriptor to avoid running out.
  if (bitmap)
    bitmap->shared_memory()->Close();
#endif
  return bitmap.Pass();
}

scoped_ptr<SharedMemoryBitmap>
ChildSharedBitmapManager::AllocateSharedMemoryBitmap(const gfx::Size& size) {
  TRACE_EVENT2("renderer",
               "ChildSharedBitmapManager::AllocateSharedMemoryBitmap", "width",
               size.width(), "height", size.height());
  size_t memory_size;
  if (!cc::SharedBitmap::SizeInBytes(size, &memory_size))
    return scoped_ptr<SharedMemoryBitmap>();
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
  scoped_ptr<base::SharedMemory> memory;
#if defined(OS_POSIX)
  base::SharedMemoryHandle handle;
  sender_->Send(new ChildProcessHostMsg_SyncAllocateSharedBitmap(
      memory_size, id, &handle));
  memory = make_scoped_ptr(new base::SharedMemory(handle, false));
  if (!memory->Map(memory_size))
    CollectMemoryUsageAndDie(size, memory_size);
#else
  memory = ChildThreadImpl::AllocateSharedMemory(memory_size, sender_.get());
  if (!memory)
    CollectMemoryUsageAndDie(size, memory_size);

  if (!memory->Map(memory_size))
    CollectMemoryUsageAndDie(size, memory_size);

  base::SharedMemoryHandle handle_to_send = memory->handle();
  sender_->Send(new ChildProcessHostMsg_AllocatedSharedBitmap(
      memory_size, handle_to_send, id));
#endif
  return make_scoped_ptr(new ChildSharedBitmap(sender_, memory.Pass(), id));
}

scoped_ptr<cc::SharedBitmap> ChildSharedBitmapManager::GetSharedBitmapFromId(
    const gfx::Size&,
    const cc::SharedBitmapId&) {
  NOTREACHED();
  return scoped_ptr<cc::SharedBitmap>();
}

scoped_ptr<cc::SharedBitmap> ChildSharedBitmapManager::GetBitmapForSharedMemory(
    base::SharedMemory* mem) {
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
  base::SharedMemoryHandle handle_to_send = mem->handle();
#if defined(OS_POSIX)
  if (!mem->ShareToProcess(base::GetCurrentProcessHandle(), &handle_to_send))
    return scoped_ptr<cc::SharedBitmap>();
#endif
  sender_->Send(new ChildProcessHostMsg_AllocatedSharedBitmap(
      mem->mapped_size(), handle_to_send, id));

  return make_scoped_ptr(new ChildSharedBitmap(sender_, mem, id));
}

}  // namespace content
