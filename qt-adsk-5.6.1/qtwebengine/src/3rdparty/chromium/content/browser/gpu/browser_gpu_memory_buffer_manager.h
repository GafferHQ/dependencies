// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_
#define CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"

namespace content {

class CONTENT_EXPORT BrowserGpuMemoryBufferManager
    : public gpu::GpuMemoryBufferManager,
      public base::trace_event::MemoryDumpProvider {
 public:
  typedef base::Callback<void(const gfx::GpuMemoryBufferHandle& handle)>
      AllocationCallback;

  explicit BrowserGpuMemoryBufferManager(int gpu_client_id);
  ~BrowserGpuMemoryBufferManager() override;

  static BrowserGpuMemoryBufferManager* current();

  static uint32 GetImageTextureTarget(gfx::GpuMemoryBuffer::Format format,
                                      gfx::GpuMemoryBuffer::Usage usage);

  // Overridden from gpu::GpuMemoryBufferManager:
  scoped_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage) override;
  gfx::GpuMemoryBuffer* GpuMemoryBufferFromClientBuffer(
      ClientBuffer buffer) override;
  void SetDestructionSyncPoint(gfx::GpuMemoryBuffer* buffer,
                               uint32 sync_point) override;

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;

  // Virtual for testing.
  virtual scoped_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBufferForScanout(
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      int32 surface_id);

  void AllocateGpuMemoryBufferForChildProcess(
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage,
      base::ProcessHandle child_process_handle,
      int child_client_id,
      const AllocationCallback& callback);
  void ChildProcessDeletedGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      base::ProcessHandle child_process_handle,
      int child_client_id,
      uint32 sync_point);
  void ProcessRemoved(base::ProcessHandle process_handle, int client_id);

 private:
  struct BufferInfo {
    BufferInfo()
        : type(gfx::EMPTY_BUFFER),
          format(gfx::GpuMemoryBuffer::RGBA_8888),
          usage(gfx::GpuMemoryBuffer::MAP),
          gpu_host_id(0) {}
    BufferInfo(const gfx::Size& size,
               gfx::GpuMemoryBufferType type,
               gfx::GpuMemoryBuffer::Format format,
               gfx::GpuMemoryBuffer::Usage usage,
               int gpu_host_id)
        : size(size),
          type(type),
          format(format),
          usage(usage),
          gpu_host_id(gpu_host_id) {}

    gfx::Size size;
    gfx::GpuMemoryBufferType type;
    gfx::GpuMemoryBuffer::Format format;
    gfx::GpuMemoryBuffer::Usage usage;
    int gpu_host_id;
  };
  struct AllocateGpuMemoryBufferRequest;

  scoped_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBufferForSurface(
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage,
      int32 surface_id);
  bool IsGpuMemoryBufferConfigurationSupported(
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage) const;
  void AllocateGpuMemoryBufferForSurfaceOnIO(
      AllocateGpuMemoryBufferRequest* request);
  void GpuMemoryBufferAllocatedForSurfaceOnIO(
      AllocateGpuMemoryBufferRequest* request,
      const gfx::GpuMemoryBufferHandle& handle);
  void AllocateGpuMemoryBufferOnIO(gfx::GpuMemoryBufferId id,
                                   const gfx::Size& size,
                                   gfx::GpuMemoryBuffer::Format format,
                                   gfx::GpuMemoryBuffer::Usage usage,
                                   int client_id,
                                   int surface_id,
                                   bool reused_gpu_process,
                                   const AllocationCallback& callback);
  void GpuMemoryBufferAllocatedOnIO(gfx::GpuMemoryBufferId id,
                                    int client_id,
                                    int surface_id,
                                    int gpu_host_id,
                                    bool reused_gpu_process,
                                    const AllocationCallback& callback,
                                    const gfx::GpuMemoryBufferHandle& handle);
  void DestroyGpuMemoryBufferOnIO(gfx::GpuMemoryBufferId id,
                                  int client_id,
                                  uint32 sync_point);

  const gfx::GpuMemoryBufferType factory_type_;
  const std::vector<GpuMemoryBufferFactory::Configuration>
      supported_configurations_;
  const int gpu_client_id_;

  // The GPU process host ID. This should only be accessed on the IO thread.
  int gpu_host_id_;

  // Stores info about buffers for all clients. This should only be accessed
  // on the IO thread.
  using BufferMap = base::hash_map<gfx::GpuMemoryBufferId, BufferInfo>;
  using ClientMap = base::hash_map<int, BufferMap>;
  ClientMap clients_;

  // This should only be accessed on the IO thread.
  base::WeakPtrFactory<BrowserGpuMemoryBufferManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserGpuMemoryBufferManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_
