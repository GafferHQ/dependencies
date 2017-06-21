// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_shared_memory.h"

#include "base/bind.h"
#include "base/numerics/safe_math.h"
#include "ui/gl/gl_bindings.h"

namespace content {
namespace {

void Noop(uint32 sync_point) {
}

}  // namespace

GpuMemoryBufferImplSharedMemory::GpuMemoryBufferImplSharedMemory(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback,
    scoped_ptr<base::SharedMemory> shared_memory)
    : GpuMemoryBufferImpl(id, size, format, callback),
      shared_memory_(shared_memory.Pass()) {
  DCHECK(IsFormatSupported(format));
  DCHECK(IsSizeValidForFormat(size, format));
}

GpuMemoryBufferImplSharedMemory::~GpuMemoryBufferImplSharedMemory() {
}

// static
scoped_ptr<GpuMemoryBufferImpl> GpuMemoryBufferImplSharedMemory::Create(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format) {
  size_t buffer_size = 0u;
  if (!BufferSizeInBytes(size, format, &buffer_size))
    return scoped_ptr<GpuMemoryBufferImpl>();

  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());
  if (!shared_memory->CreateAndMapAnonymous(buffer_size))
    return scoped_ptr<GpuMemoryBufferImpl>();

  return make_scoped_ptr(new GpuMemoryBufferImplSharedMemory(
      id, size, format, base::Bind(&Noop), shared_memory.Pass()));
}

// static
gfx::GpuMemoryBufferHandle
GpuMemoryBufferImplSharedMemory::AllocateForChildProcess(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    base::ProcessHandle child_process) {
  size_t buffer_size = 0u;
  if (!BufferSizeInBytes(size, format, &buffer_size))
    return gfx::GpuMemoryBufferHandle();

  base::SharedMemory shared_memory;
  if (!shared_memory.CreateAnonymous(buffer_size))
    return gfx::GpuMemoryBufferHandle();

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = id;
  shared_memory.GiveToProcess(child_process, &handle.handle);
  return handle;
}

// static
scoped_ptr<GpuMemoryBufferImpl>
GpuMemoryBufferImplSharedMemory::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback) {
  if (!base::SharedMemory::IsHandleValid(handle.handle))
    return scoped_ptr<GpuMemoryBufferImpl>();

  size_t buffer_size = 0u;
  if (!BufferSizeInBytes(size, format, &buffer_size))
    return scoped_ptr<GpuMemoryBufferImpl>();

  scoped_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle.handle, false));
  if (!shared_memory->Map(buffer_size))
    return scoped_ptr<GpuMemoryBufferImpl>();

  return make_scoped_ptr<GpuMemoryBufferImpl>(
      new GpuMemoryBufferImplSharedMemory(
          handle.id,
          size,
          format,
          callback,
          shared_memory.Pass()));
}

// static
bool GpuMemoryBufferImplSharedMemory::IsFormatSupported(Format format) {
  switch (format) {
    case ATC:
    case ATCIA:
    case DXT1:
    case DXT5:
    case ETC1:
    case R_8:
    case RGBA_4444:
    case RGBA_8888:
    case BGRA_8888:
    case YUV_420:
      return true;
    case RGBX_8888:
      return false;
  }

  NOTREACHED();
  return false;
}

// static
bool GpuMemoryBufferImplSharedMemory::IsUsageSupported(Usage usage) {
  switch (usage) {
    case MAP:
    case PERSISTENT_MAP:
      return true;
    case SCANOUT:
      return false;
  }
  NOTREACHED();
  return false;
}

// static
bool GpuMemoryBufferImplSharedMemory::IsSizeValidForFormat(
    const gfx::Size& size,
    Format format) {
  switch (format) {
    case ATC:
    case ATCIA:
    case DXT1:
    case DXT5:
    case ETC1:
      // Compressed images must have a width and height that's evenly divisible
      // by the block size.
      return size.width() % 4 == 0 && size.height() % 4 == 0;
    case R_8:
    case RGBA_4444:
    case RGBA_8888:
    case BGRA_8888:
    case RGBX_8888:
      return true;
    case YUV_420: {
      size_t num_planes = NumberOfPlanesForGpuMemoryBufferFormat(format);
      for (size_t i = 0; i < num_planes; ++i) {
        size_t factor = SubsamplingFactor(format, i);
        if (size.width() % factor || size.height() % factor)
          return false;
      }
      return true;
    }
  }

  NOTREACHED();
  return false;
}

bool GpuMemoryBufferImplSharedMemory::Map(void** data) {
  DCHECK(!mapped_);
  size_t offset = 0;
  size_t num_planes = NumberOfPlanesForGpuMemoryBufferFormat(format_);
  for (size_t i = 0; i < num_planes; ++i) {
    data[i] = reinterpret_cast<uint8*>(shared_memory_->memory()) + offset;
    size_t row_size_in_bytes = 0;
    bool valid_row_size =
        RowSizeInBytes(size_.width(), format_, i, &row_size_in_bytes);
    DCHECK(valid_row_size);
    offset +=
        row_size_in_bytes * (size_.height() / SubsamplingFactor(format_, i));
  }
  mapped_ = true;
  return true;
}

void GpuMemoryBufferImplSharedMemory::Unmap() {
  DCHECK(mapped_);
  mapped_ = false;
}

void GpuMemoryBufferImplSharedMemory::GetStride(int* stride) const {
  size_t num_planes = NumberOfPlanesForGpuMemoryBufferFormat(format_);
  for (size_t i = 0; i < num_planes; ++i) {
    size_t row_size_in_bytes = 0;
    bool valid_row_size =
        RowSizeInBytes(size_.width(), format_, i, &row_size_in_bytes);
    DCHECK(valid_row_size);
    stride[i] = row_size_in_bytes;
  }
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplSharedMemory::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = id_;
  handle.handle = shared_memory_->handle();
  return handle;
}

}  // namespace content
