// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_SHARED_BITMAP_MANAGER_H_
#define CONTENT_CHILD_CHILD_SHARED_BITMAP_MANAGER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "content/child/thread_safe_sender.h"

namespace content {

class SharedMemoryBitmap : public cc::SharedBitmap {
 public:
  base::SharedMemory* shared_memory() { return shared_memory_; }

 protected:
  SharedMemoryBitmap(uint8* pixels,
                     const cc::SharedBitmapId& id,
                     base::SharedMemory* shared_memory);

  base::SharedMemory* shared_memory_;
};

class ChildSharedBitmapManager : public cc::SharedBitmapManager {
 public:
  ChildSharedBitmapManager(scoped_refptr<ThreadSafeSender> sender);
  ~ChildSharedBitmapManager() override;

  // cc::SharedBitmapManager implementation.
  scoped_ptr<cc::SharedBitmap> AllocateSharedBitmap(
      const gfx::Size& size) override;
  scoped_ptr<cc::SharedBitmap> GetSharedBitmapFromId(
      const gfx::Size&,
      const cc::SharedBitmapId&) override;

  scoped_ptr<cc::SharedBitmap> GetBitmapForSharedMemory(
      base::SharedMemory* mem);
  scoped_ptr<SharedMemoryBitmap> AllocateSharedMemoryBitmap(
      const gfx::Size& size);

 private:
  scoped_refptr<ThreadSafeSender> sender_;

  DISALLOW_COPY_AND_ASSIGN(ChildSharedBitmapManager);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_SHARED_BITMAP_MANAGER_H_
