// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_channel_host.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/posix/eintr_wrapper.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/client/gpu_jpeg_decode_accelerator_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "ipc/ipc_sync_message_filter.h"
#include "url/gurl.h"

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "content/public/common/sandbox_init.h"
#endif

using base::AutoLock;

namespace content {

GpuListenerInfo::GpuListenerInfo() {}

GpuListenerInfo::~GpuListenerInfo() {}

ProxyFlushInfo::ProxyFlushInfo()
    : flush_pending(false),
      route_id(MSG_ROUTING_NONE),
      put_offset(0),
      flush_count(0) {
}

ProxyFlushInfo::~ProxyFlushInfo() {
}

// static
scoped_refptr<GpuChannelHost> GpuChannelHost::Create(
    GpuChannelHostFactory* factory,
    const gpu::GPUInfo& gpu_info,
    const IPC::ChannelHandle& channel_handle,
    base::WaitableEvent* shutdown_event,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager) {
  DCHECK(factory->IsMainThread());
  scoped_refptr<GpuChannelHost> host =
      new GpuChannelHost(factory, gpu_info, gpu_memory_buffer_manager);
  host->Connect(channel_handle, shutdown_event);
  return host;
}

GpuChannelHost::GpuChannelHost(
    GpuChannelHostFactory* factory,
    const gpu::GPUInfo& gpu_info,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager)
    : factory_(factory),
      gpu_info_(gpu_info),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager) {
  next_transfer_buffer_id_.GetNext();
  next_image_id_.GetNext();
  next_route_id_.GetNext();
}

void GpuChannelHost::Connect(const IPC::ChannelHandle& channel_handle,
                             base::WaitableEvent* shutdown_event) {
  DCHECK(factory_->IsMainThread());
  // Open a channel to the GPU process. We pass NULL as the main listener here
  // since we need to filter everything to route it to the right thread.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      factory_->GetIOThreadTaskRunner();
  channel_ = IPC::SyncChannel::Create(
      channel_handle, IPC::Channel::MODE_CLIENT, NULL, io_task_runner.get(),
      true, shutdown_event, factory_->GetAttachmentBroker());

  sync_filter_ = new IPC::SyncMessageFilter(shutdown_event);

  channel_->AddFilter(sync_filter_.get());

  channel_filter_ = new MessageFilter();

  // Install the filter last, because we intercept all leftover
  // messages.
  channel_->AddFilter(channel_filter_.get());
}

bool GpuChannelHost::Send(IPC::Message* msg) {
  // Callee takes ownership of message, regardless of whether Send is
  // successful. See IPC::Sender.
  scoped_ptr<IPC::Message> message(msg);
  // The GPU process never sends synchronous IPCs so clear the unblock flag to
  // preserve order.
  message->set_unblock(false);

  // Currently we need to choose between two different mechanisms for sending.
  // On the main thread we use the regular channel Send() method, on another
  // thread we use SyncMessageFilter. We also have to be careful interpreting
  // IsMainThread() since it might return false during shutdown,
  // impl we are actually calling from the main thread (discard message then).
  //
  // TODO: Can we just always use sync_filter_ since we setup the channel
  //       without a main listener?
  if (factory_->IsMainThread()) {
    // channel_ is only modified on the main thread, so we don't need to take a
    // lock here.
    if (!channel_) {
      DVLOG(1) << "GpuChannelHost::Send failed: Channel already destroyed";
      return false;
    }
    // http://crbug.com/125264
    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    bool result = channel_->Send(message.release());
    if (!result)
      DVLOG(1) << "GpuChannelHost::Send failed: Channel::Send failed";
    return result;
  }

  bool result = sync_filter_->Send(message.release());
  return result;
}

void GpuChannelHost::OrderingBarrier(
    int route_id,
    int32 put_offset,
    unsigned int flush_count,
    const std::vector<ui::LatencyInfo>& latency_info,
    bool put_offset_changed,
    bool do_flush) {
  AutoLock lock(context_lock_);
  if (flush_info_.flush_pending && flush_info_.route_id != route_id)
    InternalFlush();

  if (put_offset_changed) {
    flush_info_.flush_pending = true;
    flush_info_.route_id = route_id;
    flush_info_.put_offset = put_offset;
    flush_info_.flush_count = flush_count;
    flush_info_.latency_info.insert(flush_info_.latency_info.end(),
                                    latency_info.begin(), latency_info.end());

    if (do_flush)
      InternalFlush();
  }
}

void GpuChannelHost::InternalFlush() {
  DCHECK(flush_info_.flush_pending);
  Send(new GpuCommandBufferMsg_AsyncFlush(
      flush_info_.route_id, flush_info_.put_offset, flush_info_.flush_count,
      flush_info_.latency_info));
  flush_info_.latency_info.clear();
  flush_info_.flush_pending = false;
}

CommandBufferProxyImpl* GpuChannelHost::CreateViewCommandBuffer(
    int32 surface_id,
    CommandBufferProxyImpl* share_group,
    const std::vector<int32>& attribs,
    const GURL& active_url,
    gfx::GpuPreference gpu_preference) {
  TRACE_EVENT1("gpu",
               "GpuChannelHost::CreateViewCommandBuffer",
               "surface_id",
               surface_id);

  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id =
      share_group ? share_group->GetRouteID() : MSG_ROUTING_NONE;
  init_params.attribs = attribs;
  init_params.active_url = active_url;
  init_params.gpu_preference = gpu_preference;
  int32 route_id = GenerateRouteID();
  CreateCommandBufferResult result = factory_->CreateViewCommandBuffer(
      surface_id, init_params, route_id);
  if (result != CREATE_COMMAND_BUFFER_SUCCEEDED) {
    LOG(ERROR) << "GpuChannelHost::CreateViewCommandBuffer failed.";

    if (result == CREATE_COMMAND_BUFFER_FAILED_AND_CHANNEL_LOST) {
      // The GPU channel needs to be considered lost. The caller will
      // then set up a new connection, and the GPU channel and any
      // view command buffers will all be associated with the same GPU
      // process.
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
          factory_->GetIOThreadTaskRunner();
      io_task_runner->PostTask(
          FROM_HERE, base::Bind(&GpuChannelHost::MessageFilter::OnChannelError,
                                channel_filter_.get()));
    }

    return NULL;
  }

  CommandBufferProxyImpl* command_buffer =
      new CommandBufferProxyImpl(this, route_id);
  AddRoute(route_id, command_buffer->AsWeakPtr());

  AutoLock lock(context_lock_);
  proxies_[route_id] = command_buffer;
  return command_buffer;
}

CommandBufferProxyImpl* GpuChannelHost::CreateOffscreenCommandBuffer(
    const gfx::Size& size,
    CommandBufferProxyImpl* share_group,
    const std::vector<int32>& attribs,
    const GURL& active_url,
    gfx::GpuPreference gpu_preference) {
  TRACE_EVENT0("gpu", "GpuChannelHost::CreateOffscreenCommandBuffer");

  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id =
      share_group ? share_group->GetRouteID() : MSG_ROUTING_NONE;
  init_params.attribs = attribs;
  init_params.active_url = active_url;
  init_params.gpu_preference = gpu_preference;
  int32 route_id = GenerateRouteID();
  bool succeeded = false;
  if (!Send(new GpuChannelMsg_CreateOffscreenCommandBuffer(
          size, init_params, route_id, &succeeded))) {
    LOG(ERROR) << "Failed to send GpuChannelMsg_CreateOffscreenCommandBuffer.";
    return NULL;
  }

  if (!succeeded) {
    LOG(ERROR)
        << "GpuChannelMsg_CreateOffscreenCommandBuffer returned failure.";
    return NULL;
  }

  CommandBufferProxyImpl* command_buffer =
      new CommandBufferProxyImpl(this, route_id);
  AddRoute(route_id, command_buffer->AsWeakPtr());

  AutoLock lock(context_lock_);
  proxies_[route_id] = command_buffer;
  return command_buffer;
}

scoped_ptr<media::VideoDecodeAccelerator> GpuChannelHost::CreateVideoDecoder(
    int command_buffer_route_id) {
  TRACE_EVENT0("gpu", "GpuChannelHost::CreateVideoDecoder");
  AutoLock lock(context_lock_);
  ProxyMap::iterator it = proxies_.find(command_buffer_route_id);
  DCHECK(it != proxies_.end());
  return it->second->CreateVideoDecoder();
}

scoped_ptr<media::VideoEncodeAccelerator> GpuChannelHost::CreateVideoEncoder(
    int command_buffer_route_id) {
  TRACE_EVENT0("gpu", "GpuChannelHost::CreateVideoEncoder");
  AutoLock lock(context_lock_);
  ProxyMap::iterator it = proxies_.find(command_buffer_route_id);
  DCHECK(it != proxies_.end());
  return it->second->CreateVideoEncoder();
}

scoped_ptr<media::JpegDecodeAccelerator> GpuChannelHost::CreateJpegDecoder(
    media::JpegDecodeAccelerator::Client* client) {
  TRACE_EVENT0("gpu", "GpuChannelHost::CreateJpegDecoder");

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      factory_->GetIOThreadTaskRunner();
  int32 route_id = GenerateRouteID();
  scoped_ptr<GpuJpegDecodeAcceleratorHost> decoder(
      new GpuJpegDecodeAcceleratorHost(this, route_id, io_task_runner));
  if (!decoder->Initialize(client)) {
    return nullptr;
  }

  // The reply message of jpeg decoder should run on IO thread.
  io_task_runner->PostTask(FROM_HERE,
                           base::Bind(&GpuChannelHost::MessageFilter::AddRoute,
                                      channel_filter_.get(), route_id,
                                      decoder->GetReceiver(), io_task_runner));

  return decoder.Pass();
}

void GpuChannelHost::DestroyCommandBuffer(
    CommandBufferProxyImpl* command_buffer) {
  TRACE_EVENT0("gpu", "GpuChannelHost::DestroyCommandBuffer");

  int route_id = command_buffer->GetRouteID();
  Send(new GpuChannelMsg_DestroyCommandBuffer(route_id));
  RemoveRoute(route_id);

  AutoLock lock(context_lock_);
  proxies_.erase(route_id);
  if (flush_info_.flush_pending && flush_info_.route_id == route_id)
    flush_info_.flush_pending = false;

  delete command_buffer;
}

void GpuChannelHost::DestroyChannel() {
  DCHECK(factory_->IsMainThread());
  AutoLock lock(context_lock_);
  channel_.reset();
}

void GpuChannelHost::AddRoute(
    int route_id, base::WeakPtr<IPC::Listener> listener) {
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      factory_->GetIOThreadTaskRunner();
  io_task_runner->PostTask(FROM_HERE,
                           base::Bind(&GpuChannelHost::MessageFilter::AddRoute,
                                      channel_filter_.get(), route_id, listener,
                                      base::ThreadTaskRunnerHandle::Get()));
}

void GpuChannelHost::RemoveRoute(int route_id) {
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      factory_->GetIOThreadTaskRunner();
  io_task_runner->PostTask(
      FROM_HERE, base::Bind(&GpuChannelHost::MessageFilter::RemoveRoute,
                            channel_filter_.get(), route_id));
}

base::SharedMemoryHandle GpuChannelHost::ShareToGpuProcess(
    base::SharedMemoryHandle source_handle) {
  if (IsLost())
    return base::SharedMemory::NULLHandle();

#if defined(OS_WIN) || defined(OS_MACOSX)
  // Windows and Mac need to explicitly duplicate the handle out to another
  // process.
  base::SharedMemoryHandle target_handle;
  base::ProcessId peer_pid;
  {
    AutoLock lock(context_lock_);
    if (!channel_)
      return base::SharedMemory::NULLHandle();
    peer_pid = channel_->GetPeerPID();
  }
#if defined(OS_WIN)
  bool success =
      BrokerDuplicateHandle(source_handle, peer_pid, &target_handle,
                            FILE_GENERIC_READ | FILE_GENERIC_WRITE, 0);
#elif defined(OS_MACOSX)
  bool success = BrokerDuplicateSharedMemoryHandle(source_handle, peer_pid,
                                                   &target_handle);
#endif
  if (!success)
    return base::SharedMemory::NULLHandle();

  return target_handle;
#else
  return base::SharedMemory::DuplicateHandle(source_handle);
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
}

int32 GpuChannelHost::ReserveTransferBufferId() {
  return next_transfer_buffer_id_.GetNext();
}

gfx::GpuMemoryBufferHandle GpuChannelHost::ShareGpuMemoryBufferToGpuProcess(
    const gfx::GpuMemoryBufferHandle& source_handle,
    bool* requires_sync_point) {
  switch (source_handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      gfx::GpuMemoryBufferHandle handle;
      handle.type = gfx::SHARED_MEMORY_BUFFER;
      handle.handle = ShareToGpuProcess(source_handle.handle);
      *requires_sync_point = false;
      return handle;
    }
    case gfx::IO_SURFACE_BUFFER:
    case gfx::SURFACE_TEXTURE_BUFFER:
    case gfx::OZONE_NATIVE_BUFFER:
      *requires_sync_point = true;
      return source_handle;
    default:
      NOTREACHED();
      return gfx::GpuMemoryBufferHandle();
  }
}

int32 GpuChannelHost::ReserveImageId() {
  return next_image_id_.GetNext();
}

int32 GpuChannelHost::GenerateRouteID() {
  return next_route_id_.GetNext();
}

GpuChannelHost::~GpuChannelHost() {
#if DCHECK_IS_ON()
  AutoLock lock(context_lock_);
  DCHECK(!channel_)
      << "GpuChannelHost::DestroyChannel must be called before destruction.";
#endif
}

GpuChannelHost::MessageFilter::MessageFilter()
    : lost_(false) {
}

GpuChannelHost::MessageFilter::~MessageFilter() {}

void GpuChannelHost::MessageFilter::AddRoute(
    int route_id,
    base::WeakPtr<IPC::Listener> listener,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(listeners_.find(route_id) == listeners_.end());
  DCHECK(task_runner);
  GpuListenerInfo info;
  info.listener = listener;
  info.task_runner = task_runner;
  listeners_[route_id] = info;
}

void GpuChannelHost::MessageFilter::RemoveRoute(int route_id) {
  ListenerMap::iterator it = listeners_.find(route_id);
  if (it != listeners_.end())
    listeners_.erase(it);
}

bool GpuChannelHost::MessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  // Never handle sync message replies or we will deadlock here.
  if (message.is_reply())
    return false;

  ListenerMap::iterator it = listeners_.find(message.routing_id());
  if (it == listeners_.end())
    return false;

  const GpuListenerInfo& info = it->second;
  info.task_runner->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&IPC::Listener::OnMessageReceived),
                 info.listener, message));
  return true;
}

void GpuChannelHost::MessageFilter::OnChannelError() {
  // Set the lost state before signalling the proxies. That way, if they
  // themselves post a task to recreate the context, they will not try to re-use
  // this channel host.
  {
    AutoLock lock(lock_);
    lost_ = true;
  }

  // Inform all the proxies that an error has occurred. This will be reported
  // via OpenGL as a lost context.
  for (ListenerMap::iterator it = listeners_.begin();
       it != listeners_.end();
       it++) {
    const GpuListenerInfo& info = it->second;
    info.task_runner->PostTask(
        FROM_HERE, base::Bind(&IPC::Listener::OnChannelError, info.listener));
  }

  listeners_.clear();
}

bool GpuChannelHost::MessageFilter::IsLost() const {
  AutoLock lock(lock_);
  return lost_;
}

}  // namespace content
