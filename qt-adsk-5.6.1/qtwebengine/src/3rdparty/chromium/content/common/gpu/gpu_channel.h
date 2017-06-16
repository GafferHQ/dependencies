// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_CHANNEL_H_
#define CONTENT_COMMON_GPU_GPU_CHANNEL_H_

#include <deque>
#include <string>

#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_result_codes.h"
#include "content/common/message_router.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"
#include "ipc/ipc_sync_channel.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gpu_preference.h"

struct GPUCreateCommandBufferConfig;

namespace base {
class WaitableEvent;
}

namespace gpu {
class PreemptionFlag;
union ValueState;
class ValueStateMap;
namespace gles2 {
class SubscriptionRefSet;
}
}

namespace IPC {
class AttachmentBroker;
class MessageFilter;
}

namespace content {
class GpuChannelManager;
class GpuChannelMessageFilter;
class GpuJpegDecodeAccelerator;
class GpuWatchdog;

// Encapsulates an IPC channel between the GPU process and one renderer
// process. On the renderer side there's a corresponding GpuChannelHost.
class GpuChannel : public IPC::Listener,
                   public IPC::Sender,
                   public gpu::gles2::SubscriptionRefSet::Observer,
                   public base::trace_event::MemoryDumpProvider {
 public:
  // Takes ownership of the renderer process handle.
  GpuChannel(GpuChannelManager* gpu_channel_manager,
             GpuWatchdog* watchdog,
             gfx::GLShareGroup* share_group,
             gpu::gles2::MailboxManager* mailbox_manager,
             int client_id,
             bool software,
             bool allow_future_sync_points);
  ~GpuChannel() override;

  void Init(base::SingleThreadTaskRunner* io_task_runner,
            base::WaitableEvent* shutdown_event,
            IPC::AttachmentBroker* broker);

  // Get the GpuChannelManager that owns this channel.
  GpuChannelManager* gpu_channel_manager() const {
    return gpu_channel_manager_;
  }

  // Returns the name of the associated IPC channel.
  std::string GetChannelName();

#if defined(OS_POSIX)
  base::ScopedFD TakeRendererFileDescriptor();
#endif  // defined(OS_POSIX)

  base::ProcessId renderer_pid() const { return channel_->GetPeerPID(); }

  int client_id() const { return client_id_; }

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() const {
    return io_task_runner_;
  }

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelError() override;

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  // Requeue the message that is currently being processed to the beginning of
  // the queue. Used when the processing of a message gets aborted because of
  // unscheduling conditions.
  void RequeueMessage();

  // SubscriptionRefSet::Observer implementation
  void OnAddSubscription(unsigned int target) override;
  void OnRemoveSubscription(unsigned int target) override;

  // This is called when a command buffer transitions from the unscheduled
  // state to the scheduled state, which potentially means the channel
  // transitions from the unscheduled to the scheduled state. When this occurs
  // deferred IPC messaged are handled.
  void OnScheduled();

  // This is called when a command buffer transitions between scheduled and
  // descheduled states. When any stub is descheduled, we stop preempting
  // other channels.
  void StubSchedulingChanged(bool scheduled);

  CreateCommandBufferResult CreateViewCommandBuffer(
      const gfx::GLSurfaceHandle& window,
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32 route_id);

  gfx::GLShareGroup* share_group() const { return share_group_.get(); }

  GpuCommandBufferStub* LookupCommandBuffer(int32 route_id);

  void LoseAllContexts();
  void MarkAllContextsLost();

  // Called to add a listener for a particular message routing ID.
  // Returns true if succeeded.
  bool AddRoute(int32 route_id, IPC::Listener* listener);

  // Called to remove a listener for a particular message routing ID.
  void RemoveRoute(int32 route_id);

  gpu::PreemptionFlag* GetPreemptionFlag();

  bool handle_messages_scheduled() const { return handle_messages_scheduled_; }
  uint64 messages_processed() const { return messages_processed_; }

  // If |preemption_flag->IsSet()|, any stub on this channel
  // should stop issuing GL commands. Setting this to NULL stops deferral.
  void SetPreemptByFlag(
      scoped_refptr<gpu::PreemptionFlag> preemption_flag);

  void CacheShader(const std::string& key, const std::string& shader);

  void AddFilter(IPC::MessageFilter* filter);
  void RemoveFilter(IPC::MessageFilter* filter);

  uint64 GetMemoryUsage();

  scoped_refptr<gfx::GLImage> CreateImageForGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      uint32 internalformat);

  bool allow_future_sync_points() const { return allow_future_sync_points_; }

  void HandleUpdateValueState(unsigned int target,
                              const gpu::ValueState& state);

  // Visible for testing.
  const gpu::ValueStateMap* pending_valuebuffer_state() const {
    return pending_valuebuffer_state_.get();
  }

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend class GpuChannelMessageFilter;

  void OnDestroy();

  bool OnControlMessageReceived(const IPC::Message& msg);

  void HandleMessage();

  // Message handlers.
  void OnCreateOffscreenCommandBuffer(
      const gfx::Size& size,
      const GPUCreateCommandBufferConfig& init_params,
      int32 route_id,
      bool* succeeded);
  void OnDestroyCommandBuffer(int32 route_id);
  void OnCreateJpegDecoder(int32 route_id, IPC::Message* reply_msg);

  // Decrement the count of unhandled IPC messages and defer preemption.
  void MessageProcessed();

  // The lifetime of objects of this class is managed by a GpuChannelManager.
  // The GpuChannelManager destroy all the GpuChannels that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuChannelManager* gpu_channel_manager_;

  scoped_ptr<IPC::SyncChannel> channel_;

  uint64 messages_processed_;

  // Whether the processing of IPCs on this channel is stalled and we should
  // preempt other GpuChannels.
  scoped_refptr<gpu::PreemptionFlag> preempting_flag_;

  // If non-NULL, all stubs on this channel should stop processing GL
  // commands (via their GpuScheduler) when preempted_flag_->IsSet()
  scoped_refptr<gpu::PreemptionFlag> preempted_flag_;

  std::deque<IPC::Message*> deferred_messages_;

  // The id of the client who is on the other side of the channel.
  int client_id_;

  // Uniquely identifies the channel within this GPU process.
  std::string channel_id_;

  // Used to implement message routing functionality to CommandBuffer objects
  MessageRouter router_;

  // The share group that all contexts associated with a particular renderer
  // process use.
  scoped_refptr<gfx::GLShareGroup> share_group_;

  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;

  scoped_refptr<gpu::gles2::SubscriptionRefSet> subscription_ref_set_;

  scoped_refptr<gpu::ValueStateMap> pending_valuebuffer_state_;

  typedef IDMap<GpuCommandBufferStub, IDMapOwnPointer> StubMap;
  StubMap stubs_;

  scoped_ptr<GpuJpegDecodeAccelerator> jpeg_decoder_;

  bool log_messages_;  // True if we should log sent and received messages.
  gpu::gles2::DisallowedFeatures disallowed_features_;
  GpuWatchdog* watchdog_;
  bool software_;
  bool handle_messages_scheduled_;
  IPC::Message* currently_processing_message_;

  scoped_refptr<GpuChannelMessageFilter> filter_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  size_t num_stubs_descheduled_;

  bool allow_future_sync_points_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to Controller are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<GpuChannel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannel);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_CHANNEL_H_
