// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/host_shared_bitmap_manager.h"

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/process_memory_dump.h"
#include "content/common/view_messages.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class BitmapData : public base::RefCountedThreadSafe<BitmapData> {
 public:
  BitmapData(base::ProcessHandle process_handle,
             size_t buffer_size)
      : process_handle(process_handle),
        buffer_size(buffer_size) {}
  base::ProcessHandle process_handle;
  scoped_ptr<base::SharedMemory> memory;
  scoped_ptr<uint8[]> pixels;
  size_t buffer_size;

 private:
  friend class base::RefCountedThreadSafe<BitmapData>;
  ~BitmapData() {}
  DISALLOW_COPY_AND_ASSIGN(BitmapData);
};

namespace {

class HostSharedBitmap : public cc::SharedBitmap {
 public:
  HostSharedBitmap(uint8* pixels,
                   scoped_refptr<BitmapData> bitmap_data,
                   const cc::SharedBitmapId& id,
                   HostSharedBitmapManager* manager)
      : SharedBitmap(pixels, id),
        bitmap_data_(bitmap_data),
        manager_(manager) {}

  ~HostSharedBitmap() override {
    if (manager_)
      manager_->FreeSharedMemoryFromMap(id());
  }

 private:
  scoped_refptr<BitmapData> bitmap_data_;
  HostSharedBitmapManager* manager_;
};

const char kMemoryAllocatorName[] = "sharedbitmap";

}  // namespace

base::LazyInstance<HostSharedBitmapManager> g_shared_memory_manager =
    LAZY_INSTANCE_INITIALIZER;

HostSharedBitmapManagerClient::HostSharedBitmapManagerClient(
    HostSharedBitmapManager* manager)
    : manager_(manager) {
}

HostSharedBitmapManagerClient::~HostSharedBitmapManagerClient() {
  for (const auto& id : owned_bitmaps_)
    manager_->ChildDeletedSharedBitmap(id);
}

void HostSharedBitmapManagerClient::AllocateSharedBitmapForChild(
    base::ProcessHandle process_handle,
    size_t buffer_size,
    const cc::SharedBitmapId& id,
    base::SharedMemoryHandle* shared_memory_handle) {
  manager_->AllocateSharedBitmapForChild(process_handle, buffer_size, id,
                                         shared_memory_handle);
  owned_bitmaps_.insert(id);
}

void HostSharedBitmapManagerClient::ChildAllocatedSharedBitmap(
    size_t buffer_size,
    const base::SharedMemoryHandle& handle,
    base::ProcessHandle process_handle,
    const cc::SharedBitmapId& id) {
  manager_->ChildAllocatedSharedBitmap(buffer_size, handle, process_handle, id);
  owned_bitmaps_.insert(id);
}

void HostSharedBitmapManagerClient::ChildDeletedSharedBitmap(
    const cc::SharedBitmapId& id) {
  manager_->ChildDeletedSharedBitmap(id);
  owned_bitmaps_.erase(id);
}

HostSharedBitmapManager::HostSharedBitmapManager() {}
HostSharedBitmapManager::~HostSharedBitmapManager() {
  DCHECK(handle_map_.empty());
}

HostSharedBitmapManager* HostSharedBitmapManager::current() {
  return g_shared_memory_manager.Pointer();
}

scoped_ptr<cc::SharedBitmap> HostSharedBitmapManager::AllocateSharedBitmap(
    const gfx::Size& size) {
  base::AutoLock lock(lock_);
  size_t bitmap_size;
  if (!cc::SharedBitmap::SizeInBytes(size, &bitmap_size))
    return scoped_ptr<cc::SharedBitmap>();

  scoped_refptr<BitmapData> data(
      new BitmapData(base::GetCurrentProcessHandle(),
                     bitmap_size));
  // Bitmaps allocated in host don't need to be shared to other processes, so
  // allocate them with new instead.
  data->pixels = scoped_ptr<uint8[]>(new uint8[bitmap_size]);

  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
  handle_map_[id] = data;
  return make_scoped_ptr(
      new HostSharedBitmap(data->pixels.get(), data, id, this));
}

scoped_ptr<cc::SharedBitmap> HostSharedBitmapManager::GetSharedBitmapFromId(
    const gfx::Size& size,
    const cc::SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  BitmapMap::iterator it = handle_map_.find(id);
  if (it == handle_map_.end())
    return scoped_ptr<cc::SharedBitmap>();

  BitmapData* data = it->second.get();

  size_t bitmap_size;
  if (!cc::SharedBitmap::SizeInBytes(size, &bitmap_size) ||
      bitmap_size > data->buffer_size)
    return scoped_ptr<cc::SharedBitmap>();

  if (data->pixels) {
    return make_scoped_ptr(
        new HostSharedBitmap(data->pixels.get(), data, id, nullptr));
  }
  if (!data->memory->memory()) {
    return scoped_ptr<cc::SharedBitmap>();
  }

  return make_scoped_ptr(new HostSharedBitmap(
      static_cast<uint8*>(data->memory->memory()), data, id, nullptr));
}

bool HostSharedBitmapManager::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(lock_);

  for (const auto& bitmap : handle_map_) {
    base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
        base::StringPrintf("%s/%s", kMemoryAllocatorName,
                           base::HexEncode(bitmap.first.name,
                                           sizeof(bitmap.first.name)).c_str()));
    if (!dump)
      return false;

    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    bitmap.second->buffer_size);
  }

  return true;
}

void HostSharedBitmapManager::ChildAllocatedSharedBitmap(
    size_t buffer_size,
    const base::SharedMemoryHandle& handle,
    base::ProcessHandle process_handle,
    const cc::SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  if (handle_map_.find(id) != handle_map_.end())
    return;
  scoped_refptr<BitmapData> data(
      new BitmapData(process_handle, buffer_size));

  handle_map_[id] = data;
#if defined(OS_WIN)
  data->memory = make_scoped_ptr(
      new base::SharedMemory(handle, false, data->process_handle));
#else
  data->memory =
      make_scoped_ptr(new base::SharedMemory(handle, false));
#endif
 data->memory->Map(data->buffer_size);
 data->memory->Close();
}

void HostSharedBitmapManager::AllocateSharedBitmapForChild(
    base::ProcessHandle process_handle,
    size_t buffer_size,
    const cc::SharedBitmapId& id,
    base::SharedMemoryHandle* shared_memory_handle) {
  base::AutoLock lock(lock_);
  if (handle_map_.find(id) != handle_map_.end()) {
    *shared_memory_handle = base::SharedMemory::NULLHandle();
    return;
  }
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory);
  if (!shared_memory->CreateAndMapAnonymous(buffer_size)) {
    LOG(ERROR) << "Cannot create shared memory buffer";
    *shared_memory_handle = base::SharedMemory::NULLHandle();
    return;
  }

  scoped_refptr<BitmapData> data(
      new BitmapData(process_handle, buffer_size));
  data->memory = shared_memory.Pass();

  handle_map_[id] = data;
  if (!data->memory->ShareToProcess(process_handle, shared_memory_handle)) {
    LOG(ERROR) << "Cannot share shared memory buffer";
    *shared_memory_handle = base::SharedMemory::NULLHandle();
    return;
  }
 data->memory->Close();
}

void HostSharedBitmapManager::ChildDeletedSharedBitmap(
    const cc::SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  handle_map_.erase(id);
}

size_t HostSharedBitmapManager::AllocatedBitmapCount() const {
  base::AutoLock lock(lock_);
  return handle_map_.size();
}

void HostSharedBitmapManager::FreeSharedMemoryFromMap(
    const cc::SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  handle_map_.erase(id);
}

}  // namespace content
