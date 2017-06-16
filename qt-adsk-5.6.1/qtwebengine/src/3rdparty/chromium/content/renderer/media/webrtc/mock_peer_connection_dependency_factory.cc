// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/mock_peer_connection_impl.h"
#include "content/renderer/media/webaudio_capturer_source.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "third_party/libjingle/source/talk/media/base/videocapturer.h"
#include "third_party/webrtc/base/scoped_ref_ptr.h"

using webrtc::AudioSourceInterface;
using webrtc::AudioTrackInterface;
using webrtc::AudioTrackVector;
using webrtc::IceCandidateCollection;
using webrtc::IceCandidateInterface;
using webrtc::MediaStreamInterface;
using webrtc::ObserverInterface;
using webrtc::SessionDescriptionInterface;
using webrtc::VideoRendererInterface;
using webrtc::VideoSourceInterface;
using webrtc::VideoTrackInterface;
using webrtc::VideoTrackVector;

namespace content {

template <class V>
static typename V::iterator FindTrack(V* vector,
                                      const std::string& track_id) {
  typename V::iterator it = vector->begin();
  for (; it != vector->end(); ++it) {
    if ((*it)->id() == track_id) {
      break;
    }
  }
  return it;
};

MockMediaStream::MockMediaStream(const std::string& label) : label_(label) {}

bool MockMediaStream::AddTrack(AudioTrackInterface* track) {
  audio_track_vector_.push_back(track);
  NotifyObservers();
  return true;
}

bool MockMediaStream::AddTrack(VideoTrackInterface* track) {
  video_track_vector_.push_back(track);
  NotifyObservers();
  return true;
}

bool MockMediaStream::RemoveTrack(AudioTrackInterface* track) {
  AudioTrackVector::iterator it = FindTrack(&audio_track_vector_,
                                            track->id());
  if (it == audio_track_vector_.end())
    return false;
  audio_track_vector_.erase(it);
  NotifyObservers();
  return true;
}

bool MockMediaStream::RemoveTrack(VideoTrackInterface* track) {
  VideoTrackVector::iterator it = FindTrack(&video_track_vector_,
                                            track->id());
  if (it == video_track_vector_.end())
    return false;
  video_track_vector_.erase(it);
  NotifyObservers();
  return true;
}

std::string MockMediaStream::label() const {
  return label_;
}

AudioTrackVector MockMediaStream::GetAudioTracks() {
  return audio_track_vector_;
}

VideoTrackVector MockMediaStream::GetVideoTracks() {
  return video_track_vector_;
}

rtc::scoped_refptr<AudioTrackInterface> MockMediaStream::FindAudioTrack(
    const std::string& track_id) {
  AudioTrackVector::iterator it = FindTrack(&audio_track_vector_, track_id);
  return it == audio_track_vector_.end() ? NULL : *it;
}

rtc::scoped_refptr<VideoTrackInterface> MockMediaStream::FindVideoTrack(
    const std::string& track_id) {
  VideoTrackVector::iterator it = FindTrack(&video_track_vector_, track_id);
  return it == video_track_vector_.end() ? NULL : *it;
}

void MockMediaStream::RegisterObserver(ObserverInterface* observer) {
  DCHECK(observers_.find(observer) == observers_.end());
  observers_.insert(observer);
}

void MockMediaStream::UnregisterObserver(ObserverInterface* observer) {
  ObserverSet::iterator it = observers_.find(observer);
  DCHECK(it != observers_.end());
  observers_.erase(it);
}

void MockMediaStream::NotifyObservers() {
  for (ObserverSet::iterator it = observers_.begin(); it != observers_.end();
       ++it) {
    (*it)->OnChanged();
  }
}

MockMediaStream::~MockMediaStream() {}

class MockRtcVideoCapturer : public WebRtcVideoCapturerAdapter {
 public:
  explicit MockRtcVideoCapturer(bool is_screencast)
      : WebRtcVideoCapturerAdapter(is_screencast),
        number_of_capturered_frames_(0),
        width_(0),
        height_(0) {
  }

  void OnFrameCaptured(const scoped_refptr<media::VideoFrame>& frame) override {
    ++number_of_capturered_frames_;
    width_ = frame->visible_rect().width();
    height_ = frame->visible_rect().height();
  }

  int GetLastFrameWidth() const {
    return width_;
  }

  int GetLastFrameHeight() const {
    return height_;
  }

  int GetFrameNum() const {
    return number_of_capturered_frames_;
  }

 private:
  int number_of_capturered_frames_;
  int width_;
  int height_;
};

MockVideoRenderer::MockVideoRenderer()
    : width_(0),
      height_(0),
      num_(0) {}

MockVideoRenderer::~MockVideoRenderer() {}

bool MockVideoRenderer::SetSize(int width, int height, int reserved) {
  width_ = width;
  height_ = height;
  return true;
}

bool MockVideoRenderer::RenderFrame(const cricket::VideoFrame* frame) {
  ++num_;
  return true;
}

MockAudioSource::MockAudioSource(
    const webrtc::MediaConstraintsInterface* constraints)
    : state_(MediaSourceInterface::kLive),
      optional_constraints_(constraints->GetOptional()),
      mandatory_constraints_(constraints->GetMandatory()) {
}

MockAudioSource::~MockAudioSource() {}

void MockAudioSource::RegisterObserver(webrtc::ObserverInterface* observer) {
  DCHECK(observers_.find(observer) == observers_.end());
  observers_.insert(observer);
}

void MockAudioSource::UnregisterObserver(webrtc::ObserverInterface* observer) {
  DCHECK(observers_.find(observer) != observers_.end());
  observers_.erase(observer);
}

webrtc::MediaSourceInterface::SourceState MockAudioSource::state() const {
  return state_;
}

MockVideoSource::MockVideoSource()
    : state_(MediaSourceInterface::kInitializing) {
}

MockVideoSource::~MockVideoSource() {}

void MockVideoSource::SetVideoCapturer(cricket::VideoCapturer* capturer) {
  capturer_.reset(capturer);
}

cricket::VideoCapturer* MockVideoSource::GetVideoCapturer() {
  return capturer_.get();
}

void MockVideoSource::AddSink(cricket::VideoRenderer* output) {
  NOTIMPLEMENTED();
}

void MockVideoSource::RemoveSink(cricket::VideoRenderer* output) {
  NOTIMPLEMENTED();
}

void MockVideoSource::Stop() {
  NOTIMPLEMENTED();
}
void MockVideoSource::Restart() {
  NOTIMPLEMENTED();
}

cricket::VideoRenderer* MockVideoSource::FrameInput() {
  return &renderer_;
}

void MockVideoSource::RegisterObserver(webrtc::ObserverInterface* observer) {
  observers_.push_back(observer);
}

void MockVideoSource::UnregisterObserver(webrtc::ObserverInterface* observer) {
  for (std::vector<ObserverInterface*>::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    if (*it == observer) {
      observers_.erase(it);
      break;
    }
  }
}

void MockVideoSource::FireOnChanged() {
  std::vector<ObserverInterface*> observers(observers_);
  for (std::vector<ObserverInterface*>::iterator it = observers.begin();
       it != observers.end(); ++it) {
    (*it)->OnChanged();
  }
}

void MockVideoSource::SetLive() {
  DCHECK(state_ == MediaSourceInterface::kInitializing ||
         state_ == MediaSourceInterface::kLive);
  state_ = MediaSourceInterface::kLive;
  FireOnChanged();
}

void MockVideoSource::SetEnded() {
  DCHECK_NE(MediaSourceInterface::kEnded, state_);
  state_ = MediaSourceInterface::kEnded;
  FireOnChanged();
}

webrtc::MediaSourceInterface::SourceState MockVideoSource::state() const {
  return state_;
}

const cricket::VideoOptions* MockVideoSource::options() const {
  NOTIMPLEMENTED();
  return NULL;
}

int MockVideoSource::GetLastFrameWidth() const {
  DCHECK(capturer_);
  return
      static_cast<MockRtcVideoCapturer*>(capturer_.get())->GetLastFrameWidth();
}

int MockVideoSource::GetLastFrameHeight() const {
  DCHECK(capturer_);
  return
      static_cast<MockRtcVideoCapturer*>(capturer_.get())->GetLastFrameHeight();
}

int MockVideoSource::GetFrameNum() const {
  DCHECK(capturer_);
  return static_cast<MockRtcVideoCapturer*>(capturer_.get())->GetFrameNum();
}

MockWebRtcVideoTrack::MockWebRtcVideoTrack(
    const std::string& id,
    webrtc::VideoSourceInterface* source)
    : enabled_(false),
      id_(id),
      state_(MediaStreamTrackInterface::kLive),
      source_(source),
      renderer_(NULL) {
}

MockWebRtcVideoTrack::~MockWebRtcVideoTrack() {}

void MockWebRtcVideoTrack::AddRenderer(VideoRendererInterface* renderer) {
  DCHECK(!renderer_);
  renderer_ = renderer;
}

void MockWebRtcVideoTrack::RemoveRenderer(VideoRendererInterface* renderer) {
  DCHECK_EQ(renderer_, renderer);
  renderer_ = NULL;
}

std::string MockWebRtcVideoTrack::kind() const {
  NOTIMPLEMENTED();
  return std::string();
}

std::string MockWebRtcVideoTrack::id() const { return id_; }

bool MockWebRtcVideoTrack::enabled() const { return enabled_; }

MockWebRtcVideoTrack::TrackState MockWebRtcVideoTrack::state() const {
  return state_;
}

bool MockWebRtcVideoTrack::set_enabled(bool enable) {
  enabled_ = enable;
  return true;
}

bool MockWebRtcVideoTrack::set_state(TrackState new_state) {
  state_ = new_state;
  for (auto& o : observers_)
    o->OnChanged();
  return true;
}

void MockWebRtcVideoTrack::RegisterObserver(ObserverInterface* observer) {
  DCHECK(observers_.find(observer) == observers_.end());
  observers_.insert(observer);
}

void MockWebRtcVideoTrack::UnregisterObserver(ObserverInterface* observer) {
  DCHECK(observers_.find(observer) != observers_.end());
  observers_.erase(observer);
}

VideoSourceInterface* MockWebRtcVideoTrack::GetSource() const {
  return source_.get();
}

class MockSessionDescription : public SessionDescriptionInterface {
 public:
  MockSessionDescription(const std::string& type,
                         const std::string& sdp)
      : type_(type),
        sdp_(sdp) {
  }
  ~MockSessionDescription() override {}
  cricket::SessionDescription* description() override {
    NOTIMPLEMENTED();
    return NULL;
  }
  const cricket::SessionDescription* description() const override {
    NOTIMPLEMENTED();
    return NULL;
  }
  std::string session_id() const override {
    NOTIMPLEMENTED();
    return std::string();
  }
  std::string session_version() const override {
    NOTIMPLEMENTED();
    return std::string();
  }
  std::string type() const override { return type_; }
  bool AddCandidate(const IceCandidateInterface* candidate) override {
    NOTIMPLEMENTED();
    return false;
  }
  size_t number_of_mediasections() const override {
    NOTIMPLEMENTED();
    return 0;
  }
  const IceCandidateCollection* candidates(
      size_t mediasection_index) const override {
    NOTIMPLEMENTED();
    return NULL;
  }

  bool ToString(std::string* out) const override {
    *out = sdp_;
    return true;
  }

 private:
  std::string type_;
  std::string sdp_;
};

class MockIceCandidate : public IceCandidateInterface {
 public:
  MockIceCandidate(const std::string& sdp_mid,
                   int sdp_mline_index,
                   const std::string& sdp)
      : sdp_mid_(sdp_mid),
        sdp_mline_index_(sdp_mline_index),
        sdp_(sdp) {
    // Assign an valid address to |candidate_| to pass assert in code.
    candidate_.set_address(rtc::SocketAddress("127.0.0.1", 5000));
  }
  ~MockIceCandidate() override {}
  std::string sdp_mid() const override { return sdp_mid_; }
  int sdp_mline_index() const override { return sdp_mline_index_; }
  const cricket::Candidate& candidate() const override { return candidate_; }
  bool ToString(std::string* out) const override {
    *out = sdp_;
    return true;
  }

 private:
  std::string sdp_mid_;
  int sdp_mline_index_;
  std::string sdp_;
  cricket::Candidate candidate_;
};

MockPeerConnectionDependencyFactory::MockPeerConnectionDependencyFactory()
    : PeerConnectionDependencyFactory(NULL),
      fail_to_create_next_audio_capturer_(false) {
}

MockPeerConnectionDependencyFactory::~MockPeerConnectionDependencyFactory() {}

scoped_refptr<webrtc::PeerConnectionInterface>
MockPeerConnectionDependencyFactory::CreatePeerConnection(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    const webrtc::MediaConstraintsInterface* constraints,
    blink::WebFrame* frame,
    webrtc::PeerConnectionObserver* observer) {
  return new rtc::RefCountedObject<MockPeerConnectionImpl>(this, observer);
}

scoped_refptr<webrtc::AudioSourceInterface>
MockPeerConnectionDependencyFactory::CreateLocalAudioSource(
    const webrtc::MediaConstraintsInterface* constraints) {
  last_audio_source_ =
      new rtc::RefCountedObject<MockAudioSource>(constraints);
  return last_audio_source_;
}

WebRtcVideoCapturerAdapter*
MockPeerConnectionDependencyFactory::CreateVideoCapturer(
    bool is_screen_capture) {
  return new MockRtcVideoCapturer(is_screen_capture);
}

scoped_refptr<webrtc::VideoSourceInterface>
MockPeerConnectionDependencyFactory::CreateVideoSource(
    cricket::VideoCapturer* capturer,
    const blink::WebMediaConstraints& constraints) {
  last_video_source_ = new rtc::RefCountedObject<MockVideoSource>();
  last_video_source_->SetVideoCapturer(capturer);
  return last_video_source_;
}

scoped_refptr<WebAudioCapturerSource>
MockPeerConnectionDependencyFactory::CreateWebAudioSource(
    blink::WebMediaStreamSource* source) {
  return NULL;
}

scoped_refptr<webrtc::MediaStreamInterface>
MockPeerConnectionDependencyFactory::CreateLocalMediaStream(
    const std::string& label) {
  return new rtc::RefCountedObject<MockMediaStream>(label);
}

scoped_refptr<webrtc::VideoTrackInterface>
MockPeerConnectionDependencyFactory::CreateLocalVideoTrack(
    const std::string& id,
    webrtc::VideoSourceInterface* source) {
  scoped_refptr<webrtc::VideoTrackInterface> track(
      new rtc::RefCountedObject<MockWebRtcVideoTrack>(
          id, source));
  return track;
}

scoped_refptr<webrtc::VideoTrackInterface>
MockPeerConnectionDependencyFactory::CreateLocalVideoTrack(
    const std::string& id,
    cricket::VideoCapturer* capturer) {
  scoped_refptr<MockVideoSource> source =
      new rtc::RefCountedObject<MockVideoSource>();
  source->SetVideoCapturer(capturer);

  return
      new rtc::RefCountedObject<MockWebRtcVideoTrack>(id, source.get());
}

SessionDescriptionInterface*
MockPeerConnectionDependencyFactory::CreateSessionDescription(
    const std::string& type,
    const std::string& sdp,
    webrtc::SdpParseError* error) {
  return new MockSessionDescription(type, sdp);
}

webrtc::IceCandidateInterface*
MockPeerConnectionDependencyFactory::CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const std::string& sdp) {
  return new MockIceCandidate(sdp_mid, sdp_mline_index, sdp);
}

scoped_refptr<WebRtcAudioCapturer>
MockPeerConnectionDependencyFactory::CreateAudioCapturer(
    int render_frame_id,
    const StreamDeviceInfo& device_info,
    const blink::WebMediaConstraints& constraints,
    MediaStreamAudioSource* audio_source) {
  if (fail_to_create_next_audio_capturer_) {
    fail_to_create_next_audio_capturer_ = false;
    return NULL;
  }
  DCHECK(audio_source);
  return WebRtcAudioCapturer::CreateCapturer(-1, device_info, constraints, NULL,
                                             audio_source);
}

void MockPeerConnectionDependencyFactory::StartLocalAudioTrack(
      WebRtcLocalAudioTrack* audio_track) {
  audio_track->Start();
}

}  // namespace content
