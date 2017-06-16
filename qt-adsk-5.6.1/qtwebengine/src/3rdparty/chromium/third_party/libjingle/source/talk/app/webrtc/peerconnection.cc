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

#include "talk/app/webrtc/peerconnection.h"

#include <vector>

#include "talk/app/webrtc/dtmfsender.h"
#include "talk/app/webrtc/jsepicecandidate.h"
#include "talk/app/webrtc/jsepsessiondescription.h"
#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/app/webrtc/mediastreamhandler.h"
#include "talk/app/webrtc/streamcollection.h"
#include "webrtc/p2p/client/basicportallocator.h"
#include "talk/session/media/channelmanager.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/system_wrappers/interface/field_trial.h"

namespace {

using webrtc::PeerConnectionInterface;

// The min number of tokens must present in Turn host uri.
// e.g. user@turn.example.org
static const size_t kTurnHostTokensNum = 2;
// Number of tokens must be preset when TURN uri has transport param.
static const size_t kTurnTransportTokensNum = 2;
// The default stun port.
static const int kDefaultStunPort = 3478;
static const int kDefaultStunTlsPort = 5349;
static const char kTransport[] = "transport";
static const char kUdpTransportType[] = "udp";
static const char kTcpTransportType[] = "tcp";

// NOTE: Must be in the same order as the ServiceType enum.
static const char* kValidIceServiceTypes[] = {
    "stun", "stuns", "turn", "turns", "invalid" };

enum ServiceType {
  STUN,     // Indicates a STUN server.
  STUNS,    // Indicates a STUN server used with a TLS session.
  TURN,     // Indicates a TURN server
  TURNS,    // Indicates a TURN server used with a TLS session.
  INVALID,  // Unknown.
};

enum {
  MSG_SET_SESSIONDESCRIPTION_SUCCESS = 0,
  MSG_SET_SESSIONDESCRIPTION_FAILED,
  MSG_GETSTATS,
};

struct SetSessionDescriptionMsg : public rtc::MessageData {
  explicit SetSessionDescriptionMsg(
      webrtc::SetSessionDescriptionObserver* observer)
      : observer(observer) {
  }

  rtc::scoped_refptr<webrtc::SetSessionDescriptionObserver> observer;
  std::string error;
};

struct GetStatsMsg : public rtc::MessageData {
  GetStatsMsg(webrtc::StatsObserver* observer,
              webrtc::MediaStreamTrackInterface* track)
      : observer(observer), track(track) {
  }
  rtc::scoped_refptr<webrtc::StatsObserver> observer;
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track;
};

// |in_str| should be of format
// stunURI       = scheme ":" stun-host [ ":" stun-port ]
// scheme        = "stun" / "stuns"
// stun-host     = IP-literal / IPv4address / reg-name
// stun-port     = *DIGIT

// draft-petithuguenin-behave-turn-uris-01
// turnURI       = scheme ":" turn-host [ ":" turn-port ]
// turn-host     = username@IP-literal / IPv4address / reg-name
bool GetServiceTypeAndHostnameFromUri(const std::string& in_str,
                                      ServiceType* service_type,
                                      std::string* hostname) {
  const std::string::size_type colonpos = in_str.find(':');
  if (colonpos == std::string::npos || (colonpos + 1) == in_str.length()) {
    return false;
  }
  std::string type = in_str.substr(0, colonpos);
  for (size_t i = 0; i < ARRAY_SIZE(kValidIceServiceTypes); ++i) {
    if (type.compare(kValidIceServiceTypes[i]) == 0) {
      *service_type = static_cast<ServiceType>(i);
      break;
    }
  }
  if (*service_type == INVALID) {
    return false;
  }
  *hostname = in_str.substr(colonpos + 1, std::string::npos);
  return true;
}

// This method parses IPv6 and IPv4 literal strings, along with hostnames in
// standard hostname:port format.
// Consider following formats as correct.
// |hostname:port|, |[IPV6 address]:port|, |IPv4 address|:port,
// |hostname|, |[IPv6 address]|, |IPv4 address|
bool ParseHostnameAndPortFromString(const std::string& in_str,
                                    std::string* host,
                                    int* port) {
  if (in_str.at(0) == '[') {
    std::string::size_type closebracket = in_str.rfind(']');
    if (closebracket != std::string::npos) {
      *host = in_str.substr(1, closebracket - 1);
      std::string::size_type colonpos = in_str.find(':', closebracket);
      if (std::string::npos != colonpos) {
        if (!rtc::FromString(
            in_str.substr(closebracket + 2, std::string::npos), port)) {
          return false;
        }
      }
    } else {
      return false;
    }
  } else {
    std::string::size_type colonpos = in_str.find(':');
    if (std::string::npos != colonpos) {
      *host = in_str.substr(0, colonpos);
      if (!rtc::FromString(
          in_str.substr(colonpos + 1, std::string::npos), port)) {
        return false;
      }
    } else {
      *host = in_str;
    }
  }
  return true;
}

typedef webrtc::PortAllocatorFactoryInterface::StunConfiguration
    StunConfiguration;
typedef webrtc::PortAllocatorFactoryInterface::TurnConfiguration
    TurnConfiguration;

bool ParseIceServerUrl(const PeerConnectionInterface::IceServer& server,
                       const std::string& url,
                       std::vector<StunConfiguration>* stun_config,
                       std::vector<TurnConfiguration>* turn_config) {
  // draft-nandakumar-rtcweb-stun-uri-01
  // stunURI       = scheme ":" stun-host [ ":" stun-port ]
  // scheme        = "stun" / "stuns"
  // stun-host     = IP-literal / IPv4address / reg-name
  // stun-port     = *DIGIT

  // draft-petithuguenin-behave-turn-uris-01
  // turnURI       = scheme ":" turn-host [ ":" turn-port ]
  //                 [ "?transport=" transport ]
  // scheme        = "turn" / "turns"
  // transport     = "udp" / "tcp" / transport-ext
  // transport-ext = 1*unreserved
  // turn-host     = IP-literal / IPv4address / reg-name
  // turn-port     = *DIGIT
  std::vector<std::string> tokens;
  std::string turn_transport_type = kUdpTransportType;
  ASSERT(!url.empty());
  rtc::tokenize(url, '?', &tokens);
  std::string uri_without_transport = tokens[0];
  // Let's look into transport= param, if it exists.
  if (tokens.size() == kTurnTransportTokensNum) {  // ?transport= is present.
    std::string uri_transport_param = tokens[1];
    rtc::tokenize(uri_transport_param, '=', &tokens);
    if (tokens[0] == kTransport) {
      // As per above grammar transport param will be consist of lower case
      // letters.
      if (tokens[1] != kUdpTransportType && tokens[1] != kTcpTransportType) {
        LOG(LS_WARNING) << "Transport param should always be udp or tcp.";
        return true;
      }
      turn_transport_type = tokens[1];
    }
  }

  std::string hoststring;
  ServiceType service_type = INVALID;
  if (!GetServiceTypeAndHostnameFromUri(uri_without_transport,
                                       &service_type,
                                       &hoststring)) {
    LOG(LS_WARNING) << "Invalid transport parameter in ICE URI: "
                    << uri_without_transport;
    return true;
  }

  ASSERT(!hoststring.empty());

  // Let's break hostname.
  tokens.clear();
  rtc::tokenize(hoststring, '@', &tokens);
  ASSERT(!tokens.empty());
  std::string username(server.username);
  // TODO(pthatcher): What's the right thing to do if tokens.size() is >2?
  // E.g. a string like "foo@bar@bat".
  if (tokens.size() >= kTurnHostTokensNum) {
    username.assign(rtc::s_url_decode(tokens[0]));
    hoststring = tokens[1];
  } else {
    hoststring = tokens[0];
  }

  int port = kDefaultStunPort;
  if (service_type == TURNS) {
    port = kDefaultStunTlsPort;
    turn_transport_type = kTcpTransportType;
  }

  std::string address;
  if (!ParseHostnameAndPortFromString(hoststring, &address, &port)) {
    LOG(WARNING) << "Invalid Hostname format: " << uri_without_transport;
    return true;
  }

  if (port <= 0 || port > 0xffff) {
    LOG(WARNING) << "Invalid port: " << port;
    return true;
  }

  switch (service_type) {
    case STUN:
    case STUNS:
      stun_config->push_back(StunConfiguration(address, port));
      break;
    case TURN:
    case TURNS: {
      if (username.empty()) {
        // Turn url example from the spec |url:"turn:user@turn.example.org"|.
        std::vector<std::string> turn_tokens;
        rtc::tokenize(address, '@', &turn_tokens);
        if (turn_tokens.size() == kTurnHostTokensNum) {
          username.assign(rtc::s_url_decode(turn_tokens[0]));
          address = turn_tokens[1];
        }
      }

      bool secure = (service_type == TURNS);

      turn_config->push_back(TurnConfiguration(address, port,
                                               username,
                                               server.password,
                                               turn_transport_type,
                                               secure));
      break;
    }
    case INVALID:
    default:
      LOG(WARNING) << "Configuration not supported: " << url;
      return false;
  }
  return true;
}

bool ParseIceServers(const PeerConnectionInterface::IceServers& servers,
                     std::vector<StunConfiguration>* stun_config,
                     std::vector<TurnConfiguration>* turn_config) {
  for (const webrtc::PeerConnectionInterface::IceServer& server : servers) {
    if (!server.urls.empty()) {
      for (const std::string& url : server.urls) {
        if (url.empty()) {
          LOG(WARNING) << "Empty uri.";
          continue;
        }
        if (!ParseIceServerUrl(server, url, stun_config, turn_config)) {
          return false;
        }
      }
    } else if (!server.uri.empty()) {
      // Fallback to old .uri if new .urls isn't present.
      if (!ParseIceServerUrl(server, server.uri, stun_config, turn_config)) {
        return false;
      }
    } else {
      LOG(WARNING) << "Empty uri.";
    }
  }
  return true;
}

// Check if we can send |new_stream| on a PeerConnection.
// Currently only one audio but multiple video track is supported per
// PeerConnection.
bool CanAddLocalMediaStream(webrtc::StreamCollectionInterface* current_streams,
                            webrtc::MediaStreamInterface* new_stream) {
  if (!new_stream || !current_streams)
    return false;
  if (current_streams->find(new_stream->label()) != NULL) {
    LOG(LS_ERROR) << "MediaStream with label " << new_stream->label()
                  << " is already added.";
    return false;
  }

  return true;
}

}  // namespace

namespace webrtc {

PeerConnection::PeerConnection(PeerConnectionFactory* factory)
    : factory_(factory),
      observer_(NULL),
      uma_observer_(NULL),
      signaling_state_(kStable),
      ice_state_(kIceNew),
      ice_connection_state_(kIceConnectionNew),
      ice_gathering_state_(kIceGatheringNew) {
}

PeerConnection::~PeerConnection() {
  ASSERT(signaling_thread()->IsCurrent());
  if (mediastream_signaling_)
    mediastream_signaling_->TearDown();
  if (stream_handler_container_)
    stream_handler_container_->TearDown();
}

bool PeerConnection::Initialize(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    const MediaConstraintsInterface* constraints,
    PortAllocatorFactoryInterface* allocator_factory,
    DTLSIdentityServiceInterface* dtls_identity_service,
    PeerConnectionObserver* observer) {
  ASSERT(observer != NULL);
  if (!observer)
    return false;
  observer_ = observer;

  std::vector<PortAllocatorFactoryInterface::StunConfiguration> stun_config;
  std::vector<PortAllocatorFactoryInterface::TurnConfiguration> turn_config;
  if (!ParseIceServers(configuration.servers, &stun_config, &turn_config)) {
    return false;
  }
  port_allocator_.reset(
      allocator_factory->CreatePortAllocator(stun_config, turn_config));

  // To handle both internal and externally created port allocator, we will
  // enable BUNDLE here.
  int portallocator_flags = port_allocator_->flags();
  portallocator_flags |= cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
                         cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                         cricket::PORTALLOCATOR_ENABLE_IPV6;
  bool value;
  // If IPv6 flag was specified, we'll not override it by experiment.
  if (FindConstraint(
          constraints, MediaConstraintsInterface::kEnableIPv6, &value, NULL)) {
    if (!value) {
      portallocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6);
    }
  } else if (webrtc::field_trial::FindFullName("WebRTC-IPv6Default") ==
             "Disabled") {
    portallocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6);
  }

  if (configuration.tcp_candidate_policy == kTcpCandidatePolicyDisabled) {
    portallocator_flags |= cricket::PORTALLOCATOR_DISABLE_TCP;
    LOG(LS_INFO) << "TCP candidates are disabled.";
  }

  port_allocator_->set_flags(portallocator_flags);
  // No step delay is used while allocating ports.
  port_allocator_->set_step_delay(cricket::kMinimumStepDelay);

  mediastream_signaling_.reset(new MediaStreamSignaling(
      factory_->signaling_thread(), this, factory_->channel_manager()));

  session_.reset(new WebRtcSession(factory_->channel_manager(),
                                   factory_->signaling_thread(),
                                   factory_->worker_thread(),
                                   port_allocator_.get(),
                                   mediastream_signaling_.get()));
  stream_handler_container_.reset(new MediaStreamHandlerContainer(
      session_.get(), session_.get()));
  stats_.reset(new StatsCollector(session_.get()));

  // Initialize the WebRtcSession. It creates transport channels etc.
  if (!session_->Initialize(factory_->options(), constraints,
                            dtls_identity_service, configuration))
    return false;

  // Register PeerConnection as receiver of local ice candidates.
  // All the callbacks will be posted to the application from PeerConnection.
  session_->RegisterIceObserver(this);
  session_->SignalState.connect(this, &PeerConnection::OnSessionStateChange);
  return true;
}

rtc::scoped_refptr<StreamCollectionInterface>
PeerConnection::local_streams() {
  return mediastream_signaling_->local_streams();
}

rtc::scoped_refptr<StreamCollectionInterface>
PeerConnection::remote_streams() {
  return mediastream_signaling_->remote_streams();
}

bool PeerConnection::AddStream(MediaStreamInterface* local_stream) {
  if (IsClosed()) {
    return false;
  }
  if (!CanAddLocalMediaStream(mediastream_signaling_->local_streams(),
                              local_stream))
    return false;

  if (!mediastream_signaling_->AddLocalStream(local_stream)) {
    return false;
  }
  stats_->AddStream(local_stream);
  observer_->OnRenegotiationNeeded();
  return true;
}

void PeerConnection::RemoveStream(MediaStreamInterface* local_stream) {
  mediastream_signaling_->RemoveLocalStream(local_stream);
  if (IsClosed()) {
    return;
  }
  observer_->OnRenegotiationNeeded();
}

rtc::scoped_refptr<DtmfSenderInterface> PeerConnection::CreateDtmfSender(
    AudioTrackInterface* track) {
  if (!track) {
    LOG(LS_ERROR) << "CreateDtmfSender - track is NULL.";
    return NULL;
  }
  if (!mediastream_signaling_->local_streams()->FindAudioTrack(track->id())) {
    LOG(LS_ERROR) << "CreateDtmfSender is called with a non local audio track.";
    return NULL;
  }

  rtc::scoped_refptr<DtmfSenderInterface> sender(
      DtmfSender::Create(track, signaling_thread(), session_.get()));
  if (!sender.get()) {
    LOG(LS_ERROR) << "CreateDtmfSender failed on DtmfSender::Create.";
    return NULL;
  }
  return DtmfSenderProxy::Create(signaling_thread(), sender.get());
}

bool PeerConnection::GetStats(StatsObserver* observer,
                              MediaStreamTrackInterface* track,
                              StatsOutputLevel level) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!VERIFY(observer != NULL)) {
    LOG(LS_ERROR) << "GetStats - observer is NULL.";
    return false;
  }

  stats_->UpdateStats(level);
  signaling_thread()->Post(this, MSG_GETSTATS,
                           new GetStatsMsg(observer, track));
  return true;
}

PeerConnectionInterface::SignalingState PeerConnection::signaling_state() {
  return signaling_state_;
}

PeerConnectionInterface::IceState PeerConnection::ice_state() {
  return ice_state_;
}

PeerConnectionInterface::IceConnectionState
PeerConnection::ice_connection_state() {
  return ice_connection_state_;
}

PeerConnectionInterface::IceGatheringState
PeerConnection::ice_gathering_state() {
  return ice_gathering_state_;
}

rtc::scoped_refptr<DataChannelInterface>
PeerConnection::CreateDataChannel(
    const std::string& label,
    const DataChannelInit* config) {
  bool first_datachannel = !mediastream_signaling_->HasDataChannels();

  rtc::scoped_ptr<InternalDataChannelInit> internal_config;
  if (config) {
    internal_config.reset(new InternalDataChannelInit(*config));
  }
  rtc::scoped_refptr<DataChannelInterface> channel(
      session_->CreateDataChannel(label, internal_config.get()));
  if (!channel.get())
    return NULL;

  // Trigger the onRenegotiationNeeded event for every new RTP DataChannel, or
  // the first SCTP DataChannel.
  if (session_->data_channel_type() == cricket::DCT_RTP || first_datachannel) {
    observer_->OnRenegotiationNeeded();
  }

  return DataChannelProxy::Create(signaling_thread(), channel.get());
}

void PeerConnection::CreateOffer(CreateSessionDescriptionObserver* observer,
                                 const MediaConstraintsInterface* constraints) {
  if (!VERIFY(observer != NULL)) {
    LOG(LS_ERROR) << "CreateOffer - observer is NULL.";
    return;
  }
  RTCOfferAnswerOptions options;

  bool value;
  size_t mandatory_constraints = 0;

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kOfferToReceiveAudio,
                     &value,
                     &mandatory_constraints)) {
    options.offer_to_receive_audio =
        value ? RTCOfferAnswerOptions::kOfferToReceiveMediaTrue : 0;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kOfferToReceiveVideo,
                     &value,
                     &mandatory_constraints)) {
    options.offer_to_receive_video =
        value ? RTCOfferAnswerOptions::kOfferToReceiveMediaTrue : 0;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kVoiceActivityDetection,
                     &value,
                     &mandatory_constraints)) {
    options.voice_activity_detection = value;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kIceRestart,
                     &value,
                     &mandatory_constraints)) {
    options.ice_restart = value;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kUseRtpMux,
                     &value,
                     &mandatory_constraints)) {
    options.use_rtp_mux = value;
  }

  CreateOffer(observer, options);
}

void PeerConnection::CreateOffer(CreateSessionDescriptionObserver* observer,
                                 const RTCOfferAnswerOptions& options) {
  if (!VERIFY(observer != NULL)) {
    LOG(LS_ERROR) << "CreateOffer - observer is NULL.";
    return;
  }
  session_->CreateOffer(observer, options);
}

void PeerConnection::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const MediaConstraintsInterface* constraints) {
  if (!VERIFY(observer != NULL)) {
    LOG(LS_ERROR) << "CreateAnswer - observer is NULL.";
    return;
  }
  session_->CreateAnswer(observer, constraints);
}

void PeerConnection::SetLocalDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  if (!VERIFY(observer != NULL)) {
    LOG(LS_ERROR) << "SetLocalDescription - observer is NULL.";
    return;
  }
  if (!desc) {
    PostSetSessionDescriptionFailure(observer, "SessionDescription is NULL.");
    return;
  }
  // Update stats here so that we have the most recent stats for tracks and
  // streams that might be removed by updating the session description.
  stats_->UpdateStats(kStatsOutputLevelStandard);
  std::string error;
  if (!session_->SetLocalDescription(desc, &error)) {
    PostSetSessionDescriptionFailure(observer, error);
    return;
  }
  SetSessionDescriptionMsg* msg =  new SetSessionDescriptionMsg(observer);
  signaling_thread()->Post(this, MSG_SET_SESSIONDESCRIPTION_SUCCESS, msg);
}

void PeerConnection::SetRemoteDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  if (!VERIFY(observer != NULL)) {
    LOG(LS_ERROR) << "SetRemoteDescription - observer is NULL.";
    return;
  }
  if (!desc) {
    PostSetSessionDescriptionFailure(observer, "SessionDescription is NULL.");
    return;
  }
  // Update stats here so that we have the most recent stats for tracks and
  // streams that might be removed by updating the session description.
  stats_->UpdateStats(kStatsOutputLevelStandard);
  std::string error;
  if (!session_->SetRemoteDescription(desc, &error)) {
    PostSetSessionDescriptionFailure(observer, error);
    return;
  }
  SetSessionDescriptionMsg* msg  = new SetSessionDescriptionMsg(observer);
  signaling_thread()->Post(this, MSG_SET_SESSIONDESCRIPTION_SUCCESS, msg);
}

void PeerConnection::PostSetSessionDescriptionFailure(
    SetSessionDescriptionObserver* observer,
    const std::string& error) {
  SetSessionDescriptionMsg* msg  = new SetSessionDescriptionMsg(observer);
  msg->error = error;
  signaling_thread()->Post(this, MSG_SET_SESSIONDESCRIPTION_FAILED, msg);
}

bool PeerConnection::UpdateIce(const IceServers& configuration,
                               const MediaConstraintsInterface* constraints) {
  return false;
}

bool PeerConnection::UpdateIce(const RTCConfiguration& config) {
  if (port_allocator_) {
    std::vector<PortAllocatorFactoryInterface::StunConfiguration> stuns;
    std::vector<PortAllocatorFactoryInterface::TurnConfiguration> turns;
    if (!ParseIceServers(config.servers, &stuns, &turns)) {
      return false;
    }

    std::vector<rtc::SocketAddress> stun_hosts;
    typedef std::vector<StunConfiguration>::const_iterator StunIt;
    for (StunIt stun_it = stuns.begin(); stun_it != stuns.end(); ++stun_it) {
      stun_hosts.push_back(stun_it->server);
    }

    rtc::SocketAddress stun_addr;
    if (!stun_hosts.empty()) {
      stun_addr = stun_hosts.front();
      LOG(LS_INFO) << "UpdateIce: StunServer Address: " << stun_addr.ToString();
    }

    for (size_t i = 0; i < turns.size(); ++i) {
      cricket::RelayCredentials credentials(turns[i].username,
                                            turns[i].password);
      cricket::RelayServerConfig relay_server(cricket::RELAY_TURN);
      cricket::ProtocolType protocol;
      if (cricket::StringToProto(turns[i].transport_type.c_str(), &protocol)) {
        relay_server.ports.push_back(cricket::ProtocolAddress(
            turns[i].server, protocol, turns[i].secure));
        relay_server.credentials = credentials;
        LOG(LS_INFO) << "UpdateIce: TurnServer Address: "
                     << turns[i].server.ToString();
      } else {
        LOG(LS_WARNING) << "Ignoring TURN server " << turns[i].server << ". "
                        << "Reason= Incorrect " << turns[i].transport_type
                        << " transport parameter.";
      }
    }
  }
  return session_->SetIceTransports(config.type);
}

bool PeerConnection::AddIceCandidate(
    const IceCandidateInterface* ice_candidate) {
  return session_->ProcessIceMessage(ice_candidate);
}

void PeerConnection::RegisterUMAObserver(UMAObserver* observer) {
  uma_observer_ = observer;

  if (session_) {
    session_->set_metrics_observer(uma_observer_);
  }

  // Send information about IPv4/IPv6 status.
  if (uma_observer_ && port_allocator_) {
    if (port_allocator_->flags() & cricket::PORTALLOCATOR_ENABLE_IPV6) {
      uma_observer_->IncrementCounter(kPeerConnection_IPv6);
    } else {
      uma_observer_->IncrementCounter(kPeerConnection_IPv4);
    }
  }
}

const SessionDescriptionInterface* PeerConnection::local_description() const {
  return session_->local_description();
}

const SessionDescriptionInterface* PeerConnection::remote_description() const {
  return session_->remote_description();
}

void PeerConnection::Close() {
  // Update stats here so that we have the most recent stats for tracks and
  // streams before the channels are closed.
  stats_->UpdateStats(kStatsOutputLevelStandard);

  session_->Terminate();
}

void PeerConnection::OnSessionStateChange(cricket::BaseSession* /*session*/,
                                          cricket::BaseSession::State state) {
  switch (state) {
    case cricket::BaseSession::STATE_INIT:
      ChangeSignalingState(PeerConnectionInterface::kStable);
      break;
    case cricket::BaseSession::STATE_SENTINITIATE:
      ChangeSignalingState(PeerConnectionInterface::kHaveLocalOffer);
      break;
    case cricket::BaseSession::STATE_SENTPRACCEPT:
      ChangeSignalingState(PeerConnectionInterface::kHaveLocalPrAnswer);
      break;
    case cricket::BaseSession::STATE_RECEIVEDINITIATE:
      ChangeSignalingState(PeerConnectionInterface::kHaveRemoteOffer);
      break;
    case cricket::BaseSession::STATE_RECEIVEDPRACCEPT:
      ChangeSignalingState(PeerConnectionInterface::kHaveRemotePrAnswer);
      break;
    case cricket::BaseSession::STATE_SENTACCEPT:
    case cricket::BaseSession::STATE_RECEIVEDACCEPT:
      ChangeSignalingState(PeerConnectionInterface::kStable);
      break;
    case cricket::BaseSession::STATE_RECEIVEDTERMINATE:
      ChangeSignalingState(PeerConnectionInterface::kClosed);
      break;
    default:
      break;
  }
}

void PeerConnection::OnMessage(rtc::Message* msg) {
  switch (msg->message_id) {
    case MSG_SET_SESSIONDESCRIPTION_SUCCESS: {
      SetSessionDescriptionMsg* param =
          static_cast<SetSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnSuccess();
      delete param;
      break;
    }
    case MSG_SET_SESSIONDESCRIPTION_FAILED: {
      SetSessionDescriptionMsg* param =
          static_cast<SetSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnFailure(param->error);
      delete param;
      break;
    }
    case MSG_GETSTATS: {
      GetStatsMsg* param = static_cast<GetStatsMsg*>(msg->pdata);
      StatsReports reports;
      stats_->GetStats(param->track, &reports);
      param->observer->OnComplete(reports);
      delete param;
      break;
    }
    default:
      ASSERT(false && "Not implemented");
      break;
  }
}

void PeerConnection::OnAddRemoteStream(MediaStreamInterface* stream) {
  stats_->AddStream(stream);
  observer_->OnAddStream(stream);
}

void PeerConnection::OnRemoveRemoteStream(MediaStreamInterface* stream) {
  stream_handler_container_->RemoveRemoteStream(stream);
  observer_->OnRemoveStream(stream);
}

void PeerConnection::OnAddDataChannel(DataChannelInterface* data_channel) {
  observer_->OnDataChannel(DataChannelProxy::Create(signaling_thread(),
                                                    data_channel));
}

void PeerConnection::OnAddRemoteAudioTrack(MediaStreamInterface* stream,
                                           AudioTrackInterface* audio_track,
                                           uint32 ssrc) {
  stream_handler_container_->AddRemoteAudioTrack(stream, audio_track, ssrc);
}

void PeerConnection::OnAddRemoteVideoTrack(MediaStreamInterface* stream,
                                           VideoTrackInterface* video_track,
                                           uint32 ssrc) {
  stream_handler_container_->AddRemoteVideoTrack(stream, video_track, ssrc);
}

void PeerConnection::OnRemoveRemoteAudioTrack(
    MediaStreamInterface* stream,
    AudioTrackInterface* audio_track) {
  stream_handler_container_->RemoveRemoteTrack(stream, audio_track);
}

void PeerConnection::OnRemoveRemoteVideoTrack(
    MediaStreamInterface* stream,
    VideoTrackInterface* video_track) {
  stream_handler_container_->RemoveRemoteTrack(stream, video_track);
}
void PeerConnection::OnAddLocalAudioTrack(MediaStreamInterface* stream,
                                          AudioTrackInterface* audio_track,
                                          uint32 ssrc) {
  stream_handler_container_->AddLocalAudioTrack(stream, audio_track, ssrc);
  stats_->AddLocalAudioTrack(audio_track, ssrc);
}
void PeerConnection::OnAddLocalVideoTrack(MediaStreamInterface* stream,
                                          VideoTrackInterface* video_track,
                                          uint32 ssrc) {
  stream_handler_container_->AddLocalVideoTrack(stream, video_track, ssrc);
}

void PeerConnection::OnRemoveLocalAudioTrack(MediaStreamInterface* stream,
                                             AudioTrackInterface* audio_track,
                                             uint32 ssrc) {
  stream_handler_container_->RemoveLocalTrack(stream, audio_track);
  stats_->RemoveLocalAudioTrack(audio_track, ssrc);
}

void PeerConnection::OnRemoveLocalVideoTrack(MediaStreamInterface* stream,
                                             VideoTrackInterface* video_track) {
  stream_handler_container_->RemoveLocalTrack(stream, video_track);
}

void PeerConnection::OnRemoveLocalStream(MediaStreamInterface* stream) {
  stream_handler_container_->RemoveLocalStream(stream);
}

void PeerConnection::OnIceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {
  ASSERT(signaling_thread()->IsCurrent());
  ice_connection_state_ = new_state;
  observer_->OnIceConnectionChange(ice_connection_state_);
}

void PeerConnection::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {
  ASSERT(signaling_thread()->IsCurrent());
  if (IsClosed()) {
    return;
  }
  ice_gathering_state_ = new_state;
  observer_->OnIceGatheringChange(ice_gathering_state_);
}

void PeerConnection::OnIceCandidate(const IceCandidateInterface* candidate) {
  ASSERT(signaling_thread()->IsCurrent());
  observer_->OnIceCandidate(candidate);
}

void PeerConnection::OnIceComplete() {
  ASSERT(signaling_thread()->IsCurrent());
  observer_->OnIceComplete();
}

void PeerConnection::OnIceConnectionReceivingChange(bool receiving) {
  ASSERT(signaling_thread()->IsCurrent());
  observer_->OnIceConnectionReceivingChange(receiving);
}

void PeerConnection::ChangeSignalingState(
    PeerConnectionInterface::SignalingState signaling_state) {
  signaling_state_ = signaling_state;
  if (signaling_state == kClosed) {
    ice_connection_state_ = kIceConnectionClosed;
    observer_->OnIceConnectionChange(ice_connection_state_);
    if (ice_gathering_state_ != kIceGatheringComplete) {
      ice_gathering_state_ = kIceGatheringComplete;
      observer_->OnIceGatheringChange(ice_gathering_state_);
    }
  }
  observer_->OnSignalingChange(signaling_state_);
  observer_->OnStateChange(PeerConnectionObserver::kSignalingState);
}

}  // namespace webrtc
