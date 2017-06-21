// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "content/common/gpu/gpu_channel.h"

#include <algorithm>
#include <queue>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/media/gpu_jpeg_decode_accelerator.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/value_state.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"
#include "ipc/ipc_channel.h"
#include "ipc/message_filter.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_surface.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

namespace content {
namespace {

// Number of milliseconds between successive vsync. Many GL commands block
// on vsync, so thresholds for preemption should be multiples of this.
const int64 kVsyncIntervalMs = 17;

// Amount of time that we will wait for an IPC to be processed before
// preempting. After a preemption, we must wait this long before triggering
// another preemption.
const int64 kPreemptWaitTimeMs = 2 * kVsyncIntervalMs;

// Once we trigger a preemption, the maximum duration that we will wait
// before clearing the preemption.
const int64 kMaxPreemptTimeMs = kVsyncIntervalMs;

// Stop the preemption once the time for the longest pending IPC drops
// below this threshold.
const int64 kStopPreemptThresholdMs = kVsyncIntervalMs;

}  // anonymous namespace

// This filter does three things:
// - it counts and timestamps each message forwarded to the channel
//   so that we can preempt other channels if a message takes too long to
//   process. To guarantee fairness, we must wait a minimum amount of time
//   before preempting and we limit the amount of time that we can preempt in
//   one shot (see constants above).
// - it handles the GpuCommandBufferMsg_InsertSyncPoint message on the IO
//   thread, generating the sync point ID and responding immediately, and then
//   posting a task to insert the GpuCommandBufferMsg_RetireSyncPoint message
//   into the channel's queue.
// - it generates mailbox names for clients of the GPU process on the IO thread.
class GpuChannelMessageFilter : public IPC::MessageFilter {
 public:
  GpuChannelMessageFilter(
      base::WeakPtr<GpuChannel> gpu_channel,
      scoped_refptr<gpu::SyncPointManager> sync_point_manager,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      bool future_sync_points)
      : preemption_state_(IDLE),
        gpu_channel_(gpu_channel),
        sender_(NULL),
        sync_point_manager_(sync_point_manager),
        task_runner_(task_runner),
        messages_forwarded_to_channel_(0),
        a_stub_is_descheduled_(false),
        future_sync_points_(future_sync_points) {}

  void OnFilterAdded(IPC::Sender* sender) override {
    DCHECK(!sender_);
    sender_ = sender;
  }

  void OnFilterRemoved() override {
    DCHECK(sender_);
    sender_ = NULL;
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    DCHECK(sender_);

    bool handled = false;
    if ((message.type() == GpuCommandBufferMsg_RetireSyncPoint::ID) &&
        !future_sync_points_) {
      DLOG(ERROR) << "Untrusted client should not send "
                     "GpuCommandBufferMsg_RetireSyncPoint message";
      return true;
    }

    if (message.type() == GpuCommandBufferMsg_InsertSyncPoint::ID) {
      base::Tuple<bool> retire;
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
      if (!GpuCommandBufferMsg_InsertSyncPoint::ReadSendParam(&message,
                                                              &retire)) {
        reply->set_reply_error();
        Send(reply);
        return true;
      }
      if (!future_sync_points_ && !base::get<0>(retire)) {
        LOG(ERROR) << "Untrusted contexts can't create future sync points";
        reply->set_reply_error();
        Send(reply);
        return true;
      }
      uint32 sync_point = sync_point_manager_->GenerateSyncPoint();
      GpuCommandBufferMsg_InsertSyncPoint::WriteReplyParams(reply, sync_point);
      Send(reply);
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&GpuChannelMessageFilter::InsertSyncPointOnMainThread,
                     gpu_channel_, sync_point_manager_, message.routing_id(),
                     base::get<0>(retire), sync_point));
      handled = true;
    }

    // All other messages get processed by the GpuChannel.
    messages_forwarded_to_channel_++;
    if (preempting_flag_.get())
      pending_messages_.push(PendingMessage(messages_forwarded_to_channel_));
    UpdatePreemptionState();

    return handled;
  }

  void MessageProcessed(uint64 messages_processed) {
    while (!pending_messages_.empty() &&
           pending_messages_.front().message_number <= messages_processed)
      pending_messages_.pop();
    UpdatePreemptionState();
  }

  void SetPreemptingFlagAndSchedulingState(
      gpu::PreemptionFlag* preempting_flag,
      bool a_stub_is_descheduled) {
    preempting_flag_ = preempting_flag;
    a_stub_is_descheduled_ = a_stub_is_descheduled;
  }

  void UpdateStubSchedulingState(bool a_stub_is_descheduled) {
    a_stub_is_descheduled_ = a_stub_is_descheduled;
    UpdatePreemptionState();
  }

  bool Send(IPC::Message* message) {
    return sender_->Send(message);
  }

 protected:
  ~GpuChannelMessageFilter() override {}

 private:
  enum PreemptionState {
    // Either there's no other channel to preempt, there are no messages
    // pending processing, or we just finished preempting and have to wait
    // before preempting again.
    IDLE,
    // We are waiting kPreemptWaitTimeMs before checking if we should preempt.
    WAITING,
    // We can preempt whenever any IPC processing takes more than
    // kPreemptWaitTimeMs.
    CHECKING,
    // We are currently preempting (i.e. no stub is descheduled).
    PREEMPTING,
    // We would like to preempt, but some stub is descheduled.
    WOULD_PREEMPT_DESCHEDULED,
  };

  PreemptionState preemption_state_;

  // Maximum amount of time that we can spend in PREEMPTING.
  // It is reset when we transition to IDLE.
  base::TimeDelta max_preemption_time_;

  struct PendingMessage {
    uint64 message_number;
    base::TimeTicks time_received;

    explicit PendingMessage(uint64 message_number)
        : message_number(message_number),
          time_received(base::TimeTicks::Now()) {
    }
  };

  void UpdatePreemptionState() {
    switch (preemption_state_) {
      case IDLE:
        if (preempting_flag_.get() && !pending_messages_.empty())
          TransitionToWaiting();
        break;
      case WAITING:
        // A timer will transition us to CHECKING.
        DCHECK(timer_.IsRunning());
        break;
      case CHECKING:
        if (!pending_messages_.empty()) {
          base::TimeDelta time_elapsed =
              base::TimeTicks::Now() - pending_messages_.front().time_received;
          if (time_elapsed.InMilliseconds() < kPreemptWaitTimeMs) {
            // Schedule another check for when the IPC may go long.
            timer_.Start(
                FROM_HERE,
                base::TimeDelta::FromMilliseconds(kPreemptWaitTimeMs) -
                    time_elapsed,
                this, &GpuChannelMessageFilter::UpdatePreemptionState);
          } else {
            if (a_stub_is_descheduled_)
              TransitionToWouldPreemptDescheduled();
            else
              TransitionToPreempting();
          }
        }
        break;
      case PREEMPTING:
        // A TransitionToIdle() timer should always be running in this state.
        DCHECK(timer_.IsRunning());
        if (a_stub_is_descheduled_)
          TransitionToWouldPreemptDescheduled();
        else
          TransitionToIdleIfCaughtUp();
        break;
      case WOULD_PREEMPT_DESCHEDULED:
        // A TransitionToIdle() timer should never be running in this state.
        DCHECK(!timer_.IsRunning());
        if (!a_stub_is_descheduled_)
          TransitionToPreempting();
        else
          TransitionToIdleIfCaughtUp();
        break;
      default:
        NOTREACHED();
    }
  }

  void TransitionToIdleIfCaughtUp() {
    DCHECK(preemption_state_ == PREEMPTING ||
           preemption_state_ == WOULD_PREEMPT_DESCHEDULED);
    if (pending_messages_.empty()) {
      TransitionToIdle();
    } else {
      base::TimeDelta time_elapsed =
          base::TimeTicks::Now() - pending_messages_.front().time_received;
      if (time_elapsed.InMilliseconds() < kStopPreemptThresholdMs)
        TransitionToIdle();
    }
  }

  void TransitionToIdle() {
    DCHECK(preemption_state_ == PREEMPTING ||
           preemption_state_ == WOULD_PREEMPT_DESCHEDULED);
    // Stop any outstanding timer set to force us from PREEMPTING to IDLE.
    timer_.Stop();

    preemption_state_ = IDLE;
    preempting_flag_->Reset();
    TRACE_COUNTER_ID1("gpu", "GpuChannel::Preempting", this, 0);

    UpdatePreemptionState();
  }

  void TransitionToWaiting() {
    DCHECK_EQ(preemption_state_, IDLE);
    DCHECK(!timer_.IsRunning());

    preemption_state_ = WAITING;
    timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kPreemptWaitTimeMs),
        this, &GpuChannelMessageFilter::TransitionToChecking);
  }

  void TransitionToChecking() {
    DCHECK_EQ(preemption_state_, WAITING);
    DCHECK(!timer_.IsRunning());

    preemption_state_ = CHECKING;
    max_preemption_time_ = base::TimeDelta::FromMilliseconds(kMaxPreemptTimeMs);
    UpdatePreemptionState();
  }

  void TransitionToPreempting() {
    DCHECK(preemption_state_ == CHECKING ||
           preemption_state_ == WOULD_PREEMPT_DESCHEDULED);
    DCHECK(!a_stub_is_descheduled_);

    // Stop any pending state update checks that we may have queued
    // while CHECKING.
    if (preemption_state_ == CHECKING)
      timer_.Stop();

    preemption_state_ = PREEMPTING;
    preempting_flag_->Set();
    TRACE_COUNTER_ID1("gpu", "GpuChannel::Preempting", this, 1);

    timer_.Start(
       FROM_HERE,
       max_preemption_time_,
       this, &GpuChannelMessageFilter::TransitionToIdle);

    UpdatePreemptionState();
  }

  void TransitionToWouldPreemptDescheduled() {
    DCHECK(preemption_state_ == CHECKING ||
           preemption_state_ == PREEMPTING);
    DCHECK(a_stub_is_descheduled_);

    if (preemption_state_ == CHECKING) {
      // Stop any pending state update checks that we may have queued
      // while CHECKING.
      timer_.Stop();
    } else {
      // Stop any TransitionToIdle() timers that we may have queued
      // while PREEMPTING.
      timer_.Stop();
      max_preemption_time_ = timer_.desired_run_time() - base::TimeTicks::Now();
      if (max_preemption_time_ < base::TimeDelta()) {
        TransitionToIdle();
        return;
      }
    }

    preemption_state_ = WOULD_PREEMPT_DESCHEDULED;
    preempting_flag_->Reset();
    TRACE_COUNTER_ID1("gpu", "GpuChannel::Preempting", this, 0);

    UpdatePreemptionState();
  }

  static void InsertSyncPointOnMainThread(
      base::WeakPtr<GpuChannel> gpu_channel,
      scoped_refptr<gpu::SyncPointManager> manager,
      int32 routing_id,
      bool retire,
      uint32 sync_point) {
    // This function must ensure that the sync point will be retired. Normally
    // we'll find the stub based on the routing ID, and associate the sync point
    // with it, but if that fails for any reason (channel or stub already
    // deleted, invalid routing id), we need to retire the sync point
    // immediately.
    if (gpu_channel) {
      GpuCommandBufferStub* stub = gpu_channel->LookupCommandBuffer(routing_id);
      if (stub) {
        stub->AddSyncPoint(sync_point);
        if (retire) {
          GpuCommandBufferMsg_RetireSyncPoint message(routing_id, sync_point);
          gpu_channel->OnMessageReceived(message);
        }
        return;
      } else {
        gpu_channel->MessageProcessed();
      }
    }
    manager->RetireSyncPoint(sync_point);
  }

  // NOTE: this weak pointer is never dereferenced on the IO thread, it's only
  // passed through - therefore the WeakPtr assumptions are respected.
  base::WeakPtr<GpuChannel> gpu_channel_;
  IPC::Sender* sender_;
  scoped_refptr<gpu::SyncPointManager> sync_point_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<gpu::PreemptionFlag> preempting_flag_;

  std::queue<PendingMessage> pending_messages_;

  // Count of the number of IPCs forwarded to the GpuChannel.
  uint64 messages_forwarded_to_channel_;

  base::OneShotTimer<GpuChannelMessageFilter> timer_;

  bool a_stub_is_descheduled_;

  // True if this channel can create future sync points.
  bool future_sync_points_;
};

GpuChannel::GpuChannel(GpuChannelManager* gpu_channel_manager,
                       GpuWatchdog* watchdog,
                       gfx::GLShareGroup* share_group,
                       gpu::gles2::MailboxManager* mailbox,
                       int client_id,
                       bool software,
                       bool allow_future_sync_points)
    : gpu_channel_manager_(gpu_channel_manager),
      messages_processed_(0),
      client_id_(client_id),
      share_group_(share_group ? share_group : new gfx::GLShareGroup),
      mailbox_manager_(mailbox ? mailbox : new gpu::gles2::MailboxManagerImpl),
      watchdog_(watchdog),
      software_(software),
      handle_messages_scheduled_(false),
      currently_processing_message_(NULL),
      num_stubs_descheduled_(0),
      allow_future_sync_points_(allow_future_sync_points),
      weak_factory_(this) {
  DCHECK(gpu_channel_manager);
  DCHECK(client_id);

  channel_id_ = IPC::Channel::GenerateVerifiedChannelID("gpu");
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  log_messages_ = command_line->HasSwitch(switches::kLogPluginMessages);

  subscription_ref_set_ = new gpu::gles2::SubscriptionRefSet();
  subscription_ref_set_->AddObserver(this);
}

GpuChannel::~GpuChannel() {
  STLDeleteElements(&deferred_messages_);
  subscription_ref_set_->RemoveObserver(this);
  if (preempting_flag_.get())
    preempting_flag_->Reset();

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void GpuChannel::Init(base::SingleThreadTaskRunner* io_task_runner,
                      base::WaitableEvent* shutdown_event,
                      IPC::AttachmentBroker* broker) {
  DCHECK(!channel_.get());

  // Map renderer ID to a (single) channel to that process.
  channel_ =
      IPC::SyncChannel::Create(channel_id_, IPC::Channel::MODE_SERVER, this,
                               io_task_runner, false, shutdown_event, broker);

  filter_ = new GpuChannelMessageFilter(
      weak_factory_.GetWeakPtr(), gpu_channel_manager_->sync_point_manager(),
      base::ThreadTaskRunnerHandle::Get(), allow_future_sync_points_);
  io_task_runner_ = io_task_runner;
  channel_->AddFilter(filter_.get());
  pending_valuebuffer_state_ = new gpu::ValueStateMap();

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, base::ThreadTaskRunnerHandle::Get());
}

std::string GpuChannel::GetChannelName() {
  return channel_id_;
}

#if defined(OS_POSIX)
base::ScopedFD GpuChannel::TakeRendererFileDescriptor() {
  if (!channel_) {
    NOTREACHED();
    return base::ScopedFD();
  }
  return channel_->TakeClientFileDescriptor();
}
#endif  // defined(OS_POSIX)

bool GpuChannel::OnMessageReceived(const IPC::Message& message) {
  if (log_messages_) {
    DVLOG(1) << "received message @" << &message << " on channel @" << this
             << " with type " << message.type();
  }

  if (message.type() == GpuCommandBufferMsg_WaitForTokenInRange::ID ||
      message.type() == GpuCommandBufferMsg_WaitForGetOffsetInRange::ID) {
    // Move Wait commands to the head of the queue, so the renderer
    // doesn't have to wait any longer than necessary.
    deferred_messages_.push_front(new IPC::Message(message));
  } else {
    deferred_messages_.push_back(new IPC::Message(message));
  }

  OnScheduled();

  return true;
}

void GpuChannel::OnChannelError() {
  gpu_channel_manager_->RemoveChannel(client_id_);
}

bool GpuChannel::Send(IPC::Message* message) {
  // The GPU process must never send a synchronous IPC message to the renderer
  // process. This could result in deadlock.
  DCHECK(!message->is_sync());
  if (log_messages_) {
    DVLOG(1) << "sending message @" << message << " on channel @" << this
             << " with type " << message->type();
  }

  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

void GpuChannel::OnAddSubscription(unsigned int target) {
  gpu_channel_manager()->Send(
      new GpuHostMsg_AddSubscription(client_id_, target));
}

void GpuChannel::OnRemoveSubscription(unsigned int target) {
  gpu_channel_manager()->Send(
      new GpuHostMsg_RemoveSubscription(client_id_, target));
}

void GpuChannel::RequeueMessage() {
  DCHECK(currently_processing_message_);
  deferred_messages_.push_front(
      new IPC::Message(*currently_processing_message_));
  messages_processed_--;
  currently_processing_message_ = NULL;
}

void GpuChannel::OnScheduled() {
  if (handle_messages_scheduled_)
    return;
  // Post a task to handle any deferred messages. The deferred message queue is
  // not emptied here, which ensures that OnMessageReceived will continue to
  // defer newly received messages until the ones in the queue have all been
  // handled by HandleMessage. HandleMessage is invoked as a
  // task to prevent reentrancy.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&GpuChannel::HandleMessage, weak_factory_.GetWeakPtr()));
  handle_messages_scheduled_ = true;
}

void GpuChannel::StubSchedulingChanged(bool scheduled) {
  bool a_stub_was_descheduled = num_stubs_descheduled_ > 0;
  if (scheduled) {
    num_stubs_descheduled_--;
    OnScheduled();
  } else {
    num_stubs_descheduled_++;
  }
  DCHECK_LE(num_stubs_descheduled_, stubs_.size());
  bool a_stub_is_descheduled = num_stubs_descheduled_ > 0;

  if (a_stub_is_descheduled != a_stub_was_descheduled) {
    if (preempting_flag_.get()) {
      io_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&GpuChannelMessageFilter::UpdateStubSchedulingState,
                     filter_, a_stub_is_descheduled));
    }
  }
}

CreateCommandBufferResult GpuChannel::CreateViewCommandBuffer(
    const gfx::GLSurfaceHandle& window,
    int32 surface_id,
    const GPUCreateCommandBufferConfig& init_params,
    int32 route_id) {
  TRACE_EVENT1("gpu",
               "GpuChannel::CreateViewCommandBuffer",
               "surface_id",
               surface_id);

  GpuCommandBufferStub* share_group = stubs_.Lookup(init_params.share_group_id);

  // Virtualize compositor contexts on OS X to prevent performance regressions
  // when enabling FCM.
  // http://crbug.com/180463
  bool use_virtualized_gl_context = false;
#if defined(OS_MACOSX)
  use_virtualized_gl_context = true;
#endif

  scoped_ptr<GpuCommandBufferStub> stub(
      new GpuCommandBufferStub(this,
                               share_group,
                               window,
                               mailbox_manager_.get(),
                               subscription_ref_set_.get(),
                               pending_valuebuffer_state_.get(),
                               gfx::Size(),
                               disallowed_features_,
                               init_params.attribs,
                               init_params.gpu_preference,
                               use_virtualized_gl_context,
                               route_id,
                               surface_id,
                               watchdog_,
                               software_,
                               init_params.active_url));
  if (preempted_flag_.get())
    stub->SetPreemptByFlag(preempted_flag_);
  if (!router_.AddRoute(route_id, stub.get())) {
    DLOG(ERROR) << "GpuChannel::CreateViewCommandBuffer(): "
                   "failed to add route";
    return CREATE_COMMAND_BUFFER_FAILED_AND_CHANNEL_LOST;
  }
  stubs_.AddWithID(stub.release(), route_id);
  return CREATE_COMMAND_BUFFER_SUCCEEDED;
}

GpuCommandBufferStub* GpuChannel::LookupCommandBuffer(int32 route_id) {
  return stubs_.Lookup(route_id);
}

void GpuChannel::LoseAllContexts() {
  gpu_channel_manager_->LoseAllContexts();
}

void GpuChannel::MarkAllContextsLost() {
  for (StubMap::Iterator<GpuCommandBufferStub> it(&stubs_);
       !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->MarkContextLost();
  }
}

bool GpuChannel::AddRoute(int32 route_id, IPC::Listener* listener) {
  return router_.AddRoute(route_id, listener);
}

void GpuChannel::RemoveRoute(int32 route_id) {
  router_.RemoveRoute(route_id);
}

gpu::PreemptionFlag* GpuChannel::GetPreemptionFlag() {
  if (!preempting_flag_.get()) {
    preempting_flag_ = new gpu::PreemptionFlag;
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &GpuChannelMessageFilter::SetPreemptingFlagAndSchedulingState,
            filter_, preempting_flag_, num_stubs_descheduled_ > 0));
  }
  return preempting_flag_.get();
}

void GpuChannel::SetPreemptByFlag(
    scoped_refptr<gpu::PreemptionFlag> preempted_flag) {
  preempted_flag_ = preempted_flag;

  for (StubMap::Iterator<GpuCommandBufferStub> it(&stubs_);
       !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->SetPreemptByFlag(preempted_flag_);
  }
}

void GpuChannel::OnDestroy() {
  TRACE_EVENT0("gpu", "GpuChannel::OnDestroy");
  gpu_channel_manager_->RemoveChannel(client_id_);
}

bool GpuChannel::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChannel, msg)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateOffscreenCommandBuffer,
                        OnCreateOffscreenCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_DestroyCommandBuffer,
                        OnDestroyCommandBuffer)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuMsg_CreateJpegDecoder,
                                    OnCreateJpegDecoder)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << msg.type();
  return handled;
}

void GpuChannel::HandleMessage() {
  handle_messages_scheduled_ = false;
  if (deferred_messages_.empty())
    return;

  IPC::Message* m = NULL;
  GpuCommandBufferStub* stub = NULL;

  m = deferred_messages_.front();
  stub = stubs_.Lookup(m->routing_id());
  if (stub) {
    if (!stub->IsScheduled())
      return;
    if (stub->IsPreempted()) {
      OnScheduled();
      return;
    }
  }

  scoped_ptr<IPC::Message> message(m);
  deferred_messages_.pop_front();
  bool message_processed = true;

  currently_processing_message_ = message.get();
  bool result;
  if (message->routing_id() == MSG_ROUTING_CONTROL)
    result = OnControlMessageReceived(*message);
  else
    result = router_.RouteMessage(*message);
  currently_processing_message_ = NULL;

  if (!result) {
    // Respond to sync messages even if router failed to route.
    if (message->is_sync()) {
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&*message);
      reply->set_reply_error();
      Send(reply);
    }
  } else {
    // If the command buffer becomes unscheduled as a result of handling the
    // message but still has more commands to process, synthesize an IPC
    // message to flush that command buffer.
    if (stub) {
      if (stub->HasUnprocessedCommands()) {
        deferred_messages_.push_front(new GpuCommandBufferMsg_Rescheduled(
            stub->route_id()));
        message_processed = false;
      }
    }
  }
  if (message_processed)
    MessageProcessed();

  if (!deferred_messages_.empty()) {
    OnScheduled();
  }
}

void GpuChannel::OnCreateOffscreenCommandBuffer(
    const gfx::Size& size,
    const GPUCreateCommandBufferConfig& init_params,
    int32 route_id,
    bool* succeeded) {
  TRACE_EVENT0("gpu", "GpuChannel::OnCreateOffscreenCommandBuffer");
  GpuCommandBufferStub* share_group = stubs_.Lookup(init_params.share_group_id);

  scoped_ptr<GpuCommandBufferStub> stub(new GpuCommandBufferStub(
      this,
      share_group,
      gfx::GLSurfaceHandle(),
      mailbox_manager_.get(),
      subscription_ref_set_.get(),
      pending_valuebuffer_state_.get(),
      size,
      disallowed_features_,
      init_params.attribs,
      init_params.gpu_preference,
      false,
      route_id,
      0,
      watchdog_,
      software_,
      init_params.active_url));
  if (preempted_flag_.get())
    stub->SetPreemptByFlag(preempted_flag_);
  if (!router_.AddRoute(route_id, stub.get())) {
    DLOG(ERROR) << "GpuChannel::OnCreateOffscreenCommandBuffer(): "
                   "failed to add route";
    *succeeded = false;
    return;
  }
  stubs_.AddWithID(stub.release(), route_id);
  TRACE_EVENT1("gpu", "GpuChannel::OnCreateOffscreenCommandBuffer",
               "route_id", route_id);
  *succeeded = true;
}

void GpuChannel::OnDestroyCommandBuffer(int32 route_id) {
  TRACE_EVENT1("gpu", "GpuChannel::OnDestroyCommandBuffer",
               "route_id", route_id);

  GpuCommandBufferStub* stub = stubs_.Lookup(route_id);
  if (!stub)
    return;
  bool need_reschedule = (stub && !stub->IsScheduled());
  router_.RemoveRoute(route_id);
  stubs_.Remove(route_id);
  // In case the renderer is currently blocked waiting for a sync reply from the
  // stub, we need to make sure to reschedule the GpuChannel here.
  if (need_reschedule) {
    // This stub won't get a chance to reschedule, so update the count now.
    StubSchedulingChanged(true);
  }
}

void GpuChannel::OnCreateJpegDecoder(int32 route_id, IPC::Message* reply_msg) {
  if (!jpeg_decoder_) {
    jpeg_decoder_.reset(new GpuJpegDecodeAccelerator(this, io_task_runner_));
  }
  jpeg_decoder_->AddClient(route_id, reply_msg);
}

void GpuChannel::MessageProcessed() {
  messages_processed_++;
  if (preempting_flag_.get()) {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&GpuChannelMessageFilter::MessageProcessed,
                              filter_, messages_processed_));
  }
}

void GpuChannel::CacheShader(const std::string& key,
                             const std::string& shader) {
  gpu_channel_manager_->Send(
      new GpuHostMsg_CacheShader(client_id_, key, shader));
}

void GpuChannel::AddFilter(IPC::MessageFilter* filter) {
  channel_->AddFilter(filter);
}

void GpuChannel::RemoveFilter(IPC::MessageFilter* filter) {
  channel_->RemoveFilter(filter);
}

uint64 GpuChannel::GetMemoryUsage() {
  uint64 size = 0;
  for (StubMap::Iterator<GpuCommandBufferStub> it(&stubs_);
       !it.IsAtEnd(); it.Advance()) {
    size += it.GetCurrentValue()->GetMemoryUsage();
  }
  return size;
}

scoped_refptr<gfx::GLImage> GpuChannel::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    uint32 internalformat) {
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      scoped_refptr<gfx::GLImageSharedMemory> image(
          new gfx::GLImageSharedMemory(size, internalformat));
      if (!image->Initialize(handle, format))
        return scoped_refptr<gfx::GLImage>();

      return image;
    }
    default: {
      GpuChannelManager* manager = gpu_channel_manager();
      if (!manager->gpu_memory_buffer_factory())
        return scoped_refptr<gfx::GLImage>();

      return manager->gpu_memory_buffer_factory()
          ->AsImageFactory()
          ->CreateImageForGpuMemoryBuffer(handle,
                                          size,
                                          format,
                                          internalformat,
                                          client_id_);
    }
  }
}

void GpuChannel::HandleUpdateValueState(
    unsigned int target, const gpu::ValueState& state) {
  pending_valuebuffer_state_->UpdateState(target, state);
}

bool GpuChannel::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) {
  auto dump_name = GetChannelName();
  std::replace(dump_name.begin(), dump_name.end(), '.', '_');

  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(base::StringPrintf("gl/%s", dump_name.c_str()));

  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  GetMemoryUsage());

  return true;
}

}  // namespace content
