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

#include "talk/app/webrtc/mediastreamsignaling.h"

#include <vector>

#include "talk/app/webrtc/audiotrack.h"
#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/app/webrtc/mediastreamproxy.h"
#include "talk/app/webrtc/mediastreamtrackproxy.h"
#include "talk/app/webrtc/remoteaudiosource.h"
#include "talk/app/webrtc/remotevideocapturer.h"
#include "talk/app/webrtc/sctputils.h"
#include "talk/app/webrtc/videosource.h"
#include "talk/app/webrtc/videotrack.h"
#include "talk/media/sctp/sctpdataengine.h"
#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/stringutils.h"

static const char kDefaultStreamLabel[] = "default";
static const char kDefaultAudioTrackLabel[] = "defaulta0";
static const char kDefaultVideoTrackLabel[] = "defaultv0";

namespace webrtc {

using rtc::scoped_ptr;
using rtc::scoped_refptr;

static bool ParseConstraintsForAnswer(
    const MediaConstraintsInterface* constraints,
    cricket::MediaSessionOptions* options) {
  bool value = false;
  size_t mandatory_constraints_satisfied = 0;

  // kOfferToReceiveAudio defaults to true according to spec.
  if (!FindConstraint(constraints,
                      MediaConstraintsInterface::kOfferToReceiveAudio,
                      &value, &mandatory_constraints_satisfied) || value) {
    options->recv_audio = true;
  }

  // kOfferToReceiveVideo defaults to false according to spec. But
  // if it is an answer and video is offered, we should still accept video
  // per default.
  value = false;
  if (!FindConstraint(constraints,
                      MediaConstraintsInterface::kOfferToReceiveVideo,
                      &value, &mandatory_constraints_satisfied) || value) {
    options->recv_video = true;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kVoiceActivityDetection,
                     &value, &mandatory_constraints_satisfied)) {
    options->vad_enabled = value;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kUseRtpMux,
                     &value, &mandatory_constraints_satisfied)) {
    options->bundle_enabled = value;
  } else {
    // kUseRtpMux defaults to true according to spec.
    options->bundle_enabled = true;
  }
  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kIceRestart,
                     &value, &mandatory_constraints_satisfied)) {
    options->transport_options.ice_restart = value;
  } else {
    // kIceRestart defaults to false according to spec.
    options->transport_options.ice_restart = false;
  }

  if (!constraints) {
    return true;
  }
  return mandatory_constraints_satisfied == constraints->GetMandatory().size();
}

// Returns true if if at least one media content is present and
// |options.bundle_enabled| is true.
// Bundle will be enabled  by default if at least one media content is present
// and the constraint kUseRtpMux has not disabled bundle.
static bool EvaluateNeedForBundle(const cricket::MediaSessionOptions& options) {
  return options.bundle_enabled &&
      (options.has_audio() || options.has_video() || options.has_data());
}

static bool MediaContentDirectionHasSend(cricket::MediaContentDirection dir) {
  return dir == cricket::MD_SENDONLY || dir == cricket::MD_SENDRECV;
}

static bool IsValidOfferToReceiveMedia(int value) {
  typedef PeerConnectionInterface::RTCOfferAnswerOptions Options;
  return (value >= Options::kUndefined) &&
      (value <= Options::kMaxOfferToReceiveMedia);
}

// Add the stream and RTP data channel info to |session_options|.
static void SetStreams(
    cricket::MediaSessionOptions* session_options,
    rtc::scoped_refptr<StreamCollection> streams,
    const MediaStreamSignaling::RtpDataChannels& rtp_data_channels) {
  session_options->streams.clear();
  if (streams != NULL) {
    for (size_t i = 0; i < streams->count(); ++i) {
      MediaStreamInterface* stream = streams->at(i);

      AudioTrackVector audio_tracks(stream->GetAudioTracks());

      // For each audio track in the stream, add it to the MediaSessionOptions.
      for (size_t j = 0; j < audio_tracks.size(); ++j) {
        scoped_refptr<MediaStreamTrackInterface> track(audio_tracks[j]);
        session_options->AddSendStream(
            cricket::MEDIA_TYPE_AUDIO, track->id(), stream->label());
      }

      VideoTrackVector video_tracks(stream->GetVideoTracks());

      // For each video track in the stream, add it to the MediaSessionOptions.
      for (size_t j = 0; j < video_tracks.size(); ++j) {
        scoped_refptr<MediaStreamTrackInterface> track(video_tracks[j]);
        session_options->AddSendStream(
            cricket::MEDIA_TYPE_VIDEO, track->id(), stream->label());
      }
    }
  }

  // Check for data channels.
  MediaStreamSignaling::RtpDataChannels::const_iterator data_channel_it =
      rtp_data_channels.begin();
  for (; data_channel_it != rtp_data_channels.end(); ++data_channel_it) {
    const DataChannel* channel = data_channel_it->second;
    if (channel->state() == DataChannel::kConnecting ||
        channel->state() == DataChannel::kOpen) {
      // |streamid| and |sync_label| are both set to the DataChannel label
      // here so they can be signaled the same way as MediaStreams and Tracks.
      // For MediaStreams, the sync_label is the MediaStream label and the
      // track label is the same as |streamid|.
      const std::string& streamid = channel->label();
      const std::string& sync_label = channel->label();
      session_options->AddSendStream(
          cricket::MEDIA_TYPE_DATA, streamid, sync_label);
    }
  }
}

// Factory class for creating remote MediaStreams and MediaStreamTracks.
class RemoteMediaStreamFactory {
 public:
  explicit RemoteMediaStreamFactory(rtc::Thread* signaling_thread,
                                    cricket::ChannelManager* channel_manager)
      : signaling_thread_(signaling_thread),
        channel_manager_(channel_manager) {
  }

  rtc::scoped_refptr<MediaStreamInterface> CreateMediaStream(
      const std::string& stream_label) {
    return MediaStreamProxy::Create(
        signaling_thread_, MediaStream::Create(stream_label));
  }

  AudioTrackInterface* AddAudioTrack(webrtc::MediaStreamInterface* stream,
                                     const std::string& track_id) {
    return AddTrack<AudioTrackInterface, AudioTrack, AudioTrackProxy>(
        stream, track_id, RemoteAudioSource::Create().get());
  }

  VideoTrackInterface* AddVideoTrack(webrtc::MediaStreamInterface* stream,
                                     const std::string& track_id) {
    return AddTrack<VideoTrackInterface, VideoTrack, VideoTrackProxy>(
        stream, track_id, VideoSource::Create(channel_manager_,
                                              new RemoteVideoCapturer(),
                                              NULL).get());
  }

 private:
  template <typename TI, typename T, typename TP, typename S>
  TI* AddTrack(MediaStreamInterface* stream, const std::string& track_id,
               S* source) {
    rtc::scoped_refptr<TI> track(
        TP::Create(signaling_thread_, T::Create(track_id, source)));
    track->set_state(webrtc::MediaStreamTrackInterface::kLive);
    if (stream->AddTrack(track)) {
      return track;
    }
    return NULL;
  }

  rtc::Thread* signaling_thread_;
  cricket::ChannelManager* channel_manager_;
};

MediaStreamSignaling::MediaStreamSignaling(
    rtc::Thread* signaling_thread,
    MediaStreamSignalingObserver* stream_observer,
    cricket::ChannelManager* channel_manager)
    : signaling_thread_(signaling_thread),
      data_channel_factory_(NULL),
      stream_observer_(stream_observer),
      local_streams_(StreamCollection::Create()),
      remote_streams_(StreamCollection::Create()),
      remote_stream_factory_(new RemoteMediaStreamFactory(signaling_thread,
                                                          channel_manager)),
      last_allocated_sctp_even_sid_(-2),
      last_allocated_sctp_odd_sid_(-1) {
}

MediaStreamSignaling::~MediaStreamSignaling() {
}

void MediaStreamSignaling::TearDown() {
  OnAudioChannelClose();
  OnVideoChannelClose();
  OnDataChannelClose();
}

bool MediaStreamSignaling::IsSctpSidAvailable(int sid) const {
  if (sid < 0 || sid > static_cast<int>(cricket::kMaxSctpSid))
    return false;

  return FindDataChannelBySid(sid) < 0;
}

// Gets the first unused odd/even id based on the DTLS role. If |role| is
// SSL_CLIENT, the allocated id starts from 0 and takes even numbers; otherwise,
// the id starts from 1 and takes odd numbers. Returns false if no id can be
// allocated.
bool MediaStreamSignaling::AllocateSctpSid(rtc::SSLRole role, int* sid) {
  int& last_id = (role == rtc::SSL_CLIENT) ?
      last_allocated_sctp_even_sid_ : last_allocated_sctp_odd_sid_;

  do {
    last_id += 2;
  } while (last_id <= static_cast<int>(cricket::kMaxSctpSid) &&
           !IsSctpSidAvailable(last_id));

  if (last_id > static_cast<int>(cricket::kMaxSctpSid)) {
    return false;
  }

  *sid = last_id;
  return true;
}

bool MediaStreamSignaling::HasDataChannels() const {
  return !rtp_data_channels_.empty() || !sctp_data_channels_.empty();
}

bool MediaStreamSignaling::AddDataChannel(DataChannel* data_channel) {
  ASSERT(data_channel != NULL);
  if (data_channel->data_channel_type() == cricket::DCT_RTP) {
    if (rtp_data_channels_.find(data_channel->label()) !=
        rtp_data_channels_.end()) {
      LOG(LS_ERROR) << "DataChannel with label " << data_channel->label()
                    << " already exists.";
      return false;
    }
    rtp_data_channels_[data_channel->label()] = data_channel;
  } else {
    ASSERT(data_channel->data_channel_type() == cricket::DCT_SCTP);
    sctp_data_channels_.push_back(data_channel);
  }
  return true;
}

bool MediaStreamSignaling::AddDataChannelFromOpenMessage(
    const cricket::ReceiveDataParams& params,
    const rtc::Buffer& payload) {
  if (!data_channel_factory_) {
    LOG(LS_WARNING) << "Remote peer requested a DataChannel but DataChannels "
                    << "are not supported.";
    return false;
  }

  std::string label;
  InternalDataChannelInit config;
  config.id = params.ssrc;
  if (!ParseDataChannelOpenMessage(payload, &label, &config)) {
    LOG(LS_WARNING) << "Failed to parse the OPEN message for sid "
                    << params.ssrc;
    return false;
  }
  config.open_handshake_role = InternalDataChannelInit::kAcker;

  scoped_refptr<DataChannel> channel(
      data_channel_factory_->CreateDataChannel(label, &config));
  if (!channel.get()) {
    LOG(LS_ERROR) << "Failed to create DataChannel from the OPEN message.";
    return false;
  }

  stream_observer_->OnAddDataChannel(channel);
  return true;
}

void MediaStreamSignaling::RemoveSctpDataChannel(int sid) {
  ASSERT(sid >= 0);
  for (SctpDataChannels::iterator iter = sctp_data_channels_.begin();
       iter != sctp_data_channels_.end();
       ++iter) {
    if ((*iter)->id() == sid) {
      sctp_data_channels_.erase(iter);

      if (rtc::IsEven(sid) && sid <= last_allocated_sctp_even_sid_) {
        last_allocated_sctp_even_sid_ = sid - 2;
      } else if (rtc::IsOdd(sid) && sid <= last_allocated_sctp_odd_sid_) {
        last_allocated_sctp_odd_sid_ = sid - 2;
      }
      return;
    }
  }
}

bool MediaStreamSignaling::AddLocalStream(MediaStreamInterface* local_stream) {
  if (local_streams_->find(local_stream->label()) != NULL) {
    LOG(LS_WARNING) << "MediaStream with label " << local_stream->label()
                    << "already exist.";
    return false;
  }
  local_streams_->AddStream(local_stream);

  // Find tracks that has already been configured in SDP. This can occur if a
  // local session description that contains the MSID of these tracks is set
  // before AddLocalStream is called. It can also occur if the local session
  // description is not changed and RemoveLocalStream
  // is called and later AddLocalStream is called again with the same stream.
  AudioTrackVector audio_tracks = local_stream->GetAudioTracks();
  for (AudioTrackVector::const_iterator it = audio_tracks.begin();
       it != audio_tracks.end(); ++it) {
    const TrackInfo* track_info = FindTrackInfo(local_audio_tracks_,
                                                local_stream->label(),
                                                (*it)->id());
    if (track_info) {
      OnLocalTrackSeen(track_info->stream_label, track_info->track_id,
                       track_info->ssrc, cricket::MEDIA_TYPE_AUDIO);
    }
  }

  VideoTrackVector video_tracks = local_stream->GetVideoTracks();
  for (VideoTrackVector::const_iterator it = video_tracks.begin();
       it != video_tracks.end(); ++it) {
    const TrackInfo* track_info = FindTrackInfo(local_video_tracks_,
                                                local_stream->label(),
                                                (*it)->id());
    if (track_info) {
      OnLocalTrackSeen(track_info->stream_label, track_info->track_id,
                       track_info->ssrc, cricket::MEDIA_TYPE_VIDEO);
    }
  }
  return true;
}

void MediaStreamSignaling::RemoveLocalStream(
    MediaStreamInterface* local_stream) {
  AudioTrackVector audio_tracks = local_stream->GetAudioTracks();
  for (AudioTrackVector::const_iterator it = audio_tracks.begin();
       it != audio_tracks.end(); ++it) {
    const TrackInfo* track_info = FindTrackInfo(local_audio_tracks_,
                                                local_stream->label(),
                                                (*it)->id());
    if (track_info) {
      stream_observer_->OnRemoveLocalAudioTrack(local_stream, *it,
                                                track_info->ssrc);
    }
  }
  VideoTrackVector video_tracks = local_stream->GetVideoTracks();
  for (VideoTrackVector::const_iterator it = video_tracks.begin();
       it != video_tracks.end(); ++it) {
    const TrackInfo* track_info = FindTrackInfo(local_video_tracks_,
                                                local_stream->label(),
                                                (*it)->id());
    if (track_info) {
      stream_observer_->OnRemoveLocalVideoTrack(local_stream, *it);
    }
  }

  local_streams_->RemoveStream(local_stream);
  stream_observer_->OnRemoveLocalStream(local_stream);
}

bool MediaStreamSignaling::GetOptionsForOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
    cricket::MediaSessionOptions* session_options) {
  typedef PeerConnectionInterface::RTCOfferAnswerOptions RTCOfferAnswerOptions;
  if (!IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_audio) ||
      !IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_video)) {
    return false;
  }

  SetStreams(session_options, local_streams_, rtp_data_channels_);

  // According to the spec, offer to receive audio/video if the constraint is
  // not set and there are send streams.
  if (rtc_options.offer_to_receive_audio == RTCOfferAnswerOptions::kUndefined) {
    session_options->recv_audio =
        session_options->HasSendMediaStream(cricket::MEDIA_TYPE_AUDIO);
  } else {
    session_options->recv_audio = (rtc_options.offer_to_receive_audio > 0);
  }
  if (rtc_options.offer_to_receive_video == RTCOfferAnswerOptions::kUndefined) {
    session_options->recv_video =
        session_options->HasSendMediaStream(cricket::MEDIA_TYPE_VIDEO);
  } else {
    session_options->recv_video = (rtc_options.offer_to_receive_video > 0);
  }

  session_options->vad_enabled = rtc_options.voice_activity_detection;
  session_options->transport_options.ice_restart = rtc_options.ice_restart;
  session_options->bundle_enabled = rtc_options.use_rtp_mux;

  session_options->bundle_enabled = EvaluateNeedForBundle(*session_options);
  return true;
}

bool MediaStreamSignaling::GetOptionsForAnswer(
    const MediaConstraintsInterface* constraints,
    cricket::MediaSessionOptions* options) {
  SetStreams(options, local_streams_, rtp_data_channels_);

  options->recv_audio = false;
  options->recv_video = false;
  if (!ParseConstraintsForAnswer(constraints, options)) {
    return false;
  }
  options->bundle_enabled = EvaluateNeedForBundle(*options);
  return true;
}

// Updates or creates remote MediaStream objects given a
// remote SessionDesription.
// If the remote SessionDesription contains new remote MediaStreams
// the observer OnAddStream method is called. If a remote MediaStream is missing
// from the remote SessionDescription OnRemoveStream is called.
void MediaStreamSignaling::OnRemoteDescriptionChanged(
    const SessionDescriptionInterface* desc) {
  const cricket::SessionDescription* remote_desc = desc->description();
  rtc::scoped_refptr<StreamCollection> new_streams(
      StreamCollection::Create());

  // Find all audio rtp streams and create corresponding remote AudioTracks
  // and MediaStreams.
  const cricket::ContentInfo* audio_content = GetFirstAudioContent(remote_desc);
  if (audio_content) {
    const cricket::AudioContentDescription* desc =
        static_cast<const cricket::AudioContentDescription*>(
            audio_content->description);
    UpdateRemoteStreamsList(desc->streams(), desc->type(), new_streams);
    remote_info_.default_audio_track_needed =
        MediaContentDirectionHasSend(desc->direction()) &&
            desc->streams().empty();
  }

  // Find all video rtp streams and create corresponding remote VideoTracks
  // and MediaStreams.
  const cricket::ContentInfo* video_content = GetFirstVideoContent(remote_desc);
  if (video_content) {
    const cricket::VideoContentDescription* desc =
        static_cast<const cricket::VideoContentDescription*>(
            video_content->description);
    UpdateRemoteStreamsList(desc->streams(), desc->type(), new_streams);
    remote_info_.default_video_track_needed =
        MediaContentDirectionHasSend(desc->direction()) &&
            desc->streams().empty();
  }

  // Update the DataChannels with the information from the remote peer.
  const cricket::ContentInfo* data_content = GetFirstDataContent(remote_desc);
  if (data_content) {
    const cricket::DataContentDescription* data_desc =
        static_cast<const cricket::DataContentDescription*>(
            data_content->description);
    if (rtc::starts_with(
            data_desc->protocol().data(), cricket::kMediaProtocolRtpPrefix)) {
      UpdateRemoteRtpDataChannels(data_desc->streams());
    }
  }

  // Iterate new_streams and notify the observer about new MediaStreams.
  for (size_t i = 0; i < new_streams->count(); ++i) {
    MediaStreamInterface* new_stream = new_streams->at(i);
    stream_observer_->OnAddRemoteStream(new_stream);
  }

  // Find removed MediaStreams.
  if (remote_info_.IsDefaultMediaStreamNeeded() &&
      remote_streams_->find(kDefaultStreamLabel) != NULL) {
    // The default media stream already exists. No need to do anything.
  } else {
    UpdateEndedRemoteMediaStreams();
    remote_info_.msid_supported |= remote_streams_->count() > 0;
  }
  MaybeCreateDefaultStream();
}

void MediaStreamSignaling::OnLocalDescriptionChanged(
    const SessionDescriptionInterface* desc) {
  const cricket::ContentInfo* audio_content =
      GetFirstAudioContent(desc->description());
  if (audio_content) {
    if (audio_content->rejected) {
      RejectRemoteTracks(cricket::MEDIA_TYPE_AUDIO);
    }
    const cricket::AudioContentDescription* audio_desc =
        static_cast<const cricket::AudioContentDescription*>(
            audio_content->description);
    UpdateLocalTracks(audio_desc->streams(), audio_desc->type());
  }

  const cricket::ContentInfo* video_content =
      GetFirstVideoContent(desc->description());
  if (video_content) {
    if (video_content->rejected) {
      RejectRemoteTracks(cricket::MEDIA_TYPE_VIDEO);
    }
    const cricket::VideoContentDescription* video_desc =
        static_cast<const cricket::VideoContentDescription*>(
            video_content->description);
    UpdateLocalTracks(video_desc->streams(), video_desc->type());
  }

  const cricket::ContentInfo* data_content =
      GetFirstDataContent(desc->description());
  if (data_content) {
    const cricket::DataContentDescription* data_desc =
        static_cast<const cricket::DataContentDescription*>(
            data_content->description);
    if (rtc::starts_with(
            data_desc->protocol().data(), cricket::kMediaProtocolRtpPrefix)) {
      UpdateLocalRtpDataChannels(data_desc->streams());
    }
  }
}

void MediaStreamSignaling::OnAudioChannelClose() {
  RejectRemoteTracks(cricket::MEDIA_TYPE_AUDIO);
}

void MediaStreamSignaling::OnVideoChannelClose() {
  RejectRemoteTracks(cricket::MEDIA_TYPE_VIDEO);
}

void MediaStreamSignaling::OnDataChannelClose() {
  // Use a temporary copy of the RTP/SCTP DataChannel list because the
  // DataChannel may callback to us and try to modify the list.
  RtpDataChannels temp_rtp_dcs;
  temp_rtp_dcs.swap(rtp_data_channels_);
  RtpDataChannels::iterator it1 = temp_rtp_dcs.begin();
  for (; it1 != temp_rtp_dcs.end(); ++it1) {
    it1->second->OnDataEngineClose();
  }

  SctpDataChannels temp_sctp_dcs;
  temp_sctp_dcs.swap(sctp_data_channels_);
  SctpDataChannels::iterator it2 = temp_sctp_dcs.begin();
  for (; it2 != temp_sctp_dcs.end(); ++it2) {
    (*it2)->OnDataEngineClose();
  }
}

void MediaStreamSignaling::UpdateRemoteStreamsList(
    const cricket::StreamParamsVec& streams,
    cricket::MediaType media_type,
    StreamCollection* new_streams) {
  TrackInfos* current_tracks = GetRemoteTracks(media_type);

  // Find removed tracks. Ie tracks where the track id or ssrc don't match the
  // new StreamParam.
  TrackInfos::iterator track_it = current_tracks->begin();
  while (track_it != current_tracks->end()) {
    const TrackInfo& info = *track_it;
    const cricket::StreamParams* params =
        cricket::GetStreamBySsrc(streams, info.ssrc);
    if (!params || params->id != info.track_id) {
      OnRemoteTrackRemoved(info.stream_label, info.track_id, media_type);
      track_it = current_tracks->erase(track_it);
    } else {
      ++track_it;
    }
  }

  // Find new and active tracks.
  for (cricket::StreamParamsVec::const_iterator it = streams.begin();
       it != streams.end(); ++it) {
    // The sync_label is the MediaStream label and the |stream.id| is the
    // track id.
    const std::string& stream_label = it->sync_label;
    const std::string& track_id = it->id;
    uint32 ssrc = it->first_ssrc();

    rtc::scoped_refptr<MediaStreamInterface> stream =
        remote_streams_->find(stream_label);
    if (!stream) {
      // This is a new MediaStream. Create a new remote MediaStream.
      stream = remote_stream_factory_->CreateMediaStream(stream_label);
      remote_streams_->AddStream(stream);
      new_streams->AddStream(stream);
    }

    const TrackInfo* track_info = FindTrackInfo(*current_tracks, stream_label,
                                                track_id);
    if (!track_info) {
      current_tracks->push_back(TrackInfo(stream_label, track_id, ssrc));
      OnRemoteTrackSeen(stream_label, track_id, it->first_ssrc(), media_type);
    }
  }
}

void MediaStreamSignaling::OnRemoteTrackSeen(const std::string& stream_label,
                                             const std::string& track_id,
                                             uint32 ssrc,
                                             cricket::MediaType media_type) {
  MediaStreamInterface* stream = remote_streams_->find(stream_label);

  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    AudioTrackInterface* audio_track =
        remote_stream_factory_->AddAudioTrack(stream, track_id);
    stream_observer_->OnAddRemoteAudioTrack(stream, audio_track, ssrc);
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    VideoTrackInterface* video_track =
        remote_stream_factory_->AddVideoTrack(stream, track_id);
    stream_observer_->OnAddRemoteVideoTrack(stream, video_track, ssrc);
  } else {
    ASSERT(false && "Invalid media type");
  }
}

void MediaStreamSignaling::OnRemoteTrackRemoved(
    const std::string& stream_label,
    const std::string& track_id,
    cricket::MediaType media_type) {
  MediaStreamInterface* stream = remote_streams_->find(stream_label);

  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    rtc::scoped_refptr<AudioTrackInterface> audio_track =
        stream->FindAudioTrack(track_id);
    if (audio_track) {
      audio_track->set_state(webrtc::MediaStreamTrackInterface::kEnded);
      stream->RemoveTrack(audio_track);
      stream_observer_->OnRemoveRemoteAudioTrack(stream, audio_track);
    }
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    rtc::scoped_refptr<VideoTrackInterface> video_track =
        stream->FindVideoTrack(track_id);
    if (video_track) {
      video_track->set_state(webrtc::MediaStreamTrackInterface::kEnded);
      stream->RemoveTrack(video_track);
      stream_observer_->OnRemoveRemoteVideoTrack(stream, video_track);
    }
  } else {
    ASSERT(false && "Invalid media type");
  }
}

void MediaStreamSignaling::RejectRemoteTracks(cricket::MediaType media_type) {
  TrackInfos* current_tracks = GetRemoteTracks(media_type);
  for (TrackInfos::iterator track_it = current_tracks->begin();
       track_it != current_tracks->end(); ++track_it) {
    const TrackInfo& info = *track_it;
    MediaStreamInterface* stream = remote_streams_->find(info.stream_label);
    if (media_type == cricket::MEDIA_TYPE_AUDIO) {
      AudioTrackInterface* track = stream->FindAudioTrack(info.track_id);
      // There's no guarantee the track is still available, e.g. the track may
      // have been removed from the stream by javascript.
      if (track) {
        track->set_state(webrtc::MediaStreamTrackInterface::kEnded);
      }
    }
    if (media_type == cricket::MEDIA_TYPE_VIDEO) {
      VideoTrackInterface* track = stream->FindVideoTrack(info.track_id);
      // There's no guarantee the track is still available, e.g. the track may
      // have been removed from the stream by javascript.
      if (track) {
        track->set_state(webrtc::MediaStreamTrackInterface::kEnded);
      }
    }
  }
}

void MediaStreamSignaling::UpdateEndedRemoteMediaStreams() {
  std::vector<scoped_refptr<MediaStreamInterface> > streams_to_remove;
  for (size_t i = 0; i < remote_streams_->count(); ++i) {
    MediaStreamInterface*stream = remote_streams_->at(i);
    if (stream->GetAudioTracks().empty() && stream->GetVideoTracks().empty()) {
      streams_to_remove.push_back(stream);
    }
  }

  std::vector<scoped_refptr<MediaStreamInterface> >::const_iterator it;
  for (it = streams_to_remove.begin(); it != streams_to_remove.end(); ++it) {
    remote_streams_->RemoveStream(*it);
    stream_observer_->OnRemoveRemoteStream(*it);
  }
}

void MediaStreamSignaling::MaybeCreateDefaultStream() {
  if (!remote_info_.IsDefaultMediaStreamNeeded())
    return;

  bool default_created = false;

  scoped_refptr<MediaStreamInterface> default_remote_stream =
      remote_streams_->find(kDefaultStreamLabel);
  if (default_remote_stream == NULL) {
    default_created = true;
    default_remote_stream =
        remote_stream_factory_->CreateMediaStream(kDefaultStreamLabel);
    remote_streams_->AddStream(default_remote_stream);
  }
  if (remote_info_.default_audio_track_needed &&
      default_remote_stream->GetAudioTracks().size() == 0) {
    remote_audio_tracks_.push_back(TrackInfo(kDefaultStreamLabel,
                                             kDefaultAudioTrackLabel, 0));

    OnRemoteTrackSeen(kDefaultStreamLabel, kDefaultAudioTrackLabel, 0,
                       cricket::MEDIA_TYPE_AUDIO);
  }
  if (remote_info_.default_video_track_needed &&
      default_remote_stream->GetVideoTracks().size() == 0) {
    remote_video_tracks_.push_back(TrackInfo(kDefaultStreamLabel,
                                             kDefaultVideoTrackLabel, 0));
    OnRemoteTrackSeen(kDefaultStreamLabel, kDefaultVideoTrackLabel, 0,
                       cricket::MEDIA_TYPE_VIDEO);
  }
  if (default_created) {
    stream_observer_->OnAddRemoteStream(default_remote_stream);
  }
}

MediaStreamSignaling::TrackInfos* MediaStreamSignaling::GetRemoteTracks(
    cricket::MediaType type) {
  if (type == cricket::MEDIA_TYPE_AUDIO)
    return &remote_audio_tracks_;
  else if (type == cricket::MEDIA_TYPE_VIDEO)
    return &remote_video_tracks_;
  ASSERT(false && "Unknown MediaType");
  return NULL;
}

MediaStreamSignaling::TrackInfos* MediaStreamSignaling::GetLocalTracks(
    cricket::MediaType media_type) {
  ASSERT(media_type == cricket::MEDIA_TYPE_AUDIO ||
         media_type == cricket::MEDIA_TYPE_VIDEO);

  return (media_type == cricket::MEDIA_TYPE_AUDIO) ?
      &local_audio_tracks_ : &local_video_tracks_;
}

void MediaStreamSignaling::UpdateLocalTracks(
    const std::vector<cricket::StreamParams>& streams,
    cricket::MediaType media_type) {
  TrackInfos* current_tracks = GetLocalTracks(media_type);

  // Find removed tracks. Ie tracks where the track id, stream label or ssrc
  // don't match the new StreamParam.
  TrackInfos::iterator track_it = current_tracks->begin();
  while (track_it != current_tracks->end()) {
    const TrackInfo& info = *track_it;
    const cricket::StreamParams* params =
        cricket::GetStreamBySsrc(streams, info.ssrc);
    if (!params || params->id != info.track_id ||
        params->sync_label != info.stream_label) {
      OnLocalTrackRemoved(info.stream_label, info.track_id, info.ssrc,
                          media_type);
      track_it = current_tracks->erase(track_it);
    } else {
      ++track_it;
    }
  }

  // Find new and active tracks.
  for (cricket::StreamParamsVec::const_iterator it = streams.begin();
       it != streams.end(); ++it) {
    // The sync_label is the MediaStream label and the |stream.id| is the
    // track id.
    const std::string& stream_label = it->sync_label;
    const std::string& track_id = it->id;
    uint32 ssrc = it->first_ssrc();
    const TrackInfo* track_info = FindTrackInfo(*current_tracks,
                                                stream_label,
                                                track_id);
    if (!track_info) {
      current_tracks->push_back(TrackInfo(stream_label, track_id, ssrc));
      OnLocalTrackSeen(stream_label, track_id, it->first_ssrc(),
                       media_type);
    }
  }
}

void MediaStreamSignaling::OnLocalTrackSeen(
    const std::string& stream_label,
    const std::string& track_id,
    uint32 ssrc,
    cricket::MediaType media_type) {
  MediaStreamInterface* stream = local_streams_->find(stream_label);
  if (!stream) {
    LOG(LS_WARNING) << "An unknown local MediaStream with label "
                    << stream_label <<  " has been configured.";
    return;
  }

  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    AudioTrackInterface* audio_track = stream->FindAudioTrack(track_id);
    if (!audio_track) {
      LOG(LS_WARNING) << "An unknown local AudioTrack with id , "
                      << track_id <<  " has been configured.";
      return;
    }
    stream_observer_->OnAddLocalAudioTrack(stream, audio_track, ssrc);
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    VideoTrackInterface* video_track = stream->FindVideoTrack(track_id);
    if (!video_track) {
      LOG(LS_WARNING) << "An unknown local VideoTrack with id , "
                      << track_id <<  " has been configured.";
      return;
    }
    stream_observer_->OnAddLocalVideoTrack(stream, video_track, ssrc);
  } else {
    ASSERT(false && "Invalid media type");
  }
}

void MediaStreamSignaling::OnLocalTrackRemoved(
    const std::string& stream_label,
    const std::string& track_id,
    uint32 ssrc,
    cricket::MediaType media_type) {
  MediaStreamInterface* stream = local_streams_->find(stream_label);
  if (!stream) {
    // This is the normal case. Ie RemoveLocalStream has been called and the
    // SessionDescriptions has been renegotiated.
    return;
  }
  // A track has been removed from the SessionDescription but the MediaStream
  // is still associated with MediaStreamSignaling. This only occurs if the SDP
  // doesn't match with the calls to AddLocalStream and RemoveLocalStream.

  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    AudioTrackInterface* audio_track = stream->FindAudioTrack(track_id);
    if (!audio_track) {
      return;
    }
    stream_observer_->OnRemoveLocalAudioTrack(stream, audio_track, ssrc);
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    VideoTrackInterface* video_track = stream->FindVideoTrack(track_id);
    if (!video_track) {
      return;
    }
    stream_observer_->OnRemoveLocalVideoTrack(stream, video_track);
  } else {
    ASSERT(false && "Invalid media type.");
  }
}

void MediaStreamSignaling::UpdateLocalRtpDataChannels(
    const cricket::StreamParamsVec& streams) {
  std::vector<std::string> existing_channels;

  // Find new and active data channels.
  for (cricket::StreamParamsVec::const_iterator it =streams.begin();
       it != streams.end(); ++it) {
    // |it->sync_label| is actually the data channel label. The reason is that
    // we use the same naming of data channels as we do for
    // MediaStreams and Tracks.
    // For MediaStreams, the sync_label is the MediaStream label and the
    // track label is the same as |streamid|.
    const std::string& channel_label = it->sync_label;
    RtpDataChannels::iterator data_channel_it =
        rtp_data_channels_.find(channel_label);
    if (!VERIFY(data_channel_it != rtp_data_channels_.end())) {
      continue;
    }
    // Set the SSRC the data channel should use for sending.
    data_channel_it->second->SetSendSsrc(it->first_ssrc());
    existing_channels.push_back(data_channel_it->first);
  }

  UpdateClosingDataChannels(existing_channels, true);
}

void MediaStreamSignaling::UpdateRemoteRtpDataChannels(
    const cricket::StreamParamsVec& streams) {
  std::vector<std::string> existing_channels;

  // Find new and active data channels.
  for (cricket::StreamParamsVec::const_iterator it = streams.begin();
       it != streams.end(); ++it) {
    // The data channel label is either the mslabel or the SSRC if the mslabel
    // does not exist. Ex a=ssrc:444330170 mslabel:test1.
    std::string label = it->sync_label.empty() ?
        rtc::ToString(it->first_ssrc()) : it->sync_label;
    RtpDataChannels::iterator data_channel_it =
        rtp_data_channels_.find(label);
    if (data_channel_it == rtp_data_channels_.end()) {
      // This is a new data channel.
      CreateRemoteDataChannel(label, it->first_ssrc());
    } else {
      data_channel_it->second->SetReceiveSsrc(it->first_ssrc());
    }
    existing_channels.push_back(label);
  }

  UpdateClosingDataChannels(existing_channels, false);
}

void MediaStreamSignaling::UpdateClosingDataChannels(
    const std::vector<std::string>& active_channels, bool is_local_update) {
  RtpDataChannels::iterator it = rtp_data_channels_.begin();
  while (it != rtp_data_channels_.end()) {
    DataChannel* data_channel = it->second;
    if (std::find(active_channels.begin(), active_channels.end(),
                  data_channel->label()) != active_channels.end()) {
      ++it;
      continue;
    }

    if (is_local_update)
      data_channel->SetSendSsrc(0);
    else
      data_channel->RemotePeerRequestClose();

    if (data_channel->state() == DataChannel::kClosed) {
      rtp_data_channels_.erase(it);
      it = rtp_data_channels_.begin();
    } else {
      ++it;
    }
  }
}

void MediaStreamSignaling::CreateRemoteDataChannel(const std::string& label,
                                                   uint32 remote_ssrc) {
  if (!data_channel_factory_) {
    LOG(LS_WARNING) << "Remote peer requested a DataChannel but DataChannels "
                    << "are not supported.";
    return;
  }
  scoped_refptr<DataChannel> channel(
      data_channel_factory_->CreateDataChannel(label, NULL));
  if (!channel.get()) {
    LOG(LS_WARNING) << "Remote peer requested a DataChannel but"
                    << "CreateDataChannel failed.";
    return;
  }
  channel->SetReceiveSsrc(remote_ssrc);
  stream_observer_->OnAddDataChannel(channel);
}

void MediaStreamSignaling::OnDataTransportCreatedForSctp() {
  SctpDataChannels::iterator it = sctp_data_channels_.begin();
  for (; it != sctp_data_channels_.end(); ++it) {
    (*it)->OnTransportChannelCreated();
  }
}

void MediaStreamSignaling::OnDtlsRoleReadyForSctp(rtc::SSLRole role) {
  SctpDataChannels::iterator it = sctp_data_channels_.begin();
  for (; it != sctp_data_channels_.end(); ++it) {
    if ((*it)->id() < 0) {
      int sid;
      if (!AllocateSctpSid(role, &sid)) {
        LOG(LS_ERROR) << "Failed to allocate SCTP sid.";
        continue;
      }
      (*it)->SetSctpSid(sid);
    }
  }
}


void MediaStreamSignaling::OnRemoteSctpDataChannelClosed(uint32 sid) {
  int index = FindDataChannelBySid(sid);
  if (index < 0) {
    LOG(LS_WARNING) << "Unexpected sid " << sid
                    << " of the remotely closed DataChannel.";
    return;
  }
  sctp_data_channels_[index]->Close();
}

const MediaStreamSignaling::TrackInfo*
MediaStreamSignaling::FindTrackInfo(
    const MediaStreamSignaling::TrackInfos& infos,
    const std::string& stream_label,
    const std::string track_id) const {

  for (TrackInfos::const_iterator it = infos.begin();
      it != infos.end(); ++it) {
    if (it->stream_label == stream_label && it->track_id == track_id)
      return &*it;
  }
  return NULL;
}

int MediaStreamSignaling::FindDataChannelBySid(int sid) const {
  for (size_t i = 0; i < sctp_data_channels_.size(); ++i) {
    if (sctp_data_channels_[i]->id() == sid) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace webrtc
