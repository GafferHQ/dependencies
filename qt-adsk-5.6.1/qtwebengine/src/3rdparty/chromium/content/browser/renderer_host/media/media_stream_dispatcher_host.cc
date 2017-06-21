// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/common/media/media_stream_messages.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/browser/render_process_host.h"
#include "url/gurl.h"

namespace content {

MediaStreamDispatcherHost::MediaStreamDispatcherHost(
    int render_process_id,
    const ResourceContext::SaltCallback& salt_callback,
    MediaStreamManager* media_stream_manager)
    : BrowserMessageFilter(MediaStreamMsgStart),
      render_process_id_(render_process_id),
      salt_callback_(salt_callback),
      media_stream_manager_(media_stream_manager) {
}

void MediaStreamDispatcherHost::StreamGenerated(
    int render_frame_id,
    int page_request_id,
    const std::string& label,
    const StreamDeviceInfoArray& audio_devices,
    const StreamDeviceInfoArray& video_devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "MediaStreamDispatcherHost::StreamGenerated("
           << ", {label = " << label <<  "})";

  Send(new MediaStreamMsg_StreamGenerated(
      render_frame_id, page_request_id, label, audio_devices, video_devices));
}

void MediaStreamDispatcherHost::StreamGenerationFailed(
    int render_frame_id,
    int page_request_id,
    content::MediaStreamRequestResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "MediaStreamDispatcherHost::StreamGenerationFailed("
           << ", {page_request_id = " << page_request_id <<  "}"
           << ", { result= " << result << "})";


  Send(new MediaStreamMsg_StreamGenerationFailed(render_frame_id,
                                                 page_request_id,
                                                 result));
}

void MediaStreamDispatcherHost::DeviceStopped(int render_frame_id,
                                              const std::string& label,
                                              const StreamDeviceInfo& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "MediaStreamDispatcherHost::DeviceStopped("
           << "{label = " << label << "}, "
           << "{type = " << device.device.type << "}, "
           << "{device_id = " << device.device.id << "})";

  Send(new MediaStreamMsg_DeviceStopped(render_frame_id, label, device));
}

void MediaStreamDispatcherHost::DevicesEnumerated(
    int render_frame_id,
    int page_request_id,
    const std::string& label,
    const StreamDeviceInfoArray& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "MediaStreamDispatcherHost::DevicesEnumerated("
           << ", {page_request_id = " << page_request_id <<  "})";

  Send(new MediaStreamMsg_DevicesEnumerated(render_frame_id, page_request_id,
                                            devices));
}

void MediaStreamDispatcherHost::DeviceOpened(
    int render_frame_id,
    int page_request_id,
    const std::string& label,
    const StreamDeviceInfo& video_device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "MediaStreamDispatcherHost::DeviceOpened("
           << ", {page_request_id = " << page_request_id <<  "})";

  Send(new MediaStreamMsg_DeviceOpened(
      render_frame_id, page_request_id, label, video_device));
}

bool MediaStreamDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaStreamDispatcherHost, message)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_GenerateStream, OnGenerateStream)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_CancelGenerateStream,
                        OnCancelGenerateStream)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_StopStreamDevice,
                        OnStopStreamDevice)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_EnumerateDevices,
                        OnEnumerateDevices)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_CancelEnumerateDevices,
                        OnCancelEnumerateDevices)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_OpenDevice,
                        OnOpenDevice)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_CloseDevice,
                        OnCloseDevice)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaStreamDispatcherHost::OnChannelClosing() {
  DVLOG(1) << "MediaStreamDispatcherHost::OnChannelClosing";

  // Since the IPC sender is gone, close all requesting/requested streams.
  media_stream_manager_->CancelAllRequests(render_process_id_);
}

MediaStreamDispatcherHost::~MediaStreamDispatcherHost() {
}

void MediaStreamDispatcherHost::OnGenerateStream(
    int render_frame_id,
    int page_request_id,
    const StreamOptions& components,
    const GURL& security_origin,
    bool user_gesture) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnGenerateStream("
           << render_frame_id << ", "
           << page_request_id << ", ["
           << " audio:" << components.audio_requested
           << " video:" << components.video_requested
           << " ], "
           << security_origin.spec()
           << ", " << user_gesture << ")";

  if (!IsURLAllowed(security_origin))
    return;

  media_stream_manager_->GenerateStream(
      this, render_process_id_, render_frame_id, salt_callback_,
      page_request_id, components, security_origin, user_gesture);
}

void MediaStreamDispatcherHost::OnCancelGenerateStream(int render_frame_id,
                                                       int page_request_id) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnCancelGenerateStream("
           << render_frame_id << ", "
           << page_request_id << ")";
  media_stream_manager_->CancelRequest(render_process_id_, render_frame_id,
                                       page_request_id);
}

void MediaStreamDispatcherHost::OnStopStreamDevice(
    int render_frame_id,
    const std::string& device_id) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnStopStreamDevice("
           << render_frame_id << ", "
           << device_id << ")";
  media_stream_manager_->StopStreamDevice(render_process_id_, render_frame_id,
                                          device_id);
}

void MediaStreamDispatcherHost::OnEnumerateDevices(
    int render_frame_id,
    int page_request_id,
    MediaStreamType type,
    const GURL& security_origin) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnEnumerateDevices("
           << render_frame_id << ", "
           << page_request_id << ", "
           << type << ", "
           << security_origin.spec() << ")";

  if (!IsURLAllowed(security_origin))
    return;

  media_stream_manager_->EnumerateDevices(
      this, render_process_id_, render_frame_id, salt_callback_,
      page_request_id, type, security_origin);
}

void MediaStreamDispatcherHost::OnCancelEnumerateDevices(
    int render_frame_id,
    int page_request_id) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnCancelEnumerateDevices("
           << render_frame_id << ", "
           << page_request_id << ")";
  media_stream_manager_->CancelRequest(render_process_id_, render_frame_id,
                                       page_request_id);
}

void MediaStreamDispatcherHost::OnOpenDevice(
    int render_frame_id,
    int page_request_id,
    const std::string& device_id,
    MediaStreamType type,
    const GURL& security_origin) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnOpenDevice("
           << render_frame_id << ", "
           << page_request_id << ", device_id: "
           << device_id.c_str() << ", type: "
           << type << ", "
           << security_origin.spec() << ")";

  if (!IsURLAllowed(security_origin))
    return;

  media_stream_manager_->OpenDevice(
      this, render_process_id_, render_frame_id, salt_callback_,
      page_request_id, device_id, type, security_origin);
}

void MediaStreamDispatcherHost::OnCloseDevice(
    int render_frame_id,
    const std::string& label) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnCloseDevice("
           << render_frame_id << ", "
           << label << ")";

  media_stream_manager_->CancelRequest(label);
}

bool MediaStreamDispatcherHost::IsURLAllowed(const GURL& url) {
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          render_process_id_, url)) {
    LOG(ERROR) << "MSDH: Renderer requested a URL it's not allowed to use.";
    return false;
  }

  return true;
}

}  // namespace content
