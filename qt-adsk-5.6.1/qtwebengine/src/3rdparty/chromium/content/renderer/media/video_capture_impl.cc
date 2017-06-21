// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Notes about usage of this object by VideoCaptureImplManager.
//
// VideoCaptureImplManager access this object by using a Unretained()
// binding and tasks on the IO thread. It is then important that
// VideoCaptureImpl never post task to itself. All operations must be
// synchronous.

#include "content/renderer/media/video_capture_impl.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "content/child/child_process.h"
#include "content/common/media/video_capture_messages.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"

namespace content {

namespace {

// This is called on an unknown thread when the VideoFrame destructor executes.
// As of this writing, this callback mechanism is the only interface in
// VideoFrame to provide the final value for |release_sync_point|.
// VideoCaptureImpl::DidFinishConsumingFrame() will read the value saved here,
// and pass it back to the IO thread to pass back to the host via the
// BufferReady IPC.
void SaveReleaseSyncPoint(uint32* storage, uint32 release_sync_point) {
  *storage = release_sync_point;
}

}  // namespace

class VideoCaptureImpl::ClientBuffer
    : public base::RefCountedThreadSafe<ClientBuffer> {
 public:
  ClientBuffer(scoped_ptr<base::SharedMemory> buffer,
               size_t buffer_size)
      : buffer(buffer.Pass()),
        buffer_size(buffer_size) {}
  const scoped_ptr<base::SharedMemory> buffer;
  const size_t buffer_size;

 private:
  friend class base::RefCountedThreadSafe<ClientBuffer>;

  virtual ~ClientBuffer() {}

  DISALLOW_COPY_AND_ASSIGN(ClientBuffer);
};

VideoCaptureImpl::ClientInfo::ClientInfo() {}
VideoCaptureImpl::ClientInfo::~ClientInfo() {}

VideoCaptureImpl::VideoCaptureImpl(
    const media::VideoCaptureSessionId session_id,
    VideoCaptureMessageFilter* filter)
    : message_filter_(filter),
      device_id_(0),
      session_id_(session_id),
      suspended_(false),
      state_(VIDEO_CAPTURE_STATE_STOPPED),
      weak_factory_(this) {
  DCHECK(filter);
}

VideoCaptureImpl::~VideoCaptureImpl() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
}

void VideoCaptureImpl::Init() {
  // For creating callbacks in unittest, this class may be constructed from a
  // different thread than the IO thread, e.g. wherever unittest runs on.
  // Therefore, this function should define the thread ownership.
#if DCHECK_IS_ON()
  io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
#endif
  message_filter_->AddDelegate(this);
}

void VideoCaptureImpl::DeInit() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (state_ == VIDEO_CAPTURE_STATE_STARTED)
    Send(new VideoCaptureHostMsg_Stop(device_id_));
  message_filter_->RemoveDelegate(this);
}

void VideoCaptureImpl::SuspendCapture(bool suspend) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  Send(suspend ?
       static_cast<IPC::Message*>(new VideoCaptureHostMsg_Pause(device_id_)) :
       static_cast<IPC::Message*>(
           new VideoCaptureHostMsg_Resume(device_id_, session_id_, params_)));
}

void VideoCaptureImpl::StartCapture(
    int client_id,
    const media::VideoCaptureParams& params,
    const VideoCaptureStateUpdateCB& state_update_cb,
    const VideoCaptureDeliverFrameCB& deliver_frame_cb) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ClientInfo client_info;
  client_info.params = params;
  client_info.state_update_cb = state_update_cb;
  client_info.deliver_frame_cb = deliver_frame_cb;

  if (state_ == VIDEO_CAPTURE_STATE_ERROR) {
    state_update_cb.Run(VIDEO_CAPTURE_STATE_ERROR);
  } else if (clients_pending_on_filter_.count(client_id) ||
             clients_pending_on_restart_.count(client_id) ||
             clients_.count(client_id)) {
    LOG(FATAL) << "This client has already started.";
  } else if (!device_id_) {
    clients_pending_on_filter_[client_id] = client_info;
  } else {
    // Note: |state_| might not be started at this point. But we tell
    // client that we have started.
    state_update_cb.Run(VIDEO_CAPTURE_STATE_STARTED);
    if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
      clients_[client_id] = client_info;
      // TODO(sheu): Allowing resolution change will require that all
      // outstanding clients of a capture session support resolution change.
      DCHECK_EQ(params_.resolution_change_policy,
                params.resolution_change_policy);
    } else if (state_ == VIDEO_CAPTURE_STATE_STOPPING) {
      clients_pending_on_restart_[client_id] = client_info;
      DVLOG(1) << "StartCapture: Got new resolution "
               << params.requested_format.frame_size.ToString()
               << " during stopping.";
    } else {
      clients_[client_id] = client_info;
      if (state_ == VIDEO_CAPTURE_STATE_STARTED)
        return;
      params_ = params;
      if (params_.requested_format.frame_rate >
          media::limits::kMaxFramesPerSecond) {
        params_.requested_format.frame_rate =
            media::limits::kMaxFramesPerSecond;
      }
      DVLOG(1) << "StartCapture: starting with first resolution "
               << params_.requested_format.frame_size.ToString();
      first_frame_timestamp_ = base::TimeTicks();
      StartCaptureInternal();
    }
  }
}

void VideoCaptureImpl::StopCapture(int client_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // A client ID can be in only one client list.
  // If this ID is in any client list, we can just remove it from
  // that client list and don't have to run the other following RemoveClient().
  if (!RemoveClient(client_id, &clients_pending_on_filter_)) {
    if (!RemoveClient(client_id, &clients_pending_on_restart_)) {
      RemoveClient(client_id, &clients_);
    }
  }

  if (clients_.empty()) {
    DVLOG(1) << "StopCapture: No more client, stopping ...";
    StopDevice();
    client_buffers_.clear();
    weak_factory_.InvalidateWeakPtrs();
  }
}

void VideoCaptureImpl::GetDeviceSupportedFormats(
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  device_formats_cb_queue_.push_back(callback);
  if (device_formats_cb_queue_.size() == 1)
    Send(new VideoCaptureHostMsg_GetDeviceSupportedFormats(device_id_,
                                                           session_id_));
}

void VideoCaptureImpl::GetDeviceFormatsInUse(
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  device_formats_in_use_cb_queue_.push_back(callback);
  if (device_formats_in_use_cb_queue_.size() == 1)
    Send(
        new VideoCaptureHostMsg_GetDeviceFormatsInUse(device_id_, session_id_));
}

void VideoCaptureImpl::OnBufferCreated(base::SharedMemoryHandle handle,
                                       int length,
                                       int buffer_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // In case client calls StopCapture before the arrival of created buffer,
  // just close this buffer and return.
  if (state_ != VIDEO_CAPTURE_STATE_STARTED) {
    base::SharedMemory::CloseHandle(handle);
    return;
  }

  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory(handle, false));
  if (!shm->Map(length)) {
    DLOG(ERROR) << "OnBufferCreated: Map failed.";
    return;
  }

  bool inserted =
      client_buffers_.insert(std::make_pair(
                                 buffer_id,
                                 new ClientBuffer(shm.Pass(),
                                                  length))).second;
  DCHECK(inserted);
}

void VideoCaptureImpl::OnBufferDestroyed(int buffer_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  const ClientBufferMap::iterator iter = client_buffers_.find(buffer_id);
  if (iter == client_buffers_.end())
    return;

  DCHECK(!iter->second.get() || iter->second->HasOneRef())
      << "Instructed to delete buffer we are still using.";
  client_buffers_.erase(iter);
}

void VideoCaptureImpl::OnBufferReceived(
    int buffer_id,
    base::TimeTicks timestamp,
    const base::DictionaryValue& metadata,
    media::VideoFrame::Format pixel_format,
    media::VideoFrame::StorageType storage_type,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gpu::MailboxHolder& mailbox_holder) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (state_ != VIDEO_CAPTURE_STATE_STARTED || suspended_) {
    Send(new VideoCaptureHostMsg_BufferReady(device_id_, buffer_id, 0, -1.0));
    return;
  }
  if (first_frame_timestamp_.is_null())
    first_frame_timestamp_ = timestamp;

  // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
  TRACE_EVENT_INSTANT2("cast_perf_test", "OnBufferReceived",
                       TRACE_EVENT_SCOPE_THREAD, "timestamp",
                       timestamp.ToInternalValue(), "time_delta",
                       (timestamp - first_frame_timestamp_).ToInternalValue());

  scoped_refptr<media::VideoFrame> frame;
  uint32* release_sync_point_storage = nullptr;
  scoped_refptr<ClientBuffer> buffer;

  if (mailbox_holder.mailbox.IsZero()) {
    DCHECK_EQ(media::VideoFrame::I420, pixel_format);
    const ClientBufferMap::const_iterator iter =
        client_buffers_.find(buffer_id);
    DCHECK(iter != client_buffers_.end());
    buffer = iter->second;
    frame = media::VideoFrame::WrapExternalSharedMemory(
        pixel_format,
        coded_size,
        visible_rect,
        gfx::Size(visible_rect.width(), visible_rect.height()),
        reinterpret_cast<uint8*>(buffer->buffer->memory()),
        buffer->buffer_size,
        buffer->buffer->handle(),
        0  /* shared_memory_offset */,
        timestamp - first_frame_timestamp_);

  } else {
    DCHECK_EQ(media::VideoFrame::ARGB, pixel_format);
    DCHECK(mailbox_holder.mailbox.Verify());  // Paranoia?
    // To be deleted in DidFinishConsumingFrame().
    release_sync_point_storage = new uint32(0);
    frame = media::VideoFrame::WrapNativeTexture(
        pixel_format,
        mailbox_holder,
        base::Bind(&SaveReleaseSyncPoint, release_sync_point_storage),
        coded_size,
        gfx::Rect(coded_size),
        coded_size,
        timestamp - first_frame_timestamp_);
  }
  frame->AddDestructionObserver(
      base::Bind(&VideoCaptureImpl::DidFinishConsumingFrame,
                 frame->metadata(),
                 release_sync_point_storage,
                 media::BindToCurrentLoop(base::Bind(
                     &VideoCaptureImpl::OnClientBufferFinished,
                     weak_factory_.GetWeakPtr(),
                     buffer_id,
                     buffer))));

  frame->metadata()->MergeInternalValuesFrom(metadata);

  for (const auto& client : clients_)
    client.second.deliver_frame_cb.Run(frame, timestamp);
}

void VideoCaptureImpl::OnClientBufferFinished(
    int buffer_id,
    const scoped_refptr<ClientBuffer>& /* ignored_buffer */,
    uint32 release_sync_point,
    double consumer_resource_utilization) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  Send(new VideoCaptureHostMsg_BufferReady(device_id_,
                                           buffer_id,
                                           release_sync_point,
                                           consumer_resource_utilization));
}

void VideoCaptureImpl::OnStateChanged(VideoCaptureState state) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  switch (state) {
    case VIDEO_CAPTURE_STATE_STARTED:
      // Camera has started in the browser process. Since we have already
      // told all clients that we have started there's nothing to do.
      break;
    case VIDEO_CAPTURE_STATE_STOPPED:
      state_ = VIDEO_CAPTURE_STATE_STOPPED;
      DVLOG(1) << "OnStateChanged: stopped!, device_id = " << device_id_;
      client_buffers_.clear();
      weak_factory_.InvalidateWeakPtrs();
      if (!clients_.empty() || !clients_pending_on_restart_.empty())
        RestartCapture();
      break;
    case VIDEO_CAPTURE_STATE_PAUSED:
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(VIDEO_CAPTURE_STATE_PAUSED);
      break;
    case VIDEO_CAPTURE_STATE_ERROR:
      DVLOG(1) << "OnStateChanged: error!, device_id = " << device_id_;
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(VIDEO_CAPTURE_STATE_ERROR);
      clients_.clear();
      state_ = VIDEO_CAPTURE_STATE_ERROR;
      break;
    case VIDEO_CAPTURE_STATE_ENDED:
      DVLOG(1) << "OnStateChanged: ended!, device_id = " << device_id_;
      for (const auto& client : clients_) {
        // We'll only notify the client that the stream has stopped.
        client.second.state_update_cb.Run(VIDEO_CAPTURE_STATE_STOPPED);
      }
      clients_.clear();
      state_ = VIDEO_CAPTURE_STATE_ENDED;
      break;
    default:
      break;
  }
}

void VideoCaptureImpl::OnDeviceSupportedFormatsEnumerated(
    const media::VideoCaptureFormats& supported_formats) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  for (size_t i = 0; i < device_formats_cb_queue_.size(); ++i)
    device_formats_cb_queue_[i].Run(supported_formats);
  device_formats_cb_queue_.clear();
}

void VideoCaptureImpl::OnDeviceFormatsInUseReceived(
    const media::VideoCaptureFormats& formats_in_use) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  for (size_t i = 0; i < device_formats_in_use_cb_queue_.size(); ++i)
    device_formats_in_use_cb_queue_[i].Run(formats_in_use);
  device_formats_in_use_cb_queue_.clear();
}

void VideoCaptureImpl::OnDelegateAdded(int32 device_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "OnDelegateAdded: device_id " << device_id;

  device_id_ = device_id;
  ClientInfoMap::iterator it = clients_pending_on_filter_.begin();
  while (it != clients_pending_on_filter_.end()) {
    const int client_id = it->first;
    const ClientInfo client_info = it->second;
    clients_pending_on_filter_.erase(it++);
    StartCapture(client_id, client_info.params, client_info.state_update_cb,
                 client_info.deliver_frame_cb);
  }
}

void VideoCaptureImpl::StopDevice() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    state_ = VIDEO_CAPTURE_STATE_STOPPING;
    Send(new VideoCaptureHostMsg_Stop(device_id_));
    params_.requested_format.frame_size.SetSize(0, 0);
  }
}

void VideoCaptureImpl::RestartCapture() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, VIDEO_CAPTURE_STATE_STOPPED);

  int width = 0;
  int height = 0;
  clients_.insert(clients_pending_on_restart_.begin(),
                  clients_pending_on_restart_.end());
  clients_pending_on_restart_.clear();
  for (const auto& client : clients_) {
    width = std::max(width,
                     client.second.params.requested_format.frame_size.width());
    height = std::max(
        height, client.second.params.requested_format.frame_size.height());
  }
  params_.requested_format.frame_size.SetSize(width, height);
  DVLOG(1) << "RestartCapture, "
           << params_.requested_format.frame_size.ToString();
  StartCaptureInternal();
}

void VideoCaptureImpl::StartCaptureInternal() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(device_id_);

  Send(new VideoCaptureHostMsg_Start(device_id_, session_id_, params_));
  state_ = VIDEO_CAPTURE_STATE_STARTED;
}

void VideoCaptureImpl::Send(IPC::Message* message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  message_filter_->Send(message);
}

bool VideoCaptureImpl::RemoveClient(int client_id, ClientInfoMap* clients) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  bool found = false;

  const ClientInfoMap::iterator it = clients->find(client_id);
  if (it != clients->end()) {
    it->second.state_update_cb.Run(VIDEO_CAPTURE_STATE_STOPPED);
    clients->erase(it);
    found = true;
  }
  return found;
}

// static
void VideoCaptureImpl::DidFinishConsumingFrame(
    const media::VideoFrameMetadata* metadata,
    uint32* release_sync_point_storage,
    const base::Callback<void(uint32, double)>& callback_to_io_thread) {
  // Note: This function may be called on any thread by the VideoFrame
  // destructor.  |metadata| is still valid for read-access at this point.

  uint32 release_sync_point = 0u;
  if (release_sync_point_storage) {
    release_sync_point = *release_sync_point_storage;
    delete release_sync_point_storage;
  }

  double consumer_resource_utilization = -1.0;
  if (!metadata->GetDouble(media::VideoFrameMetadata::RESOURCE_UTILIZATION,
                           &consumer_resource_utilization)) {
    consumer_resource_utilization = -1.0;
  }

  callback_to_io_thread.Run(release_sync_point, consumer_resource_utilization);
}

}  // namespace content
