// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_H_
#define CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_H_

#include <deque>
#include <string>
#include <vector>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"

namespace base {
class WaitableEvent;
}

namespace gfx {
class GLFence;
class GLShareGroup;
}

namespace gpu {
class SyncPointManager;
union ValueState;
namespace gles2 {
class MailboxManager;
class ProgramCache;
class ShaderTranslatorCache;
}
}

namespace IPC {
class AttachmentBroker;
struct ChannelHandle;
class SyncChannel;
}

struct GPUCreateCommandBufferConfig;

namespace content {
class GpuChannel;
class GpuMemoryBufferFactory;
class GpuWatchdog;
class MessageRouter;

// A GpuChannelManager is a thread responsible for issuing rendering commands
// managing the lifetimes of GPU channels and forwarding IPC requests from the
// browser process to them based on the corresponding renderer ID.
class CONTENT_EXPORT GpuChannelManager : public IPC::Listener,
                          public IPC::Sender {
 public:
  // |broker| must outlive GpuChannelManager and any channels it creates.
  GpuChannelManager(MessageRouter* router,
                    GpuWatchdog* watchdog,
                    base::SingleThreadTaskRunner* io_task_runner,
                    base::WaitableEvent* shutdown_event,
                    IPC::SyncChannel* channel,
                    IPC::AttachmentBroker* broker,
                    GpuMemoryBufferFactory* gpu_memory_buffer_factory);
  ~GpuChannelManager() override;

  // Remove the channel for a particular renderer.
  void RemoveChannel(int client_id);

  // Listener overrides.
  bool OnMessageReceived(const IPC::Message& msg) override;

  // Sender overrides.
  bool Send(IPC::Message* msg) override;

  bool HandleMessagesScheduled();
  uint64 MessagesProcessed();

  void LoseAllContexts();

  int GenerateRouteID();
  void AddRoute(int32 routing_id, IPC::Listener* listener);
  void RemoveRoute(int32 routing_id);

  gpu::gles2::ProgramCache* program_cache();
  gpu::gles2::ShaderTranslatorCache* shader_translator_cache();

  gpu::gles2::MailboxManager* mailbox_manager() { return mailbox_manager_.get(); }

  GpuMemoryManager* gpu_memory_manager() { return &gpu_memory_manager_; }

  GpuChannel* LookupChannel(int32 client_id);

  gpu::SyncPointManager* sync_point_manager() {
    return sync_point_manager_.get();
  }

  typedef base::hash_map<uint32, gfx::GLFence*> SyncPointGLFences;
  SyncPointGLFences sync_point_gl_fences_;

  gfx::GLSurface* GetDefaultOffscreenSurface();

  GpuMemoryBufferFactory* gpu_memory_buffer_factory() {
    return gpu_memory_buffer_factory_;
  }

 private:
  typedef base::ScopedPtrHashMap<int, scoped_ptr<GpuChannel>> GpuChannelMap;

  // Message handlers.
  void OnEstablishChannel(int client_id,
                          bool share_context,
                          bool allow_future_sync_points);
  void OnCloseChannel(const IPC::ChannelHandle& channel_handle);
  void OnVisibilityChanged(
      int32 render_view_id, int32 client_id, bool visible);
  void OnCreateViewCommandBuffer(
      const gfx::GLSurfaceHandle& window,
      int32 render_view_id,
      int32 client_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32 route_id);
  void OnLoadedShader(std::string shader);
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id, int client_id);
  void DestroyGpuMemoryBufferOnIO(gfx::GpuMemoryBufferId id, int client_id);
  void OnDestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                int client_id,
                                int32 sync_point);

  void OnRelinquishResources();
  void OnResourcesRelinquished();

  void OnUpdateValueState(int client_id,
                          unsigned int target,
                          const gpu::ValueState& state);

  void OnLoseAllContexts();
  void CheckRelinquishGpuResources();

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  base::WaitableEvent* shutdown_event_;

  // Used to send and receive IPC messages from the browser process.
  MessageRouter* const router_;

  // These objects manage channels to individual renderer processes there is
  // one channel for each renderer process that has connected to this GPU
  // process.
  GpuChannelMap gpu_channels_;
  scoped_refptr<gfx::GLShareGroup> share_group_;
  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;
  GpuMemoryManager gpu_memory_manager_;
  GpuWatchdog* watchdog_;
  scoped_refptr<gpu::SyncPointManager> sync_point_manager_;
  scoped_ptr<gpu::gles2::ProgramCache> program_cache_;
  scoped_refptr<gpu::gles2::ShaderTranslatorCache> shader_translator_cache_;
  scoped_refptr<gfx::GLSurface> default_offscreen_surface_;
  GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;
  IPC::SyncChannel* channel_;
  bool relinquish_resources_pending_;
  // Must outlive this instance of GpuChannelManager.
  IPC::AttachmentBroker* attachment_broker_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to Controller are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<GpuChannelManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelManager);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_H_
