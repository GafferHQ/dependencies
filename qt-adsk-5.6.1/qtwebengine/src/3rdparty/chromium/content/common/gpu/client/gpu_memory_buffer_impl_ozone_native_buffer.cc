// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_ozone_native_buffer.h"

#include "ui/ozone/public/surface_factory_ozone.h"

namespace content {

GpuMemoryBufferImplOzoneNativeBuffer::GpuMemoryBufferImplOzoneNativeBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback)
    : GpuMemoryBufferImpl(id, size, format, callback) {
}

GpuMemoryBufferImplOzoneNativeBuffer::~GpuMemoryBufferImplOzoneNativeBuffer() {
}

// static
scoped_ptr<GpuMemoryBufferImpl>
GpuMemoryBufferImplOzoneNativeBuffer::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    Format format,
    Usage usage,
    const DestructionCallback& callback) {
  return make_scoped_ptr<GpuMemoryBufferImpl>(
      new GpuMemoryBufferImplOzoneNativeBuffer(
          handle.id, size, format, callback));
}

bool GpuMemoryBufferImplOzoneNativeBuffer::Map(void** data) {
  NOTREACHED();
  return false;
}

void GpuMemoryBufferImplOzoneNativeBuffer::Unmap() {
  NOTREACHED();
}

void GpuMemoryBufferImplOzoneNativeBuffer::GetStride(int* stride) const {
  NOTREACHED();
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplOzoneNativeBuffer::GetHandle()
    const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::OZONE_NATIVE_BUFFER;
  handle.id = id_;
  return handle;
}

}  // namespace content
