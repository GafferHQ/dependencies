// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_message_filter.h"

#include <string>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/media/webrtc_logging.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_logging.h"

namespace content {

namespace {
const int kStreamIDNotSet = -1;
}

class AudioMessageFilter::AudioOutputIPCImpl
    : public NON_EXPORTED_BASE(media::AudioOutputIPC) {
 public:
  AudioOutputIPCImpl(const scoped_refptr<AudioMessageFilter>& filter,
                     int render_frame_id);
  ~AudioOutputIPCImpl() override;

  // media::AudioOutputIPC implementation.
  void CreateStream(media::AudioOutputIPCDelegate* delegate,
                    const media::AudioParameters& params,
                    int session_id) override;
  void PlayStream() override;
  void PauseStream() override;
  void CloseStream() override;
  void SetVolume(double volume) override;
  void SwitchOutputDevice(const std::string& device_id,
                          const GURL& security_origin,
                          int request_id) override;

 private:
  const scoped_refptr<AudioMessageFilter> filter_;
  const int render_frame_id_;
  int stream_id_;
};

AudioMessageFilter* AudioMessageFilter::g_filter = NULL;

AudioMessageFilter::AudioMessageFilter(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : sender_(NULL), io_task_runner_(io_task_runner) {
  DCHECK(!g_filter);
  g_filter = this;
}

AudioMessageFilter::~AudioMessageFilter() {
  DCHECK_EQ(g_filter, this);
  g_filter = NULL;
}

// static
AudioMessageFilter* AudioMessageFilter::Get() {
  return g_filter;
}

AudioMessageFilter::AudioOutputIPCImpl::AudioOutputIPCImpl(
    const scoped_refptr<AudioMessageFilter>& filter,
    int render_frame_id)
    : filter_(filter),
      render_frame_id_(render_frame_id),
      stream_id_(kStreamIDNotSet) {}

AudioMessageFilter::AudioOutputIPCImpl::~AudioOutputIPCImpl() {}

scoped_ptr<media::AudioOutputIPC> AudioMessageFilter::CreateAudioOutputIPC(
    int render_frame_id) {
  DCHECK_GT(render_frame_id, 0);
  return scoped_ptr<media::AudioOutputIPC>(
      new AudioOutputIPCImpl(this, render_frame_id));
}

void AudioMessageFilter::AudioOutputIPCImpl::CreateStream(
    media::AudioOutputIPCDelegate* delegate,
    const media::AudioParameters& params,
    int session_id) {
  DCHECK(filter_->io_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate);
  DCHECK_EQ(stream_id_, kStreamIDNotSet);
  stream_id_ = filter_->delegates_.Add(delegate);
  filter_->Send(new AudioHostMsg_CreateStream(stream_id_, render_frame_id_,
                                              session_id, params));
}

void AudioMessageFilter::AudioOutputIPCImpl::PlayStream() {
  DCHECK_NE(stream_id_, kStreamIDNotSet);
  filter_->Send(new AudioHostMsg_PlayStream(stream_id_));
}

void AudioMessageFilter::AudioOutputIPCImpl::PauseStream() {
  DCHECK_NE(stream_id_, kStreamIDNotSet);
  filter_->Send(new AudioHostMsg_PauseStream(stream_id_));
}

void AudioMessageFilter::AudioOutputIPCImpl::CloseStream() {
  DCHECK(filter_->io_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(stream_id_, kStreamIDNotSet);
  filter_->Send(new AudioHostMsg_CloseStream(stream_id_));
  filter_->delegates_.Remove(stream_id_);
  stream_id_ = kStreamIDNotSet;
}

void AudioMessageFilter::AudioOutputIPCImpl::SetVolume(double volume) {
  DCHECK_NE(stream_id_, kStreamIDNotSet);
  filter_->Send(new AudioHostMsg_SetVolume(stream_id_, volume));
}

void AudioMessageFilter::AudioOutputIPCImpl::SwitchOutputDevice(
    const std::string& device_id,
    const GURL& security_origin,
    int request_id) {
  DCHECK_NE(stream_id_, kStreamIDNotSet);
  DVLOG(1) << __FUNCTION__
           << "(" << device_id << ", " << security_origin << ")";
  filter_->Send(new AudioHostMsg_SwitchOutputDevice(
      stream_id_, render_frame_id_, device_id, security_origin, request_id));
}

void AudioMessageFilter::Send(IPC::Message* message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!sender_) {
    delete message;
  } else {
    sender_->Send(message);
  }
}

bool AudioMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AudioMessageFilter, message)
    IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamCreated, OnStreamCreated)
    IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamStateChanged, OnStreamStateChanged)
    IPC_MESSAGE_HANDLER(AudioMsg_NotifyOutputDeviceSwitched,
                        OnOutputDeviceSwitched)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AudioMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  sender_ = sender;
}

void AudioMessageFilter::OnFilterRemoved() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // Once removed, a filter will not be used again.  At this time all
  // delegates must be notified so they release their reference.
  OnChannelClosing();
}

void AudioMessageFilter::OnChannelClosing() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  sender_ = NULL;

  DLOG_IF(WARNING, !delegates_.IsEmpty())
      << "Not all audio devices have been closed.";

  IDMap<media::AudioOutputIPCDelegate>::iterator it(&delegates_);
  while (!it.IsAtEnd()) {
    it.GetCurrentValue()->OnIPCClosed();
    delegates_.Remove(it.GetCurrentKey());
    it.Advance();
  }
}

void AudioMessageFilter::OnStreamCreated(
    int stream_id,
    base::SharedMemoryHandle handle,
    base::SyncSocket::TransitDescriptor socket_descriptor,
    uint32 length) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  WebRtcLogMessage(base::StringPrintf(
      "AMF::OnStreamCreated. stream_id=%d",
      stream_id));

  base::SyncSocket::Handle socket_handle =
      base::SyncSocket::UnwrapHandle(socket_descriptor);

  media::AudioOutputIPCDelegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got OnStreamCreated() event for a non-existent or removed"
                  << " audio renderer. (stream_id=" << stream_id << ").";
    base::SharedMemory::CloseHandle(handle);
    base::SyncSocket socket(socket_handle);
    return;
  }
  delegate->OnStreamCreated(handle, socket_handle, length);
}

void AudioMessageFilter::OnStreamStateChanged(
    int stream_id, media::AudioOutputIPCDelegateState state) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  media::AudioOutputIPCDelegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got OnStreamStateChanged() event for a non-existent or"
                  << " removed audio renderer.  State: " << state;
    return;
  }
  delegate->OnStateChanged(state);
}

void AudioMessageFilter::OnOutputDeviceSwitched(
    int stream_id,
    int request_id,
    media::SwitchOutputDeviceResult result) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__
           << "(" << stream_id << ", " << request_id << ", " << result << ")";
  media::AudioOutputIPCDelegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got OnOutputDeviceSwitched() event for a nonexistent or"
                  << " removed audio renderer.  State: " << result;
    return;
  }
  delegate->OnOutputDeviceSwitched(request_id, result);
}

}  // namespace content
