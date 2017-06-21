// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_capture_devices_impl.h"

#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

void EnsureMonitorCaptureDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamManager::EnsureDeviceMonitorStarted,
                 base::Unretained(
                     BrowserMainLoop::GetInstance()->media_stream_manager())));
}

}  // namespace

MediaCaptureDevices* MediaCaptureDevices::GetInstance() {
  return MediaCaptureDevicesImpl::GetInstance();
}

MediaCaptureDevicesImpl* MediaCaptureDevicesImpl::GetInstance() {
  return Singleton<MediaCaptureDevicesImpl>::get();
}

const MediaStreamDevices&
MediaCaptureDevicesImpl::GetAudioCaptureDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!devices_enumerated_) {
    EnsureMonitorCaptureDevices();
    devices_enumerated_ = true;
  }
  return audio_devices_;
}

const MediaStreamDevices&
MediaCaptureDevicesImpl::GetVideoCaptureDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!devices_enumerated_) {
    EnsureMonitorCaptureDevices();
    devices_enumerated_ = true;
  }
  return video_devices_;
}

void MediaCaptureDevicesImpl::OnAudioCaptureDevicesChanged(
    const MediaStreamDevices& devices) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    UpdateAudioDevicesOnUIThread(devices);
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&MediaCaptureDevicesImpl::UpdateAudioDevicesOnUIThread,
                   base::Unretained(this), devices));
  }
}

void MediaCaptureDevicesImpl::OnVideoCaptureDevicesChanged(
    const MediaStreamDevices& devices) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    UpdateVideoDevicesOnUIThread(devices);
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&MediaCaptureDevicesImpl::UpdateVideoDevicesOnUIThread,
                   base::Unretained(this), devices));
  }
}

MediaCaptureDevicesImpl::MediaCaptureDevicesImpl()
    : devices_enumerated_(false) {
}

MediaCaptureDevicesImpl::~MediaCaptureDevicesImpl() {
}

void MediaCaptureDevicesImpl::UpdateAudioDevicesOnUIThread(
    const MediaStreamDevices& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  devices_enumerated_ = true;
  audio_devices_ = devices;
}

void MediaCaptureDevicesImpl::UpdateVideoDevicesOnUIThread(
    const MediaStreamDevices& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  devices_enumerated_ = true;
  video_devices_ = devices;
}

}  // namespace content
