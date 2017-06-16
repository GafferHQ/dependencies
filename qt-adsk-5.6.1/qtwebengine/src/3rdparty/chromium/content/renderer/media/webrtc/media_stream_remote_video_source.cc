// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_remote_video_source.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "content/renderer/media/webrtc/track_observer.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/libjingle/source/talk/media/base/videoframe.h"

namespace content {

// Internal class used for receiving frames from the webrtc track on a
// libjingle thread and forward it to the IO-thread.
class MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate
    : public base::RefCountedThreadSafe<RemoteVideoSourceDelegate>,
      public webrtc::VideoRendererInterface {
 public:
  RemoteVideoSourceDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      const VideoCaptureDeliverFrameCB& new_frame_callback);

 protected:
  friend class base::RefCountedThreadSafe<RemoteVideoSourceDelegate>;
  ~RemoteVideoSourceDelegate() override;

  // Implements webrtc::VideoRendererInterface used for receiving video frames
  // from the PeerConnection video track. May be called on a libjingle internal
  // thread.
  void RenderFrame(const cricket::VideoFrame* frame) override;

  void DoRenderFrameOnIOThread(
      const scoped_refptr<media::VideoFrame>& video_frame);

 private:
  // Bound to the render thread.
  base::ThreadChecker thread_checker_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // |frame_callback_| is accessed on the IO thread.
  VideoCaptureDeliverFrameCB frame_callback_;
};

MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate::
    RemoteVideoSourceDelegate(
        scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
        const VideoCaptureDeliverFrameCB& new_frame_callback)
    : io_task_runner_(io_task_runner), frame_callback_(new_frame_callback) {
}

MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::~RemoteVideoSourceDelegate() {
}

void MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate::RenderFrame(
    const cricket::VideoFrame* incoming_frame) {
  TRACE_EVENT0("webrtc", "RemoteVideoSourceDelegate::RenderFrame");
  base::TimeDelta timestamp = base::TimeDelta::FromMicroseconds(
      incoming_frame->GetElapsedTime() / rtc::kNumNanosecsPerMicrosec);

  scoped_refptr<media::VideoFrame> video_frame;
  if (incoming_frame->GetNativeHandle() != NULL) {
    video_frame =
        static_cast<media::VideoFrame*>(incoming_frame->GetNativeHandle());
    video_frame->set_timestamp(timestamp);
  } else {
    const cricket::VideoFrame* frame =
        incoming_frame->GetCopyWithRotationApplied();

    gfx::Size size(frame->GetWidth(), frame->GetHeight());

    // Non-square pixels are unsupported.
    DCHECK_EQ(frame->GetPixelWidth(), 1u);
    DCHECK_EQ(frame->GetPixelHeight(), 1u);

    // Make a shallow copy. Both |frame| and |video_frame| will share a single
    // reference counted frame buffer. Const cast and hope no one will overwrite
    // the data.
    // TODO(magjed): Update media::VideoFrame to support const data so we don't
    // need to const cast here.
    video_frame = media::VideoFrame::WrapExternalYuvData(
        media::VideoFrame::YV12, size, gfx::Rect(size), size,
        frame->GetYPitch(), frame->GetUPitch(), frame->GetVPitch(),
        const_cast<uint8_t*>(frame->GetYPlane()),
        const_cast<uint8_t*>(frame->GetUPlane()),
        const_cast<uint8_t*>(frame->GetVPlane()), timestamp);
    video_frame->AddDestructionObserver(
        base::Bind(&base::DeletePointer<cricket::VideoFrame>, frame->Copy()));
  }

  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RemoteVideoSourceDelegate::DoRenderFrameOnIOThread,
                            this, video_frame));
}

void MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::DoRenderFrameOnIOThread(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("webrtc", "RemoteVideoSourceDelegate::DoRenderFrameOnIOThread");
  // TODO(hclam): Give the estimated capture time.
  frame_callback_.Run(video_frame, base::TimeTicks());
}

MediaStreamRemoteVideoSource::MediaStreamRemoteVideoSource(
    scoped_ptr<TrackObserver> observer)
    : observer_(observer.Pass()) {
  // The callback will be automatically cleared when 'observer_' goes out of
  // scope and no further callbacks will occur.
  observer_->SetCallback(base::Bind(&MediaStreamRemoteVideoSource::OnChanged,
      base::Unretained(this)));
}

MediaStreamRemoteVideoSource::~MediaStreamRemoteVideoSource() {
  DCHECK(CalledOnValidThread());
}

void MediaStreamRemoteVideoSource::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height,
    double max_requested_frame_rate,
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(CalledOnValidThread());
  media::VideoCaptureFormats formats;
  // Since the remote end is free to change the resolution at any point in time
  // the supported formats are unknown.
  callback.Run(formats);
}

void MediaStreamRemoteVideoSource::StartSourceImpl(
    const media::VideoCaptureFormat& format,
    const blink::WebMediaConstraints& constraints,
    const VideoCaptureDeliverFrameCB& frame_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!delegate_.get());
  delegate_ = new RemoteVideoSourceDelegate(io_task_runner(), frame_callback);
  scoped_refptr<webrtc::VideoTrackInterface> video_track(
      static_cast<webrtc::VideoTrackInterface*>(observer_->track().get()));
  video_track->AddRenderer(delegate_.get());
  OnStartDone(MEDIA_DEVICE_OK);
}

void MediaStreamRemoteVideoSource::StopSourceImpl() {
  DCHECK(CalledOnValidThread());
  DCHECK(state() != MediaStreamVideoSource::ENDED);
  scoped_refptr<webrtc::VideoTrackInterface> video_track(
      static_cast<webrtc::VideoTrackInterface*>(observer_->track().get()));
  video_track->RemoveRenderer(delegate_.get());
}

webrtc::VideoRendererInterface*
MediaStreamRemoteVideoSource::RenderInterfaceForTest() {
  return delegate_.get();
}

void MediaStreamRemoteVideoSource::OnChanged(
    webrtc::MediaStreamTrackInterface::TrackState state) {
  DCHECK(CalledOnValidThread());
  switch (state) {
    case webrtc::MediaStreamTrackInterface::kInitializing:
      // Ignore the kInitializing state since there is no match in
      // WebMediaStreamSource::ReadyState.
      break;
    case webrtc::MediaStreamTrackInterface::kLive:
      SetReadyState(blink::WebMediaStreamSource::ReadyStateLive);
      break;
    case webrtc::MediaStreamTrackInterface::kEnded:
      SetReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace content
