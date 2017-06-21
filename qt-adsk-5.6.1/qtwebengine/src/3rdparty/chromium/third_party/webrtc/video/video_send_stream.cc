/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/video_send_stream.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace_event.h"
#include "webrtc/video/video_capture_input.h"
#include "webrtc/video_engine/encoder_state_feedback.h"
#include "webrtc/video_engine/vie_channel.h"
#include "webrtc/video_engine/vie_channel_group.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
std::string
VideoSendStream::Config::EncoderSettings::ToString() const {
  std::stringstream ss;
  ss << "{payload_name: " << payload_name;
  ss << ", payload_type: " << payload_type;
  ss << ", encoder: " << (encoder != nullptr ? "(VideoEncoder)" : "nullptr");
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::Rtp::Rtx::ToString()
    const {
  std::stringstream ss;
  ss << "{ssrcs: [";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << ", payload_type: " << payload_type;
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{ssrcs: [";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << ", max_packet_size: " << max_packet_size;
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << ", nack: {rtp_history_ms: " << nack.rtp_history_ms << '}';
  ss << ", fec: " << fec.ToString();
  ss << ", rtx: " << rtx.ToString();
  ss << ", c_name: " << c_name;
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{encoder_settings: " << encoder_settings.ToString();
  ss << ", rtp: " << rtp.ToString();
  ss << ", pre_encode_callback: "
     << (pre_encode_callback != nullptr ? "(I420FrameCallback)" : "nullptr");
  ss << ", post_encode_callback: " << (post_encode_callback != nullptr
                                           ? "(EncodedFrameObserver)"
                                           : "nullptr");
  ss << "local_renderer: " << (local_renderer != nullptr ? "(VideoRenderer)"
                                                         : "nullptr");
  ss << ", render_delay_ms: " << render_delay_ms;
  ss << ", target_delay_ms: " << target_delay_ms;
  ss << ", suspend_below_min_bitrate: " << (suspend_below_min_bitrate ? "on"
                                                                      : "off");
  ss << '}';
  return ss.str();
}

namespace internal {
VideoSendStream::VideoSendStream(
    newapi::Transport* transport,
    CpuOveruseObserver* overuse_observer,
    int num_cpu_cores,
    ProcessThread* module_process_thread,
    ChannelGroup* channel_group,
    int channel_id,
    const VideoSendStream::Config& config,
    const VideoEncoderConfig& encoder_config,
    const std::map<uint32_t, RtpState>& suspended_ssrcs)
    : transport_adapter_(transport),
      encoded_frame_proxy_(config.post_encode_callback),
      config_(config),
      suspended_ssrcs_(suspended_ssrcs),
      module_process_thread_(module_process_thread),
      channel_group_(channel_group),
      channel_id_(channel_id),
      use_config_bitrate_(true),
      stats_proxy_(Clock::GetRealTimeClock(), config) {
  CHECK(channel_group->CreateSendChannel(channel_id_, 0, &transport_adapter_,
                                         num_cpu_cores, true));
  vie_channel_ = channel_group_->GetChannel(channel_id_);
  vie_encoder_ = channel_group_->GetEncoder(channel_id_);

  DCHECK(!config_.rtp.ssrcs.empty());

  for (size_t i = 0; i < config_.rtp.extensions.size(); ++i) {
    const std::string& extension = config_.rtp.extensions[i].name;
    int id = config_.rtp.extensions[i].id;
    // One-byte-extension local identifiers are in the range 1-14 inclusive.
    DCHECK_GE(id, 1);
    DCHECK_LE(id, 14);
    if (extension == RtpExtension::kTOffset) {
      CHECK_EQ(0, vie_channel_->SetSendTimestampOffsetStatus(true, id));
    } else if (extension == RtpExtension::kAbsSendTime) {
      CHECK_EQ(0, vie_channel_->SetSendAbsoluteSendTimeStatus(true, id));
    } else if (extension == RtpExtension::kVideoRotation) {
      CHECK_EQ(0, vie_channel_->SetSendVideoRotationStatus(true, id));
    } else {
      RTC_NOTREACHED() << "Registering unsupported RTP extension.";
    }
  }

  // TODO(pbos): Consider configuring REMB in Call.
  channel_group_->SetChannelRembStatus(true, false, vie_channel_);

  // Enable NACK, FEC or both.
  bool enable_protection_nack = false;
  bool enable_protection_fec = false;
  if (config_.rtp.fec.red_payload_type != -1) {
    enable_protection_fec = true;
    DCHECK(config_.rtp.fec.ulpfec_payload_type != -1);
    if (config_.rtp.nack.rtp_history_ms > 0) {
      enable_protection_nack = true;
      vie_channel_->SetHybridNACKFECStatus(
          true, static_cast<unsigned char>(config_.rtp.fec.red_payload_type),
          static_cast<unsigned char>(config_.rtp.fec.ulpfec_payload_type));
    } else {
      vie_channel_->SetFECStatus(
          true, static_cast<unsigned char>(config_.rtp.fec.red_payload_type),
          static_cast<unsigned char>(config_.rtp.fec.ulpfec_payload_type));
    }
    // TODO(changbin): Should set RTX for RED mapping in RTP sender in future.
  } else {
    enable_protection_nack = config_.rtp.nack.rtp_history_ms > 0;
    vie_channel_->SetNACKStatus(config_.rtp.nack.rtp_history_ms > 0);
  }
  vie_encoder_->UpdateProtectionMethod(enable_protection_nack,
                                      enable_protection_fec);

  ConfigureSsrcs();

  vie_channel_->SetRTCPCName(config_.rtp.c_name.c_str());

  input_.reset(new internal::VideoCaptureInput(
      module_process_thread_, vie_encoder_, config_.local_renderer,
      &stats_proxy_, overuse_observer));

  // 28 to match packet overhead in ModuleRtpRtcpImpl.
  DCHECK_LE(config_.rtp.max_packet_size, static_cast<size_t>(0xFFFF - 28));
  vie_channel_->SetMTU(static_cast<uint16_t>(config_.rtp.max_packet_size + 28));

  DCHECK(config.encoder_settings.encoder != nullptr);
  DCHECK_GE(config.encoder_settings.payload_type, 0);
  DCHECK_LE(config.encoder_settings.payload_type, 127);
  // TODO(pbos): Wire up codec internal-source setting or remove setting.
  CHECK_EQ(0, vie_encoder_->RegisterExternalEncoder(
                  config.encoder_settings.encoder,
                  config.encoder_settings.payload_type, false));

  CHECK(ReconfigureVideoEncoder(encoder_config));

  vie_channel_->RegisterSendSideDelayObserver(&stats_proxy_);
  vie_encoder_->RegisterSendStatisticsProxy(&stats_proxy_);

  vie_encoder_->RegisterPreEncodeCallback(config_.pre_encode_callback);
  if (config_.post_encode_callback)
    vie_encoder_->RegisterPostEncodeImageCallback(&encoded_frame_proxy_);

  if (config_.suspend_below_min_bitrate) {
    vie_encoder_->SuspendBelowMinBitrate();
    // Must enable pacing when enabling SuspendBelowMinBitrate. Otherwise, no
    // padding will be sent when the video is suspended so the video will be
    // unable to recover.
    // TODO(pbos): Pacing should probably be enabled outside of VideoSendStream.
    vie_channel_->SetTransmissionSmoothingStatus(true);
  }

  vie_channel_->RegisterSendChannelRtcpStatisticsCallback(&stats_proxy_);
  vie_channel_->RegisterSendChannelRtpStatisticsCallback(&stats_proxy_);
  vie_channel_->RegisterRtcpPacketTypeCounterObserver(&stats_proxy_);
  vie_channel_->RegisterSendBitrateObserver(&stats_proxy_);
  vie_channel_->RegisterSendFrameCountObserver(&stats_proxy_);

  vie_encoder_->RegisterCodecObserver(&stats_proxy_);
}

VideoSendStream::~VideoSendStream() {
  vie_encoder_->RegisterCodecObserver(nullptr);

  vie_channel_->RegisterSendFrameCountObserver(nullptr);
  vie_channel_->RegisterSendBitrateObserver(nullptr);
  vie_channel_->RegisterRtcpPacketTypeCounterObserver(nullptr);
  vie_channel_->RegisterSendChannelRtpStatisticsCallback(nullptr);
  vie_channel_->RegisterSendChannelRtcpStatisticsCallback(nullptr);

  vie_encoder_->RegisterPreEncodeCallback(nullptr);
  vie_encoder_->RegisterPostEncodeImageCallback(nullptr);

  // Remove capture input (thread) so that it's not running after the current
  // channel is deleted.
  input_.reset();

  vie_encoder_->DeRegisterExternalEncoder(
      config_.encoder_settings.payload_type);

  channel_group_->DeleteChannel(channel_id_);
}

VideoCaptureInput* VideoSendStream::Input() {
  return input_.get();
}

void VideoSendStream::Start() {
  transport_adapter_.Enable();
  vie_encoder_->Pause();
  if (vie_channel_->StartSend() == 0) {
    // Was not already started, trigger a keyframe.
    vie_encoder_->SendKeyFrame();
  }
  vie_encoder_->Restart();
  vie_channel_->StartReceive();
}

void VideoSendStream::Stop() {
  // TODO(pbos): Make sure the encoder stops here.
  vie_channel_->StopSend();
  vie_channel_->StopReceive();
  transport_adapter_.Disable();
}

bool VideoSendStream::ReconfigureVideoEncoder(
    const VideoEncoderConfig& config) {
  TRACE_EVENT0("webrtc", "VideoSendStream::(Re)configureVideoEncoder");
  LOG(LS_INFO) << "(Re)configureVideoEncoder: " << config.ToString();
  const std::vector<VideoStream>& streams = config.streams;
  DCHECK(!streams.empty());
  DCHECK_GE(config_.rtp.ssrcs.size(), streams.size());

  VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(video_codec));
  if (config_.encoder_settings.payload_name == "VP8") {
    video_codec.codecType = kVideoCodecVP8;
  } else if (config_.encoder_settings.payload_name == "VP9") {
    video_codec.codecType = kVideoCodecVP9;
  } else if (config_.encoder_settings.payload_name == "H264") {
    video_codec.codecType = kVideoCodecH264;
  } else {
    video_codec.codecType = kVideoCodecGeneric;
  }

  switch (config.content_type) {
    case VideoEncoderConfig::ContentType::kRealtimeVideo:
      video_codec.mode = kRealtimeVideo;
      break;
    case VideoEncoderConfig::ContentType::kScreen:
      video_codec.mode = kScreensharing;
      if (config.streams.size() == 1 &&
          config.streams[0].temporal_layer_thresholds_bps.size() == 1) {
        video_codec.targetBitrate =
            config.streams[0].temporal_layer_thresholds_bps[0] / 1000;
      }
      break;
  }

  if (video_codec.codecType == kVideoCodecVP8) {
    video_codec.codecSpecific.VP8 = VideoEncoder::GetDefaultVp8Settings();
  } else if (video_codec.codecType == kVideoCodecVP9) {
    video_codec.codecSpecific.VP9 = VideoEncoder::GetDefaultVp9Settings();
  } else if (video_codec.codecType == kVideoCodecH264) {
    video_codec.codecSpecific.H264 = VideoEncoder::GetDefaultH264Settings();
  }

  if (video_codec.codecType == kVideoCodecVP8) {
    if (config.encoder_specific_settings != nullptr) {
      video_codec.codecSpecific.VP8 = *reinterpret_cast<const VideoCodecVP8*>(
                                          config.encoder_specific_settings);
    }
    video_codec.codecSpecific.VP8.numberOfTemporalLayers =
        static_cast<unsigned char>(
            streams.back().temporal_layer_thresholds_bps.size() + 1);
  } else if (video_codec.codecType == kVideoCodecVP9) {
    if (config.encoder_specific_settings != nullptr) {
      video_codec.codecSpecific.VP9 = *reinterpret_cast<const VideoCodecVP9*>(
                                          config.encoder_specific_settings);
    }
    video_codec.codecSpecific.VP9.numberOfTemporalLayers =
        static_cast<unsigned char>(
            streams.back().temporal_layer_thresholds_bps.size() + 1);
  } else if (video_codec.codecType == kVideoCodecH264) {
    if (config.encoder_specific_settings != nullptr) {
      video_codec.codecSpecific.H264 = *reinterpret_cast<const VideoCodecH264*>(
                                           config.encoder_specific_settings);
    }
  } else {
    // TODO(pbos): Support encoder_settings codec-agnostically.
    DCHECK(config.encoder_specific_settings == nullptr)
        << "Encoder-specific settings for codec type not wired up.";
  }

  strncpy(video_codec.plName,
          config_.encoder_settings.payload_name.c_str(),
          kPayloadNameSize - 1);
  video_codec.plName[kPayloadNameSize - 1] = '\0';
  video_codec.plType = config_.encoder_settings.payload_type;
  video_codec.numberOfSimulcastStreams =
      static_cast<unsigned char>(streams.size());
  video_codec.minBitrate = streams[0].min_bitrate_bps / 1000;
  DCHECK_LE(streams.size(), static_cast<size_t>(kMaxSimulcastStreams));
  for (size_t i = 0; i < streams.size(); ++i) {
    SimulcastStream* sim_stream = &video_codec.simulcastStream[i];
    DCHECK_GT(streams[i].width, 0u);
    DCHECK_GT(streams[i].height, 0u);
    DCHECK_GT(streams[i].max_framerate, 0);
    // Different framerates not supported per stream at the moment.
    DCHECK_EQ(streams[i].max_framerate, streams[0].max_framerate);
    DCHECK_GE(streams[i].min_bitrate_bps, 0);
    DCHECK_GE(streams[i].target_bitrate_bps, streams[i].min_bitrate_bps);
    DCHECK_GE(streams[i].max_bitrate_bps, streams[i].target_bitrate_bps);
    DCHECK_GE(streams[i].max_qp, 0);

    sim_stream->width = static_cast<unsigned short>(streams[i].width);
    sim_stream->height = static_cast<unsigned short>(streams[i].height);
    sim_stream->minBitrate = streams[i].min_bitrate_bps / 1000;
    sim_stream->targetBitrate = streams[i].target_bitrate_bps / 1000;
    sim_stream->maxBitrate = streams[i].max_bitrate_bps / 1000;
    sim_stream->qpMax = streams[i].max_qp;
    sim_stream->numberOfTemporalLayers = static_cast<unsigned char>(
        streams[i].temporal_layer_thresholds_bps.size() + 1);

    video_codec.width = std::max(video_codec.width,
                                 static_cast<unsigned short>(streams[i].width));
    video_codec.height = std::max(
        video_codec.height, static_cast<unsigned short>(streams[i].height));
    video_codec.minBitrate =
        std::min(video_codec.minBitrate,
                 static_cast<unsigned int>(streams[i].min_bitrate_bps / 1000));
    video_codec.maxBitrate += streams[i].max_bitrate_bps / 1000;
    video_codec.qpMax = std::max(video_codec.qpMax,
                                 static_cast<unsigned int>(streams[i].max_qp));
  }

  // Set to zero to not update the bitrate controller from ViEEncoder, as
  // the bitrate controller is already set from Call.
  video_codec.startBitrate = 0;

  DCHECK_GT(streams[0].max_framerate, 0);
  video_codec.maxFramerate = streams[0].max_framerate;

  if (!SetSendCodec(video_codec))
    return false;

  // Clear stats for disabled layers.
  for (size_t i = video_codec.numberOfSimulcastStreams;
       i < config_.rtp.ssrcs.size(); ++i) {
    stats_proxy_.OnInactiveSsrc(config_.rtp.ssrcs[i]);
  }

  DCHECK_GE(config.min_transmit_bitrate_bps, 0);
  vie_encoder_->SetMinTransmitBitrate(config.min_transmit_bitrate_bps / 1000);

  encoder_config_ = config;
  use_config_bitrate_ = false;
  return true;
}

bool VideoSendStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return vie_channel_->ReceivedRTCPPacket(packet, length) == 0;
}

VideoSendStream::Stats VideoSendStream::GetStats() {
  return stats_proxy_.GetStats();
}

void VideoSendStream::ConfigureSsrcs() {
  vie_channel_->SetSSRC(config_.rtp.ssrcs.front(), kViEStreamTypeNormal, 0);
  for (size_t i = 0; i < config_.rtp.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.ssrcs[i];
    vie_channel_->SetSSRC(ssrc, kViEStreamTypeNormal,
                          static_cast<unsigned char>(i));
    RtpStateMap::iterator it = suspended_ssrcs_.find(ssrc);
    if (it != suspended_ssrcs_.end())
      vie_channel_->SetRtpStateForSsrc(ssrc, it->second);
  }

  if (config_.rtp.rtx.ssrcs.empty()) {
    return;
  }

  // Set up RTX.
  DCHECK_EQ(config_.rtp.rtx.ssrcs.size(), config_.rtp.ssrcs.size());
  for (size_t i = 0; i < config_.rtp.rtx.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.rtx.ssrcs[i];
    vie_channel_->SetSSRC(config_.rtp.rtx.ssrcs[i], kViEStreamTypeRtx,
                          static_cast<unsigned char>(i));
    RtpStateMap::iterator it = suspended_ssrcs_.find(ssrc);
    if (it != suspended_ssrcs_.end())
      vie_channel_->SetRtpStateForSsrc(ssrc, it->second);
  }

  DCHECK_GE(config_.rtp.rtx.payload_type, 0);
  vie_channel_->SetRtxSendPayloadType(config_.rtp.rtx.payload_type,
                                      config_.encoder_settings.payload_type);
}

std::map<uint32_t, RtpState> VideoSendStream::GetRtpStates() const {
  std::map<uint32_t, RtpState> rtp_states;
  for (size_t i = 0; i < config_.rtp.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.ssrcs[i];
    rtp_states[ssrc] = vie_channel_->GetRtpStateForSsrc( ssrc);
  }

  for (size_t i = 0; i < config_.rtp.rtx.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.rtx.ssrcs[i];
    rtp_states[ssrc] = vie_channel_->GetRtpStateForSsrc(ssrc);
  }

  return rtp_states;
}

void VideoSendStream::SignalNetworkState(Call::NetworkState state) {
  // When network goes up, enable RTCP status before setting transmission state.
  // When it goes down, disable RTCP afterwards. This ensures that any packets
  // sent due to the network state changed will not be dropped.
  if (state == Call::kNetworkUp)
    vie_channel_->SetRTCPMode(kRtcpCompound);
  vie_encoder_->SetNetworkTransmissionState(state == Call::kNetworkUp);
  if (state == Call::kNetworkDown)
    vie_channel_->SetRTCPMode(kRtcpOff);
}

int64_t VideoSendStream::GetRtt() const {
  webrtc::RtcpStatistics rtcp_stats;
  uint16_t frac_lost;
  uint32_t cumulative_lost;
  uint32_t extended_max_sequence_number;
  uint32_t jitter;
  int64_t rtt_ms;
  if (vie_channel_->GetSendRtcpStatistics(&frac_lost, &cumulative_lost,
                                          &extended_max_sequence_number,
                                          &jitter, &rtt_ms) == 0) {
    return rtt_ms;
  }
  return -1;
}

bool VideoSendStream::SetSendCodec(VideoCodec video_codec) {
  if (video_codec.maxBitrate == 0) {
    // Unset max bitrate -> cap to one bit per pixel.
    video_codec.maxBitrate =
        (video_codec.width * video_codec.height * video_codec.maxFramerate) /
        1000;
  }

  if (video_codec.minBitrate < kViEMinCodecBitrate)
    video_codec.minBitrate = kViEMinCodecBitrate;
  if (video_codec.maxBitrate < kViEMinCodecBitrate)
    video_codec.maxBitrate = kViEMinCodecBitrate;

  // Stop the media flow while reconfiguring.
  vie_encoder_->Pause();

  if (vie_encoder_->SetEncoder(video_codec) != 0) {
    LOG(LS_ERROR) << "Failed to set encoder.";
    return false;
  }

  if (vie_channel_->SetSendCodec(video_codec, false) != 0) {
    LOG(LS_ERROR) << "Failed to set send codec.";
    return false;
  }

  // Not all configured SSRCs have to be utilized (simulcast senders don't have
  // to send on all SSRCs at once etc.)
  std::vector<uint32_t> used_ssrcs = config_.rtp.ssrcs;
  used_ssrcs.resize(static_cast<size_t>(video_codec.numberOfSimulcastStreams));

  // Update used SSRCs.
  vie_encoder_->SetSsrcs(used_ssrcs);
  EncoderStateFeedback* encoder_state_feedback =
      channel_group_->GetEncoderStateFeedback();
  encoder_state_feedback->UpdateSsrcs(used_ssrcs, vie_encoder_);

  // Update the protection mode, we might be switching NACK/FEC.
  vie_encoder_->UpdateProtectionMethod(vie_encoder_->nack_enabled(),
                                       vie_channel_->IsSendingFecEnabled());

  // Restart the media flow
  vie_encoder_->Restart();

  return true;
}

}  // namespace internal
}  // namespace webrtc
