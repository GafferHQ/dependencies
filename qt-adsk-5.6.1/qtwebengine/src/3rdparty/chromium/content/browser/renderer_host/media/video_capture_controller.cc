// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_controller.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"
#include "content/browser/renderer_host/media/video_capture_device_client.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/video_frame.h"

#if !defined(OS_ANDROID)
#include "content/browser/compositor/image_transport_factory.h"
#endif

using media::VideoCaptureFormat;
using media::VideoFrame;
using media::VideoFrameMetadata;

namespace content {

namespace {

static const int kInfiniteRatio = 99999;

#define UMA_HISTOGRAM_ASPECT_RATIO(name, width, height) \
    UMA_HISTOGRAM_SPARSE_SLOWLY( \
        name, \
        (height) ? ((width) * 100) / (height) : kInfiniteRatio);

class SyncPointClientImpl : public VideoFrame::SyncPointClient {
 public:
  explicit SyncPointClientImpl(GLHelper* gl_helper) : gl_helper_(gl_helper) {}
  ~SyncPointClientImpl() override {}
  uint32 InsertSyncPoint() override { return gl_helper_->InsertSyncPoint(); }
  void WaitSyncPoint(uint32 sync_point) override {
    gl_helper_->WaitSyncPoint(sync_point);
  }

 private:
  GLHelper* gl_helper_;
};

void ReturnVideoFrame(const scoped_refptr<VideoFrame>& video_frame,
                      uint32 sync_point) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_ANDROID)
  NOTREACHED();
#else
  GLHelper* gl_helper = ImageTransportFactory::GetInstance()->GetGLHelper();
  // UpdateReleaseSyncPoint() creates a new sync_point using |gl_helper|, so
  // wait the given |sync_point| using |gl_helper|.
  if (gl_helper) {
    gl_helper->WaitSyncPoint(sync_point);
    SyncPointClientImpl client(gl_helper);
    video_frame->UpdateReleaseSyncPoint(&client);
  }
#endif
}

}  // anonymous namespace

struct VideoCaptureController::ControllerClient {
  ControllerClient(VideoCaptureControllerID id,
                   VideoCaptureControllerEventHandler* handler,
                   base::ProcessHandle render_process,
                   media::VideoCaptureSessionId session_id,
                   const media::VideoCaptureParams& params)
      : controller_id(id),
        event_handler(handler),
        render_process_handle(render_process),
        session_id(session_id),
        parameters(params),
        session_closed(false),
        paused(false) {}

  ~ControllerClient() {}

  // ID used for identifying this object.
  const VideoCaptureControllerID controller_id;
  VideoCaptureControllerEventHandler* const event_handler;

  // Handle to the render process that will receive the capture buffers.
  const base::ProcessHandle render_process_handle;
  const media::VideoCaptureSessionId session_id;
  const media::VideoCaptureParams parameters;

  // Buffers that are currently known to this client.
  std::set<int> known_buffers;

  // Buffers currently held by this client, and syncpoint callback to call when
  // they are returned from the client.
  typedef std::map<int, scoped_refptr<VideoFrame>> ActiveBufferMap;
  ActiveBufferMap active_buffers;

  // State of capture session, controlled by VideoCaptureManager directly. This
  // transitions to true as soon as StopSession() occurs, at which point the
  // client is sent an OnEnded() event. However, because the client retains a
  // VideoCaptureController* pointer, its ControllerClient entry lives on until
  // it unregisters itself via RemoveClient(), which may happen asynchronously.
  //
  // TODO(nick): If we changed the semantics of VideoCaptureHost so that
  // OnEnded() events were processed synchronously (with the RemoveClient() done
  // implicitly), we could avoid tracking this state here in the Controller, and
  // simplify the code in both places.
  bool session_closed;

  // Indicates whether the client is paused, if true, VideoCaptureController
  // stops updating its buffer.
  bool paused;
};

VideoCaptureController::VideoCaptureController(int max_buffers)
    : buffer_pool_(new VideoCaptureBufferPool(max_buffers)),
      state_(VIDEO_CAPTURE_STATE_STARTED),
      has_received_frames_(false),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

base::WeakPtr<VideoCaptureController>
VideoCaptureController::GetWeakPtrForIOThread() {
  return weak_ptr_factory_.GetWeakPtr();
}

scoped_ptr<media::VideoCaptureDevice::Client>
VideoCaptureController::NewDeviceClient(
    const scoped_refptr<base::SingleThreadTaskRunner>& capture_task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return make_scoped_ptr(new VideoCaptureDeviceClient(
      this->GetWeakPtrForIOThread(), buffer_pool_, capture_task_runner));
}

void VideoCaptureController::AddClient(
    VideoCaptureControllerID id,
    VideoCaptureControllerEventHandler* event_handler,
    base::ProcessHandle render_process,
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::AddClient, id " << id
           << ", " << params.requested_format.frame_size.ToString()
           << ", " << params.requested_format.frame_rate
           << ", " << session_id
           << ")";

  // If this is the first client added to the controller, cache the parameters.
  if (!controller_clients_.size())
    video_capture_format_ = params.requested_format;

  // Signal error in case device is already in error state.
  if (state_ == VIDEO_CAPTURE_STATE_ERROR) {
    event_handler->OnError(id);
    return;
  }

  // Do nothing if this client has called AddClient before.
  if (FindClient(id, event_handler, controller_clients_))
    return;

  ControllerClient* client = new ControllerClient(
      id, event_handler, render_process, session_id, params);
  // If we already have gotten frame_info from the device, repeat it to the new
  // client.
  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    controller_clients_.push_back(client);
    return;
  }
}

int VideoCaptureController::RemoveClient(
    VideoCaptureControllerID id,
    VideoCaptureControllerEventHandler* event_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::RemoveClient, id " << id;

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);
  if (!client)
    return kInvalidMediaCaptureSessionId;

  // Take back all buffers held by the |client|.
  for (const auto& buffer : client->active_buffers)
    buffer_pool_->RelinquishConsumerHold(buffer.first, 1);
  client->active_buffers.clear();

  int session_id = client->session_id;
  controller_clients_.remove(client);
  delete client;

  return session_id;
}

void VideoCaptureController::PauseOrResumeClient(
    VideoCaptureControllerID id,
    VideoCaptureControllerEventHandler* event_handler,
    bool pause) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::PauseOrResumeClient, id "
           << id << ", " << pause;

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);
  if (!client)
    return;

  DLOG_IF(WARNING, client->paused == pause) << "Redundant client configuration";
  client->paused = pause;
}

void VideoCaptureController::StopSession(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::StopSession, id " << session_id;

  ControllerClient* client = FindClient(session_id, controller_clients_);

  if (client) {
    client->session_closed = true;
    client->event_handler->OnEnded(client->controller_id);
  }
}

void VideoCaptureController::ReturnBuffer(
    VideoCaptureControllerID id,
    VideoCaptureControllerEventHandler* event_handler,
    int buffer_id,
    uint32 sync_point,
    double consumer_resource_utilization) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);

  // If this buffer is not held by this client, or this client doesn't exist
  // in controller, do nothing.
  ControllerClient::ActiveBufferMap::iterator iter;
  if (!client || (iter = client->active_buffers.find(buffer_id)) ==
                     client->active_buffers.end()) {
    NOTREACHED();
    return;
  }

  // Set the RESOURCE_UTILIZATION to the maximum of those provided by each
  // consumer (via separate calls to this method that refer to the same
  // VideoFrame).  The producer of this VideoFrame may check this value, after
  // all consumer holds are relinquished, to make quality versus performance
  // trade-off decisions.
  scoped_refptr<VideoFrame> frame = iter->second;
  if (std::isfinite(consumer_resource_utilization) &&
      consumer_resource_utilization >= 0.0) {
    double resource_utilization = -1.0;
    if (frame->metadata()->GetDouble(VideoFrameMetadata::RESOURCE_UTILIZATION,
                                     &resource_utilization)) {
      frame->metadata()->SetDouble(VideoFrameMetadata::RESOURCE_UTILIZATION,
                                   std::max(consumer_resource_utilization,
                                            resource_utilization));
    } else {
      frame->metadata()->SetDouble(VideoFrameMetadata::RESOURCE_UTILIZATION,
                                   consumer_resource_utilization);
    }
  }

  client->active_buffers.erase(iter);
  buffer_pool_->RelinquishConsumerHold(buffer_id, 1);

#if defined(OS_ANDROID)
  DCHECK_EQ(0u, sync_point);
#endif
  if (sync_point)
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&ReturnVideoFrame, frame, sync_point));
}

const media::VideoCaptureFormat&
VideoCaptureController::GetVideoCaptureFormat() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return video_capture_format_;
}

VideoCaptureController::~VideoCaptureController() {
  STLDeleteContainerPointers(controller_clients_.begin(),
                             controller_clients_.end());
}

void VideoCaptureController::DoIncomingCapturedVideoFrameOnIOThread(
    scoped_ptr<media::VideoCaptureDevice::Client::Buffer> buffer,
    const scoped_refptr<VideoFrame>& frame,
    const base::TimeTicks& timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const int buffer_id = buffer->id();
  DCHECK_NE(buffer_id, VideoCaptureBufferPool::kInvalidId);

  int count = 0;
  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    if (!frame->metadata()->HasKey(VideoFrameMetadata::FRAME_RATE)) {
      frame->metadata()->SetDouble(VideoFrameMetadata::FRAME_RATE,
                                   video_capture_format_.frame_rate);
    }
    scoped_ptr<base::DictionaryValue> metadata(new base::DictionaryValue());
    frame->metadata()->MergeInternalValuesInto(metadata.get());

    for (const auto& client : controller_clients_) {
      if (client->session_closed || client->paused)
        continue;

      CHECK((frame->IsMappable() && frame->format() == VideoFrame::I420) ||
            (!frame->IsMappable() && frame->HasTextures() &&
             frame->format() == VideoFrame::ARGB))
          << "Format and/or storage type combination not supported (received: "
          << VideoFrame::FormatToString(frame->format()) << ")";

      if (frame->HasTextures()) {
        DCHECK(frame->coded_size() == frame->visible_rect().size())
            << "Textures are always supposed to be tightly packed.";
        DCHECK_EQ(1u, VideoFrame::NumPlanes(frame->format()));
      } else if (frame->format() == VideoFrame::I420) {
        const bool is_new_buffer =
            client->known_buffers.insert(buffer_id).second;
        if (is_new_buffer) {
          // On the first use of a buffer on a client, share the memory handle.
          size_t memory_size = 0;
          base::SharedMemoryHandle remote_handle = buffer_pool_->ShareToProcess(
              buffer_id, client->render_process_handle, &memory_size);
          client->event_handler->OnBufferCreated(
              client->controller_id, remote_handle, memory_size, buffer_id);
        }
      }

      client->event_handler->OnBufferReady(client->controller_id,
                                           buffer_id,
                                           frame,
                                           timestamp);
      const bool inserted =
          client->active_buffers.insert(std::make_pair(buffer_id, frame))
              .second;
      DCHECK(inserted) << "Unexpected duplicate buffer: " << buffer_id;
      count++;
    }
  }

  if (!has_received_frames_) {
    UMA_HISTOGRAM_COUNTS("Media.VideoCapture.Width",
                         frame->visible_rect().width());
    UMA_HISTOGRAM_COUNTS("Media.VideoCapture.Height",
                         frame->visible_rect().height());
    UMA_HISTOGRAM_ASPECT_RATIO("Media.VideoCapture.AspectRatio",
                               frame->visible_rect().width(),
                               frame->visible_rect().height());
    double frame_rate = 0.0f;
    if (!frame->metadata()->GetDouble(VideoFrameMetadata::FRAME_RATE,
                                      &frame_rate)) {
      frame_rate = video_capture_format_.frame_rate;
    }
    UMA_HISTOGRAM_COUNTS("Media.VideoCapture.FrameRate", frame_rate);
    has_received_frames_ = true;
  }

  buffer_pool_->HoldForConsumers(buffer_id, count);
}

void VideoCaptureController::DoErrorOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  state_ = VIDEO_CAPTURE_STATE_ERROR;

  for (const auto* client : controller_clients_) {
    if (client->session_closed)
       continue;
    client->event_handler->OnError(client->controller_id);
  }
}

void VideoCaptureController::DoLogOnIOThread(const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaStreamManager::SendMessageToNativeLog("Video capture: " + message);
}

void VideoCaptureController::DoBufferDestroyedOnIOThread(
    int buffer_id_to_drop) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (auto* client : controller_clients_) {
    if (client->session_closed)
      continue;

    if (client->known_buffers.erase(buffer_id_to_drop)) {
      client->event_handler->OnBufferDestroyed(client->controller_id,
                                               buffer_id_to_drop);
    }
  }
}

VideoCaptureController::ControllerClient* VideoCaptureController::FindClient(
    VideoCaptureControllerID id,
    VideoCaptureControllerEventHandler* handler,
    const ControllerClients& clients) {
  for (auto* client : clients) {
    if (client->controller_id == id && client->event_handler == handler)
      return client;
  }
  return NULL;
}

VideoCaptureController::ControllerClient* VideoCaptureController::FindClient(
    int session_id,
    const ControllerClients& clients) {
  for (auto client : clients) {
    if (client->session_id == session_id)
      return client;
  }
  return NULL;
}

int VideoCaptureController::GetClientCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return controller_clients_.size();
}

int VideoCaptureController::GetActiveClientCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int active_client_count = 0;
  for (ControllerClient* client : controller_clients_) {
    if (!client->paused)
      ++active_client_count;
  }
  return active_client_count;
}

}  // namespace content
