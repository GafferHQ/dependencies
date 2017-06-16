// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/surface/transport_dib.h"

#include <sys/stat.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "skia/ext/platform_canvas.h"

TransportDIB::TransportDIB()
    : size_(0) {
}

TransportDIB::TransportDIB(TransportDIB::Handle dib)
    : shared_memory_(dib, false /* read write */),
      size_(0) {
}

TransportDIB::~TransportDIB() {
}

// static
TransportDIB* TransportDIB::Create(size_t size, uint32 sequence_num) {
  TransportDIB* dib = new TransportDIB;
  if (!dib->shared_memory_.CreateAndMapAnonymous(size)) {
    delete dib;
    return NULL;
  }

  dib->size_ = size;
  return dib;
}

// static
TransportDIB* TransportDIB::Map(Handle handle) {
  scoped_ptr<TransportDIB> dib(CreateWithHandle(handle));
  if (!dib->Map())
    return NULL;
  return dib.release();
}

// static
TransportDIB* TransportDIB::CreateWithHandle(Handle handle) {
  return new TransportDIB(handle);
}

// static
bool TransportDIB::is_valid_handle(Handle dib) {
  return base::SharedMemory::IsHandleValid(dib);
}

skia::PlatformCanvas* TransportDIB::GetPlatformCanvas(int w, int h) {
  if ((!memory() && !Map()) || !VerifyCanvasSize(w, h))
    return NULL;
  return skia::CreatePlatformCanvas(w, h, true,
                                    reinterpret_cast<uint8_t*>(memory()),
                                    skia::RETURN_NULL_ON_FAILURE);
}

bool TransportDIB::Map() {
  if (!is_valid_handle(shared_memory_.handle()))
    return false;
#if defined(OS_ANDROID)
  if (!shared_memory_.Map(0))
    return false;
  size_ = shared_memory_.mapped_size();
#else
  if (memory())
    return true;

  int size = base::SharedMemory::GetSizeFromSharedMemoryHandle(
      shared_memory_.handle());
  if (size == -1 || !shared_memory_.Map(size))
    return false;

  size_ = size;
#endif
  return true;
}

void* TransportDIB::memory() const {
  return shared_memory_.memory();
}
