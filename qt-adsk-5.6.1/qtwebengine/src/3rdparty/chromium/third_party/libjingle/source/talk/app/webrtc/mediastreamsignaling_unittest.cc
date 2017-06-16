/*
 * libjingle
 * Copyright 2012 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>
#include <vector>

#include "talk/app/webrtc/audiotrack.h"
#include "talk/app/webrtc/mediastream.h"
#include "talk/app/webrtc/mediastreamsignaling.h"
#include "talk/app/webrtc/sctputils.h"
#include "talk/app/webrtc/streamcollection.h"
#include "talk/app/webrtc/test/fakeconstraints.h"
#include "talk/app/webrtc/test/fakedatachannelprovider.h"
#include "talk/app/webrtc/videotrack.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/devices/fakedevicemanager.h"
#include "webrtc/p2p/base/constants.h"
#include "webrtc/p2p/base/sessiondescription.h"
#include "talk/session/media/channelmanager.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread.h"

static const char kStreams[][8] = {"stream1", "stream2"};
static const char kAudioTracks[][32] = {"audiotrack0", "audiotrack1"};
static const char kVideoTracks[][32] = {"videotrack0", "videotrack1"};

using webrtc::AudioTrack;
using webrtc::AudioTrackInterface;
using webrtc::AudioTrackVector;
using webrtc::VideoTrack;
using webrtc::VideoTrackInterface;
using webrtc::VideoTrackVector;
using webrtc::DataChannelInterface;
using webrtc::FakeConstraints;
using webrtc::IceCandidateInterface;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaStreamInterface;
using webrtc::MediaStreamTrackInterface;
using webrtc::PeerConnectionInterface;
using webrtc::SdpParseError;
using webrtc::SessionDescriptionInterface;
using webrtc::StreamCollection;
using webrtc::StreamCollectionInterface;

typedef PeerConnectionInterface::RTCOfferAnswerOptions RTCOfferAnswerOptions;

// Reference SDP with a MediaStream with label "stream1" and audio track with
// id "audio_1" and a video track with id "video_1;
static const char kSdpStringWithStream1[] =
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "m=audio 1 RTP/AVPF 103\r\n"
    "a=mid:audio\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "a=ssrc:1 cname:stream1\r\n"
    "a=ssrc:1 mslabel:stream1\r\n"
    "a=ssrc:1 label:audiotrack0\r\n"
    "m=video 1 RTP/AVPF 120\r\n"
    "a=mid:video\r\n"
    "a=rtpmap:120 VP8/90000\r\n"
    "a=ssrc:2 cname:stream1\r\n"
    "a=ssrc:2 mslabel:stream1\r\n"
    "a=ssrc:2 label:videotrack0\r\n";

// Reference SDP with two MediaStreams with label "stream1" and "stream2. Each
// MediaStreams have one audio track and one video track.
// This uses MSID.
static const char kSdpStringWith2Stream[] =
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=msid-semantic: WMS stream1 stream2\r\n"
    "m=audio 1 RTP/AVPF 103\r\n"
    "a=mid:audio\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "a=ssrc:1 cname:stream1\r\n"
    "a=ssrc:1 msid:stream1 audiotrack0\r\n"
    "a=ssrc:3 cname:stream2\r\n"
    "a=ssrc:3 msid:stream2 audiotrack1\r\n"
    "m=video 1 RTP/AVPF 120\r\n"
    "a=mid:video\r\n"
    "a=rtpmap:120 VP8/0\r\n"
    "a=ssrc:2 cname:stream1\r\n"
    "a=ssrc:2 msid:stream1 videotrack0\r\n"
    "a=ssrc:4 cname:stream2\r\n"
    "a=ssrc:4 msid:stream2 videotrack1\r\n";

// Reference SDP without MediaStreams. Msid is not supported.
static const char kSdpStringWithoutStreams[] =
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "m=audio 1 RTP/AVPF 103\r\n"
    "a=mid:audio\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "m=video 1 RTP/AVPF 120\r\n"
    "a=mid:video\r\n"
    "a=rtpmap:120 VP8/90000\r\n";

// Reference SDP without MediaStreams. Msid is supported.
static const char kSdpStringWithMsidWithoutStreams[] =
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=msid-semantic: WMS\r\n"
    "m=audio 1 RTP/AVPF 103\r\n"
    "a=mid:audio\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "m=video 1 RTP/AVPF 120\r\n"
    "a=mid:video\r\n"
    "a=rtpmap:120 VP8/90000\r\n";

// Reference SDP without MediaStreams and audio only.
static const char kSdpStringWithoutStreamsAudioOnly[] =
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "m=audio 1 RTP/AVPF 103\r\n"
    "a=mid:audio\r\n"
    "a=rtpmap:103 ISAC/16000\r\n";

// Reference SENDONLY SDP without MediaStreams. Msid is not supported.
static const char kSdpStringSendOnlyWithWithoutStreams[] =
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "m=audio 1 RTP/AVPF 103\r\n"
    "a=mid:audio\r\n"
    "a=sendonly"
    "a=rtpmap:103 ISAC/16000\r\n"
    "m=video 1 RTP/AVPF 120\r\n"
    "a=mid:video\r\n"
    "a=sendonly"
    "a=rtpmap:120 VP8/90000\r\n";

static const char kSdpStringInit[] =
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=msid-semantic: WMS\r\n";

static const char kSdpStringAudio[] =
    "m=audio 1 RTP/AVPF 103\r\n"
    "a=mid:audio\r\n"
    "a=rtpmap:103 ISAC/16000\r\n";

static const char kSdpStringVideo[] =
    "m=video 1 RTP/AVPF 120\r\n"
    "a=mid:video\r\n"
    "a=rtpmap:120 VP8/90000\r\n";

static const char kSdpStringMs1Audio0[] =
    "a=ssrc:1 cname:stream1\r\n"
    "a=ssrc:1 msid:stream1 audiotrack0\r\n";

static const char kSdpStringMs1Video0[] =
    "a=ssrc:2 cname:stream1\r\n"
    "a=ssrc:2 msid:stream1 videotrack0\r\n";

static const char kSdpStringMs1Audio1[] =
    "a=ssrc:3 cname:stream1\r\n"
    "a=ssrc:3 msid:stream1 audiotrack1\r\n";

static const char kSdpStringMs1Video1[] =
    "a=ssrc:4 cname:stream1\r\n"
    "a=ssrc:4 msid:stream1 videotrack1\r\n";

// Verifies that |options| contain all tracks in |collection| and that
// the |options| has set the the has_audio and has_video flags correct.
static void VerifyMediaOptions(StreamCollectionInterface* collection,
                               const cricket::MediaSessionOptions& options) {
  if (!collection) {
    return;
  }

  size_t stream_index = 0;
  for (size_t i = 0; i < collection->count(); ++i) {
    MediaStreamInterface* stream = collection->at(i);
    AudioTrackVector audio_tracks = stream->GetAudioTracks();
    ASSERT_GE(options.streams.size(), stream_index + audio_tracks.size());
    for (size_t j = 0; j < audio_tracks.size(); ++j) {
      webrtc::AudioTrackInterface* audio = audio_tracks[j];
      EXPECT_EQ(options.streams[stream_index].sync_label, stream->label());
      EXPECT_EQ(options.streams[stream_index++].id, audio->id());
      EXPECT_TRUE(options.has_audio());
    }
    VideoTrackVector video_tracks = stream->GetVideoTracks();
    ASSERT_GE(options.streams.size(), stream_index + video_tracks.size());
    for (size_t j = 0; j < video_tracks.size(); ++j) {
      webrtc::VideoTrackInterface* video = video_tracks[j];
      EXPECT_EQ(options.streams[stream_index].sync_label, stream->label());
      EXPECT_EQ(options.streams[stream_index++].id, video->id());
      EXPECT_TRUE(options.has_video());
    }
  }
}

static bool CompareStreamCollections(StreamCollectionInterface* s1,
                                     StreamCollectionInterface* s2) {
  if (s1 == NULL || s2 == NULL || s1->count() != s2->count())
    return false;

  for (size_t i = 0; i != s1->count(); ++i) {
    if (s1->at(i)->label() != s2->at(i)->label())
      return false;
    webrtc::AudioTrackVector audio_tracks1 = s1->at(i)->GetAudioTracks();
    webrtc::AudioTrackVector audio_tracks2 = s2->at(i)->GetAudioTracks();
    webrtc::VideoTrackVector video_tracks1 = s1->at(i)->GetVideoTracks();
    webrtc::VideoTrackVector video_tracks2 = s2->at(i)->GetVideoTracks();

    if (audio_tracks1.size() != audio_tracks2.size())
      return false;
    for (size_t j = 0; j != audio_tracks1.size(); ++j) {
       if (audio_tracks1[j]->id() != audio_tracks2[j]->id())
         return false;
    }
    if (video_tracks1.size() != video_tracks2.size())
      return false;
    for (size_t j = 0; j != video_tracks1.size(); ++j) {
      if (video_tracks1[j]->id() != video_tracks2[j]->id())
        return false;
    }
  }
  return true;
}

class FakeDataChannelFactory : public webrtc::DataChannelFactory {
 public:
  FakeDataChannelFactory(FakeDataChannelProvider* provider,
                         cricket::DataChannelType dct,
                         webrtc::MediaStreamSignaling* media_stream_signaling)
      : provider_(provider),
        type_(dct),
        media_stream_signaling_(media_stream_signaling) {}

  virtual rtc::scoped_refptr<webrtc::DataChannel> CreateDataChannel(
      const std::string& label,
      const webrtc::InternalDataChannelInit* config) {
    last_init_ = *config;
    rtc::scoped_refptr<webrtc::DataChannel> data_channel =
        webrtc::DataChannel::Create(provider_, type_, label, *config);
    media_stream_signaling_->AddDataChannel(data_channel);
    return data_channel;
  }

  const webrtc::InternalDataChannelInit& last_init() const {
      return last_init_;
  }

 private:
  FakeDataChannelProvider* provider_;
  cricket::DataChannelType type_;
  webrtc::MediaStreamSignaling* media_stream_signaling_;
  webrtc::InternalDataChannelInit last_init_;
};

class MockSignalingObserver : public webrtc::MediaStreamSignalingObserver {
 public:
  MockSignalingObserver()
      : remote_media_streams_(StreamCollection::Create()) {
  }

  virtual ~MockSignalingObserver() {
  }

  // New remote stream have been discovered.
  virtual void OnAddRemoteStream(MediaStreamInterface* remote_stream) {
    remote_media_streams_->AddStream(remote_stream);
  }

  // Remote stream is no longer available.
  virtual void OnRemoveRemoteStream(MediaStreamInterface* remote_stream) {
    remote_media_streams_->RemoveStream(remote_stream);
  }

  virtual void OnAddDataChannel(DataChannelInterface* data_channel) {
  }

  virtual void OnAddLocalAudioTrack(MediaStreamInterface* stream,
                                    AudioTrackInterface* audio_track,
                                    uint32 ssrc) {
    AddTrack(&local_audio_tracks_, stream, audio_track, ssrc);
  }

  virtual void OnAddLocalVideoTrack(MediaStreamInterface* stream,
                                    VideoTrackInterface* video_track,
                                    uint32 ssrc) {
    AddTrack(&local_video_tracks_, stream, video_track, ssrc);
  }

  virtual void OnRemoveLocalAudioTrack(MediaStreamInterface* stream,
                                       AudioTrackInterface* audio_track,
                                       uint32 ssrc) {
    RemoveTrack(&local_audio_tracks_, stream, audio_track);
  }

  virtual void OnRemoveLocalVideoTrack(MediaStreamInterface* stream,
                                       VideoTrackInterface* video_track) {
    RemoveTrack(&local_video_tracks_, stream, video_track);
  }

  virtual void OnAddRemoteAudioTrack(MediaStreamInterface* stream,
                                     AudioTrackInterface* audio_track,
                                     uint32 ssrc) {
    AddTrack(&remote_audio_tracks_, stream, audio_track, ssrc);
  }

  virtual void OnAddRemoteVideoTrack(MediaStreamInterface* stream,
                                     VideoTrackInterface* video_track,
                                     uint32 ssrc) {
    AddTrack(&remote_video_tracks_, stream, video_track, ssrc);
  }

  virtual void OnRemoveRemoteAudioTrack(MediaStreamInterface* stream,
                                        AudioTrackInterface* audio_track) {
    RemoveTrack(&remote_audio_tracks_, stream, audio_track);
  }

  virtual void OnRemoveRemoteVideoTrack(MediaStreamInterface* stream,
                                        VideoTrackInterface* video_track) {
    RemoveTrack(&remote_video_tracks_, stream, video_track);
  }

  virtual void OnRemoveLocalStream(MediaStreamInterface* stream) {
  }

  MediaStreamInterface* RemoteStream(const std::string& label) {
    return remote_media_streams_->find(label);
  }

  StreamCollectionInterface* remote_streams() const {
    return remote_media_streams_;
  }

  size_t NumberOfRemoteAudioTracks() { return remote_audio_tracks_.size(); }

  void  VerifyRemoteAudioTrack(const std::string& stream_label,
                               const std::string& track_id,
                               uint32 ssrc) {
    VerifyTrack(remote_audio_tracks_, stream_label, track_id, ssrc);
  }

  size_t NumberOfRemoteVideoTracks() { return remote_video_tracks_.size(); }

  void  VerifyRemoteVideoTrack(const std::string& stream_label,
                               const std::string& track_id,
                               uint32 ssrc) {
    VerifyTrack(remote_video_tracks_, stream_label, track_id, ssrc);
  }

  size_t NumberOfLocalAudioTracks() { return local_audio_tracks_.size(); }
  void  VerifyLocalAudioTrack(const std::string& stream_label,
                              const std::string& track_id,
                              uint32 ssrc) {
    VerifyTrack(local_audio_tracks_, stream_label, track_id, ssrc);
  }

  size_t NumberOfLocalVideoTracks() { return local_video_tracks_.size(); }

  void  VerifyLocalVideoTrack(const std::string& stream_label,
                              const std::string& track_id,
                              uint32 ssrc) {
    VerifyTrack(local_video_tracks_, stream_label, track_id, ssrc);
  }

 private:
  struct TrackInfo {
    TrackInfo() {}
    TrackInfo(const std::string& stream_label, const std::string track_id,
              uint32 ssrc)
        : stream_label(stream_label),
          track_id(track_id),
          ssrc(ssrc) {
    }
    std::string stream_label;
    std::string track_id;
    uint32 ssrc;
  };
  typedef std::vector<TrackInfo> TrackInfos;

  void AddTrack(TrackInfos* track_infos, MediaStreamInterface* stream,
                MediaStreamTrackInterface* track,
                uint32 ssrc) {
    (*track_infos).push_back(TrackInfo(stream->label(), track->id(),
                                       ssrc));
  }

  void RemoveTrack(TrackInfos* track_infos, MediaStreamInterface* stream,
                   MediaStreamTrackInterface* track) {
    for (TrackInfos::iterator it = track_infos->begin();
         it != track_infos->end(); ++it) {
      if (it->stream_label == stream->label() && it->track_id == track->id()) {
        track_infos->erase(it);
        return;
      }
    }
    ADD_FAILURE();
  }

  const TrackInfo* FindTrackInfo(const TrackInfos& infos,
                                 const std::string& stream_label,
                                 const std::string track_id) const {
    for (TrackInfos::const_iterator it = infos.begin();
        it != infos.end(); ++it) {
      if (it->stream_label == stream_label && it->track_id == track_id)
        return &*it;
    }
    return NULL;
  }


  void VerifyTrack(const TrackInfos& track_infos,
                   const std::string& stream_label,
                   const std::string& track_id,
                   uint32 ssrc) {
    const TrackInfo* track_info = FindTrackInfo(track_infos,
                                                stream_label,
                                                track_id);
    ASSERT_TRUE(track_info != NULL);
    EXPECT_EQ(ssrc, track_info->ssrc);
  }

  TrackInfos remote_audio_tracks_;
  TrackInfos remote_video_tracks_;
  TrackInfos local_audio_tracks_;
  TrackInfos local_video_tracks_;

  rtc::scoped_refptr<StreamCollection> remote_media_streams_;
};

class MediaStreamSignalingForTest : public webrtc::MediaStreamSignaling {
 public:
  MediaStreamSignalingForTest(MockSignalingObserver* observer,
                              cricket::ChannelManager* channel_manager)
      : webrtc::MediaStreamSignaling(rtc::Thread::Current(), observer,
                                     channel_manager) {
  };

  using webrtc::MediaStreamSignaling::GetOptionsForOffer;
  using webrtc::MediaStreamSignaling::GetOptionsForAnswer;
  using webrtc::MediaStreamSignaling::OnRemoteDescriptionChanged;
  using webrtc::MediaStreamSignaling::remote_streams;
};

class MediaStreamSignalingTest: public testing::Test {
 protected:
  virtual void SetUp() {
    observer_.reset(new MockSignalingObserver());
    channel_manager_.reset(
        new cricket::ChannelManager(new cricket::FakeMediaEngine(),
                                    new cricket::FakeDeviceManager(),
                                    rtc::Thread::Current()));
    signaling_.reset(new MediaStreamSignalingForTest(observer_.get(),
                                                     channel_manager_.get()));
    data_channel_provider_.reset(new FakeDataChannelProvider());
  }

  // Create a collection of streams.
  // CreateStreamCollection(1) creates a collection that
  // correspond to kSdpString1.
  // CreateStreamCollection(2) correspond to kSdpString2.
  rtc::scoped_refptr<StreamCollection>
  CreateStreamCollection(int number_of_streams) {
    rtc::scoped_refptr<StreamCollection> local_collection(
        StreamCollection::Create());

    for (int i = 0; i < number_of_streams; ++i) {
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream(
          webrtc::MediaStream::Create(kStreams[i]));

      // Add a local audio track.
      rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
          webrtc::AudioTrack::Create(kAudioTracks[i], NULL));
      stream->AddTrack(audio_track);

      // Add a local video track.
      rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
          webrtc::VideoTrack::Create(kVideoTracks[i], NULL));
      stream->AddTrack(video_track);

      local_collection->AddStream(stream);
    }
    return local_collection;
  }

  // This functions Creates a MediaStream with label kStreams[0] and
  // |number_of_audio_tracks| and |number_of_video_tracks| tracks and the
  // corresponding SessionDescriptionInterface. The SessionDescriptionInterface
  // is returned in |desc| and the MediaStream is stored in
  // |reference_collection_|
  void CreateSessionDescriptionAndReference(
      size_t number_of_audio_tracks,
      size_t number_of_video_tracks,
      SessionDescriptionInterface** desc) {
    ASSERT_TRUE(desc != NULL);
    ASSERT_LE(number_of_audio_tracks, 2u);
    ASSERT_LE(number_of_video_tracks, 2u);

    reference_collection_ = StreamCollection::Create();
    std::string sdp_ms1 = std::string(kSdpStringInit);

    std::string mediastream_label = kStreams[0];

    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream(
            webrtc::MediaStream::Create(mediastream_label));
    reference_collection_->AddStream(stream);

    if (number_of_audio_tracks > 0) {
      sdp_ms1 += std::string(kSdpStringAudio);
      sdp_ms1 += std::string(kSdpStringMs1Audio0);
      AddAudioTrack(kAudioTracks[0], stream);
    }
    if (number_of_audio_tracks > 1) {
      sdp_ms1 += kSdpStringMs1Audio1;
      AddAudioTrack(kAudioTracks[1], stream);
    }

    if (number_of_video_tracks > 0) {
      sdp_ms1 += std::string(kSdpStringVideo);
      sdp_ms1 += std::string(kSdpStringMs1Video0);
      AddVideoTrack(kVideoTracks[0], stream);
    }
    if (number_of_video_tracks > 1) {
      sdp_ms1 += kSdpStringMs1Video1;
      AddVideoTrack(kVideoTracks[1], stream);
    }

    *desc = webrtc::CreateSessionDescription(
        SessionDescriptionInterface::kOffer, sdp_ms1, NULL);
  }

  void AddAudioTrack(const std::string& track_id,
                     MediaStreamInterface* stream) {
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
        webrtc::AudioTrack::Create(track_id, NULL));
    ASSERT_TRUE(stream->AddTrack(audio_track));
  }

  void AddVideoTrack(const std::string& track_id,
                     MediaStreamInterface* stream) {
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
        webrtc::VideoTrack::Create(track_id, NULL));
    ASSERT_TRUE(stream->AddTrack(video_track));
  }

  rtc::scoped_refptr<webrtc::DataChannel> AddDataChannel(
      cricket::DataChannelType type, const std::string& label, int id) {
    webrtc::InternalDataChannelInit config;
    config.id = id;
    rtc::scoped_refptr<webrtc::DataChannel> data_channel(
        webrtc::DataChannel::Create(
            data_channel_provider_.get(), type, label, config));
    EXPECT_TRUE(data_channel.get() != NULL);
    EXPECT_TRUE(signaling_->AddDataChannel(data_channel.get()));
    return data_channel;
  }

  // ChannelManager is used by VideoSource, so it should be released after all
  // the video tracks. Put it as the first private variable should ensure that.
  rtc::scoped_ptr<cricket::ChannelManager> channel_manager_;
  rtc::scoped_refptr<StreamCollection> reference_collection_;
  rtc::scoped_ptr<MockSignalingObserver> observer_;
  rtc::scoped_ptr<MediaStreamSignalingForTest> signaling_;
  rtc::scoped_ptr<FakeDataChannelProvider> data_channel_provider_;
};

TEST_F(MediaStreamSignalingTest, GetOptionsForOfferWithInvalidAudioOption) {
  RTCOfferAnswerOptions rtc_options;
  rtc_options.offer_to_receive_audio = RTCOfferAnswerOptions::kUndefined - 1;

  cricket::MediaSessionOptions options;
  EXPECT_FALSE(signaling_->GetOptionsForOffer(rtc_options, &options));

  rtc_options.offer_to_receive_audio =
      RTCOfferAnswerOptions::kMaxOfferToReceiveMedia + 1;
  EXPECT_FALSE(signaling_->GetOptionsForOffer(rtc_options, &options));
}


TEST_F(MediaStreamSignalingTest, GetOptionsForOfferWithInvalidVideoOption) {
  RTCOfferAnswerOptions rtc_options;
  rtc_options.offer_to_receive_video =
      RTCOfferAnswerOptions::kUndefined - 1;

  cricket::MediaSessionOptions options;
  EXPECT_FALSE(signaling_->GetOptionsForOffer(rtc_options, &options));

  rtc_options.offer_to_receive_video =
      RTCOfferAnswerOptions::kMaxOfferToReceiveMedia + 1;
  EXPECT_FALSE(signaling_->GetOptionsForOffer(rtc_options, &options));
}

// Test that a MediaSessionOptions is created for an offer if
// OfferToReceiveAudio and OfferToReceiveVideo options are set but no
// MediaStreams are sent.
TEST_F(MediaStreamSignalingTest, GetMediaSessionOptionsForOfferWithAudioVideo) {
  RTCOfferAnswerOptions rtc_options;
  rtc_options.offer_to_receive_audio = 1;
  rtc_options.offer_to_receive_video = 1;

  cricket::MediaSessionOptions options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(rtc_options, &options));
  EXPECT_TRUE(options.has_audio());
  EXPECT_TRUE(options.has_video());
  EXPECT_TRUE(options.bundle_enabled);
}

// Test that a correct MediaSessionOptions is created for an offer if
// OfferToReceiveAudio is set but no MediaStreams are sent.
TEST_F(MediaStreamSignalingTest, GetMediaSessionOptionsForOfferWithAudio) {
  RTCOfferAnswerOptions rtc_options;
  rtc_options.offer_to_receive_audio = 1;

  cricket::MediaSessionOptions options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(rtc_options, &options));
  EXPECT_TRUE(options.has_audio());
  EXPECT_FALSE(options.has_video());
  EXPECT_TRUE(options.bundle_enabled);
}

// Test that a correct MediaSessionOptions is created for an offer if
// the default OfferOptons is used or MediaStreams are sent.
TEST_F(MediaStreamSignalingTest, GetDefaultMediaSessionOptionsForOffer) {
  RTCOfferAnswerOptions rtc_options;

  cricket::MediaSessionOptions options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(rtc_options, &options));
  EXPECT_FALSE(options.has_audio());
  EXPECT_FALSE(options.has_video());
  EXPECT_FALSE(options.bundle_enabled);
  EXPECT_TRUE(options.vad_enabled);
  EXPECT_FALSE(options.transport_options.ice_restart);
}

// Test that a correct MediaSessionOptions is created for an offer if
// OfferToReceiveVideo is set but no MediaStreams are sent.
TEST_F(MediaStreamSignalingTest, GetMediaSessionOptionsForOfferWithVideo) {
  RTCOfferAnswerOptions rtc_options;
  rtc_options.offer_to_receive_audio = 0;
  rtc_options.offer_to_receive_video = 1;

  cricket::MediaSessionOptions options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(rtc_options, &options));
  EXPECT_FALSE(options.has_audio());
  EXPECT_TRUE(options.has_video());
  EXPECT_TRUE(options.bundle_enabled);
}

// Test that a correct MediaSessionOptions is created for an offer if
// UseRtpMux is set to false.
TEST_F(MediaStreamSignalingTest,
       GetMediaSessionOptionsForOfferWithBundleDisabled) {
  RTCOfferAnswerOptions rtc_options;
  rtc_options.offer_to_receive_audio = 1;
  rtc_options.offer_to_receive_video = 1;
  rtc_options.use_rtp_mux = false;

  cricket::MediaSessionOptions options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(rtc_options, &options));
  EXPECT_TRUE(options.has_audio());
  EXPECT_TRUE(options.has_video());
  EXPECT_FALSE(options.bundle_enabled);
}

// Test that a correct MediaSessionOptions is created to restart ice if
// IceRestart is set. It also tests that subsequent MediaSessionOptions don't
// have |transport_options.ice_restart| set.
TEST_F(MediaStreamSignalingTest,
       GetMediaSessionOptionsForOfferWithIceRestart) {
  RTCOfferAnswerOptions rtc_options;
  rtc_options.ice_restart = true;

  cricket::MediaSessionOptions options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(rtc_options, &options));
  EXPECT_TRUE(options.transport_options.ice_restart);

  rtc_options = RTCOfferAnswerOptions();
  EXPECT_TRUE(signaling_->GetOptionsForOffer(rtc_options, &options));
  EXPECT_FALSE(options.transport_options.ice_restart);
}

// Test that a correct MediaSessionOptions are created for an offer if
// a MediaStream is sent and later updated with a new track.
// MediaConstraints are not used.
TEST_F(MediaStreamSignalingTest, AddTrackToLocalMediaStream) {
  RTCOfferAnswerOptions rtc_options;
  rtc::scoped_refptr<StreamCollection> local_streams(
      CreateStreamCollection(1));
  MediaStreamInterface* local_stream = local_streams->at(0);
  EXPECT_TRUE(signaling_->AddLocalStream(local_stream));
  cricket::MediaSessionOptions options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(rtc_options, &options));
  VerifyMediaOptions(local_streams, options);

  cricket::MediaSessionOptions updated_options;
  local_stream->AddTrack(AudioTrack::Create(kAudioTracks[1], NULL));
  EXPECT_TRUE(signaling_->GetOptionsForOffer(rtc_options, &options));
  VerifyMediaOptions(local_streams, options);
}

// Test that the MediaConstraints in an answer don't affect if audio and video
// is offered in an offer but that if kOfferToReceiveAudio or
// kOfferToReceiveVideo constraints are true in an offer, the media type will be
// included in subsequent answers.
TEST_F(MediaStreamSignalingTest, MediaConstraintsInAnswer) {
  FakeConstraints answer_c;
  answer_c.SetMandatoryReceiveAudio(true);
  answer_c.SetMandatoryReceiveVideo(true);

  cricket::MediaSessionOptions answer_options;
  EXPECT_TRUE(signaling_->GetOptionsForAnswer(&answer_c, &answer_options));
  EXPECT_TRUE(answer_options.has_audio());
  EXPECT_TRUE(answer_options.has_video());

  RTCOfferAnswerOptions rtc_offer_optoins;

  cricket::MediaSessionOptions offer_options;
  EXPECT_TRUE(
      signaling_->GetOptionsForOffer(rtc_offer_optoins, &offer_options));
  EXPECT_FALSE(offer_options.has_audio());
  EXPECT_FALSE(offer_options.has_video());

  RTCOfferAnswerOptions updated_rtc_offer_optoins;
  updated_rtc_offer_optoins.offer_to_receive_audio = 1;
  updated_rtc_offer_optoins.offer_to_receive_video = 1;

  cricket::MediaSessionOptions updated_offer_options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(updated_rtc_offer_optoins,
                                             &updated_offer_options));
  EXPECT_TRUE(updated_offer_options.has_audio());
  EXPECT_TRUE(updated_offer_options.has_video());

  // Since an offer has been created with both audio and video, subsequent
  // offers and answers should contain both audio and video.
  // Answers will only contain the media types that exist in the offer
  // regardless of the value of |updated_answer_options.has_audio| and
  // |updated_answer_options.has_video|.
  FakeConstraints updated_answer_c;
  answer_c.SetMandatoryReceiveAudio(false);
  answer_c.SetMandatoryReceiveVideo(false);

  cricket::MediaSessionOptions updated_answer_options;
  EXPECT_TRUE(signaling_->GetOptionsForAnswer(&updated_answer_c,
                                              &updated_answer_options));
  EXPECT_TRUE(updated_answer_options.has_audio());
  EXPECT_TRUE(updated_answer_options.has_video());

  RTCOfferAnswerOptions default_rtc_options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(default_rtc_options,
                                             &updated_offer_options));
  // By default, |has_audio| or |has_video| are false if there is no media
  // track.
  EXPECT_FALSE(updated_offer_options.has_audio());
  EXPECT_FALSE(updated_offer_options.has_video());
}

// This test verifies that the remote MediaStreams corresponding to a received
// SDP string is created. In this test the two separate MediaStreams are
// signaled.
TEST_F(MediaStreamSignalingTest, UpdateRemoteStreams) {
  rtc::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithStream1, NULL));
  EXPECT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());

  rtc::scoped_refptr<StreamCollection> reference(
      CreateStreamCollection(1));
  EXPECT_TRUE(CompareStreamCollections(signaling_->remote_streams(),
                                       reference.get()));
  EXPECT_TRUE(CompareStreamCollections(observer_->remote_streams(),
                                       reference.get()));
  EXPECT_EQ(1u, observer_->NumberOfRemoteAudioTracks());
  observer_->VerifyRemoteAudioTrack(kStreams[0], kAudioTracks[0], 1);
  EXPECT_EQ(1u, observer_->NumberOfRemoteVideoTracks());
  observer_->VerifyRemoteVideoTrack(kStreams[0], kVideoTracks[0], 2);
  ASSERT_EQ(1u, observer_->remote_streams()->count());
  MediaStreamInterface* remote_stream =  observer_->remote_streams()->at(0);
  EXPECT_TRUE(remote_stream->GetVideoTracks()[0]->GetSource() != NULL);

  // Create a session description based on another SDP with another
  // MediaStream.
  rtc::scoped_ptr<SessionDescriptionInterface> update_desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWith2Stream, NULL));
  EXPECT_TRUE(update_desc != NULL);
  signaling_->OnRemoteDescriptionChanged(update_desc.get());

  rtc::scoped_refptr<StreamCollection> reference2(
      CreateStreamCollection(2));
  EXPECT_TRUE(CompareStreamCollections(signaling_->remote_streams(),
                                       reference2.get()));
  EXPECT_TRUE(CompareStreamCollections(observer_->remote_streams(),
                                       reference2.get()));

  EXPECT_EQ(2u, observer_->NumberOfRemoteAudioTracks());
  observer_->VerifyRemoteAudioTrack(kStreams[0], kAudioTracks[0], 1);
  observer_->VerifyRemoteAudioTrack(kStreams[1], kAudioTracks[1], 3);
  EXPECT_EQ(2u, observer_->NumberOfRemoteVideoTracks());
  observer_->VerifyRemoteVideoTrack(kStreams[0], kVideoTracks[0], 2);
  observer_->VerifyRemoteVideoTrack(kStreams[1], kVideoTracks[1], 4);
}

// This test verifies that the remote MediaStreams corresponding to a received
// SDP string is created. In this test the same remote MediaStream is signaled
// but MediaStream tracks are added and removed.
TEST_F(MediaStreamSignalingTest, AddRemoveTrackFromExistingRemoteMediaStream) {
  rtc::scoped_ptr<SessionDescriptionInterface> desc_ms1;
  CreateSessionDescriptionAndReference(1, 1, desc_ms1.use());
  signaling_->OnRemoteDescriptionChanged(desc_ms1.get());
  EXPECT_TRUE(CompareStreamCollections(signaling_->remote_streams(),
                                       reference_collection_));

  // Add extra audio and video tracks to the same MediaStream.
  rtc::scoped_ptr<SessionDescriptionInterface> desc_ms1_two_tracks;
  CreateSessionDescriptionAndReference(2, 2, desc_ms1_two_tracks.use());
  signaling_->OnRemoteDescriptionChanged(desc_ms1_two_tracks.get());
  EXPECT_TRUE(CompareStreamCollections(signaling_->remote_streams(),
                                       reference_collection_));
  EXPECT_TRUE(CompareStreamCollections(observer_->remote_streams(),
                                       reference_collection_));

  // Remove the extra audio and video tracks again.
  rtc::scoped_ptr<SessionDescriptionInterface> desc_ms2;
  CreateSessionDescriptionAndReference(1, 1, desc_ms2.use());
  signaling_->OnRemoteDescriptionChanged(desc_ms2.get());
  EXPECT_TRUE(CompareStreamCollections(signaling_->remote_streams(),
                                       reference_collection_));
  EXPECT_TRUE(CompareStreamCollections(observer_->remote_streams(),
                                       reference_collection_));
}

// This test that remote tracks are ended if a
// local session description is set that rejects the media content type.
TEST_F(MediaStreamSignalingTest, RejectMediaContent) {
  rtc::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithStream1, NULL));
  EXPECT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());

  ASSERT_EQ(1u, observer_->remote_streams()->count());
  MediaStreamInterface* remote_stream =  observer_->remote_streams()->at(0);
  ASSERT_EQ(1u, remote_stream->GetVideoTracks().size());
  ASSERT_EQ(1u, remote_stream->GetAudioTracks().size());

  rtc::scoped_refptr<webrtc::VideoTrackInterface> remote_video =
      remote_stream->GetVideoTracks()[0];
  EXPECT_EQ(webrtc::MediaStreamTrackInterface::kLive, remote_video->state());
  rtc::scoped_refptr<webrtc::AudioTrackInterface> remote_audio =
      remote_stream->GetAudioTracks()[0];
  EXPECT_EQ(webrtc::MediaStreamTrackInterface::kLive, remote_audio->state());

  cricket::ContentInfo* video_info =
      desc->description()->GetContentByName("video");
  ASSERT_TRUE(video_info != NULL);
  video_info->rejected = true;
  signaling_->OnLocalDescriptionChanged(desc.get());
  EXPECT_EQ(webrtc::MediaStreamTrackInterface::kEnded, remote_video->state());
  EXPECT_EQ(webrtc::MediaStreamTrackInterface::kLive, remote_audio->state());

  cricket::ContentInfo* audio_info =
      desc->description()->GetContentByName("audio");
  ASSERT_TRUE(audio_info != NULL);
  audio_info->rejected = true;
  signaling_->OnLocalDescriptionChanged(desc.get());
  EXPECT_EQ(webrtc::MediaStreamTrackInterface::kEnded, remote_audio->state());
}

// This test that it won't crash if the remote track as been removed outside
// of MediaStreamSignaling and then MediaStreamSignaling tries to reject
// this track.
TEST_F(MediaStreamSignalingTest, RemoveTrackThenRejectMediaContent) {
  rtc::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithStream1, NULL));
  EXPECT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());

  MediaStreamInterface* remote_stream =  observer_->remote_streams()->at(0);
  remote_stream->RemoveTrack(remote_stream->GetVideoTracks()[0]);
  remote_stream->RemoveTrack(remote_stream->GetAudioTracks()[0]);

  cricket::ContentInfo* video_info =
      desc->description()->GetContentByName("video");
  video_info->rejected = true;
  signaling_->OnLocalDescriptionChanged(desc.get());

  cricket::ContentInfo* audio_info =
      desc->description()->GetContentByName("audio");
  audio_info->rejected = true;
  signaling_->OnLocalDescriptionChanged(desc.get());

  // No crash is a pass.
}

// This tests that a default MediaStream is created if a remote session
// description doesn't contain any streams and no MSID support.
// It also tests that the default stream is updated if a video m-line is added
// in a subsequent session description.
TEST_F(MediaStreamSignalingTest, SdpWithoutMsidCreatesDefaultStream) {
  rtc::scoped_ptr<SessionDescriptionInterface> desc_audio_only(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithoutStreamsAudioOnly,
                                       NULL));
  ASSERT_TRUE(desc_audio_only != NULL);
  signaling_->OnRemoteDescriptionChanged(desc_audio_only.get());

  EXPECT_EQ(1u, signaling_->remote_streams()->count());
  ASSERT_EQ(1u, observer_->remote_streams()->count());
  MediaStreamInterface* remote_stream = observer_->remote_streams()->at(0);

  EXPECT_EQ(1u, remote_stream->GetAudioTracks().size());
  EXPECT_EQ(0u, remote_stream->GetVideoTracks().size());
  EXPECT_EQ("default", remote_stream->label());

  rtc::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithoutStreams, NULL));
  ASSERT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());
  EXPECT_EQ(1u, signaling_->remote_streams()->count());
  ASSERT_EQ(1u, remote_stream->GetAudioTracks().size());
  EXPECT_EQ("defaulta0", remote_stream->GetAudioTracks()[0]->id());
  ASSERT_EQ(1u, remote_stream->GetVideoTracks().size());
  EXPECT_EQ("defaultv0", remote_stream->GetVideoTracks()[0]->id());
  observer_->VerifyRemoteAudioTrack("default", "defaulta0", 0);
  observer_->VerifyRemoteVideoTrack("default", "defaultv0", 0);
}

// This tests that a default MediaStream is created if a remote session
// description doesn't contain any streams and media direction is send only.
TEST_F(MediaStreamSignalingTest, RecvOnlySdpWithoutMsidCreatesDefaultStream) {
  rtc::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringSendOnlyWithWithoutStreams,
                                       NULL));
  ASSERT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());

  EXPECT_EQ(1u, signaling_->remote_streams()->count());
  ASSERT_EQ(1u, observer_->remote_streams()->count());
  MediaStreamInterface* remote_stream = observer_->remote_streams()->at(0);

  EXPECT_EQ(1u, remote_stream->GetAudioTracks().size());
  EXPECT_EQ(1u, remote_stream->GetVideoTracks().size());
  EXPECT_EQ("default", remote_stream->label());
}

// This tests that it won't crash when MediaStreamSignaling tries to remove
//  a remote track that as already been removed from the mediastream.
TEST_F(MediaStreamSignalingTest, RemoveAlreadyGoneRemoteStream) {
  rtc::scoped_ptr<SessionDescriptionInterface> desc_audio_only(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithoutStreams,
                                       NULL));
  ASSERT_TRUE(desc_audio_only != NULL);
  signaling_->OnRemoteDescriptionChanged(desc_audio_only.get());
  MediaStreamInterface* remote_stream = observer_->remote_streams()->at(0);
  remote_stream->RemoveTrack(remote_stream->GetAudioTracks()[0]);
  remote_stream->RemoveTrack(remote_stream->GetVideoTracks()[0]);

  rtc::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithoutStreams, NULL));
  ASSERT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());

  // No crash is a pass.
}

// This tests that a default MediaStream is created if the remote session
// description doesn't contain any streams and don't contain an indication if
// MSID is supported.
TEST_F(MediaStreamSignalingTest,
       SdpWithoutMsidAndStreamsCreatesDefaultStream) {
  rtc::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithoutStreams,
                                       NULL));
  ASSERT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());

  ASSERT_EQ(1u, observer_->remote_streams()->count());
  MediaStreamInterface* remote_stream = observer_->remote_streams()->at(0);
  EXPECT_EQ(1u, remote_stream->GetAudioTracks().size());
  EXPECT_EQ(1u, remote_stream->GetVideoTracks().size());
}

// This tests that a default MediaStream is not created if the remote session
// description doesn't contain any streams but does support MSID.
TEST_F(MediaStreamSignalingTest, SdpWitMsidDontCreatesDefaultStream) {
  rtc::scoped_ptr<SessionDescriptionInterface> desc_msid_without_streams(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithMsidWithoutStreams,
                                       NULL));
  signaling_->OnRemoteDescriptionChanged(desc_msid_without_streams.get());
  EXPECT_EQ(0u, observer_->remote_streams()->count());
}

// This test that a default MediaStream is not created if a remote session
// description is updated to not have any MediaStreams.
TEST_F(MediaStreamSignalingTest, VerifyDefaultStreamIsNotCreated) {
  rtc::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithStream1,
                                       NULL));
  ASSERT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());
  rtc::scoped_refptr<StreamCollection> reference(
      CreateStreamCollection(1));
  EXPECT_TRUE(CompareStreamCollections(observer_->remote_streams(),
                                       reference.get()));

  rtc::scoped_ptr<SessionDescriptionInterface> desc_without_streams(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithoutStreams,
                                       NULL));
  signaling_->OnRemoteDescriptionChanged(desc_without_streams.get());
  EXPECT_EQ(0u, observer_->remote_streams()->count());
}

// This test that the correct MediaStreamSignalingObserver methods are called
// when MediaStreamSignaling::OnLocalDescriptionChanged is called with an
// updated local session description.
TEST_F(MediaStreamSignalingTest, LocalDescriptionChanged) {
  rtc::scoped_ptr<SessionDescriptionInterface> desc_1;
  CreateSessionDescriptionAndReference(2, 2, desc_1.use());

  signaling_->AddLocalStream(reference_collection_->at(0));
  signaling_->OnLocalDescriptionChanged(desc_1.get());
  EXPECT_EQ(2u, observer_->NumberOfLocalAudioTracks());
  EXPECT_EQ(2u, observer_->NumberOfLocalVideoTracks());
  observer_->VerifyLocalAudioTrack(kStreams[0], kAudioTracks[0], 1);
  observer_->VerifyLocalVideoTrack(kStreams[0], kVideoTracks[0], 2);
  observer_->VerifyLocalAudioTrack(kStreams[0], kAudioTracks[1], 3);
  observer_->VerifyLocalVideoTrack(kStreams[0], kVideoTracks[1], 4);

  // Remove an audio and video track.
  rtc::scoped_ptr<SessionDescriptionInterface> desc_2;
  CreateSessionDescriptionAndReference(1, 1, desc_2.use());
  signaling_->OnLocalDescriptionChanged(desc_2.get());
  EXPECT_EQ(1u, observer_->NumberOfLocalAudioTracks());
  EXPECT_EQ(1u, observer_->NumberOfLocalVideoTracks());
  observer_->VerifyLocalAudioTrack(kStreams[0], kAudioTracks[0], 1);
  observer_->VerifyLocalVideoTrack(kStreams[0], kVideoTracks[0], 2);
}

// This test that the correct MediaStreamSignalingObserver methods are called
// when MediaStreamSignaling::AddLocalStream is called after
// MediaStreamSignaling::OnLocalDescriptionChanged is called.
TEST_F(MediaStreamSignalingTest, AddLocalStreamAfterLocalDescriptionChanged) {
  rtc::scoped_ptr<SessionDescriptionInterface> desc_1;
  CreateSessionDescriptionAndReference(2, 2, desc_1.use());

  signaling_->OnLocalDescriptionChanged(desc_1.get());
  EXPECT_EQ(0u, observer_->NumberOfLocalAudioTracks());
  EXPECT_EQ(0u, observer_->NumberOfLocalVideoTracks());

  signaling_->AddLocalStream(reference_collection_->at(0));
  EXPECT_EQ(2u, observer_->NumberOfLocalAudioTracks());
  EXPECT_EQ(2u, observer_->NumberOfLocalVideoTracks());
  observer_->VerifyLocalAudioTrack(kStreams[0], kAudioTracks[0], 1);
  observer_->VerifyLocalVideoTrack(kStreams[0], kVideoTracks[0], 2);
  observer_->VerifyLocalAudioTrack(kStreams[0], kAudioTracks[1], 3);
  observer_->VerifyLocalVideoTrack(kStreams[0], kVideoTracks[1], 4);
}

// This test that the correct MediaStreamSignalingObserver methods are called
// if the ssrc on a local track is changed when
// MediaStreamSignaling::OnLocalDescriptionChanged is called.
TEST_F(MediaStreamSignalingTest, ChangeSsrcOnTrackInLocalSessionDescription) {
  rtc::scoped_ptr<SessionDescriptionInterface> desc;
  CreateSessionDescriptionAndReference(1, 1, desc.use());

  signaling_->AddLocalStream(reference_collection_->at(0));
  signaling_->OnLocalDescriptionChanged(desc.get());
  EXPECT_EQ(1u, observer_->NumberOfLocalAudioTracks());
  EXPECT_EQ(1u, observer_->NumberOfLocalVideoTracks());
  observer_->VerifyLocalAudioTrack(kStreams[0], kAudioTracks[0], 1);
  observer_->VerifyLocalVideoTrack(kStreams[0], kVideoTracks[0], 2);

  // Change the ssrc of the audio and video track.
  std::string sdp;
  desc->ToString(&sdp);
  std::string ssrc_org = "a=ssrc:1";
  std::string ssrc_to = "a=ssrc:97";
  rtc::replace_substrs(ssrc_org.c_str(), ssrc_org.length(),
                             ssrc_to.c_str(), ssrc_to.length(),
                             &sdp);
  ssrc_org = "a=ssrc:2";
  ssrc_to = "a=ssrc:98";
  rtc::replace_substrs(ssrc_org.c_str(), ssrc_org.length(),
                             ssrc_to.c_str(), ssrc_to.length(),
                             &sdp);
  rtc::scoped_ptr<SessionDescriptionInterface> updated_desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       sdp, NULL));

  signaling_->OnLocalDescriptionChanged(updated_desc.get());
  EXPECT_EQ(1u, observer_->NumberOfLocalAudioTracks());
  EXPECT_EQ(1u, observer_->NumberOfLocalVideoTracks());
  observer_->VerifyLocalAudioTrack(kStreams[0], kAudioTracks[0], 97);
  observer_->VerifyLocalVideoTrack(kStreams[0], kVideoTracks[0], 98);
}

// This test that the correct MediaStreamSignalingObserver methods are called
// if a new session description is set with the same tracks but they are now
// sent on a another MediaStream.
TEST_F(MediaStreamSignalingTest, SignalSameTracksInSeparateMediaStream) {
  rtc::scoped_ptr<SessionDescriptionInterface> desc;
  CreateSessionDescriptionAndReference(1, 1, desc.use());

  signaling_->AddLocalStream(reference_collection_->at(0));
  signaling_->OnLocalDescriptionChanged(desc.get());
  EXPECT_EQ(1u, observer_->NumberOfLocalAudioTracks());
  EXPECT_EQ(1u, observer_->NumberOfLocalVideoTracks());

  std::string stream_label_0 = kStreams[0];
  observer_->VerifyLocalAudioTrack(stream_label_0, kAudioTracks[0], 1);
  observer_->VerifyLocalVideoTrack(stream_label_0, kVideoTracks[0], 2);

  // Add a new MediaStream but with the same tracks as in the first stream.
  std::string stream_label_1 = kStreams[1];
  rtc::scoped_refptr<webrtc::MediaStreamInterface> stream_1(
      webrtc::MediaStream::Create(kStreams[1]));
  stream_1->AddTrack(reference_collection_->at(0)->GetVideoTracks()[0]);
  stream_1->AddTrack(reference_collection_->at(0)->GetAudioTracks()[0]);
  signaling_->AddLocalStream(stream_1);

  // Replace msid in the original SDP.
  std::string sdp;
  desc->ToString(&sdp);
  rtc::replace_substrs(
      kStreams[0], strlen(kStreams[0]), kStreams[1], strlen(kStreams[1]), &sdp);

  rtc::scoped_ptr<SessionDescriptionInterface> updated_desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       sdp, NULL));

  signaling_->OnLocalDescriptionChanged(updated_desc.get());
  observer_->VerifyLocalAudioTrack(kStreams[1], kAudioTracks[0], 1);
  observer_->VerifyLocalVideoTrack(kStreams[1], kVideoTracks[0], 2);
  EXPECT_EQ(1u, observer_->NumberOfLocalAudioTracks());
  EXPECT_EQ(1u, observer_->NumberOfLocalVideoTracks());
}

// Verifies that an even SCTP id is allocated for SSL_CLIENT and an odd id for
// SSL_SERVER.
TEST_F(MediaStreamSignalingTest, SctpIdAllocationBasedOnRole) {
  int id;
  ASSERT_TRUE(signaling_->AllocateSctpSid(rtc::SSL_SERVER, &id));
  EXPECT_EQ(1, id);
  ASSERT_TRUE(signaling_->AllocateSctpSid(rtc::SSL_CLIENT, &id));
  EXPECT_EQ(0, id);
  ASSERT_TRUE(signaling_->AllocateSctpSid(rtc::SSL_SERVER, &id));
  EXPECT_EQ(3, id);
  ASSERT_TRUE(signaling_->AllocateSctpSid(rtc::SSL_CLIENT, &id));
  EXPECT_EQ(2, id);
}

// Verifies that SCTP ids of existing DataChannels are not reused.
TEST_F(MediaStreamSignalingTest, SctpIdAllocationNoReuse) {
  int old_id = 1;
  AddDataChannel(cricket::DCT_SCTP, "a", old_id);

  int new_id;
  ASSERT_TRUE(signaling_->AllocateSctpSid(rtc::SSL_SERVER, &new_id));
  EXPECT_NE(old_id, new_id);

  // Creates a DataChannel with id 0.
  old_id = 0;
  AddDataChannel(cricket::DCT_SCTP, "a", old_id);
  ASSERT_TRUE(signaling_->AllocateSctpSid(rtc::SSL_CLIENT, &new_id));
  EXPECT_NE(old_id, new_id);
}

// Verifies that SCTP ids of removed DataChannels can be reused.
TEST_F(MediaStreamSignalingTest, SctpIdReusedForRemovedDataChannel) {
  int odd_id = 1;
  int even_id = 0;
  AddDataChannel(cricket::DCT_SCTP, "a", odd_id);
  AddDataChannel(cricket::DCT_SCTP, "a", even_id);

  int allocated_id = -1;
  ASSERT_TRUE(signaling_->AllocateSctpSid(rtc::SSL_SERVER,
                                          &allocated_id));
  EXPECT_EQ(odd_id + 2, allocated_id);
  AddDataChannel(cricket::DCT_SCTP, "a", allocated_id);

  ASSERT_TRUE(signaling_->AllocateSctpSid(rtc::SSL_CLIENT,
                                          &allocated_id));
  EXPECT_EQ(even_id + 2, allocated_id);
  AddDataChannel(cricket::DCT_SCTP, "a", allocated_id);

  signaling_->RemoveSctpDataChannel(odd_id);
  signaling_->RemoveSctpDataChannel(even_id);

  // Verifies that removed DataChannel ids are reused.
  ASSERT_TRUE(signaling_->AllocateSctpSid(rtc::SSL_SERVER,
                                          &allocated_id));
  EXPECT_EQ(odd_id, allocated_id);

  ASSERT_TRUE(signaling_->AllocateSctpSid(rtc::SSL_CLIENT,
                                          &allocated_id));
  EXPECT_EQ(even_id, allocated_id);

  // Verifies that used higher DataChannel ids are not reused.
  ASSERT_TRUE(signaling_->AllocateSctpSid(rtc::SSL_SERVER,
                                          &allocated_id));
  EXPECT_NE(odd_id + 2, allocated_id);

  ASSERT_TRUE(signaling_->AllocateSctpSid(rtc::SSL_CLIENT,
                                          &allocated_id));
  EXPECT_NE(even_id + 2, allocated_id);

}

// Verifies that duplicated label is not allowed for RTP data channel.
TEST_F(MediaStreamSignalingTest, RtpDuplicatedLabelNotAllowed) {
  AddDataChannel(cricket::DCT_RTP, "a", -1);

  webrtc::InternalDataChannelInit config;
  rtc::scoped_refptr<webrtc::DataChannel> data_channel =
      webrtc::DataChannel::Create(
          data_channel_provider_.get(), cricket::DCT_RTP, "a", config);
  ASSERT_TRUE(data_channel.get() != NULL);
  EXPECT_FALSE(signaling_->AddDataChannel(data_channel.get()));
}

// Verifies that duplicated label is allowed for SCTP data channel.
TEST_F(MediaStreamSignalingTest, SctpDuplicatedLabelAllowed) {
  AddDataChannel(cricket::DCT_SCTP, "a", -1);
  AddDataChannel(cricket::DCT_SCTP, "a", -1);
}

// Verifies the correct configuration is used to create DataChannel from an OPEN
// message.
TEST_F(MediaStreamSignalingTest, CreateDataChannelFromOpenMessage) {
  FakeDataChannelFactory fake_factory(data_channel_provider_.get(),
                                      cricket::DCT_SCTP,
                                      signaling_.get());
  signaling_->SetDataChannelFactory(&fake_factory);
  webrtc::DataChannelInit config;
  config.id = 1;
  rtc::Buffer payload;
  webrtc::WriteDataChannelOpenMessage("a", config, &payload);
  cricket::ReceiveDataParams params;
  params.ssrc = config.id;
  EXPECT_TRUE(signaling_->AddDataChannelFromOpenMessage(params, payload));
  EXPECT_EQ(config.id, fake_factory.last_init().id);
  EXPECT_FALSE(fake_factory.last_init().negotiated);
  EXPECT_EQ(webrtc::InternalDataChannelInit::kAcker,
            fake_factory.last_init().open_handshake_role);
}

// Verifies that duplicated label from OPEN message is allowed.
TEST_F(MediaStreamSignalingTest, DuplicatedLabelFromOpenMessageAllowed) {
  AddDataChannel(cricket::DCT_SCTP, "a", -1);

  FakeDataChannelFactory fake_factory(data_channel_provider_.get(),
                                      cricket::DCT_SCTP,
                                      signaling_.get());
  signaling_->SetDataChannelFactory(&fake_factory);
  webrtc::DataChannelInit config;
  config.id = 0;
  rtc::Buffer payload;
  webrtc::WriteDataChannelOpenMessage("a", config, &payload);
  cricket::ReceiveDataParams params;
  params.ssrc = config.id;
  EXPECT_TRUE(signaling_->AddDataChannelFromOpenMessage(params, payload));
}

// Verifies that a DataChannel closed remotely is closed locally.
TEST_F(MediaStreamSignalingTest,
       SctpDataChannelClosedLocallyWhenClosedRemotely) {
  webrtc::InternalDataChannelInit config;
  config.id = 0;

  rtc::scoped_refptr<webrtc::DataChannel> data_channel =
      webrtc::DataChannel::Create(
          data_channel_provider_.get(), cricket::DCT_SCTP, "a", config);
  ASSERT_TRUE(data_channel.get() != NULL);
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting,
            data_channel->state());

  EXPECT_TRUE(signaling_->AddDataChannel(data_channel.get()));

  signaling_->OnRemoteSctpDataChannelClosed(config.id);
  EXPECT_EQ(webrtc::DataChannelInterface::kClosed, data_channel->state());
}

// Verifies that DataChannel added from OPEN message is added to
// MediaStreamSignaling only once (webrtc issue 3778).
TEST_F(MediaStreamSignalingTest, DataChannelFromOpenMessageAddedOnce) {
  FakeDataChannelFactory fake_factory(data_channel_provider_.get(),
                                      cricket::DCT_SCTP,
                                      signaling_.get());
  signaling_->SetDataChannelFactory(&fake_factory);
  webrtc::DataChannelInit config;
  config.id = 1;
  rtc::Buffer payload;
  webrtc::WriteDataChannelOpenMessage("a", config, &payload);
  cricket::ReceiveDataParams params;
  params.ssrc = config.id;
  EXPECT_TRUE(signaling_->AddDataChannelFromOpenMessage(params, payload));
  EXPECT_TRUE(signaling_->HasDataChannels());

  // Removes the DataChannel and verifies that no DataChannel is left.
  signaling_->RemoveSctpDataChannel(config.id);
  EXPECT_FALSE(signaling_->HasDataChannels());
}
