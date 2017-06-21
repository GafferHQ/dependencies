// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"

#include <vector>

#include "base/command_line.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/renderer_preferences.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_processor.h"
#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/peer_connection_identity_service.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/media/rtc_video_decoder_factory.h"
#include "content/renderer/media/rtc_video_encoder_factory.h"
#include "content/renderer/media/webaudio_capturer_source.h"
#include "content/renderer/media/webrtc/stun_field_trial.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "content/renderer/media/webrtc_logging.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/p2p/ipc_network_manager.h"
#include "content/renderer/p2p/ipc_socket_factory.h"
#include "content/renderer/p2p/port_allocator.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "jingle/glue/thread_wrapper.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.h"

#if defined(USE_OPENSSL)
#include "third_party/webrtc/base/ssladapter.h"
#else
#include "net/socket/nss_ssl_util.h"
#endif

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_bridge.h"
#endif

namespace content {

// Map of corresponding media constraints and platform effects.
struct {
  const char* constraint;
  const media::AudioParameters::PlatformEffectsMask effect;
} const kConstraintEffectMap[] = {
  { content::kMediaStreamAudioDucking,
    media::AudioParameters::DUCKING },
  { webrtc::MediaConstraintsInterface::kGoogEchoCancellation,
    media::AudioParameters::ECHO_CANCELLER },
};

// If any platform effects are available, check them against the constraints.
// Disable effects to match false constraints, but if a constraint is true, set
// the constraint to false to later disable the software effect.
//
// This function may modify both |constraints| and |effects|.
void HarmonizeConstraintsAndEffects(RTCMediaConstraints* constraints,
                                    int* effects) {
  if (*effects != media::AudioParameters::NO_EFFECTS) {
    for (size_t i = 0; i < arraysize(kConstraintEffectMap); ++i) {
      bool value;
      size_t is_mandatory = 0;
      if (!webrtc::FindConstraint(constraints,
                                  kConstraintEffectMap[i].constraint,
                                  &value,
                                  &is_mandatory) || !value) {
        // If the constraint is false, or does not exist, disable the platform
        // effect.
        *effects &= ~kConstraintEffectMap[i].effect;
        DVLOG(1) << "Disabling platform effect: "
                 << kConstraintEffectMap[i].effect;
      } else if (*effects & kConstraintEffectMap[i].effect) {
        // If the constraint is true, leave the platform effect enabled, and
        // set the constraint to false to later disable the software effect.
        if (is_mandatory) {
          constraints->AddMandatory(kConstraintEffectMap[i].constraint,
              webrtc::MediaConstraintsInterface::kValueFalse, true);
        } else {
          constraints->AddOptional(kConstraintEffectMap[i].constraint,
              webrtc::MediaConstraintsInterface::kValueFalse, true);
        }
        DVLOG(1) << "Disabling constraint: "
                 << kConstraintEffectMap[i].constraint;
      } else if (kConstraintEffectMap[i].effect ==
                 media::AudioParameters::DUCKING && value && !is_mandatory) {
        // Special handling of the DUCKING flag that sets the optional
        // constraint to |false| to match what the device will support.
        constraints->AddOptional(kConstraintEffectMap[i].constraint,
            webrtc::MediaConstraintsInterface::kValueFalse, true);
        // No need to modify |effects| since the ducking flag is already off.
        DCHECK((*effects & media::AudioParameters::DUCKING) == 0);
      }
    }
  }
}

class P2PPortAllocatorFactory : public webrtc::PortAllocatorFactoryInterface {
 public:
  P2PPortAllocatorFactory(P2PSocketDispatcher* socket_dispatcher,
                          rtc::NetworkManager* network_manager,
                          rtc::PacketSocketFactory* socket_factory,
                          const GURL& origin,
                          bool enable_multiple_routes)
      : socket_dispatcher_(socket_dispatcher),
        network_manager_(network_manager),
        socket_factory_(socket_factory),
        origin_(origin),
        enable_multiple_routes_(enable_multiple_routes) {}

  cricket::PortAllocator* CreatePortAllocator(
      const std::vector<StunConfiguration>& stun_servers,
      const std::vector<TurnConfiguration>& turn_configurations) override {
    P2PPortAllocator::Config config;
    for (size_t i = 0; i < stun_servers.size(); ++i) {
      config.stun_servers.insert(rtc::SocketAddress(
          stun_servers[i].server.hostname(),
          stun_servers[i].server.port()));
    }
    for (size_t i = 0; i < turn_configurations.size(); ++i) {
      P2PPortAllocator::Config::RelayServerConfig relay_config;
      relay_config.server_address = turn_configurations[i].server.hostname();
      relay_config.port = turn_configurations[i].server.port();
      relay_config.username = turn_configurations[i].username;
      relay_config.password = turn_configurations[i].password;
      relay_config.transport_type = turn_configurations[i].transport_type;
      relay_config.secure = turn_configurations[i].secure;
      config.relays.push_back(relay_config);

      // Use turn servers as stun servers.
      config.stun_servers.insert(rtc::SocketAddress(
          turn_configurations[i].server.hostname(),
          turn_configurations[i].server.port()));
    }
    config.enable_multiple_routes = enable_multiple_routes_;

    return new P2PPortAllocator(
        socket_dispatcher_.get(), network_manager_,
        socket_factory_, config, origin_);
  }

 protected:
  ~P2PPortAllocatorFactory() override {}

 private:
  scoped_refptr<P2PSocketDispatcher> socket_dispatcher_;
  // |network_manager_| and |socket_factory_| are a weak references, owned by
  // PeerConnectionDependencyFactory.
  rtc::NetworkManager* network_manager_;
  rtc::PacketSocketFactory* socket_factory_;
  // The origin URL of the WebFrame that created the
  // P2PPortAllocatorFactory.
  GURL origin_;
  // When false, only 'any' address (all 0s) will be bound for address
  // discovery.
  bool enable_multiple_routes_;
};

PeerConnectionDependencyFactory::PeerConnectionDependencyFactory(
    P2PSocketDispatcher* p2p_socket_dispatcher)
    : network_manager_(NULL),
      p2p_socket_dispatcher_(p2p_socket_dispatcher),
      signaling_thread_(NULL),
      worker_thread_(NULL),
      chrome_signaling_thread_("Chrome_libJingle_Signaling"),
      chrome_worker_thread_("Chrome_libJingle_WorkerThread") {
  TryScheduleStunProbeTrial();
}

PeerConnectionDependencyFactory::~PeerConnectionDependencyFactory() {
  DVLOG(1) << "~PeerConnectionDependencyFactory()";
  DCHECK(pc_factory_ == NULL);
}

blink::WebRTCPeerConnectionHandler*
PeerConnectionDependencyFactory::CreateRTCPeerConnectionHandler(
    blink::WebRTCPeerConnectionHandlerClient* client) {
  // Save histogram data so we can see how much PeerConnetion is used.
  // The histogram counts the number of calls to the JS API
  // webKitRTCPeerConnection.
  UpdateWebRTCMethodCount(WEBKIT_RTC_PEER_CONNECTION);

  return new RTCPeerConnectionHandler(client, this);
}

bool PeerConnectionDependencyFactory::InitializeMediaStreamAudioSource(
    int render_frame_id,
    const blink::WebMediaConstraints& audio_constraints,
    MediaStreamAudioSource* source_data) {
  DVLOG(1) << "InitializeMediaStreamAudioSources()";

  // Do additional source initialization if the audio source is a valid
  // microphone or tab audio.
  RTCMediaConstraints native_audio_constraints(audio_constraints);
  MediaAudioConstraints::ApplyFixedAudioConstraints(&native_audio_constraints);

  StreamDeviceInfo device_info = source_data->device_info();
  RTCMediaConstraints constraints = native_audio_constraints;
  // May modify both |constraints| and |effects|.
  HarmonizeConstraintsAndEffects(&constraints,
                                 &device_info.device.input.effects);

  scoped_refptr<WebRtcAudioCapturer> capturer(CreateAudioCapturer(
      render_frame_id, device_info, audio_constraints, source_data));
  if (!capturer.get()) {
    const std::string log_string =
        "PCDF::InitializeMediaStreamAudioSource: fails to create capturer";
    WebRtcLogMessage(log_string);
    DVLOG(1) << log_string;
    // TODO(xians): Don't we need to check if source_observer is observing
    // something? If not, then it looks like we have a leak here.
    // OTOH, if it _is_ observing something, then the callback might
    // be called multiple times which is likely also a bug.
    return false;
  }
  source_data->SetAudioCapturer(capturer.get());

  // Creates a LocalAudioSource object which holds audio options.
  // TODO(xians): The option should apply to the track instead of the source.
  // TODO(perkj): Move audio constraints parsing to Chrome.
  // Currently there are a few constraints that are parsed by libjingle and
  // the state is set to ended if parsing fails.
  scoped_refptr<webrtc::AudioSourceInterface> rtc_source(
      CreateLocalAudioSource(&constraints).get());
  if (rtc_source->state() != webrtc::MediaSourceInterface::kLive) {
    DLOG(WARNING) << "Failed to create rtc LocalAudioSource.";
    return false;
  }
  source_data->SetLocalAudioSource(rtc_source.get());
  return true;
}

WebRtcVideoCapturerAdapter*
PeerConnectionDependencyFactory::CreateVideoCapturer(
    bool is_screeencast) {
  // We need to make sure the libjingle thread wrappers have been created
  // before we can use an instance of a WebRtcVideoCapturerAdapter. This is
  // since the base class of WebRtcVideoCapturerAdapter is a
  // cricket::VideoCapturer and it uses the libjingle thread wrappers.
  if (!GetPcFactory().get())
    return NULL;
  return new WebRtcVideoCapturerAdapter(is_screeencast);
}

scoped_refptr<webrtc::VideoSourceInterface>
PeerConnectionDependencyFactory::CreateVideoSource(
    cricket::VideoCapturer* capturer,
    const blink::WebMediaConstraints& constraints) {
  RTCMediaConstraints webrtc_constraints(constraints);
  scoped_refptr<webrtc::VideoSourceInterface> source =
      GetPcFactory()->CreateVideoSource(capturer, &webrtc_constraints).get();
  return source;
}

const scoped_refptr<webrtc::PeerConnectionFactoryInterface>&
PeerConnectionDependencyFactory::GetPcFactory() {
  if (!pc_factory_.get())
    CreatePeerConnectionFactory();
  CHECK(pc_factory_.get());
  return pc_factory_;
}


void PeerConnectionDependencyFactory::WillDestroyCurrentMessageLoop() {
  CleanupPeerConnectionFactory();
}

void PeerConnectionDependencyFactory::CreatePeerConnectionFactory() {
  DCHECK(!pc_factory_.get());
  DCHECK(!signaling_thread_);
  DCHECK(!worker_thread_);
  DCHECK(!network_manager_);
  DCHECK(!socket_factory_);
  DCHECK(!chrome_signaling_thread_.IsRunning());
  DCHECK(!chrome_worker_thread_.IsRunning());

  DVLOG(1) << "PeerConnectionDependencyFactory::CreatePeerConnectionFactory()";

  base::MessageLoop::current()->AddDestructionObserver(this);
  // To allow sending to the signaling/worker threads.
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);

  CHECK(chrome_signaling_thread_.Start());
  CHECK(chrome_worker_thread_.Start());

  base::WaitableEvent start_worker_event(true, false);
  chrome_worker_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&PeerConnectionDependencyFactory::InitializeWorkerThread,
                 base::Unretained(this), &worker_thread_, &start_worker_event));

  base::WaitableEvent create_network_manager_event(true, false);
  chrome_worker_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&PeerConnectionDependencyFactory::
                     CreateIpcNetworkManagerOnWorkerThread,
                 base::Unretained(this), &create_network_manager_event));

  start_worker_event.Wait();
  create_network_manager_event.Wait();

  CHECK(worker_thread_);

  // Init SSL, which will be needed by PeerConnection.
#if defined(USE_OPENSSL)
  if (!rtc::InitializeSSL()) {
    LOG(ERROR) << "Failed on InitializeSSL.";
    NOTREACHED();
    return;
  }
#else
  // TODO(ronghuawu): Replace this call with InitializeSSL.
  net::EnsureNSSSSLInit();
#endif

  base::WaitableEvent start_signaling_event(true, false);
  chrome_signaling_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&PeerConnectionDependencyFactory::InitializeSignalingThread,
                 base::Unretained(this),
                 RenderThreadImpl::current()->GetGpuFactories(),
                 &start_signaling_event));

  start_signaling_event.Wait();
  CHECK(signaling_thread_);
}

void PeerConnectionDependencyFactory::InitializeSignalingThread(
    const scoped_refptr<media::GpuVideoAcceleratorFactories>& gpu_factories,
    base::WaitableEvent* event) {
  DCHECK(chrome_signaling_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(worker_thread_);
  DCHECK(p2p_socket_dispatcher_.get());

  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
  signaling_thread_ = jingle_glue::JingleThreadWrapper::current();

  EnsureWebRtcAudioDeviceImpl();

  socket_factory_.reset(
      new IpcPacketSocketFactory(p2p_socket_dispatcher_.get()));

  scoped_ptr<cricket::WebRtcVideoDecoderFactory> decoder_factory;
  scoped_ptr<cricket::WebRtcVideoEncoderFactory> encoder_factory;

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (gpu_factories && gpu_factories->IsGpuVideoAcceleratorEnabled()) {
    if (!cmd_line->HasSwitch(switches::kDisableWebRtcHWDecoding))
      decoder_factory.reset(new RTCVideoDecoderFactory(gpu_factories));

    if (!cmd_line->HasSwitch(switches::kDisableWebRtcHWEncoding))
      encoder_factory.reset(new RTCVideoEncoderFactory(gpu_factories));
  }

#if defined(OS_ANDROID)
  if (!media::MediaCodecBridge::SupportsSetParameters())
    encoder_factory.reset();
#endif

  pc_factory_ = webrtc::CreatePeerConnectionFactory(
      worker_thread_, signaling_thread_, audio_device_.get(),
      encoder_factory.release(), decoder_factory.release());
  CHECK(pc_factory_.get());

  webrtc::PeerConnectionFactoryInterface::Options factory_options;
  factory_options.disable_sctp_data_channels = false;
  factory_options.disable_encryption =
      cmd_line->HasSwitch(switches::kDisableWebRtcEncryption);
  if (cmd_line->HasSwitch(switches::kEnableWebRtcDtls12))
    factory_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
  pc_factory_->SetOptions(factory_options);

  event->Signal();
}

bool PeerConnectionDependencyFactory::PeerConnectionFactoryCreated() {
  return pc_factory_.get() != NULL;
}

scoped_refptr<webrtc::PeerConnectionInterface>
PeerConnectionDependencyFactory::CreatePeerConnection(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    const webrtc::MediaConstraintsInterface* constraints,
    blink::WebFrame* web_frame,
    webrtc::PeerConnectionObserver* observer) {
  CHECK(web_frame);
  CHECK(observer);
  if (!GetPcFactory().get())
    return NULL;

  // Copy the flag from Preference associated with this WebFrame.
  bool enable_multiple_routes = true;
  PeerConnectionIdentityService* identity_service =
      new PeerConnectionIdentityService(
          GURL(web_frame->document().url()),
          GURL(web_frame->document().firstPartyForCookies()));

  if (web_frame && web_frame->view()) {
    RenderViewImpl* renderer_view_impl =
        RenderViewImpl::FromWebView(web_frame->view());
    if (renderer_view_impl) {
      enable_multiple_routes = renderer_view_impl->renderer_preferences()
                                    .enable_webrtc_multiple_routes;
    }
  }

  scoped_refptr<P2PPortAllocatorFactory> pa_factory =
      new rtc::RefCountedObject<P2PPortAllocatorFactory>(
          p2p_socket_dispatcher_.get(), network_manager_, socket_factory_.get(),
          GURL(web_frame->document().url().spec()).GetOrigin(),
          enable_multiple_routes);

  return GetPcFactory()->CreatePeerConnection(config,
                                              constraints,
                                              pa_factory.get(),
                                              identity_service,
                                              observer).get();
}

scoped_refptr<webrtc::MediaStreamInterface>
PeerConnectionDependencyFactory::CreateLocalMediaStream(
    const std::string& label) {
  return GetPcFactory()->CreateLocalMediaStream(label).get();
}

scoped_refptr<webrtc::AudioSourceInterface>
PeerConnectionDependencyFactory::CreateLocalAudioSource(
    const webrtc::MediaConstraintsInterface* constraints) {
  scoped_refptr<webrtc::AudioSourceInterface> source =
      GetPcFactory()->CreateAudioSource(constraints).get();
  return source;
}

void PeerConnectionDependencyFactory::CreateLocalAudioTrack(
    const blink::WebMediaStreamTrack& track) {
  blink::WebMediaStreamSource source = track.source();
  DCHECK_EQ(source.type(), blink::WebMediaStreamSource::TypeAudio);
  MediaStreamAudioSource* source_data =
      static_cast<MediaStreamAudioSource*>(source.extraData());

  scoped_refptr<WebAudioCapturerSource> webaudio_source;
  if (!source_data) {
    if (source.requiresAudioConsumer()) {
      // We're adding a WebAudio MediaStream.
      // Create a specific capturer for each WebAudio consumer.
      webaudio_source = CreateWebAudioSource(&source);
      source_data =
          static_cast<MediaStreamAudioSource*>(source.extraData());
    } else {
      // TODO(perkj): Implement support for sources from
      // remote MediaStreams.
      NOTIMPLEMENTED();
      return;
    }
  }

  // Creates an adapter to hold all the libjingle objects.
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter(
      WebRtcLocalAudioTrackAdapter::Create(track.id().utf8(),
                                           source_data->local_audio_source()));
  static_cast<webrtc::AudioTrackInterface*>(adapter.get())->set_enabled(
      track.isEnabled());

  // TODO(xians): Merge |source| to the capturer(). We can't do this today
  // because only one capturer() is supported while one |source| is created
  // for each audio track.
  scoped_ptr<WebRtcLocalAudioTrack> audio_track(new WebRtcLocalAudioTrack(
      adapter.get(), source_data->GetAudioCapturer(), webaudio_source.get()));

  StartLocalAudioTrack(audio_track.get());

  // Pass the ownership of the native local audio track to the blink track.
  blink::WebMediaStreamTrack writable_track = track;
  writable_track.setExtraData(audio_track.release());
}

void PeerConnectionDependencyFactory::StartLocalAudioTrack(
    WebRtcLocalAudioTrack* audio_track) {
  // Start the audio track. This will hook the |audio_track| to the capturer
  // as the sink of the audio, and only start the source of the capturer if
  // it is the first audio track connecting to the capturer.
  audio_track->Start();
}

scoped_refptr<WebAudioCapturerSource>
PeerConnectionDependencyFactory::CreateWebAudioSource(
    blink::WebMediaStreamSource* source) {
  DVLOG(1) << "PeerConnectionDependencyFactory::CreateWebAudioSource()";

  scoped_refptr<WebAudioCapturerSource>
      webaudio_capturer_source(new WebAudioCapturerSource(*source));
  MediaStreamAudioSource* source_data = new MediaStreamAudioSource();

  // Use the current default capturer for the WebAudio track so that the
  // WebAudio track can pass a valid delay value and |need_audio_processing|
  // flag to PeerConnection.
  // TODO(xians): Remove this after moving APM to Chrome.
  if (GetWebRtcAudioDevice()) {
    source_data->SetAudioCapturer(
        GetWebRtcAudioDevice()->GetDefaultCapturer());
  }

  // Create a LocalAudioSource object which holds audio options.
  // SetLocalAudioSource() affects core audio parts in third_party/Libjingle.
  source_data->SetLocalAudioSource(CreateLocalAudioSource(NULL).get());
  source->setExtraData(source_data);

  // Replace the default source with WebAudio as source instead.
  source->addAudioConsumer(webaudio_capturer_source.get());

  return webaudio_capturer_source;
}

scoped_refptr<webrtc::VideoTrackInterface>
PeerConnectionDependencyFactory::CreateLocalVideoTrack(
    const std::string& id,
    webrtc::VideoSourceInterface* source) {
  return GetPcFactory()->CreateVideoTrack(id, source).get();
}

scoped_refptr<webrtc::VideoTrackInterface>
PeerConnectionDependencyFactory::CreateLocalVideoTrack(
    const std::string& id, cricket::VideoCapturer* capturer) {
  if (!capturer) {
    LOG(ERROR) << "CreateLocalVideoTrack called with null VideoCapturer.";
    return NULL;
  }

  // Create video source from the |capturer|.
  scoped_refptr<webrtc::VideoSourceInterface> source =
      GetPcFactory()->CreateVideoSource(capturer, NULL).get();

  // Create native track from the source.
  return GetPcFactory()->CreateVideoTrack(id, source.get()).get();
}

webrtc::SessionDescriptionInterface*
PeerConnectionDependencyFactory::CreateSessionDescription(
    const std::string& type,
    const std::string& sdp,
    webrtc::SdpParseError* error) {
  return webrtc::CreateSessionDescription(type, sdp, error);
}

webrtc::IceCandidateInterface*
PeerConnectionDependencyFactory::CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const std::string& sdp) {
  return webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, sdp, nullptr);
}

WebRtcAudioDeviceImpl*
PeerConnectionDependencyFactory::GetWebRtcAudioDevice() {
  return audio_device_.get();
}

void PeerConnectionDependencyFactory::InitializeWorkerThread(
    rtc::Thread** thread,
    base::WaitableEvent* event) {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
  *thread = jingle_glue::JingleThreadWrapper::current();
  event->Signal();
}

void PeerConnectionDependencyFactory::TryScheduleStunProbeTrial() {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  if (!cmd_line->HasSwitch(switches::kWebRtcStunProbeTrialParameter))
    return;

  GetPcFactory();

  // The underneath IPC channel has to be connected before sending any IPC
  // message.
  if (!p2p_socket_dispatcher_->connected()) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PeerConnectionDependencyFactory::TryScheduleStunProbeTrial,
                   base::Unretained(this)),
        base::TimeDelta::FromSeconds(1));
    return;
  }

  const std::string params =
      cmd_line->GetSwitchValueASCII(switches::kWebRtcStunProbeTrialParameter);

  chrome_worker_thread_.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(
          &PeerConnectionDependencyFactory::StartStunProbeTrialOnWorkerThread,
          base::Unretained(this), params),
      base::TimeDelta::FromMilliseconds(kExperimentStartDelayMs));
}

void PeerConnectionDependencyFactory::StartStunProbeTrialOnWorkerThread(
    const std::string& params) {
  DCHECK(chrome_worker_thread_.task_runner()->BelongsToCurrentThread());
  rtc::NetworkManager::NetworkList networks;
  network_manager_->GetNetworks(&networks);
  stun_prober_ = StartStunProbeTrial(networks, params, socket_factory_.get());
}

void PeerConnectionDependencyFactory::CreateIpcNetworkManagerOnWorkerThread(
    base::WaitableEvent* event) {
  DCHECK(chrome_worker_thread_.task_runner()->BelongsToCurrentThread());
  network_manager_ = new IpcNetworkManager(p2p_socket_dispatcher_.get());
  event->Signal();
}

void PeerConnectionDependencyFactory::DeleteIpcNetworkManager() {
  DCHECK(chrome_worker_thread_.task_runner()->BelongsToCurrentThread());
  delete network_manager_;
  network_manager_ = NULL;
}

void PeerConnectionDependencyFactory::CleanupPeerConnectionFactory() {
  DVLOG(1) << "PeerConnectionDependencyFactory::CleanupPeerConnectionFactory()";
  pc_factory_ = NULL;
  if (network_manager_) {
    // The network manager needs to free its resources on the thread they were
    // created, which is the worked thread.
    if (chrome_worker_thread_.IsRunning()) {
      chrome_worker_thread_.task_runner()->PostTask(
          FROM_HERE,
          base::Bind(&PeerConnectionDependencyFactory::DeleteIpcNetworkManager,
                     base::Unretained(this)));
      // Stopping the thread will wait until all tasks have been
      // processed before returning. We wait for the above task to finish before
      // letting the the function continue to avoid any potential race issues.
      chrome_worker_thread_.Stop();
    } else {
      NOTREACHED() << "Worker thread not running.";
    }
  }
}

scoped_refptr<WebRtcAudioCapturer>
PeerConnectionDependencyFactory::CreateAudioCapturer(
    int render_frame_id,
    const StreamDeviceInfo& device_info,
    const blink::WebMediaConstraints& constraints,
    MediaStreamAudioSource* audio_source) {
  // TODO(xians): Handle the cases when gUM is called without a proper render
  // view, for example, by an extension.
  DCHECK_GE(render_frame_id, 0);

  EnsureWebRtcAudioDeviceImpl();
  DCHECK(GetWebRtcAudioDevice());
  return WebRtcAudioCapturer::CreateCapturer(
      render_frame_id, device_info, constraints, GetWebRtcAudioDevice(),
      audio_source);
}

scoped_refptr<base::SingleThreadTaskRunner>
PeerConnectionDependencyFactory::GetWebRtcWorkerThread() const {
  DCHECK(CalledOnValidThread());
  return chrome_worker_thread_.IsRunning() ? chrome_worker_thread_.task_runner()
                                           : nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
PeerConnectionDependencyFactory::GetWebRtcSignalingThread() const {
  DCHECK(CalledOnValidThread());
  return chrome_signaling_thread_.IsRunning()
             ? chrome_signaling_thread_.task_runner()
             : nullptr;
}

void PeerConnectionDependencyFactory::EnsureWebRtcAudioDeviceImpl() {
  if (audio_device_.get())
    return;

  audio_device_ = new WebRtcAudioDeviceImpl();
}

}  // namespace content
