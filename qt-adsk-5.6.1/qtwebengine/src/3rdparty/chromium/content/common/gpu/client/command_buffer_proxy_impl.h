// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_
#define CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_

#include <map>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "ipc/ipc_listener.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/swap_result.h"

struct GPUCommandBufferConsoleMessage;

namespace base {
class SharedMemory;
}

namespace gpu {
struct Mailbox;
}

namespace media {
class VideoDecodeAccelerator;
class VideoEncodeAccelerator;
}

namespace content {
class GpuChannelHost;

// Client side proxy that forwards messages synchronously to a
// CommandBufferStub.
class CommandBufferProxyImpl
    : public gpu::CommandBuffer,
      public gpu::GpuControl,
      public IPC::Listener,
      public base::SupportsWeakPtr<CommandBufferProxyImpl> {
 public:
  class DeletionObserver {
   public:
    // Called during the destruction of the CommandBufferProxyImpl.
    virtual void OnWillDeleteImpl() = 0;

   protected:
    virtual ~DeletionObserver() {}
  };

  typedef base::Callback<void(
      const std::string& msg, int id)> GpuConsoleMessageCallback;

  CommandBufferProxyImpl(GpuChannelHost* channel, int route_id);
  ~CommandBufferProxyImpl() override;

  // Sends an IPC message to create a GpuVideoDecodeAccelerator. Creates and
  // returns it as an owned pointer to a media::VideoDecodeAccelerator.  Returns
  // NULL on failure to create the GpuVideoDecodeAcceleratorHost.
  // Note that the GpuVideoDecodeAccelerator may still fail to be created in
  // the GPU process, even if this returns non-NULL. In this case the VDA client
  // is notified of an error later, after Initialize().
  scoped_ptr<media::VideoDecodeAccelerator> CreateVideoDecoder();

  // Sends an IPC message to create a GpuVideoEncodeAccelerator. Creates and
  // returns it as an owned pointer to a media::VideoEncodeAccelerator.  Returns
  // NULL on failure to create the GpuVideoEncodeAcceleratorHost.
  // Note that the GpuVideoEncodeAccelerator may still fail to be created in
  // the GPU process, even if this returns non-NULL. In this case the VEA client
  // is notified of an error later, after Initialize();
  scoped_ptr<media::VideoEncodeAccelerator> CreateVideoEncoder();

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelError() override;

  // CommandBuffer implementation:
  bool Initialize() override;
  State GetLastState() override;
  int32 GetLastToken() override;
  void Flush(int32 put_offset) override;
  void OrderingBarrier(int32 put_offset) override;
  void WaitForTokenInRange(int32 start, int32 end) override;
  void WaitForGetOffsetInRange(int32 start, int32 end) override;
  void SetGetBuffer(int32 shm_id) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(size_t size,
                                                  int32* id) override;
  void DestroyTransferBuffer(int32 id) override;

  // gpu::GpuControl implementation:
  gpu::Capabilities GetCapabilities() override;
  int32 CreateImage(ClientBuffer buffer,
                    size_t width,
                    size_t height,
                    unsigned internalformat) override;
  void DestroyImage(int32 id) override;
  int32 CreateGpuMemoryBufferImage(size_t width,
                                   size_t height,
                                   unsigned internalformat,
                                   unsigned usage) override;
  uint32 InsertSyncPoint() override;
  uint32_t InsertFutureSyncPoint() override;
  void RetireSyncPoint(uint32_t sync_point) override;
  void SignalSyncPoint(uint32 sync_point,
                       const base::Closure& callback) override;
  void SignalQuery(uint32 query, const base::Closure& callback) override;
  void SetSurfaceVisible(bool visible) override;
  uint32 CreateStreamTexture(uint32 texture_id) override;
  void SetLock(base::Lock* lock) override;
  bool IsGpuChannelLost() override;

  int GetRouteID() const;
  bool ProduceFrontBuffer(const gpu::Mailbox& mailbox);
  void SetContextLostCallback(const base::Closure& callback);

  typedef base::Callback<void(const gpu::MemoryAllocation&)>
      MemoryAllocationChangedCallback;
  void SetMemoryAllocationChangedCallback(
      const MemoryAllocationChangedCallback& callback);
  void AddDeletionObserver(DeletionObserver* observer);
  void RemoveDeletionObserver(DeletionObserver* observer);

  bool EnsureBackbuffer();

  void SetOnConsoleMessageCallback(
      const GpuConsoleMessageCallback& callback);

  void SetLatencyInfo(const std::vector<ui::LatencyInfo>& latency_info);
  using SwapBuffersCompletionCallback =
      base::Callback<void(const std::vector<ui::LatencyInfo>& latency_info,
                          gfx::SwapResult result)>;
  void SetSwapBuffersCompletionCallback(
      const SwapBuffersCompletionCallback& callback);

  using UpdateVSyncParametersCallback =
      base::Callback<void(base::TimeTicks timebase, base::TimeDelta interval)>;
  void SetUpdateVSyncParametersCallback(
      const UpdateVSyncParametersCallback& callback);

  // TODO(apatrick): this is a temporary optimization while skia is calling
  // ContentGLContext::MakeCurrent prior to every GL call. It saves returning 6
  // ints redundantly when only the error is needed for the
  // CommandBufferProxyImpl implementation.
  gpu::error::Error GetLastError() override;

  GpuChannelHost* channel() const { return channel_; }

  base::SharedMemoryHandle GetSharedStateHandle() const {
    return shared_state_shm_->handle();
  }

 private:
  typedef std::map<int32, scoped_refptr<gpu::Buffer> > TransferBufferMap;
  typedef base::hash_map<uint32, base::Closure> SignalTaskMap;

  void CheckLock() {
    if (lock_)
      lock_->AssertAcquired();
  }

  // Send an IPC message over the GPU channel. This is private to fully
  // encapsulate the channel; all callers of this function must explicitly
  // verify that the context has not been lost.
  bool Send(IPC::Message* msg);

  // Message handlers:
  void OnUpdateState(const gpu::CommandBuffer::State& state);
  void OnDestroyed(gpu::error::ContextLostReason reason,
                   gpu::error::Error error);
  void OnConsoleMessage(const GPUCommandBufferConsoleMessage& message);
  void OnSetMemoryAllocation(const gpu::MemoryAllocation& allocation);
  void OnSignalSyncPointAck(uint32 id);
  void OnSwapBuffersCompleted(const std::vector<ui::LatencyInfo>& latency_info,
                              gfx::SwapResult result);
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval);

  // Try to read an updated copy of the state from shared memory.
  void TryUpdateState();

  // The shared memory area used to update state.
  gpu::CommandBufferSharedState* shared_state() const;

  base::Lock* lock_;

  // Unowned list of DeletionObservers.
  base::ObserverList<DeletionObserver> deletion_observers_;

  // The last cached state received from the service.
  State last_state_;

  // The shared memory area used to update state.
  scoped_ptr<base::SharedMemory> shared_state_shm_;

  // |*this| is owned by |*channel_| and so is always outlived by it, so using a
  // raw pointer is ok.
  GpuChannelHost* channel_;
  int route_id_;
  unsigned int flush_count_;
  int32 last_put_offset_;
  int32 last_barrier_put_offset_;

  base::Closure context_lost_callback_;

  MemoryAllocationChangedCallback memory_allocation_changed_callback_;

  GpuConsoleMessageCallback console_message_callback_;

  // Tasks to be invoked in SignalSyncPoint responses.
  uint32 next_signal_id_;
  SignalTaskMap signal_tasks_;

  gpu::Capabilities capabilities_;

  std::vector<ui::LatencyInfo> latency_info_;

  SwapBuffersCompletionCallback swap_buffers_completion_callback_;
  UpdateVSyncParametersCallback update_vsync_parameters_completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferProxyImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_
