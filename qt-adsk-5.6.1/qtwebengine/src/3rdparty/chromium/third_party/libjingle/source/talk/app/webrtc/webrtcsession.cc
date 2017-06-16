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

#include "talk/app/webrtc/webrtcsession.h"

#include <limits.h>

#include <algorithm>
#include <vector>

#include "talk/app/webrtc/jsepicecandidate.h"
#include "talk/app/webrtc/jsepsessiondescription.h"
#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/app/webrtc/mediastreamsignaling.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/webrtcsessiondescriptionfactory.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/videocapturer.h"
#include "talk/session/media/channel.h"
#include "talk/session/media/channelmanager.h"
#include "talk/session/media/mediasession.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/p2p/base/portallocator.h"

using cricket::ContentInfo;
using cricket::ContentInfos;
using cricket::MediaContentDescription;
using cricket::SessionDescription;
using cricket::TransportInfo;

namespace webrtc {

// Error messages
const char kBundleWithoutRtcpMux[] = "RTCP-MUX must be enabled when BUNDLE "
                                     "is enabled.";
const char kCreateChannelFailed[] = "Failed to create channels.";
const char kInvalidCandidates[] = "Description contains invalid candidates.";
const char kInvalidSdp[] = "Invalid session description.";
const char kMlineMismatch[] =
    "Offer and answer descriptions m-lines are not matching. Rejecting answer.";
const char kPushDownTDFailed[] =
    "Failed to push down transport description:";
const char kSdpWithoutDtlsFingerprint[] =
    "Called with SDP without DTLS fingerprint.";
const char kSdpWithoutSdesCrypto[] =
    "Called with SDP without SDES crypto.";
const char kSdpWithoutIceUfragPwd[] =
    "Called with SDP without ice-ufrag and ice-pwd.";
const char kSessionError[] = "Session error code: ";
const char kSessionErrorDesc[] = "Session error description: ";
const char kDtlsSetupFailureRtp[] =
    "Couldn't set up DTLS-SRTP on RTP channel.";
const char kDtlsSetupFailureRtcp[] =
    "Couldn't set up DTLS-SRTP on RTCP channel.";
const int kMaxUnsignalledRecvStreams = 20;

// Compares |answer| against |offer|. Comparision is done
// for number of m-lines in answer against offer. If matches true will be
// returned otherwise false.
static bool VerifyMediaDescriptions(
    const SessionDescription* answer, const SessionDescription* offer) {
  if (offer->contents().size() != answer->contents().size())
    return false;

  for (size_t i = 0; i < offer->contents().size(); ++i) {
    if ((offer->contents()[i].name) != answer->contents()[i].name) {
      return false;
    }
    const MediaContentDescription* offer_mdesc =
        static_cast<const MediaContentDescription*>(
            offer->contents()[i].description);
    const MediaContentDescription* answer_mdesc =
        static_cast<const MediaContentDescription*>(
            answer->contents()[i].description);
    if (offer_mdesc->type() != answer_mdesc->type()) {
      return false;
    }
  }
  return true;
}

// Checks that each non-rejected content has SDES crypto keys or a DTLS
// fingerprint. Mismatches, such as replying with a DTLS fingerprint to SDES
// keys, will be caught in Transport negotiation, and backstopped by Channel's
// |secure_required| check.
static bool VerifyCrypto(const SessionDescription* desc,
                         bool dtls_enabled,
                         std::string* error) {
  const ContentInfos& contents = desc->contents();
  for (size_t index = 0; index < contents.size(); ++index) {
    const ContentInfo* cinfo = &contents[index];
    if (cinfo->rejected) {
      continue;
    }

    // If the content isn't rejected, crypto must be present.
    const MediaContentDescription* media =
        static_cast<const MediaContentDescription*>(cinfo->description);
    const TransportInfo* tinfo = desc->GetTransportInfoByName(cinfo->name);
    if (!media || !tinfo) {
      // Something is not right.
      LOG(LS_ERROR) << kInvalidSdp;
      *error = kInvalidSdp;
      return false;
    }
    if (dtls_enabled) {
      if (!tinfo->description.identity_fingerprint) {
        LOG(LS_WARNING) <<
            "Session description must have DTLS fingerprint if DTLS enabled.";
        *error = kSdpWithoutDtlsFingerprint;
        return false;
      }
    } else {
      if (media->cryptos().empty()) {
        LOG(LS_WARNING) <<
            "Session description must have SDES when DTLS disabled.";
        *error = kSdpWithoutSdesCrypto;
        return false;
      }
    }
  }

  return true;
}

// Checks that each non-rejected content has ice-ufrag and ice-pwd set.
static bool VerifyIceUfragPwdPresent(const SessionDescription* desc) {
  const ContentInfos& contents = desc->contents();
  for (size_t index = 0; index < contents.size(); ++index) {
    const ContentInfo* cinfo = &contents[index];
    if (cinfo->rejected) {
      continue;
    }

    // If the content isn't rejected, ice-ufrag and ice-pwd must be present.
    const TransportInfo* tinfo = desc->GetTransportInfoByName(cinfo->name);
    if (!tinfo) {
      // Something is not right.
      LOG(LS_ERROR) << kInvalidSdp;
      return false;
    }
    if (tinfo->description.ice_ufrag.empty() ||
        tinfo->description.ice_pwd.empty()) {
      LOG(LS_ERROR) << "Session description must have ice ufrag and pwd.";
      return false;
    }
  }
  return true;
}

// Forces |sdesc->crypto_required| to the appropriate state based on the
// current security policy, to ensure a failure occurs if there is an error
// in crypto negotiation.
// Called when processing the local session description.
static void UpdateSessionDescriptionSecurePolicy(cricket::CryptoType type,
                                                 SessionDescription* sdesc) {
  if (!sdesc) {
    return;
  }

  // Updating the |crypto_required_| in MediaContentDescription to the
  // appropriate state based on the current security policy.
  for (cricket::ContentInfos::iterator iter = sdesc->contents().begin();
       iter != sdesc->contents().end(); ++iter) {
    if (cricket::IsMediaContent(&*iter)) {
      MediaContentDescription* mdesc =
          static_cast<MediaContentDescription*> (iter->description);
      if (mdesc) {
        mdesc->set_crypto_required(type);
      }
    }
  }
}

static bool GetAudioSsrcByTrackId(
    const SessionDescription* session_description,
    const std::string& track_id, uint32 *ssrc) {
  const cricket::ContentInfo* audio_info =
      cricket::GetFirstAudioContent(session_description);
  if (!audio_info) {
    LOG(LS_ERROR) << "Audio not used in this call";
    return false;
  }

  const cricket::MediaContentDescription* audio_content =
      static_cast<const cricket::MediaContentDescription*>(
          audio_info->description);
  const cricket::StreamParams* stream =
      cricket::GetStreamByIds(audio_content->streams(), "", track_id);
  if (!stream) {
    return false;
  }

  *ssrc = stream->first_ssrc();
  return true;
}

static bool GetTrackIdBySsrc(const SessionDescription* session_description,
                             uint32 ssrc, std::string* track_id) {
  ASSERT(track_id != NULL);

  const cricket::ContentInfo* audio_info =
      cricket::GetFirstAudioContent(session_description);
  if (audio_info) {
    const cricket::MediaContentDescription* audio_content =
        static_cast<const cricket::MediaContentDescription*>(
            audio_info->description);

    const auto* found =
        cricket::GetStreamBySsrc(audio_content->streams(), ssrc);
    if (found) {
      *track_id = found->id;
      return true;
    }
  }

  const cricket::ContentInfo* video_info =
      cricket::GetFirstVideoContent(session_description);
  if (video_info) {
    const cricket::MediaContentDescription* video_content =
        static_cast<const cricket::MediaContentDescription*>(
            video_info->description);

    const auto* found =
        cricket::GetStreamBySsrc(video_content->streams(), ssrc);
    if (found) {
      *track_id = found->id;
      return true;
    }
  }
  return false;
}

static bool BadSdp(const std::string& source,
                   const std::string& type,
                   const std::string& reason,
                   std::string* err_desc) {
  std::ostringstream desc;
  desc << "Failed to set " << source << " " << type << " sdp: " << reason;

  if (err_desc) {
    *err_desc = desc.str();
  }
  LOG(LS_ERROR) << desc.str();
  return false;
}

static bool BadSdp(cricket::ContentSource source,
                   const std::string& type,
                   const std::string& reason,
                   std::string* err_desc) {
  if (source == cricket::CS_LOCAL) {
    return BadSdp("local", type, reason, err_desc);
  } else {
    return BadSdp("remote", type, reason, err_desc);
  }
}

static bool BadLocalSdp(const std::string& type,
                        const std::string& reason,
                        std::string* err_desc) {
  return BadSdp(cricket::CS_LOCAL, type, reason, err_desc);
}

static bool BadRemoteSdp(const std::string& type,
                         const std::string& reason,
                         std::string* err_desc) {
  return BadSdp(cricket::CS_REMOTE, type, reason, err_desc);
}

static bool BadOfferSdp(cricket::ContentSource source,
                        const std::string& reason,
                        std::string* err_desc) {
  return BadSdp(source, SessionDescriptionInterface::kOffer, reason, err_desc);
}

static bool BadPranswerSdp(cricket::ContentSource source,
                           const std::string& reason,
                           std::string* err_desc) {
  return BadSdp(source, SessionDescriptionInterface::kPrAnswer,
                reason, err_desc);
}

static bool BadAnswerSdp(cricket::ContentSource source,
                         const std::string& reason,
                         std::string* err_desc) {
  return BadSdp(source, SessionDescriptionInterface::kAnswer, reason, err_desc);
}

#define GET_STRING_OF_STATE(state)  \
  case cricket::BaseSession::state:  \
    result = #state;  \
    break;

static std::string GetStateString(cricket::BaseSession::State state) {
  std::string result;
  switch (state) {
    GET_STRING_OF_STATE(STATE_INIT)
    GET_STRING_OF_STATE(STATE_SENTINITIATE)
    GET_STRING_OF_STATE(STATE_RECEIVEDINITIATE)
    GET_STRING_OF_STATE(STATE_SENTPRACCEPT)
    GET_STRING_OF_STATE(STATE_SENTACCEPT)
    GET_STRING_OF_STATE(STATE_RECEIVEDPRACCEPT)
    GET_STRING_OF_STATE(STATE_RECEIVEDACCEPT)
    GET_STRING_OF_STATE(STATE_SENTMODIFY)
    GET_STRING_OF_STATE(STATE_RECEIVEDMODIFY)
    GET_STRING_OF_STATE(STATE_SENTREJECT)
    GET_STRING_OF_STATE(STATE_RECEIVEDREJECT)
    GET_STRING_OF_STATE(STATE_SENTREDIRECT)
    GET_STRING_OF_STATE(STATE_SENTTERMINATE)
    GET_STRING_OF_STATE(STATE_RECEIVEDTERMINATE)
    GET_STRING_OF_STATE(STATE_INPROGRESS)
    GET_STRING_OF_STATE(STATE_DEINIT)
    default:
      ASSERT(false);
      break;
  }
  return result;
}

#define GET_STRING_OF_ERROR_CODE(err)  \
  case cricket::BaseSession::err:  \
    result = #err;  \
    break;

static std::string GetErrorCodeString(cricket::BaseSession::Error err) {
  std::string result;
  switch (err) {
    GET_STRING_OF_ERROR_CODE(ERROR_NONE)
    GET_STRING_OF_ERROR_CODE(ERROR_TIME)
    GET_STRING_OF_ERROR_CODE(ERROR_RESPONSE)
    GET_STRING_OF_ERROR_CODE(ERROR_NETWORK)
    GET_STRING_OF_ERROR_CODE(ERROR_CONTENT)
    GET_STRING_OF_ERROR_CODE(ERROR_TRANSPORT)
    default:
      ASSERT(false);
      break;
  }
  return result;
}

static std::string MakeErrorString(const std::string& error,
                                   const std::string& desc) {
  std::ostringstream ret;
  ret << error << " " << desc;
  return ret.str();
}

static std::string MakeTdErrorString(const std::string& desc) {
  return MakeErrorString(kPushDownTDFailed, desc);
}

// Set |option| to the highest-priority value of |key| in the optional
// constraints if the key is found and has a valid value.
template<typename T>
static void SetOptionFromOptionalConstraint(
    const MediaConstraintsInterface* constraints,
    const std::string& key, cricket::Settable<T>* option) {
  if (!constraints) {
    return;
  }
  std::string string_value;
  T value;
  if (constraints->GetOptional().FindFirst(key, &string_value)) {
    if (rtc::FromString(string_value, &value)) {
      option->Set(value);
    }
  }
}

uint32 ConvertIceTransportTypeToCandidateFilter(
    PeerConnectionInterface::IceTransportsType type) {
  switch (type) {
    case PeerConnectionInterface::kNone:
        return cricket::CF_NONE;
    case PeerConnectionInterface::kRelay:
        return cricket::CF_RELAY;
    case PeerConnectionInterface::kNoHost:
        return (cricket::CF_ALL & ~cricket::CF_HOST);
    case PeerConnectionInterface::kAll:
        return cricket::CF_ALL;
    default: ASSERT(false);
  }
  return cricket::CF_NONE;
}

// Help class used to remember if a a remote peer has requested ice restart by
// by sending a description with new ice ufrag and password.
class IceRestartAnswerLatch {
 public:
  IceRestartAnswerLatch() : ice_restart_(false) { }

  // Returns true if CheckForRemoteIceRestart has been called with a new session
  // description where ice password and ufrag has changed since last time
  // Reset() was called.
  bool Get() const {
    return ice_restart_;
  }

  void Reset() {
    if (ice_restart_) {
      ice_restart_ = false;
    }
  }

  void CheckForRemoteIceRestart(
      const SessionDescriptionInterface* old_desc,
      const SessionDescriptionInterface* new_desc) {
    if (!old_desc || new_desc->type() != SessionDescriptionInterface::kOffer) {
      return;
    }
    const SessionDescription* new_sd = new_desc->description();
    const SessionDescription* old_sd = old_desc->description();
    const ContentInfos& contents = new_sd->contents();
    for (size_t index = 0; index < contents.size(); ++index) {
      const ContentInfo* cinfo = &contents[index];
      if (cinfo->rejected) {
        continue;
      }
      // If the content isn't rejected, check if ufrag and password has
      // changed.
      const cricket::TransportDescription* new_transport_desc =
          new_sd->GetTransportDescriptionByName(cinfo->name);
      const cricket::TransportDescription* old_transport_desc =
          old_sd->GetTransportDescriptionByName(cinfo->name);
      if (!new_transport_desc || !old_transport_desc) {
        // No transport description exist. This is not an ice restart.
        continue;
      }
      if (cricket::IceCredentialsChanged(old_transport_desc->ice_ufrag,
                                         old_transport_desc->ice_pwd,
                                         new_transport_desc->ice_ufrag,
                                         new_transport_desc->ice_pwd)) {
        LOG(LS_INFO) << "Remote peer request ice restart.";
        ice_restart_ = true;
        break;
      }
    }
  }

 private:
  bool ice_restart_;
};

WebRtcSession::WebRtcSession(
    cricket::ChannelManager* channel_manager,
    rtc::Thread* signaling_thread,
    rtc::Thread* worker_thread,
    cricket::PortAllocator* port_allocator,
    MediaStreamSignaling* mediastream_signaling)
    : cricket::BaseSession(signaling_thread,
                           worker_thread,
                           port_allocator,
                           rtc::ToString(rtc::CreateRandomId64() & LLONG_MAX),
                           cricket::NS_JINGLE_RTP,
                           false),
      // RFC 3264: The numeric value of the session id and version in the
      // o line MUST be representable with a "64 bit signed integer".
      // Due to this constraint session id |sid_| is max limited to LLONG_MAX.
      channel_manager_(channel_manager),
      mediastream_signaling_(mediastream_signaling),
      ice_observer_(NULL),
      ice_connection_state_(PeerConnectionInterface::kIceConnectionNew),
      ice_connection_receiving_(true),
      older_version_remote_peer_(false),
      dtls_enabled_(false),
      data_channel_type_(cricket::DCT_NONE),
      ice_restart_latch_(new IceRestartAnswerLatch),
      metrics_observer_(NULL) {
}

WebRtcSession::~WebRtcSession() {
  ASSERT(signaling_thread()->IsCurrent());
  // Destroy video_channel_ first since it may have a pointer to the
  // voice_channel_.
  if (video_channel_) {
    SignalVideoChannelDestroyed();
    channel_manager_->DestroyVideoChannel(video_channel_.release());
  }
  if (voice_channel_) {
    SignalVoiceChannelDestroyed();
    channel_manager_->DestroyVoiceChannel(voice_channel_.release(), nullptr);
  }
  if (data_channel_) {
    SignalDataChannelDestroyed();
    channel_manager_->DestroyDataChannel(data_channel_.release());
  }
  for (size_t i = 0; i < saved_candidates_.size(); ++i) {
    delete saved_candidates_[i];
  }
  delete identity();
}

bool WebRtcSession::Initialize(
    const PeerConnectionFactoryInterface::Options& options,
    const MediaConstraintsInterface*  constraints,
    DTLSIdentityServiceInterface* dtls_identity_service,
    const PeerConnectionInterface::RTCConfiguration& rtc_configuration) {
  bundle_policy_ = rtc_configuration.bundle_policy;
  rtcp_mux_policy_ = rtc_configuration.rtcp_mux_policy;
  SetSslMaxProtocolVersion(options.ssl_max_version);

  // TODO(perkj): Take |constraints| into consideration. Return false if not all
  // mandatory constraints can be fulfilled. Note that |constraints|
  // can be null.
  bool value;

  if (options.disable_encryption) {
    dtls_enabled_ = false;
  } else {
    // Enable DTLS by default if |dtls_identity_service| is valid.
    dtls_enabled_ = (dtls_identity_service != NULL);
    // |constraints| can override the default |dtls_enabled_| value.
    if (FindConstraint(
          constraints,
          MediaConstraintsInterface::kEnableDtlsSrtp,
          &value, NULL)) {
      dtls_enabled_ = value;
    }
  }

  // Enable creation of RTP data channels if the kEnableRtpDataChannels is set.
  // It takes precendence over the disable_sctp_data_channels
  // PeerConnectionFactoryInterface::Options.
  if (FindConstraint(
      constraints, MediaConstraintsInterface::kEnableRtpDataChannels,
      &value, NULL) && value) {
    LOG(LS_INFO) << "Allowing RTP data engine.";
    data_channel_type_ = cricket::DCT_RTP;
  } else {
    // DTLS has to be enabled to use SCTP.
    if (!options.disable_sctp_data_channels && dtls_enabled_) {
      LOG(LS_INFO) << "Allowing SCTP data engine.";
      data_channel_type_ = cricket::DCT_SCTP;
    }
  }
  if (data_channel_type_ != cricket::DCT_NONE) {
    mediastream_signaling_->SetDataChannelFactory(this);
  }

  // Find DSCP constraint.
  if (FindConstraint(
        constraints,
        MediaConstraintsInterface::kEnableDscp,
        &value, NULL)) {
    audio_options_.dscp.Set(value);
    video_options_.dscp.Set(value);
  }

  // Find Suspend Below Min Bitrate constraint.
  if (FindConstraint(
          constraints,
          MediaConstraintsInterface::kEnableVideoSuspendBelowMinBitrate,
          &value,
          NULL)) {
    video_options_.suspend_below_min_bitrate.Set(value);
  }

  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kScreencastMinBitrate,
      &video_options_.screencast_min_bitrate);

  // Find constraints for cpu overuse detection.
  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kCpuUnderuseThreshold,
      &video_options_.cpu_underuse_threshold);
  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kCpuOveruseThreshold,
      &video_options_.cpu_overuse_threshold);
  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kCpuOveruseDetection,
      &video_options_.cpu_overuse_detection);
  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kCpuOveruseEncodeUsage,
      &video_options_.cpu_overuse_encode_usage);
  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kCpuUnderuseEncodeRsdThreshold,
      &video_options_.cpu_underuse_encode_rsd_threshold);
  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kCpuOveruseEncodeRsdThreshold,
      &video_options_.cpu_overuse_encode_rsd_threshold);

  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kNumUnsignalledRecvStreams,
      &video_options_.unsignalled_recv_stream_limit);
  if (video_options_.unsignalled_recv_stream_limit.IsSet()) {
    int stream_limit;
    video_options_.unsignalled_recv_stream_limit.Get(&stream_limit);
    stream_limit = std::min(kMaxUnsignalledRecvStreams, stream_limit);
    stream_limit = std::max(0, stream_limit);
    video_options_.unsignalled_recv_stream_limit.Set(stream_limit);
  }

  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kHighStartBitrate,
      &video_options_.video_start_bitrate);

  if (FindConstraint(
      constraints,
      MediaConstraintsInterface::kVeryHighBitrate,
      &value,
      NULL)) {
    video_options_.video_highest_bitrate.Set(
        cricket::VideoOptions::VERY_HIGH);
  } else if (FindConstraint(
      constraints,
      MediaConstraintsInterface::kHighBitrate,
      &value,
      NULL)) {
    video_options_.video_highest_bitrate.Set(
        cricket::VideoOptions::HIGH);
  }

  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kCombinedAudioVideoBwe,
      &audio_options_.combined_audio_video_bwe);

  audio_options_.audio_jitter_buffer_max_packets.Set(
      rtc_configuration.audio_jitter_buffer_max_packets);

  audio_options_.audio_jitter_buffer_fast_accelerate.Set(
      rtc_configuration.audio_jitter_buffer_fast_accelerate);

  const cricket::VideoCodec default_codec(
      JsepSessionDescription::kDefaultVideoCodecId,
      JsepSessionDescription::kDefaultVideoCodecName,
      JsepSessionDescription::kMaxVideoCodecWidth,
      JsepSessionDescription::kMaxVideoCodecHeight,
      JsepSessionDescription::kDefaultVideoCodecFramerate,
      JsepSessionDescription::kDefaultVideoCodecPreference);
  channel_manager_->SetDefaultVideoEncoderConfig(
      cricket::VideoEncoderConfig(default_codec));

  webrtc_session_desc_factory_.reset(new WebRtcSessionDescriptionFactory(
      signaling_thread(),
      channel_manager_,
      mediastream_signaling_,
      dtls_identity_service,
      this,
      id(),
      data_channel_type_,
      dtls_enabled_));

  webrtc_session_desc_factory_->SignalIdentityReady.connect(
      this, &WebRtcSession::OnIdentityReady);

  if (options.disable_encryption) {
    webrtc_session_desc_factory_->SetSdesPolicy(cricket::SEC_DISABLED);
  }
  port_allocator()->set_candidate_filter(
      ConvertIceTransportTypeToCandidateFilter(rtc_configuration.type));
  return true;
}

void WebRtcSession::Terminate() {
  SetState(STATE_RECEIVEDTERMINATE);
  RemoveUnusedChannelsAndTransports(NULL);
  ASSERT(!voice_channel_);
  ASSERT(!video_channel_);
  ASSERT(!data_channel_);
}

bool WebRtcSession::StartCandidatesAllocation() {
  // SpeculativelyConnectTransportChannels, will call ConnectChannels method
  // from TransportProxy to start gathering ice candidates.
  SpeculativelyConnectAllTransportChannels();
  if (!saved_candidates_.empty()) {
    // If there are saved candidates which arrived before local description is
    // set, copy those to remote description.
    CopySavedCandidates(remote_desc_.get());
  }
  // Push remote candidates present in remote description to transport channels.
  UseCandidatesInSessionDescription(remote_desc_.get());
  return true;
}

void WebRtcSession::SetSdesPolicy(cricket::SecurePolicy secure_policy) {
  webrtc_session_desc_factory_->SetSdesPolicy(secure_policy);
}

cricket::SecurePolicy WebRtcSession::SdesPolicy() const {
  return webrtc_session_desc_factory_->SdesPolicy();
}

bool WebRtcSession::GetSslRole(rtc::SSLRole* role) {
  if (local_description() == NULL || remote_description() == NULL) {
    LOG(LS_INFO) << "Local and Remote descriptions must be applied to get "
                 << "SSL Role of the session.";
    return false;
  }

  // TODO(mallinath) - Return role of each transport, as role may differ from
  // one another.
  // In current implementaion we just return the role of first transport in the
  // transport map.
  for (cricket::TransportMap::const_iterator iter = transport_proxies().begin();
       iter != transport_proxies().end(); ++iter) {
    if (iter->second->impl()) {
      return iter->second->impl()->GetSslRole(role);
    }
  }
  return false;
}

void WebRtcSession::CreateOffer(
    CreateSessionDescriptionObserver* observer,
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  webrtc_session_desc_factory_->CreateOffer(observer, options);
}

void WebRtcSession::CreateAnswer(CreateSessionDescriptionObserver* observer,
                                 const MediaConstraintsInterface* constraints) {
  webrtc_session_desc_factory_->CreateAnswer(observer, constraints);
}

bool WebRtcSession::SetLocalDescription(SessionDescriptionInterface* desc,
                                        std::string* err_desc) {
  // Takes the ownership of |desc| regardless of the result.
  rtc::scoped_ptr<SessionDescriptionInterface> desc_temp(desc);

  // Validate SDP.
  if (!ValidateSessionDescription(desc, cricket::CS_LOCAL, err_desc)) {
    return false;
  }

  // Update the initiator flag if this session is the initiator.
  Action action = GetAction(desc->type());
  if (state() == STATE_INIT && action == kOffer) {
    set_initiator(true);
  }

  cricket::SecurePolicy sdes_policy =
      webrtc_session_desc_factory_->SdesPolicy();
  cricket::CryptoType crypto_required = dtls_enabled_ ?
      cricket::CT_DTLS : (sdes_policy == cricket::SEC_REQUIRED ?
          cricket::CT_SDES : cricket::CT_NONE);
  // Update the MediaContentDescription crypto settings as per the policy set.
  UpdateSessionDescriptionSecurePolicy(crypto_required, desc->description());

  set_local_description(desc->description()->Copy());
  local_desc_.reset(desc_temp.release());

  // Transport and Media channels will be created only when offer is set.
  if (action == kOffer && !CreateChannels(local_desc_->description())) {
    // TODO(mallinath) - Handle CreateChannel failure, as new local description
    // is applied. Restore back to old description.
    return BadLocalSdp(desc->type(), kCreateChannelFailed, err_desc);
  }

  // Remove channel and transport proxies, if MediaContentDescription is
  // rejected.
  RemoveUnusedChannelsAndTransports(local_desc_->description());

  if (!UpdateSessionState(action, cricket::CS_LOCAL, err_desc)) {
    return false;
  }

  // Kick starting the ice candidates allocation.
  StartCandidatesAllocation();

  // Update state and SSRC of local MediaStreams and DataChannels based on the
  // local session description.
  mediastream_signaling_->OnLocalDescriptionChanged(local_desc_.get());

  rtc::SSLRole role;
  if (data_channel_type_ == cricket::DCT_SCTP && GetSslRole(&role)) {
    mediastream_signaling_->OnDtlsRoleReadyForSctp(role);
  }
  if (error() != cricket::BaseSession::ERROR_NONE) {
    return BadLocalSdp(desc->type(), GetSessionErrorMsg(), err_desc);
  }
  return true;
}

bool WebRtcSession::SetRemoteDescription(SessionDescriptionInterface* desc,
                                         std::string* err_desc) {
  // Takes the ownership of |desc| regardless of the result.
  rtc::scoped_ptr<SessionDescriptionInterface> desc_temp(desc);

  // Validate SDP.
  if (!ValidateSessionDescription(desc, cricket::CS_REMOTE, err_desc)) {
    return false;
  }

  // Transport and Media channels will be created only when offer is set.
  Action action = GetAction(desc->type());
  if (action == kOffer && !CreateChannels(desc->description())) {
    // TODO(mallinath) - Handle CreateChannel failure, as new local description
    // is applied. Restore back to old description.
    return BadRemoteSdp(desc->type(), kCreateChannelFailed, err_desc);
  }

  // Remove channel and transport proxies, if MediaContentDescription is
  // rejected.
  RemoveUnusedChannelsAndTransports(desc->description());

  // NOTE: Candidates allocation will be initiated only when SetLocalDescription
  // is called.
  set_remote_description(desc->description()->Copy());
  if (!UpdateSessionState(action, cricket::CS_REMOTE, err_desc)) {
    return false;
  }

  // Update remote MediaStreams.
  mediastream_signaling_->OnRemoteDescriptionChanged(desc);
  if (local_description() && !UseCandidatesInSessionDescription(desc)) {
    return BadRemoteSdp(desc->type(), kInvalidCandidates, err_desc);
  }

  // Copy all saved candidates.
  CopySavedCandidates(desc);
  // We retain all received candidates.
  WebRtcSessionDescriptionFactory::CopyCandidatesFromSessionDescription(
      remote_desc_.get(), desc);
  // Check if this new SessionDescription contains new ice ufrag and password
  // that indicates the remote peer requests ice restart.
  ice_restart_latch_->CheckForRemoteIceRestart(remote_desc_.get(),
                                               desc);
  remote_desc_.reset(desc_temp.release());

  rtc::SSLRole role;
  if (data_channel_type_ == cricket::DCT_SCTP && GetSslRole(&role)) {
    mediastream_signaling_->OnDtlsRoleReadyForSctp(role);
  }

  if (error() != cricket::BaseSession::ERROR_NONE) {
    return BadRemoteSdp(desc->type(), GetSessionErrorMsg(), err_desc);
  }

  // Set the the ICE connection state to connecting since the connection may
  // become writable with peer reflexive candidates before any remote candidate
  // is signaled.
  // TODO(pthatcher): This is a short-term solution for crbug/446908. A real fix
  // is to have a new signal the indicates a change in checking state from the
  // transport and expose a new checking() member from transport that can be
  // read to determine the current checking state. The existing SignalConnecting
  // actually means "gathering candidates", so cannot be be used here.
  if (desc->type() != SessionDescriptionInterface::kOffer &&
      ice_connection_state_ == PeerConnectionInterface::kIceConnectionNew) {
    SetIceConnectionState(PeerConnectionInterface::kIceConnectionChecking);
  }
  return true;
}

bool WebRtcSession::UpdateSessionState(
    Action action, cricket::ContentSource source,
    std::string* err_desc) {
  // If there's already a pending error then no state transition should happen.
  // But all call-sites should be verifying this before calling us!
  ASSERT(error() == cricket::BaseSession::ERROR_NONE);
  std::string td_err;
  if (action == kOffer) {
    if (!PushdownTransportDescription(source, cricket::CA_OFFER, &td_err)) {
      return BadOfferSdp(source, MakeTdErrorString(td_err), err_desc);
    }
    SetState(source == cricket::CS_LOCAL ?
        STATE_SENTINITIATE : STATE_RECEIVEDINITIATE);
    if (!PushdownMediaDescription(cricket::CA_OFFER, source, err_desc)) {
      SetError(BaseSession::ERROR_CONTENT, *err_desc);
    }
    if (error() != cricket::BaseSession::ERROR_NONE) {
      return BadOfferSdp(source, GetSessionErrorMsg(), err_desc);
    }
  } else if (action == kPrAnswer) {
    if (!PushdownTransportDescription(source, cricket::CA_PRANSWER, &td_err)) {
      return BadPranswerSdp(source, MakeTdErrorString(td_err), err_desc);
    }
    EnableChannels();
    SetState(source == cricket::CS_LOCAL ?
        STATE_SENTPRACCEPT : STATE_RECEIVEDPRACCEPT);
    if (!PushdownMediaDescription(cricket::CA_PRANSWER, source, err_desc)) {
      SetError(BaseSession::ERROR_CONTENT, *err_desc);
    }
    if (error() != cricket::BaseSession::ERROR_NONE) {
      return BadPranswerSdp(source, GetSessionErrorMsg(), err_desc);
    }
  } else if (action == kAnswer) {
    if (!PushdownTransportDescription(source, cricket::CA_ANSWER, &td_err)) {
      return BadAnswerSdp(source, MakeTdErrorString(td_err), err_desc);
    }
    MaybeEnableMuxingSupport();
    EnableChannels();
    SetState(source == cricket::CS_LOCAL ?
        STATE_SENTACCEPT : STATE_RECEIVEDACCEPT);
    if (!PushdownMediaDescription(cricket::CA_ANSWER, source, err_desc)) {
      SetError(BaseSession::ERROR_CONTENT, *err_desc);
    }
    if (error() != cricket::BaseSession::ERROR_NONE) {
      return BadAnswerSdp(source, GetSessionErrorMsg(), err_desc);
    }
  }
  return true;
}

bool WebRtcSession::PushdownMediaDescription(
    cricket::ContentAction action,
    cricket::ContentSource source,
    std::string* err) {
  auto set_content = [this, action, source, err](cricket::BaseChannel* ch) {
    if (!ch) {
      return true;
    } else if (source == cricket::CS_LOCAL) {
      return ch->PushdownLocalDescription(
          base_local_description(), action, err);
    } else {
      return ch->PushdownRemoteDescription(
          base_remote_description(), action, err);
    }
  };

  return (set_content(voice_channel()) &&
          set_content(video_channel()) &&
          set_content(data_channel()));
}

WebRtcSession::Action WebRtcSession::GetAction(const std::string& type) {
  if (type == SessionDescriptionInterface::kOffer) {
    return WebRtcSession::kOffer;
  } else if (type == SessionDescriptionInterface::kPrAnswer) {
    return WebRtcSession::kPrAnswer;
  } else if (type == SessionDescriptionInterface::kAnswer) {
    return WebRtcSession::kAnswer;
  }
  ASSERT(false && "unknown action type");
  return WebRtcSession::kOffer;
}

bool WebRtcSession::GetTransportStats(cricket::SessionStats* stats) {
  ASSERT(signaling_thread()->IsCurrent());

  const auto get_transport_stats = [stats](const std::string& content_name,
                                           cricket::Transport* transport) {
    const std::string& transport_id = transport->content_name();
    stats->proxy_to_transport[content_name] = transport_id;
    if (stats->transport_stats.find(transport_id)
        != stats->transport_stats.end()) {
      // Transport stats already done for this transport.
      return true;
    }

    cricket::TransportStats tstats;
    if (!transport->GetStats(&tstats)) {
      return false;
    }

    stats->transport_stats[transport_id] = tstats;
    return true;
  };

  for (const auto& kv : transport_proxies()) {
    cricket::Transport* transport = kv.second->impl();
    if (transport && !get_transport_stats(kv.first, transport)) {
      return false;
    }
  }
  return true;
}

bool WebRtcSession::ProcessIceMessage(const IceCandidateInterface* candidate) {
  if (state() == STATE_INIT) {
     LOG(LS_ERROR) << "ProcessIceMessage: ICE candidates can't be added "
                   << "without any offer (local or remote) "
                   << "session description.";
     return false;
  }

  if (!candidate) {
    LOG(LS_ERROR) << "ProcessIceMessage: Candidate is NULL";
    return false;
  }

  bool valid = false;
  if (!ReadyToUseRemoteCandidate(candidate, NULL, &valid)) {
    if (valid) {
      LOG(LS_INFO) << "ProcessIceMessage: Candidate saved";
      saved_candidates_.push_back(
          new JsepIceCandidate(candidate->sdp_mid(),
                               candidate->sdp_mline_index(),
                               candidate->candidate()));
    }
    return valid;
  }

  // Add this candidate to the remote session description.
  if (!remote_desc_->AddCandidate(candidate)) {
    LOG(LS_ERROR) << "ProcessIceMessage: Candidate cannot be used";
    return false;
  }

  return UseCandidate(candidate);
}

bool WebRtcSession::SetIceTransports(
    PeerConnectionInterface::IceTransportsType type) {
  return port_allocator()->set_candidate_filter(
        ConvertIceTransportTypeToCandidateFilter(type));
}

bool WebRtcSession::GetLocalTrackIdBySsrc(uint32 ssrc, std::string* track_id) {
  if (!base_local_description())
    return false;
  return webrtc::GetTrackIdBySsrc(base_local_description(), ssrc, track_id);
}

bool WebRtcSession::GetRemoteTrackIdBySsrc(uint32 ssrc, std::string* track_id) {
  if (!base_remote_description())
    return false;
  return webrtc::GetTrackIdBySsrc(base_remote_description(), ssrc, track_id);
}

std::string WebRtcSession::BadStateErrMsg(State state) {
  std::ostringstream desc;
  desc << "Called in wrong state: " << GetStateString(state);
  return desc.str();
}

void WebRtcSession::SetAudioPlayout(uint32 ssrc, bool enable,
                                    cricket::AudioRenderer* renderer) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!voice_channel_) {
    LOG(LS_ERROR) << "SetAudioPlayout: No audio channel exists.";
    return;
  }
  if (!voice_channel_->SetRemoteRenderer(ssrc, renderer)) {
    // SetRenderer() can fail if the ssrc does not match any playout channel.
    LOG(LS_ERROR) << "SetAudioPlayout: ssrc is incorrect: " << ssrc;
    return;
  }
  if (!voice_channel_->SetOutputScaling(ssrc, enable ? 1 : 0, enable ? 1 : 0)) {
    // Allow that SetOutputScaling fail if |enable| is false but assert
    // otherwise. This in the normal case when the underlying media channel has
    // already been deleted.
    ASSERT(enable == false);
  }
}

void WebRtcSession::SetAudioSend(uint32 ssrc, bool enable,
                                 const cricket::AudioOptions& options,
                                 cricket::AudioRenderer* renderer) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!voice_channel_) {
    LOG(LS_ERROR) << "SetAudioSend: No audio channel exists.";
    return;
  }
  if (!voice_channel_->SetLocalRenderer(ssrc, renderer)) {
    // SetRenderer() can fail if the ssrc does not match any send channel.
    LOG(LS_ERROR) << "SetAudioSend: ssrc is incorrect: " << ssrc;
    return;
  }
  if (!voice_channel_->MuteStream(ssrc, !enable)) {
    // Allow that MuteStream fail if |enable| is false but assert otherwise.
    // This in the normal case when the underlying media channel has already
    // been deleted.
    ASSERT(enable == false);
    return;
  }
  if (enable)
    voice_channel_->SetChannelOptions(options);
}

void WebRtcSession::SetAudioPlayoutVolume(uint32 ssrc, double volume) {
  ASSERT(signaling_thread()->IsCurrent());
  ASSERT(volume >= 0 && volume <= 10);
  if (!voice_channel_) {
    LOG(LS_ERROR) << "SetAudioPlayoutVolume: No audio channel exists.";
    return;
  }

  if (!voice_channel_->SetOutputScaling(ssrc, volume, volume))
    ASSERT(false);
}

bool WebRtcSession::SetCaptureDevice(uint32 ssrc,
                                     cricket::VideoCapturer* camera) {
  ASSERT(signaling_thread()->IsCurrent());

  if (!video_channel_) {
    // |video_channel_| doesnt't exist. Probably because the remote end doesnt't
    // support video.
    LOG(LS_WARNING) << "Video not used in this call.";
    return false;
  }
  if (!video_channel_->SetCapturer(ssrc, camera)) {
    // Allow that SetCapturer fail if |camera| is NULL but assert otherwise.
    // This in the normal case when the underlying media channel has already
    // been deleted.
    ASSERT(camera == NULL);
    return false;
  }
  return true;
}

void WebRtcSession::SetVideoPlayout(uint32 ssrc,
                                    bool enable,
                                    cricket::VideoRenderer* renderer) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!video_channel_) {
    LOG(LS_WARNING) << "SetVideoPlayout: No video channel exists.";
    return;
  }
  if (!video_channel_->SetRenderer(ssrc, enable ? renderer : NULL)) {
    // Allow that SetRenderer fail if |renderer| is NULL but assert otherwise.
    // This in the normal case when the underlying media channel has already
    // been deleted.
    ASSERT(renderer == NULL);
  }
}

void WebRtcSession::SetVideoSend(uint32 ssrc, bool enable,
                                 const cricket::VideoOptions* options) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!video_channel_) {
    LOG(LS_WARNING) << "SetVideoSend: No video channel exists.";
    return;
  }
  if (!video_channel_->MuteStream(ssrc, !enable)) {
    // Allow that MuteStream fail if |enable| is false but assert otherwise.
    // This in the normal case when the underlying media channel has already
    // been deleted.
    ASSERT(enable == false);
    return;
  }
  if (enable && options)
    video_channel_->SetChannelOptions(*options);
}

bool WebRtcSession::CanInsertDtmf(const std::string& track_id) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!voice_channel_) {
    LOG(LS_ERROR) << "CanInsertDtmf: No audio channel exists.";
    return false;
  }
  uint32 send_ssrc = 0;
  // The Dtmf is negotiated per channel not ssrc, so we only check if the ssrc
  // exists.
  if (!GetAudioSsrcByTrackId(base_local_description(), track_id,
                             &send_ssrc)) {
    LOG(LS_ERROR) << "CanInsertDtmf: Track does not exist: " << track_id;
    return false;
  }
  return voice_channel_->CanInsertDtmf();
}

bool WebRtcSession::InsertDtmf(const std::string& track_id,
                               int code, int duration) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!voice_channel_) {
    LOG(LS_ERROR) << "InsertDtmf: No audio channel exists.";
    return false;
  }
  uint32 send_ssrc = 0;
  if (!VERIFY(GetAudioSsrcByTrackId(base_local_description(),
                                    track_id, &send_ssrc))) {
    LOG(LS_ERROR) << "InsertDtmf: Track does not exist: " << track_id;
    return false;
  }
  if (!voice_channel_->InsertDtmf(send_ssrc, code, duration,
                                  cricket::DF_SEND)) {
    LOG(LS_ERROR) << "Failed to insert DTMF to channel.";
    return false;
  }
  return true;
}

sigslot::signal0<>* WebRtcSession::GetOnDestroyedSignal() {
  return &SignalVoiceChannelDestroyed;
}

bool WebRtcSession::SendData(const cricket::SendDataParams& params,
                             const rtc::Buffer& payload,
                             cricket::SendDataResult* result) {
  if (!data_channel_) {
    LOG(LS_ERROR) << "SendData called when data_channel_ is NULL.";
    return false;
  }
  return data_channel_->SendData(params, payload, result);
}

bool WebRtcSession::ConnectDataChannel(DataChannel* webrtc_data_channel) {
  if (!data_channel_) {
    LOG(LS_ERROR) << "ConnectDataChannel called when data_channel_ is NULL.";
    return false;
  }
  data_channel_->SignalReadyToSendData.connect(webrtc_data_channel,
                                               &DataChannel::OnChannelReady);
  data_channel_->SignalDataReceived.connect(webrtc_data_channel,
                                            &DataChannel::OnDataReceived);
  return true;
}

void WebRtcSession::DisconnectDataChannel(DataChannel* webrtc_data_channel) {
  if (!data_channel_) {
    LOG(LS_ERROR) << "DisconnectDataChannel called when data_channel_ is NULL.";
    return;
  }
  data_channel_->SignalReadyToSendData.disconnect(webrtc_data_channel);
  data_channel_->SignalDataReceived.disconnect(webrtc_data_channel);
}

void WebRtcSession::AddSctpDataStream(int sid) {
  if (!data_channel_) {
    LOG(LS_ERROR) << "AddDataChannelStreams called when data_channel_ is NULL.";
    return;
  }
  data_channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(sid));
  data_channel_->AddSendStream(cricket::StreamParams::CreateLegacy(sid));
}

void WebRtcSession::RemoveSctpDataStream(int sid) {
  mediastream_signaling_->RemoveSctpDataChannel(sid);

  if (!data_channel_) {
    LOG(LS_ERROR) << "RemoveDataChannelStreams called when data_channel_ is "
                  << "NULL.";
    return;
  }
  data_channel_->RemoveRecvStream(sid);
  data_channel_->RemoveSendStream(sid);
}

bool WebRtcSession::ReadyToSendData() const {
  return data_channel_ && data_channel_->ready_to_send_data();
}

rtc::scoped_refptr<DataChannel> WebRtcSession::CreateDataChannel(
    const std::string& label,
    const InternalDataChannelInit* config) {
  if (state() == STATE_RECEIVEDTERMINATE) {
    return NULL;
  }
  if (data_channel_type_ == cricket::DCT_NONE) {
    LOG(LS_ERROR) << "CreateDataChannel: Data is not supported in this call.";
    return NULL;
  }
  InternalDataChannelInit new_config =
      config ? (*config) : InternalDataChannelInit();
  if (data_channel_type_ == cricket::DCT_SCTP) {
    if (new_config.id < 0) {
      rtc::SSLRole role;
      if (GetSslRole(&role) &&
          !mediastream_signaling_->AllocateSctpSid(role, &new_config.id)) {
        LOG(LS_ERROR) << "No id can be allocated for the SCTP data channel.";
        return NULL;
      }
    } else if (!mediastream_signaling_->IsSctpSidAvailable(new_config.id)) {
      LOG(LS_ERROR) << "Failed to create a SCTP data channel "
                    << "because the id is already in use or out of range.";
      return NULL;
    }
  }

  rtc::scoped_refptr<DataChannel> channel(DataChannel::Create(
      this, data_channel_type_, label, new_config));
  if (channel && !mediastream_signaling_->AddDataChannel(channel))
    return NULL;

  return channel;
}

cricket::DataChannelType WebRtcSession::data_channel_type() const {
  return data_channel_type_;
}

bool WebRtcSession::IceRestartPending() const {
  return ice_restart_latch_->Get();
}

void WebRtcSession::ResetIceRestartLatch() {
  ice_restart_latch_->Reset();
}

void WebRtcSession::OnIdentityReady(rtc::SSLIdentity* identity) {
  SetIdentity(identity);
}

bool WebRtcSession::waiting_for_identity() const {
  return webrtc_session_desc_factory_->waiting_for_identity();
}

void WebRtcSession::SetIceConnectionState(
      PeerConnectionInterface::IceConnectionState state) {
  if (ice_connection_state_ == state) {
    return;
  }

  // ASSERT that the requested transition is allowed.  Note that
  // WebRtcSession does not implement "kIceConnectionClosed" (that is handled
  // within PeerConnection).  This switch statement should compile away when
  // ASSERTs are disabled.
  switch (ice_connection_state_) {
    case PeerConnectionInterface::kIceConnectionNew:
      ASSERT(state == PeerConnectionInterface::kIceConnectionChecking);
      break;
    case PeerConnectionInterface::kIceConnectionChecking:
      ASSERT(state == PeerConnectionInterface::kIceConnectionFailed ||
             state == PeerConnectionInterface::kIceConnectionConnected);
      break;
    case PeerConnectionInterface::kIceConnectionConnected:
      ASSERT(state == PeerConnectionInterface::kIceConnectionDisconnected ||
             state == PeerConnectionInterface::kIceConnectionChecking ||
             state == PeerConnectionInterface::kIceConnectionCompleted);
      break;
    case PeerConnectionInterface::kIceConnectionCompleted:
      ASSERT(state == PeerConnectionInterface::kIceConnectionConnected ||
             state == PeerConnectionInterface::kIceConnectionDisconnected);
      break;
    case PeerConnectionInterface::kIceConnectionFailed:
      ASSERT(state == PeerConnectionInterface::kIceConnectionNew);
      break;
    case PeerConnectionInterface::kIceConnectionDisconnected:
      ASSERT(state == PeerConnectionInterface::kIceConnectionChecking ||
             state == PeerConnectionInterface::kIceConnectionConnected ||
             state == PeerConnectionInterface::kIceConnectionCompleted ||
             state == PeerConnectionInterface::kIceConnectionFailed);
      break;
    case PeerConnectionInterface::kIceConnectionClosed:
      ASSERT(false);
      break;
    default:
      ASSERT(false);
      break;
  }

  ice_connection_state_ = state;
  if (ice_observer_) {
    ice_observer_->OnIceConnectionChange(ice_connection_state_);
  }
}

void WebRtcSession::OnTransportRequestSignaling(
    cricket::Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());
  transport->OnSignalingReady();
  if (ice_observer_) {
    ice_observer_->OnIceGatheringChange(
      PeerConnectionInterface::kIceGatheringGathering);
  }
}

void WebRtcSession::OnTransportConnecting(cricket::Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());
  // start monitoring for the write state of the transport.
  OnTransportWritable(transport);
}

void WebRtcSession::OnTransportWritable(cricket::Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());
  if (transport->all_channels_writable()) {
    SetIceConnectionState(PeerConnectionInterface::kIceConnectionConnected);
  } else if (transport->HasChannels()) {
    // If the current state is Connected or Completed, then there were writable
    // channels but now there are not, so the next state must be Disconnected.
    if (ice_connection_state_ ==
            PeerConnectionInterface::kIceConnectionConnected ||
        ice_connection_state_ ==
            PeerConnectionInterface::kIceConnectionCompleted) {
      SetIceConnectionState(
          PeerConnectionInterface::kIceConnectionDisconnected);
    }
  }
}

void WebRtcSession::OnTransportCompleted(cricket::Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());
  PeerConnectionInterface::IceConnectionState old_state = ice_connection_state_;
  SetIceConnectionState(PeerConnectionInterface::kIceConnectionCompleted);
  // Only report once when Ice connection is completed.
  if (old_state != PeerConnectionInterface::kIceConnectionCompleted) {
    cricket::TransportStats stats;
    if (metrics_observer_ && transport->GetStats(&stats)) {
      ReportBestConnectionState(stats);
      ReportNegotiatedCiphers(stats);
    }
  }
}

void WebRtcSession::OnTransportFailed(cricket::Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());
  SetIceConnectionState(PeerConnectionInterface::kIceConnectionFailed);
}

void WebRtcSession::OnTransportReceiving(cricket::Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());
  // The ice connection is considered receiving if at least one transport is
  // receiving on any channels.
  bool receiving = false;
  for (const auto& kv : transport_proxies()) {
    cricket::Transport* transport = kv.second->impl();
    if (transport && transport->any_channel_receiving()) {
      receiving = true;
      break;
    }
  }
  SetIceConnectionReceiving(receiving);
}

void WebRtcSession::SetIceConnectionReceiving(bool receiving) {
  if (ice_connection_receiving_ == receiving) {
    return;
  }
  ice_connection_receiving_ = receiving;
  if (ice_observer_) {
    ice_observer_->OnIceConnectionReceivingChange(receiving);
  }
}

void WebRtcSession::OnTransportProxyCandidatesReady(
    cricket::TransportProxy* proxy, const cricket::Candidates& candidates) {
  ASSERT(signaling_thread()->IsCurrent());
  ProcessNewLocalCandidate(proxy->content_name(), candidates);
}

void WebRtcSession::OnCandidatesAllocationDone() {
  ASSERT(signaling_thread()->IsCurrent());
  if (ice_observer_) {
    ice_observer_->OnIceGatheringChange(
      PeerConnectionInterface::kIceGatheringComplete);
    ice_observer_->OnIceComplete();
  }
}

// Enabling voice and video channel.
void WebRtcSession::EnableChannels() {
  if (voice_channel_ && !voice_channel_->enabled())
    voice_channel_->Enable(true);

  if (video_channel_ && !video_channel_->enabled())
    video_channel_->Enable(true);

  if (data_channel_ && !data_channel_->enabled())
    data_channel_->Enable(true);
}

void WebRtcSession::ProcessNewLocalCandidate(
    const std::string& content_name,
    const cricket::Candidates& candidates) {
  int sdp_mline_index;
  if (!GetLocalCandidateMediaIndex(content_name, &sdp_mline_index)) {
    LOG(LS_ERROR) << "ProcessNewLocalCandidate: content name "
                  << content_name << " not found";
    return;
  }

  for (cricket::Candidates::const_iterator citer = candidates.begin();
      citer != candidates.end(); ++citer) {
    // Use content_name as the candidate media id.
    JsepIceCandidate candidate(content_name, sdp_mline_index, *citer);
    if (ice_observer_) {
      ice_observer_->OnIceCandidate(&candidate);
    }
    if (local_desc_) {
      local_desc_->AddCandidate(&candidate);
    }
  }
}

// Returns the media index for a local ice candidate given the content name.
bool WebRtcSession::GetLocalCandidateMediaIndex(const std::string& content_name,
                                                int* sdp_mline_index) {
  if (!base_local_description() || !sdp_mline_index)
    return false;

  bool content_found = false;
  const ContentInfos& contents = base_local_description()->contents();
  for (size_t index = 0; index < contents.size(); ++index) {
    if (contents[index].name == content_name) {
      *sdp_mline_index = static_cast<int>(index);
      content_found = true;
      break;
    }
  }
  return content_found;
}

bool WebRtcSession::UseCandidatesInSessionDescription(
    const SessionDescriptionInterface* remote_desc) {
  if (!remote_desc)
    return true;
  bool ret = true;

  for (size_t m = 0; m < remote_desc->number_of_mediasections(); ++m) {
    const IceCandidateCollection* candidates = remote_desc->candidates(m);
    for  (size_t n = 0; n < candidates->count(); ++n) {
      const IceCandidateInterface* candidate = candidates->at(n);
      bool valid = false;
      if (!ReadyToUseRemoteCandidate(candidate, remote_desc, &valid)) {
        if (valid) {
          LOG(LS_INFO) << "UseCandidatesInSessionDescription: Candidate saved.";
          saved_candidates_.push_back(
              new JsepIceCandidate(candidate->sdp_mid(),
                                   candidate->sdp_mline_index(),
                                   candidate->candidate()));
        }
        continue;
      }

      ret = UseCandidate(candidate);
      if (!ret)
        break;
    }
  }
  return ret;
}

bool WebRtcSession::UseCandidate(
    const IceCandidateInterface* candidate) {

  size_t mediacontent_index = static_cast<size_t>(candidate->sdp_mline_index());
  size_t remote_content_size = base_remote_description()->contents().size();
  if (mediacontent_index >= remote_content_size) {
    LOG(LS_ERROR)
        << "UseRemoteCandidateInSession: Invalid candidate media index.";
    return false;
  }

  cricket::ContentInfo content =
      base_remote_description()->contents()[mediacontent_index];
  std::vector<cricket::Candidate> candidates;
  candidates.push_back(candidate->candidate());
  // Invoking BaseSession method to handle remote candidates.
  std::string error;
  if (OnRemoteCandidates(content.name, candidates, &error)) {
    // Candidates successfully submitted for checking.
    if (ice_connection_state_ == PeerConnectionInterface::kIceConnectionNew ||
        ice_connection_state_ ==
            PeerConnectionInterface::kIceConnectionDisconnected) {
      // If state is New, then the session has just gotten its first remote ICE
      // candidates, so go to Checking.
      // If state is Disconnected, the session is re-using old candidates or
      // receiving additional ones, so go to Checking.
      // If state is Connected, stay Connected.
      // TODO(bemasc): If state is Connected, and the new candidates are for a
      // newly added transport, then the state actually _should_ move to
      // checking.  Add a way to distinguish that case.
      SetIceConnectionState(PeerConnectionInterface::kIceConnectionChecking);
    }
    // TODO(bemasc): If state is Completed, go back to Connected.
  } else {
    if (!error.empty()) {
      LOG(LS_WARNING) << error;
    }
  }
  return true;
}

void WebRtcSession::RemoveUnusedChannelsAndTransports(
    const SessionDescription* desc) {
  // Destroy video_channel_ first since it may have a pointer to the
  // voice_channel_.
  const cricket::ContentInfo* video_info =
      cricket::GetFirstVideoContent(desc);
  if ((!video_info || video_info->rejected) && video_channel_) {
    mediastream_signaling_->OnVideoChannelClose();
    SignalVideoChannelDestroyed();
    const std::string content_name = video_channel_->content_name();
    channel_manager_->DestroyVideoChannel(video_channel_.release());
    DestroyTransportProxy(content_name);
  }

  const cricket::ContentInfo* voice_info =
      cricket::GetFirstAudioContent(desc);
  if ((!voice_info || voice_info->rejected) && voice_channel_) {
    mediastream_signaling_->OnAudioChannelClose();
    SignalVoiceChannelDestroyed();
    const std::string content_name = voice_channel_->content_name();
    channel_manager_->DestroyVoiceChannel(voice_channel_.release(),
                                          video_channel_.get());
    DestroyTransportProxy(content_name);
  }

  const cricket::ContentInfo* data_info =
      cricket::GetFirstDataContent(desc);
  if ((!data_info || data_info->rejected) && data_channel_) {
    mediastream_signaling_->OnDataChannelClose();
    SignalDataChannelDestroyed();
    const std::string content_name = data_channel_->content_name();
    channel_manager_->DestroyDataChannel(data_channel_.release());
    DestroyTransportProxy(content_name);
  }
}

// TODO(mallinath) - Add a correct error code if the channels are not creatued
// due to BUNDLE is enabled but rtcp-mux is disabled.
bool WebRtcSession::CreateChannels(const SessionDescription* desc) {
  // Creating the media channels and transport proxies.
  const cricket::ContentInfo* voice = cricket::GetFirstAudioContent(desc);
  if (voice && !voice->rejected && !voice_channel_) {
    if (!CreateVoiceChannel(voice)) {
      LOG(LS_ERROR) << "Failed to create voice channel.";
      return false;
    }
  }

  const cricket::ContentInfo* video = cricket::GetFirstVideoContent(desc);
  if (video && !video->rejected && !video_channel_) {
    if (!CreateVideoChannel(video)) {
      LOG(LS_ERROR) << "Failed to create video channel.";
      return false;
    }
  }

  const cricket::ContentInfo* data = cricket::GetFirstDataContent(desc);
  if (data_channel_type_ != cricket::DCT_NONE &&
      data && !data->rejected && !data_channel_) {
    if (!CreateDataChannel(data)) {
      LOG(LS_ERROR) << "Failed to create data channel.";
      return false;
    }
  }

  if (rtcp_mux_policy_ == PeerConnectionInterface::kRtcpMuxPolicyRequire) {
    if (voice_channel()) {
      voice_channel()->ActivateRtcpMux();
    }
    if (video_channel()) {
      video_channel()->ActivateRtcpMux();
    }
    if (data_channel()) {
      data_channel()->ActivateRtcpMux();
    }
  }

  // Enable bundle before when kMaxBundle policy is in effect.
  if (bundle_policy_ == PeerConnectionInterface::kBundlePolicyMaxBundle) {
    const cricket::ContentGroup* bundle_group = desc->GetGroupByName(
        cricket::GROUP_TYPE_BUNDLE);
    if (!bundle_group) {
      LOG(LS_WARNING) << "max-bundle specified without BUNDLE specified";
      return false;
    }
    if (!BaseSession::BundleContentGroup(bundle_group)) {
      LOG(LS_WARNING) << "max-bundle failed to enable bundling.";
      return false;
    }
  }

  return true;
}

bool WebRtcSession::CreateVoiceChannel(const cricket::ContentInfo* content) {
  voice_channel_.reset(channel_manager_->CreateVoiceChannel(
      this, content->name, true, audio_options_));
  if (!voice_channel_) {
    return false;
  }

  voice_channel_->SignalDtlsSetupFailure.connect(
      this, &WebRtcSession::OnDtlsSetupFailure);
  return true;
}

bool WebRtcSession::CreateVideoChannel(const cricket::ContentInfo* content) {
  video_channel_.reset(channel_manager_->CreateVideoChannel(
      this, content->name, true, video_options_, voice_channel_.get()));
  if (!video_channel_) {
    return false;
  }

  video_channel_->SignalDtlsSetupFailure.connect(
      this, &WebRtcSession::OnDtlsSetupFailure);
  return true;
}

bool WebRtcSession::CreateDataChannel(const cricket::ContentInfo* content) {
  bool sctp = (data_channel_type_ == cricket::DCT_SCTP);
  data_channel_.reset(channel_manager_->CreateDataChannel(
      this, content->name, !sctp, data_channel_type_));
  if (!data_channel_) {
    return false;
  }

  if (sctp) {
    mediastream_signaling_->OnDataTransportCreatedForSctp();
    data_channel_->SignalDataReceived.connect(
        this, &WebRtcSession::OnDataChannelMessageReceived);
    data_channel_->SignalStreamClosedRemotely.connect(
        mediastream_signaling_,
        &MediaStreamSignaling::OnRemoteSctpDataChannelClosed);
  }

  data_channel_->SignalDtlsSetupFailure.connect(
      this, &WebRtcSession::OnDtlsSetupFailure);
  return true;
}

void WebRtcSession::OnDtlsSetupFailure(cricket::BaseChannel*, bool rtcp) {
  SetError(BaseSession::ERROR_TRANSPORT, rtcp ? kDtlsSetupFailureRtcp :
           kDtlsSetupFailureRtp);
}

void WebRtcSession::CopySavedCandidates(
    SessionDescriptionInterface* dest_desc) {
  if (!dest_desc) {
    ASSERT(false);
    return;
  }
  for (size_t i = 0; i < saved_candidates_.size(); ++i) {
    dest_desc->AddCandidate(saved_candidates_[i]);
    delete saved_candidates_[i];
  }
  saved_candidates_.clear();
}

void WebRtcSession::OnDataChannelMessageReceived(
    cricket::DataChannel* channel,
    const cricket::ReceiveDataParams& params,
    const rtc::Buffer& payload) {
  ASSERT(data_channel_type_ == cricket::DCT_SCTP);
  if (params.type == cricket::DMT_CONTROL &&
      mediastream_signaling_->IsSctpSidAvailable(params.ssrc)) {
    // Received CONTROL on unused sid, process as an OPEN message.
    mediastream_signaling_->AddDataChannelFromOpenMessage(params, payload);
  }
  // otherwise ignore the message.
}

// Returns false if bundle is enabled and rtcp_mux is disabled.
bool WebRtcSession::ValidateBundleSettings(const SessionDescription* desc) {
  bool bundle_enabled = desc->HasGroup(cricket::GROUP_TYPE_BUNDLE);
  if (!bundle_enabled)
    return true;

  const cricket::ContentGroup* bundle_group =
      desc->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
  ASSERT(bundle_group != NULL);

  const cricket::ContentInfos& contents = desc->contents();
  for (cricket::ContentInfos::const_iterator citer = contents.begin();
       citer != contents.end(); ++citer) {
    const cricket::ContentInfo* content = (&*citer);
    ASSERT(content != NULL);
    if (bundle_group->HasContentName(content->name) &&
        !content->rejected && content->type == cricket::NS_JINGLE_RTP) {
      if (!HasRtcpMuxEnabled(content))
        return false;
    }
  }
  // RTCP-MUX is enabled in all the contents.
  return true;
}

bool WebRtcSession::HasRtcpMuxEnabled(
    const cricket::ContentInfo* content) {
  const cricket::MediaContentDescription* description =
      static_cast<cricket::MediaContentDescription*>(content->description);
  return description->rtcp_mux();
}

bool WebRtcSession::ValidateSessionDescription(
    const SessionDescriptionInterface* sdesc,
    cricket::ContentSource source, std::string* err_desc) {
  std::string type;
  if (error() != cricket::BaseSession::ERROR_NONE) {
    return BadSdp(source, type, GetSessionErrorMsg(), err_desc);
  }

  if (!sdesc || !sdesc->description()) {
    return BadSdp(source, type, kInvalidSdp, err_desc);
  }

  type = sdesc->type();
  Action action = GetAction(sdesc->type());
  if (source == cricket::CS_LOCAL) {
    if (!ExpectSetLocalDescription(action))
      return BadLocalSdp(type, BadStateErrMsg(state()), err_desc);
  } else {
    if (!ExpectSetRemoteDescription(action))
      return BadRemoteSdp(type, BadStateErrMsg(state()), err_desc);
  }

  // Verify crypto settings.
  std::string crypto_error;
  if ((webrtc_session_desc_factory_->SdesPolicy() == cricket::SEC_REQUIRED ||
       dtls_enabled_) &&
      !VerifyCrypto(sdesc->description(), dtls_enabled_, &crypto_error)) {
    return BadSdp(source, type, crypto_error, err_desc);
  }

  // Verify ice-ufrag and ice-pwd.
  if (!VerifyIceUfragPwdPresent(sdesc->description())) {
    return BadSdp(source, type, kSdpWithoutIceUfragPwd, err_desc);
  }

  if (!ValidateBundleSettings(sdesc->description())) {
    return BadSdp(source, type, kBundleWithoutRtcpMux, err_desc);
  }

  // Verify m-lines in Answer when compared against Offer.
  if (action == kAnswer) {
    const cricket::SessionDescription* offer_desc =
        (source == cricket::CS_LOCAL) ? remote_description()->description() :
            local_description()->description();
    if (!VerifyMediaDescriptions(sdesc->description(), offer_desc)) {
      return BadAnswerSdp(source, kMlineMismatch, err_desc);
    }
  }

  return true;
}

bool WebRtcSession::ExpectSetLocalDescription(Action action) {
  return ((action == kOffer && state() == STATE_INIT) ||
          // update local offer
          (action == kOffer && state() == STATE_SENTINITIATE) ||
          // update the current ongoing session.
          (action == kOffer && state() == STATE_RECEIVEDACCEPT) ||
          (action == kOffer && state() == STATE_SENTACCEPT) ||
          (action == kOffer && state() == STATE_INPROGRESS) ||
          // accept remote offer
          (action == kAnswer && state() == STATE_RECEIVEDINITIATE) ||
          (action == kAnswer && state() == STATE_SENTPRACCEPT) ||
          (action == kPrAnswer && state() == STATE_RECEIVEDINITIATE) ||
          (action == kPrAnswer && state() == STATE_SENTPRACCEPT));
}

bool WebRtcSession::ExpectSetRemoteDescription(Action action) {
  return ((action == kOffer && state() == STATE_INIT) ||
          // update remote offer
          (action == kOffer && state() == STATE_RECEIVEDINITIATE) ||
          // update the current ongoing session
          (action == kOffer && state() == STATE_RECEIVEDACCEPT) ||
          (action == kOffer && state() == STATE_SENTACCEPT) ||
          (action == kOffer && state() == STATE_INPROGRESS) ||
          // accept local offer
          (action == kAnswer && state() == STATE_SENTINITIATE) ||
          (action == kAnswer && state() == STATE_RECEIVEDPRACCEPT) ||
          (action == kPrAnswer && state() == STATE_SENTINITIATE) ||
          (action == kPrAnswer && state() == STATE_RECEIVEDPRACCEPT));
}

std::string WebRtcSession::GetSessionErrorMsg() {
  std::ostringstream desc;
  desc << kSessionError << GetErrorCodeString(error()) << ". ";
  desc << kSessionErrorDesc << error_desc() << ".";
  return desc.str();
}

// We need to check the local/remote description for the Transport instead of
// the session, because a new Transport added during renegotiation may have
// them unset while the session has them set from the previous negotiation.
// Not doing so may trigger the auto generation of transport description and
// mess up DTLS identity information, ICE credential, etc.
bool WebRtcSession::ReadyToUseRemoteCandidate(
    const IceCandidateInterface* candidate,
    const SessionDescriptionInterface* remote_desc,
    bool* valid) {
  *valid = true;;
  cricket::TransportProxy* transport_proxy = NULL;

  const SessionDescriptionInterface* current_remote_desc =
      remote_desc ? remote_desc : remote_description();

  if (!current_remote_desc)
    return false;

  size_t mediacontent_index =
      static_cast<size_t>(candidate->sdp_mline_index());
  size_t remote_content_size =
      current_remote_desc->description()->contents().size();
  if (mediacontent_index >= remote_content_size) {
    LOG(LS_ERROR)
        << "ReadyToUseRemoteCandidate: Invalid candidate media index.";

    *valid = false;
    return false;
  }

  cricket::ContentInfo content =
      current_remote_desc->description()->contents()[mediacontent_index];
  transport_proxy = GetTransportProxy(content.name);

  return transport_proxy && transport_proxy->local_description_set() &&
      transport_proxy->remote_description_set();
}

// Walk through the ConnectionInfos to gather best connection usage
// for IPv4 and IPv6.
void WebRtcSession::ReportBestConnectionState(
    const cricket::TransportStats& stats) {
  DCHECK(metrics_observer_ != NULL);
  for (cricket::TransportChannelStatsList::const_iterator it =
         stats.channel_stats.begin();
       it != stats.channel_stats.end(); ++it) {
    for (cricket::ConnectionInfos::const_iterator it_info =
           it->connection_infos.begin();
         it_info != it->connection_infos.end(); ++it_info) {
      if (!it_info->best_connection) {
        continue;
      }
      if (it_info->local_candidate.address().family() == AF_INET) {
        metrics_observer_->IncrementCounter(kBestConnections_IPv4);
      } else if (it_info->local_candidate.address().family() ==
                 AF_INET6) {
        metrics_observer_->IncrementCounter(kBestConnections_IPv6);
      } else {
        RTC_NOTREACHED();
      }
      return;
    }
  }
}

void WebRtcSession::ReportNegotiatedCiphers(
    const cricket::TransportStats& stats) {
  DCHECK(metrics_observer_ != NULL);
  if (!dtls_enabled_ || stats.channel_stats.empty()) {
    return;
  }

  const std::string& srtp_cipher = stats.channel_stats[0].srtp_cipher;
  const std::string& ssl_cipher = stats.channel_stats[0].ssl_cipher;
  if (srtp_cipher.empty() && ssl_cipher.empty()) {
    return;
  }

  PeerConnectionMetricsName srtp_name;
  PeerConnectionMetricsName ssl_name;
  if (stats.content_name == cricket::CN_AUDIO) {
    srtp_name = kAudioSrtpCipher;
    ssl_name = kAudioSslCipher;
  } else if (stats.content_name == cricket::CN_VIDEO) {
    srtp_name = kVideoSrtpCipher;
    ssl_name = kVideoSslCipher;
  } else if (stats.content_name == cricket::CN_DATA) {
    srtp_name = kDataSrtpCipher;
    ssl_name = kDataSslCipher;
  } else {
    RTC_NOTREACHED();
    return;
  }

  if (!srtp_cipher.empty()) {
    metrics_observer_->AddHistogramSample(srtp_name, srtp_cipher);
  }
  if (!ssl_cipher.empty()) {
    metrics_observer_->AddHistogramSample(ssl_name, ssl_cipher);
  }
}

}  // namespace webrtc
