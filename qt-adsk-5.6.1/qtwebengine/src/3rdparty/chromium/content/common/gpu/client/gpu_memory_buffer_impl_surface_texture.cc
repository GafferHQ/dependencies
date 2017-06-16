// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_surface_texture.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "content/common/android/surface_texture_manager.h"
#include "ui/gl/gl_bindings.h"

namespace content {
namespace {

int WindowFormat(gfx::GpuMemoryBuffer::Format format) {
  switch (format) {
    case gfx::GpuMemoryBuffer::RGBA_8888:
      return WINDOW_FORMAT_RGBA_8888;
    case gfx::GpuMemoryBuffer::ATC:
    case gfx::GpuMemoryBuffer::ATCIA:
    case gfx::GpuMemoryBuffer::DXT1:
    case gfx::GpuMemoryBuffer::DXT5:
    case gfx::GpuMemoryBuffer::ETC1:
    case gfx::GpuMemoryBuffer::R_8:
    case gfx::GpuMemoryBuffer::RGBA_4444:
    case gfx::GpuMemoryBuffer::RGBX_8888:
    case gfx::GpuMemoryBuffer::BGRA_8888:
    case gfx::GpuMemoryBuffer::YUV_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

}  // namespace

GpuMemoryBufferImplSurfaceTexture::GpuMemoryBufferImplSurfaceTexture(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback,
    ANativeWindow* native_window)
    : GpuMemoryBufferImpl(id, size, format, callback),
      native_window_(native_window),
      stride_(0) {
}

GpuMemoryBufferImplSurfaceTexture::~GpuMemoryBufferImplSurfaceTexture() {
  ANativeWindow_release(native_window_);
}

// static
scoped_ptr<GpuMemoryBufferImpl>
GpuMemoryBufferImplSurfaceTexture::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback) {
  ANativeWindow* native_window = SurfaceTextureManager::GetInstance()->
      AcquireNativeWidgetForSurfaceTexture(handle.id);
  if (!native_window)
    return scoped_ptr<GpuMemoryBufferImpl>();

  ANativeWindow_setBuffersGeometry(
      native_window, size.width(), size.height(), WindowFormat(format));

  return make_scoped_ptr<GpuMemoryBufferImpl>(
      new GpuMemoryBufferImplSurfaceTexture(
          handle.id, size, format, callback, native_window));
}

bool GpuMemoryBufferImplSurfaceTexture::Map(void** data) {
  TRACE_EVENT0("gpu", "GpuMemoryBufferImplSurfaceTexture::Map");

  DCHECK(!mapped_);
  DCHECK(native_window_);
  ANativeWindow_Buffer buffer;
  int status = ANativeWindow_lock(native_window_, &buffer, NULL);
  if (status) {
    VLOG(1) << "ANativeWindow_lock failed with error code: " << status;
    return false;
  }

  DCHECK_LE(size_.width(), buffer.stride);
  size_t row_size_in_bytes = 0;
  bool valid_row_size =
      RowSizeInBytes(buffer.stride, format_, 0, &row_size_in_bytes);
  DCHECK(valid_row_size);

  stride_ = row_size_in_bytes;
  mapped_ = true;
  *data = buffer.bits;
  return true;
}

void GpuMemoryBufferImplSurfaceTexture::Unmap() {
  TRACE_EVENT0("gpu", "GpuMemoryBufferImplSurfaceTexture::Unmap");

  DCHECK(mapped_);
  ANativeWindow_unlockAndPost(native_window_);
  mapped_ = false;
}

void GpuMemoryBufferImplSurfaceTexture::GetStride(int* stride) const {
  *stride = stride_;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplSurfaceTexture::GetHandle()
    const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SURFACE_TEXTURE_BUFFER;
  handle.id = id_;
  return handle;
}

}  // namespace content
