// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/mock_media_stream_dispatcher.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "content/renderer/media/user_media_client_impl.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaDeviceInfo.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrackSourcesRequest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebHeap.h"

namespace content {

class MockMediaStreamVideoCapturerSource : public MockMediaStreamVideoSource {
 public:
  MockMediaStreamVideoCapturerSource(
      const StreamDeviceInfo& device,
      const SourceStoppedCallback& stop_callback,
      PeerConnectionDependencyFactory* factory)
  : MockMediaStreamVideoSource(false) {
    SetDeviceInfo(device);
    SetStopCallback(stop_callback);
  }
};

class UserMediaClientImplUnderTest : public UserMediaClientImpl {
 public:
  enum RequestState {
    REQUEST_NOT_STARTED,
    REQUEST_NOT_COMPLETE,
    REQUEST_SUCCEEDED,
    REQUEST_FAILED,
  };

  UserMediaClientImplUnderTest(
      PeerConnectionDependencyFactory* dependency_factory,
      scoped_ptr<MediaStreamDispatcher> media_stream_dispatcher)
      : UserMediaClientImpl(
            NULL, dependency_factory, media_stream_dispatcher.Pass()),
        state_(REQUEST_NOT_STARTED),
        result_(NUM_MEDIA_REQUEST_RESULTS),
        result_name_(""),
        factory_(dependency_factory),
        video_source_(NULL) {
  }

  void RequestUserMedia() {
    blink::WebUserMediaRequest user_media_request;
    state_ = REQUEST_NOT_COMPLETE;
    requestUserMedia(user_media_request);
  }

  void RequestMediaDevices() {
    blink::WebMediaDevicesRequest media_devices_request;
    state_ = REQUEST_NOT_COMPLETE;
    requestMediaDevices(media_devices_request);
  }

  void RequestSources() {
    blink::WebMediaStreamTrackSourcesRequest sources_request;
    state_ = REQUEST_NOT_COMPLETE;
    requestSources(sources_request);
  }

  void GetUserMediaRequestSucceeded(
      const blink::WebMediaStream& stream,
      blink::WebUserMediaRequest request_info) override {
    last_generated_stream_ = stream;
    state_ = REQUEST_SUCCEEDED;
  }

  void GetUserMediaRequestFailed(
      blink::WebUserMediaRequest request_info,
      content::MediaStreamRequestResult result,
      const blink::WebString& result_name) override {
    last_generated_stream_.reset();
    state_ = REQUEST_FAILED;
    result_ = result;
    result_name_ = result_name;
  }

  void EnumerateDevicesSucceded(
      blink::WebMediaDevicesRequest* request,
      blink::WebVector<blink::WebMediaDeviceInfo>& devices) override {
    state_ = REQUEST_SUCCEEDED;
    last_devices_ = devices;
  }

  void EnumerateSourcesSucceded(
      blink::WebMediaStreamTrackSourcesRequest* request,
      blink::WebVector<blink::WebSourceInfo>& sources) override {
    state_ = REQUEST_SUCCEEDED;
    last_sources_ = sources;
  }

  MediaStreamVideoSource* CreateVideoSource(
      const StreamDeviceInfo& device,
      const MediaStreamSource::SourceStoppedCallback& stop_callback) override {
    video_source_ = new MockMediaStreamVideoCapturerSource(device,
                                                           stop_callback,
                                                           factory_);
    return video_source_;
  }

  const blink::WebMediaStream& last_generated_stream() {
    return last_generated_stream_;
  }

  const blink::WebVector<blink::WebMediaDeviceInfo>& last_devices() {
    return last_devices_;
  }

  const blink::WebVector<blink::WebSourceInfo>& last_sources() {
    return last_sources_;
  }

  void ClearLastGeneratedStream() {
    last_generated_stream_.reset();
  }

  MockMediaStreamVideoCapturerSource* last_created_video_source() const {
    return video_source_;
  }

  RequestState request_state() const { return state_; }
  content::MediaStreamRequestResult error_reason() const { return result_; }
  blink::WebString error_name() const { return result_name_; }

 private:
  blink::WebMediaStream last_generated_stream_;
  RequestState state_;
  content::MediaStreamRequestResult result_;
  blink::WebString result_name_;
  blink::WebVector<blink::WebMediaDeviceInfo> last_devices_;
  blink::WebVector<blink::WebSourceInfo> last_sources_;
  PeerConnectionDependencyFactory* factory_;
  MockMediaStreamVideoCapturerSource* video_source_;
};

class UserMediaClientImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Create our test object.
    child_process_.reset(new ChildProcess());
    dependency_factory_.reset(new MockPeerConnectionDependencyFactory());
    ms_dispatcher_ = new MockMediaStreamDispatcher();
    used_media_impl_.reset(new UserMediaClientImplUnderTest(
        dependency_factory_.get(),
        scoped_ptr<MediaStreamDispatcher>(ms_dispatcher_).Pass()));
  }

  void TearDown() override {
    used_media_impl_.reset();
    blink::WebHeap::collectAllGarbageForTesting();
  }

  blink::WebMediaStream RequestLocalMediaStream() {
    used_media_impl_->RequestUserMedia();
    FakeMediaStreamDispatcherRequestUserMediaComplete();
    StartMockedVideoSource();

    EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_SUCCEEDED,
              used_media_impl_->request_state());

    blink::WebMediaStream desc = used_media_impl_->last_generated_stream();
    content::MediaStream* native_stream =
        content::MediaStream::GetMediaStream(desc);
    if (!native_stream) {
      ADD_FAILURE();
      return desc;
    }

    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
    desc.audioTracks(audio_tracks);
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    desc.videoTracks(video_tracks);

    EXPECT_EQ(1u, audio_tracks.size());
    EXPECT_EQ(1u, video_tracks.size());
    EXPECT_NE(audio_tracks[0].id(), video_tracks[0].id());
    return desc;
  }

  void FakeMediaStreamDispatcherRequestUserMediaComplete() {
    // Audio request ID is used as the shared request ID.
    used_media_impl_->OnStreamGenerated(
        ms_dispatcher_->audio_input_request_id(),
        ms_dispatcher_->stream_label(),
        ms_dispatcher_->audio_input_array(),
        ms_dispatcher_->video_array());
  }

  void FakeMediaStreamDispatcherRequestMediaDevicesComplete() {
    used_media_impl_->OnDevicesEnumerated(
        ms_dispatcher_->audio_input_request_id(),
        ms_dispatcher_->audio_input_array());
    used_media_impl_->OnDevicesEnumerated(
        ms_dispatcher_->audio_output_request_id(),
        ms_dispatcher_->audio_output_array());
    used_media_impl_->OnDevicesEnumerated(
        ms_dispatcher_->video_request_id(),
        ms_dispatcher_->video_array());
  }

  void FakeMediaStreamDispatcherRequestSourcesComplete() {
    used_media_impl_->OnDevicesEnumerated(
        ms_dispatcher_->audio_input_request_id(),
        ms_dispatcher_->audio_input_array());
    used_media_impl_->OnDevicesEnumerated(
        ms_dispatcher_->video_request_id(),
        ms_dispatcher_->video_array());
  }

  void StartMockedVideoSource() {
    MockMediaStreamVideoCapturerSource* video_source =
        used_media_impl_->last_created_video_source();
    if (video_source->SourceHasAttemptedToStart())
      video_source->StartMockedSource();
  }

  void FailToStartMockedVideoSource() {
    MockMediaStreamVideoCapturerSource* video_source =
        used_media_impl_->last_created_video_source();
    if (video_source->SourceHasAttemptedToStart())
      video_source->FailToStartMockedSource();
    blink::WebHeap::collectGarbageForTesting();
  }

  void FailToCreateNextAudioCapturer() {
    dependency_factory_->FailToCreateNextAudioCapturer();
    blink::WebHeap::collectGarbageForTesting();
  }

 protected:
  base::MessageLoop message_loop_;
  scoped_ptr<ChildProcess> child_process_;
  MockMediaStreamDispatcher* ms_dispatcher_;  // Owned by |used_media_impl_|.
  scoped_ptr<UserMediaClientImplUnderTest> used_media_impl_;
  scoped_ptr<MockPeerConnectionDependencyFactory> dependency_factory_;
};

TEST_F(UserMediaClientImplTest, GenerateMediaStream) {
  // Generate a stream with both audio and video.
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();
}

// Test that the same source object is used if two MediaStreams are generated
// using the same source.
TEST_F(UserMediaClientImplTest, GenerateTwoMediaStreamsWithSameSource) {
  blink::WebMediaStream desc1 = RequestLocalMediaStream();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> desc1_video_tracks;
  desc1.videoTracks(desc1_video_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_video_tracks;
  desc2.videoTracks(desc2_video_tracks);
  EXPECT_EQ(desc1_video_tracks[0].source().id(),
            desc2_video_tracks[0].source().id());

  EXPECT_EQ(desc1_video_tracks[0].source().extraData(),
            desc2_video_tracks[0].source().extraData());

  blink::WebVector<blink::WebMediaStreamTrack> desc1_audio_tracks;
  desc1.audioTracks(desc1_audio_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_audio_tracks;
  desc2.audioTracks(desc2_audio_tracks);
  EXPECT_EQ(desc1_audio_tracks[0].source().id(),
            desc2_audio_tracks[0].source().id());

  EXPECT_EQ(desc1_audio_tracks[0].source().extraData(),
            desc2_audio_tracks[0].source().extraData());
}

// Test that the same source object is not used if two MediaStreams are
// generated using different sources.
TEST_F(UserMediaClientImplTest, GenerateTwoMediaStreamsWithDifferentSources) {
  blink::WebMediaStream desc1 = RequestLocalMediaStream();
  // Make sure another device is selected (another |session_id|) in  the next
  // gUM request.
  ms_dispatcher_->IncrementSessionId();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> desc1_video_tracks;
  desc1.videoTracks(desc1_video_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_video_tracks;
  desc2.videoTracks(desc2_video_tracks);
  EXPECT_NE(desc1_video_tracks[0].source().id(),
            desc2_video_tracks[0].source().id());

  EXPECT_NE(desc1_video_tracks[0].source().extraData(),
            desc2_video_tracks[0].source().extraData());

  blink::WebVector<blink::WebMediaStreamTrack> desc1_audio_tracks;
  desc1.audioTracks(desc1_audio_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_audio_tracks;
  desc2.audioTracks(desc2_audio_tracks);
  EXPECT_NE(desc1_audio_tracks[0].source().id(),
            desc2_audio_tracks[0].source().id());

  EXPECT_NE(desc1_audio_tracks[0].source().extraData(),
            desc2_audio_tracks[0].source().extraData());
}

TEST_F(UserMediaClientImplTest, StopLocalTracks) {
  // Generate a stream with both audio and video.
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  mixed_desc.audioTracks(audio_tracks);
  MediaStreamTrack* audio_track = MediaStreamTrack::GetTrack(audio_tracks[0]);
  audio_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  mixed_desc.videoTracks(video_tracks);
  MediaStreamTrack* video_track = MediaStreamTrack::GetTrack(video_tracks[0]);
  video_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// This test that a source is not stopped even if the tracks in a
// MediaStream is stopped if there are two MediaStreams with tracks using the
// same device. The source is stopped
// if there are no more MediaStream tracks using the device.
TEST_F(UserMediaClientImplTest, StopLocalTracksWhenTwoStreamUseSameDevices) {
  // Generate a stream with both audio and video.
  blink::WebMediaStream desc1 = RequestLocalMediaStream();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks1;
  desc1.audioTracks(audio_tracks1);
  MediaStreamTrack* audio_track1 = MediaStreamTrack::GetTrack(audio_tracks1[0]);
  audio_track1->Stop();
  EXPECT_EQ(0, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks2;
  desc2.audioTracks(audio_tracks2);
  MediaStreamTrack* audio_track2 = MediaStreamTrack::GetTrack(audio_tracks2[0]);
  audio_track2->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks1;
  desc1.videoTracks(video_tracks1);
  MediaStreamTrack* video_track1 = MediaStreamTrack::GetTrack(video_tracks1[0]);
  video_track1->Stop();
  EXPECT_EQ(0, ms_dispatcher_->stop_video_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks2;
  desc2.videoTracks(video_tracks2);
  MediaStreamTrack* video_track2 = MediaStreamTrack::GetTrack(video_tracks2[0]);
  video_track2->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

TEST_F(UserMediaClientImplTest, StopSourceWhenMediaStreamGoesOutOfScope) {
  // Generate a stream with both audio and video.
  RequestLocalMediaStream();
  // Makes sure the test itself don't hold a reference to the created
  // MediaStream.
  used_media_impl_->ClearLastGeneratedStream();
  blink::WebHeap::collectGarbageForTesting();

  // Expect the sources to be stopped when the MediaStream goes out of scope.
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// Test that the MediaStreams are deleted if the owning WebFrame is closing.
// In the unit test the owning frame is NULL.
TEST_F(UserMediaClientImplTest, FrameWillClose) {
  // Test a stream with both audio and video.
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();
  used_media_impl_->FrameWillClose();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// This test what happens if a video source to a MediaSteam fails to start.
TEST_F(UserMediaClientImplTest, MediaVideoSourceFailToStart) {
  used_media_impl_->RequestUserMedia();
  FakeMediaStreamDispatcherRequestUserMediaComplete();
  FailToStartMockedVideoSource();
  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_FAILED,
            used_media_impl_->request_state());
  EXPECT_EQ(MEDIA_DEVICE_TRACK_START_FAILURE,
            used_media_impl_->error_reason());
  blink::WebHeap::collectGarbageForTesting();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// This test what happens if an audio source fail to initialize.
TEST_F(UserMediaClientImplTest, MediaAudioSourceFailToInitialize) {
  FailToCreateNextAudioCapturer();
  used_media_impl_->RequestUserMedia();
  FakeMediaStreamDispatcherRequestUserMediaComplete();
  StartMockedVideoSource();
  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_FAILED,
            used_media_impl_->request_state());
  EXPECT_EQ(MEDIA_DEVICE_TRACK_START_FAILURE,
            used_media_impl_->error_reason());
  blink::WebHeap::collectGarbageForTesting();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// This test what happens if UserMediaClientImpl is deleted before a source has
// started.
TEST_F(UserMediaClientImplTest, MediaStreamImplShutDown) {
  used_media_impl_->RequestUserMedia();
  FakeMediaStreamDispatcherRequestUserMediaComplete();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_NOT_COMPLETE,
            used_media_impl_->request_state());
  used_media_impl_.reset();
}

// This test what happens if the WebFrame is closed while the MediaStream is
// being generated by the MediaStreamDispatcher.
TEST_F(UserMediaClientImplTest, ReloadFrameWhileGeneratingStream) {
  used_media_impl_->RequestUserMedia();
  used_media_impl_->FrameWillClose();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(0, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(0, ms_dispatcher_->stop_video_device_counter());
  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_NOT_COMPLETE,
            used_media_impl_->request_state());
}

// This test what happens if the WebFrame is closed while the sources are being
// started.
TEST_F(UserMediaClientImplTest, ReloadFrameWhileGeneratingSources) {
  used_media_impl_->RequestUserMedia();
  FakeMediaStreamDispatcherRequestUserMediaComplete();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  used_media_impl_->FrameWillClose();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_NOT_COMPLETE,
            used_media_impl_->request_state());
}

// This test what happens if stop is called on a track after the frame has
// been reloaded.
TEST_F(UserMediaClientImplTest, StopTrackAfterReload) {
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  used_media_impl_->FrameWillClose();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  mixed_desc.audioTracks(audio_tracks);
  MediaStreamTrack* audio_track = MediaStreamTrack::GetTrack(audio_tracks[0]);
  audio_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  mixed_desc.videoTracks(video_tracks);
  MediaStreamTrack* video_track = MediaStreamTrack::GetTrack(video_tracks[0]);
  video_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

TEST_F(UserMediaClientImplTest, EnumerateMediaDevices) {
  used_media_impl_->RequestMediaDevices();
  FakeMediaStreamDispatcherRequestMediaDevicesComplete();

  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_SUCCEEDED,
            used_media_impl_->request_state());
  EXPECT_EQ(static_cast<size_t>(5), used_media_impl_->last_devices().size());

  // Audio input device with matched output ID.
  const blink::WebMediaDeviceInfo* device =
      &used_media_impl_->last_devices()[0];
  EXPECT_FALSE(device->deviceId().isEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::MediaDeviceKindAudioInput,
            device->kind());
  EXPECT_FALSE(device->label().isEmpty());
  EXPECT_FALSE(device->groupId().isEmpty());

  // Audio input device without matched output ID.
  device = &used_media_impl_->last_devices()[1];
  EXPECT_FALSE(device->deviceId().isEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::MediaDeviceKindAudioInput,
            device->kind());
  EXPECT_FALSE(device->label().isEmpty());
  EXPECT_FALSE(device->groupId().isEmpty());

  // Video input devices.
  device = &used_media_impl_->last_devices()[2];
  EXPECT_FALSE(device->deviceId().isEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::MediaDeviceKindVideoInput,
            device->kind());
  EXPECT_FALSE(device->label().isEmpty());
  EXPECT_TRUE(device->groupId().isEmpty());

  device = &used_media_impl_->last_devices()[3];
  EXPECT_FALSE(device->deviceId().isEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::MediaDeviceKindVideoInput,
            device->kind());
  EXPECT_FALSE(device->label().isEmpty());
  EXPECT_TRUE(device->groupId().isEmpty());

  // Audio output device.
  device = &used_media_impl_->last_devices()[4];
  EXPECT_FALSE(device->deviceId().isEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::MediaDeviceKindAudioOutput,
            device->kind());
  EXPECT_FALSE(device->label().isEmpty());
  EXPECT_FALSE(device->groupId().isEmpty());

  // Verfify group IDs.
  EXPECT_TRUE(used_media_impl_->last_devices()[0].groupId().equals(
                  used_media_impl_->last_devices()[4].groupId()));
  EXPECT_FALSE(used_media_impl_->last_devices()[1].groupId().equals(
                   used_media_impl_->last_devices()[4].groupId()));
}

TEST_F(UserMediaClientImplTest, EnumerateSources) {
  used_media_impl_->RequestSources();
  FakeMediaStreamDispatcherRequestSourcesComplete();

  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_SUCCEEDED,
            used_media_impl_->request_state());
  EXPECT_EQ(static_cast<size_t>(4), used_media_impl_->last_sources().size());

  // Audio input devices.
  const blink::WebSourceInfo* source = &used_media_impl_->last_sources()[0];
  EXPECT_FALSE(source->id().isEmpty());
  EXPECT_EQ(blink::WebSourceInfo::SourceKindAudio, source->kind());
  EXPECT_FALSE(source->label().isEmpty());
  EXPECT_EQ(blink::WebSourceInfo::VideoFacingModeNone, source->facing());

  source = &used_media_impl_->last_sources()[1];
  EXPECT_FALSE(source->id().isEmpty());
  EXPECT_EQ(blink::WebSourceInfo::SourceKindAudio, source->kind());
  EXPECT_FALSE(source->label().isEmpty());
  EXPECT_EQ(blink::WebSourceInfo::VideoFacingModeNone, source->facing());

  // Video input device user facing.
  source = &used_media_impl_->last_sources()[2];
  EXPECT_FALSE(source->id().isEmpty());
  EXPECT_EQ(blink::WebSourceInfo::SourceKindVideo, source->kind());
  EXPECT_FALSE(source->label().isEmpty());
  EXPECT_EQ(blink::WebSourceInfo::VideoFacingModeUser, source->facing());

  // Video input device environment facing.
  source = &used_media_impl_->last_sources()[3];
  EXPECT_FALSE(source->id().isEmpty());
  EXPECT_EQ(blink::WebSourceInfo::SourceKindVideo, source->kind());
  EXPECT_FALSE(source->label().isEmpty());
  EXPECT_EQ(blink::WebSourceInfo::VideoFacingModeEnvironment, source->facing());
}

}  // namespace content
