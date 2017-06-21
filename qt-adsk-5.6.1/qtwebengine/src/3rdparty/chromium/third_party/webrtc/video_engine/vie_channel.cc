/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_channel.h"

#include <algorithm>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/common.h"
#include "webrtc/common_video/interface/incoming_video_stream.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/experiments.h"
#include "webrtc/frame_callback.h"
#include "webrtc/modules/pacing/include/paced_sender.h"
#include "webrtc/modules/pacing/include/packet_router.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_render/include/video_render_defines.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/metrics.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/video/receive_statistics_proxy.h"
#include "webrtc/video_engine/call_stats.h"
#include "webrtc/video_engine/payload_router.h"
#include "webrtc/video_engine/report_block_stats.h"
#include "webrtc/video_engine/vie_defines.h"

namespace webrtc {

const int kMaxDecodeWaitTimeMs = 50;
const int kInvalidRtpExtensionId = 0;
static const int kMaxTargetDelayMs = 10000;
static const float kMaxIncompleteTimeMultiplier = 3.5f;

// Helper class receiving statistics callbacks.
class ChannelStatsObserver : public CallStatsObserver {
 public:
  explicit ChannelStatsObserver(ViEChannel* owner) : owner_(owner) {}
  virtual ~ChannelStatsObserver() {}

  // Implements StatsObserver.
  virtual void OnRttUpdate(int64_t rtt) {
    owner_->OnRttUpdate(rtt);
  }

 private:
  ViEChannel* const owner_;
};

class ViEChannelProtectionCallback : public VCMProtectionCallback {
 public:
  ViEChannelProtectionCallback(ViEChannel* owner) : owner_(owner) {}
  ~ViEChannelProtectionCallback() {}


  int ProtectionRequest(
      const FecProtectionParams* delta_fec_params,
      const FecProtectionParams* key_fec_params,
      uint32_t* sent_video_rate_bps,
      uint32_t* sent_nack_rate_bps,
      uint32_t* sent_fec_rate_bps) override {
    return owner_->ProtectionRequest(delta_fec_params, key_fec_params,
                                     sent_video_rate_bps, sent_nack_rate_bps,
                                     sent_fec_rate_bps);
  }
 private:
  ViEChannel* owner_;
};

ViEChannel::ViEChannel(int32_t channel_id,
                       int32_t engine_id,
                       uint32_t number_of_cores,
                       const Config& config,
                       Transport* transport,
                       ProcessThread& module_process_thread,
                       RtcpIntraFrameObserver* intra_frame_observer,
                       RtcpBandwidthObserver* bandwidth_observer,
                       RemoteBitrateEstimator* remote_bitrate_estimator,
                       RtcpRttStats* rtt_stats,
                       PacedSender* paced_sender,
                       PacketRouter* packet_router,
                       bool sender,
                       bool disable_default_encoder)
    : channel_id_(channel_id),
      engine_id_(engine_id),
      number_of_cores_(number_of_cores),
      num_socket_threads_(kViESocketThreads),
      callback_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      rtp_rtcp_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      send_payload_router_(new PayloadRouter()),
      vcm_protection_callback_(new ViEChannelProtectionCallback(this)),
      vcm_(VideoCodingModule::Create(Clock::GetRealTimeClock(),
                                     nullptr,
                                     nullptr)),
      vie_receiver_(channel_id, vcm_, remote_bitrate_estimator, this),
      vie_sync_(vcm_),
      stats_observer_(new ChannelStatsObserver(this)),
      vcm_receive_stats_callback_(NULL),
      incoming_video_stream_(nullptr),
      module_process_thread_(module_process_thread),
      codec_observer_(NULL),
      do_key_frame_callbackRequest_(false),
      intra_frame_observer_(intra_frame_observer),
      rtt_stats_(rtt_stats),
      paced_sender_(paced_sender),
      packet_router_(packet_router),
      bandwidth_observer_(bandwidth_observer),
      send_timestamp_extension_id_(kInvalidRtpExtensionId),
      absolute_send_time_extension_id_(kInvalidRtpExtensionId),
      video_rotation_extension_id_(kInvalidRtpExtensionId),
      transport_(transport),
      decoder_reset_(true),
      wait_for_key_frame_(false),
      mtu_(0),
      sender_(sender),
      disable_default_encoder_(disable_default_encoder),
      nack_history_size_sender_(kSendSidePacketHistorySize),
      max_nack_reordering_threshold_(kMaxPacketAgeToNack),
      pre_render_callback_(NULL),
      report_block_stats_sender_(new ReportBlockStats()) {
  RtpRtcp::Configuration configuration = CreateRtpRtcpConfiguration();
  configuration.remote_bitrate_estimator = remote_bitrate_estimator;
  configuration.receive_statistics = vie_receiver_.GetReceiveStatistics();
  rtp_rtcp_.reset(RtpRtcp::CreateRtpRtcp(configuration));
  vie_receiver_.SetRtpRtcpModule(rtp_rtcp_.get());
  vcm_->SetNackSettings(kMaxNackListSize, max_nack_reordering_threshold_, 0);
}

int32_t ViEChannel::Init() {
  module_process_thread_.RegisterModule(vie_receiver_.GetReceiveStatistics());

  // RTP/RTCP initialization.
  rtp_rtcp_->SetSendingMediaStatus(false);
  module_process_thread_.RegisterModule(rtp_rtcp_.get());

  rtp_rtcp_->SetKeyFrameRequestMethod(kKeyFrameReqFirRtp);
  rtp_rtcp_->SetRTCPStatus(kRtcpCompound);
  if (paced_sender_) {
    rtp_rtcp_->SetStorePacketsStatus(true, nack_history_size_sender_);
  }
  if (sender_) {
    packet_router_->AddRtpModule(rtp_rtcp_.get());
    std::list<RtpRtcp*> send_rtp_modules(1, rtp_rtcp_.get());
    send_payload_router_->SetSendingRtpModules(send_rtp_modules);
    DCHECK(!send_payload_router_->active());
  }
  if (vcm_->SetVideoProtection(kProtectionKeyOnLoss, true)) {
    return -1;
  }
  if (vcm_->RegisterReceiveCallback(this) != 0) {
    return -1;
  }
  vcm_->RegisterFrameTypeCallback(this);
  vcm_->RegisterReceiveStatisticsCallback(this);
  vcm_->RegisterDecoderTimingCallback(this);
  vcm_->SetRenderDelay(kViEDefaultRenderDelayMs);

  module_process_thread_.RegisterModule(vcm_);
  module_process_thread_.RegisterModule(&vie_sync_);

#ifdef VIDEOCODEC_VP8
  if (!disable_default_encoder_) {
    VideoCodec video_codec;
    if (vcm_->Codec(kVideoCodecVP8, &video_codec) == VCM_OK) {
      rtp_rtcp_->RegisterSendPayload(video_codec);
      // TODO(holmer): Can we call SetReceiveCodec() here instead?
      if (!vie_receiver_.RegisterPayload(video_codec)) {
        return -1;
      }
      vcm_->RegisterReceiveCodec(&video_codec, number_of_cores_);
      vcm_->RegisterSendCodec(&video_codec, number_of_cores_,
                              rtp_rtcp_->MaxDataPayloadLength());
    } else {
      assert(false);
    }
  }
#endif

  return 0;
}

ViEChannel::~ViEChannel() {
  UpdateHistograms();
  // Make sure we don't get more callbacks from the RTP module.
  module_process_thread_.DeRegisterModule(vie_receiver_.GetReceiveStatistics());
  module_process_thread_.DeRegisterModule(rtp_rtcp_.get());
  module_process_thread_.DeRegisterModule(vcm_);
  module_process_thread_.DeRegisterModule(&vie_sync_);
  send_payload_router_->SetSendingRtpModules(std::list<RtpRtcp*>());
  packet_router_->RemoveRtpModule(rtp_rtcp_.get());
  while (simulcast_rtp_rtcp_.size() > 0) {
    std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
    RtpRtcp* rtp_rtcp = *it;
    packet_router_->RemoveRtpModule(rtp_rtcp);
    module_process_thread_.DeRegisterModule(rtp_rtcp);
    delete rtp_rtcp;
    simulcast_rtp_rtcp_.erase(it);
  }
  while (removed_rtp_rtcp_.size() > 0) {
    std::list<RtpRtcp*>::iterator it = removed_rtp_rtcp_.begin();
    delete *it;
    removed_rtp_rtcp_.erase(it);
  }
  if (decode_thread_) {
    StopDecodeThread();
  }
  // Release modules.
  VideoCodingModule::Destroy(vcm_);
}

void ViEChannel::UpdateHistograms() {
  int64_t now = Clock::GetRealTimeClock()->TimeInMilliseconds();

  if (sender_) {
    RtcpPacketTypeCounter rtcp_counter;
    GetSendRtcpPacketTypeCounter(&rtcp_counter);
    int64_t elapsed_sec = rtcp_counter.TimeSinceFirstPacketInMs(now) / 1000;
    if (elapsed_sec > metrics::kMinRunTimeInSeconds) {
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.NackPacketsReceivedPerMinute",
                                 rtcp_counter.nack_packets * 60 / elapsed_sec);
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.FirPacketsReceivedPerMinute",
                                 rtcp_counter.fir_packets * 60 / elapsed_sec);
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.PliPacketsReceivedPerMinute",
                                 rtcp_counter.pli_packets * 60 / elapsed_sec);
      if (rtcp_counter.nack_requests > 0) {
        RTC_HISTOGRAM_PERCENTAGE(
            "WebRTC.Video.UniqueNackRequestsReceivedInPercent",
            rtcp_counter.UniqueNackRequestsInPercent());
      }
      int fraction_lost = report_block_stats_sender_->FractionLostInPercent();
      if (fraction_lost != -1) {
        RTC_HISTOGRAM_PERCENTAGE("WebRTC.Video.SentPacketsLostInPercent",
                                 fraction_lost);
      }
    }

    StreamDataCounters rtp;
    StreamDataCounters rtx;
    GetSendStreamDataCounters(&rtp, &rtx);
    StreamDataCounters rtp_rtx = rtp;
    rtp_rtx.Add(rtx);
    elapsed_sec = rtp_rtx.TimeSinceFirstPacketInMs(
                      Clock::GetRealTimeClock()->TimeInMilliseconds()) /
                  1000;
    if (elapsed_sec > metrics::kMinRunTimeInSeconds) {
      RTC_HISTOGRAM_COUNTS_100000(
          "WebRTC.Video.BitrateSentInKbps",
          static_cast<int>(rtp_rtx.transmitted.TotalBytes() * 8 / elapsed_sec /
                           1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.MediaBitrateSentInKbps",
          static_cast<int>(rtp.MediaPayloadBytes() * 8 / elapsed_sec / 1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.PaddingBitrateSentInKbps",
          static_cast<int>(rtp_rtx.transmitted.padding_bytes * 8 / elapsed_sec /
                           1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.RetransmittedBitrateSentInKbps",
          static_cast<int>(rtp_rtx.retransmitted.TotalBytes() * 8 /
                           elapsed_sec / 1000));
      if (rtp_rtcp_->RtxSendStatus() != kRtxOff) {
        RTC_HISTOGRAM_COUNTS_10000(
            "WebRTC.Video.RtxBitrateSentInKbps",
            static_cast<int>(rtx.transmitted.TotalBytes() * 8 / elapsed_sec /
                             1000));
      }
      bool fec_enabled = false;
      uint8_t pltype_red;
      uint8_t pltype_fec;
      rtp_rtcp_->GenericFECStatus(fec_enabled, pltype_red, pltype_fec);
      if (fec_enabled) {
        RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.FecBitrateSentInKbps",
                                   static_cast<int>(rtp_rtx.fec.TotalBytes() *
                                                    8 / elapsed_sec / 1000));
      }
    }
  } else if (vie_receiver_.GetRemoteSsrc() > 0) {
    // Get receive stats if we are receiving packets, i.e. there is a remote
    // ssrc.
    RtcpPacketTypeCounter rtcp_counter;
    GetReceiveRtcpPacketTypeCounter(&rtcp_counter);
    int64_t elapsed_sec = rtcp_counter.TimeSinceFirstPacketInMs(now) / 1000;
    if (elapsed_sec > metrics::kMinRunTimeInSeconds) {
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.NackPacketsSentPerMinute",
          rtcp_counter.nack_packets * 60 / elapsed_sec);
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.FirPacketsSentPerMinute",
          rtcp_counter.fir_packets * 60 / elapsed_sec);
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.PliPacketsSentPerMinute",
          rtcp_counter.pli_packets * 60 / elapsed_sec);
      if (rtcp_counter.nack_requests > 0) {
        RTC_HISTOGRAM_PERCENTAGE("WebRTC.Video.UniqueNackRequestsSentInPercent",
            rtcp_counter.UniqueNackRequestsInPercent());
      }
    }

    StreamDataCounters rtp;
    StreamDataCounters rtx;
    GetReceiveStreamDataCounters(&rtp, &rtx);
    StreamDataCounters rtp_rtx = rtp;
    rtp_rtx.Add(rtx);
    elapsed_sec = rtp_rtx.TimeSinceFirstPacketInMs(now) / 1000;
    if (elapsed_sec > metrics::kMinRunTimeInSeconds) {
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.BitrateReceivedInKbps",
          static_cast<int>(rtp_rtx.transmitted.TotalBytes() * 8 / elapsed_sec /
                           1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.MediaBitrateReceivedInKbps",
          static_cast<int>(rtp.MediaPayloadBytes() * 8 / elapsed_sec / 1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.PaddingBitrateReceivedInKbps",
          static_cast<int>(rtp_rtx.transmitted.padding_bytes * 8 / elapsed_sec /
                           1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.RetransmittedBitrateReceivedInKbps",
          static_cast<int>(rtp_rtx.retransmitted.TotalBytes() * 8 /
                           elapsed_sec / 1000));
      uint32_t ssrc = 0;
      if (vie_receiver_.GetRtxSsrc(&ssrc)) {
        RTC_HISTOGRAM_COUNTS_10000(
            "WebRTC.Video.RtxBitrateReceivedInKbps",
            static_cast<int>(rtx.transmitted.TotalBytes() * 8 / elapsed_sec /
                             1000));
      }
      if (vie_receiver_.IsFecEnabled()) {
        RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.FecBitrateReceivedInKbps",
                                   static_cast<int>(rtp_rtx.fec.TotalBytes() *
                                                    8 / elapsed_sec / 1000));
      }
    }
  }
}

int32_t ViEChannel::SetSendCodec(const VideoCodec& video_codec,
                                 bool new_stream) {
  DCHECK(sender_);
  if (video_codec.codecType == kVideoCodecRED ||
      video_codec.codecType == kVideoCodecULPFEC) {
    LOG_F(LS_ERROR) << "Not a valid send codec " << video_codec.codecType;
    return -1;
  }
  if (kMaxSimulcastStreams < video_codec.numberOfSimulcastStreams) {
    LOG_F(LS_ERROR) << "Incorrect config "
                    << video_codec.numberOfSimulcastStreams;
    return -1;
  }
  // Update the RTP module with the settings.
  // Stop and Start the RTP module -> trigger new SSRC, if an SSRC hasn't been
  // set explicitly.
  bool restart_rtp = false;
  bool router_was_active = send_payload_router_->active();
  send_payload_router_->set_active(false);
  send_payload_router_->SetSendingRtpModules(std::list<RtpRtcp*>());
  packet_router_->RemoveRtpModule(rtp_rtcp_.get());
  for (RtpRtcp* module : simulcast_rtp_rtcp_)
    packet_router_->RemoveRtpModule(module);
  if (rtp_rtcp_->Sending() && new_stream) {
    restart_rtp = true;
    rtp_rtcp_->SetSendingStatus(false);
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end(); ++it) {
      (*it)->SetSendingStatus(false);
      (*it)->SetSendingMediaStatus(false);
    }
  }

  bool fec_enabled = false;
  uint8_t payload_type_red;
  uint8_t payload_type_fec;
  rtp_rtcp_->GenericFECStatus(fec_enabled, payload_type_red, payload_type_fec);

  std::vector<RtpRtcp*> registered_modules;
  std::vector<RtpRtcp*> deregistered_modules;
  {
    CriticalSectionScoped cs(rtp_rtcp_cs_.get());

    if (video_codec.numberOfSimulcastStreams > 0) {
      // Set correct bitrate to base layer.
      // Create our simulcast RTP modules.
      int num_modules_to_add = video_codec.numberOfSimulcastStreams -
                               static_cast<int>(simulcast_rtp_rtcp_.size()) - 1;
      if (num_modules_to_add < 0) {
        num_modules_to_add = 0;
      }

      // Add back removed rtp modules. Order is important (allocate from front
      // of removed modules) to preserve RTP settings such as SSRCs for
      // simulcast streams.
      std::list<RtpRtcp*> new_rtp_modules;
      for (; removed_rtp_rtcp_.size() > 0 && num_modules_to_add > 0;
           --num_modules_to_add) {
        new_rtp_modules.push_back(removed_rtp_rtcp_.front());
        removed_rtp_rtcp_.pop_front();
      }

      for (int i = 0; i < num_modules_to_add; ++i)
        new_rtp_modules.push_back(CreateRtpRtcpModule());

      // Initialize newly added modules.
      for (std::list<RtpRtcp*>::iterator it = new_rtp_modules.begin();
           it != new_rtp_modules.end(); ++it) {
        RtpRtcp* rtp_rtcp = *it;

        rtp_rtcp->SetRTCPStatus(rtp_rtcp_->RTCP());

        if (rtp_rtcp_->StorePackets()) {
          rtp_rtcp->SetStorePacketsStatus(true, nack_history_size_sender_);
        } else if (paced_sender_) {
          rtp_rtcp->SetStorePacketsStatus(true, nack_history_size_sender_);
        }

        if (fec_enabled) {
          rtp_rtcp->SetGenericFECStatus(fec_enabled, payload_type_red,
                                        payload_type_fec);
        }
        rtp_rtcp->SetSendingStatus(rtp_rtcp_->Sending());
        rtp_rtcp->SetSendingMediaStatus(rtp_rtcp_->SendingMedia());
        std::pair<int, int> payload_type_value =
            rtp_rtcp_->RtxSendPayloadType();
        rtp_rtcp->SetRtxSendPayloadType(payload_type_value.first,
                                        payload_type_value.second);
        rtp_rtcp->SetRtxSendStatus(rtp_rtcp_->RtxSendStatus());
        simulcast_rtp_rtcp_.push_back(rtp_rtcp);

        // Silently ignore error.
        registered_modules.push_back(rtp_rtcp);
      }

      // Remove last in list if we have too many.
      for (size_t j = simulcast_rtp_rtcp_.size();
           j > static_cast<size_t>(video_codec.numberOfSimulcastStreams - 1);
           j--) {
        RtpRtcp* rtp_rtcp = simulcast_rtp_rtcp_.back();
        deregistered_modules.push_back(rtp_rtcp);
        rtp_rtcp->SetSendingStatus(false);
        rtp_rtcp->SetSendingMediaStatus(false);
        rtp_rtcp->RegisterRtcpStatisticsCallback(NULL);
        rtp_rtcp->RegisterSendChannelRtpStatisticsCallback(NULL);
        simulcast_rtp_rtcp_.pop_back();
        removed_rtp_rtcp_.push_front(rtp_rtcp);
      }
      uint8_t idx = 0;
      // Configure all simulcast modules.
      for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
           it != simulcast_rtp_rtcp_.end(); it++) {
        idx++;
        RtpRtcp* rtp_rtcp = *it;
        rtp_rtcp->DeRegisterSendPayload(video_codec.plType);
        if (rtp_rtcp->RegisterSendPayload(video_codec) != 0) {
          return -1;
        }
        if (mtu_ != 0) {
          rtp_rtcp->SetMaxTransferUnit(mtu_);
        }
        if (restart_rtp) {
          rtp_rtcp->SetSendingStatus(true);
          rtp_rtcp->SetSendingMediaStatus(true);
        }
        if (send_timestamp_extension_id_ != kInvalidRtpExtensionId) {
          // Deregister in case the extension was previously enabled.
          rtp_rtcp->DeregisterSendRtpHeaderExtension(
              kRtpExtensionTransmissionTimeOffset);
          if (rtp_rtcp->RegisterSendRtpHeaderExtension(
                  kRtpExtensionTransmissionTimeOffset,
                  send_timestamp_extension_id_) != 0) {
            LOG(LS_WARNING) << "Register Transmission Time Offset failed";
          }
        } else {
          rtp_rtcp->DeregisterSendRtpHeaderExtension(
              kRtpExtensionTransmissionTimeOffset);
        }
        if (absolute_send_time_extension_id_ != kInvalidRtpExtensionId) {
          // Deregister in case the extension was previously enabled.
          rtp_rtcp->DeregisterSendRtpHeaderExtension(
              kRtpExtensionAbsoluteSendTime);
          if (rtp_rtcp->RegisterSendRtpHeaderExtension(
                  kRtpExtensionAbsoluteSendTime,
                  absolute_send_time_extension_id_) != 0) {
            LOG(LS_WARNING) << "Register Absolute Send Time failed";
          }
        } else {
          rtp_rtcp->DeregisterSendRtpHeaderExtension(
              kRtpExtensionAbsoluteSendTime);
        }
        if (video_rotation_extension_id_ != kInvalidRtpExtensionId) {
          // Deregister in case the extension was previously enabled.
          rtp_rtcp->DeregisterSendRtpHeaderExtension(
              kRtpExtensionVideoRotation);
          if (rtp_rtcp->RegisterSendRtpHeaderExtension(
                  kRtpExtensionVideoRotation, video_rotation_extension_id_) !=
              0) {
            LOG(LS_WARNING) << "Register VideoRotation extension failed";
          }
        } else {
          rtp_rtcp->DeregisterSendRtpHeaderExtension(
              kRtpExtensionVideoRotation);
        }
        rtp_rtcp->RegisterRtcpStatisticsCallback(
            rtp_rtcp_->GetRtcpStatisticsCallback());
        rtp_rtcp->RegisterSendChannelRtpStatisticsCallback(
            rtp_rtcp_->GetSendChannelRtpStatisticsCallback());
      }
      // |RegisterSimulcastRtpRtcpModules| resets all old weak pointers and old
      // modules can be deleted after this step.
      vie_receiver_.RegisterSimulcastRtpRtcpModules(simulcast_rtp_rtcp_);
    } else {
      while (!simulcast_rtp_rtcp_.empty()) {
        RtpRtcp* rtp_rtcp = simulcast_rtp_rtcp_.back();
        deregistered_modules.push_back(rtp_rtcp);
        rtp_rtcp->SetSendingStatus(false);
        rtp_rtcp->SetSendingMediaStatus(false);
        rtp_rtcp->RegisterRtcpStatisticsCallback(NULL);
        rtp_rtcp->RegisterSendChannelRtpStatisticsCallback(NULL);
        simulcast_rtp_rtcp_.pop_back();
        removed_rtp_rtcp_.push_front(rtp_rtcp);
      }
      // Clear any previous modules.
      vie_receiver_.RegisterSimulcastRtpRtcpModules(simulcast_rtp_rtcp_);
    }

    // Don't log this error, no way to check in advance if this pl_type is
    // registered or not...
    rtp_rtcp_->DeRegisterSendPayload(video_codec.plType);
    if (rtp_rtcp_->RegisterSendPayload(video_codec) != 0) {
      return -1;
    }
    if (restart_rtp) {
      rtp_rtcp_->SetSendingStatus(true);
      for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
           it != simulcast_rtp_rtcp_.end(); ++it) {
        (*it)->SetSendingStatus(true);
        (*it)->SetSendingMediaStatus(true);
      }
    }
    // Update the packet and payload routers with the sending RTP RTCP modules.
    packet_router_->AddRtpModule(rtp_rtcp_.get());
    for (RtpRtcp* module : simulcast_rtp_rtcp_)
      packet_router_->AddRtpModule(module);

    std::list<RtpRtcp*> active_send_modules;
    active_send_modules.push_back(rtp_rtcp_.get());
    for (std::list<RtpRtcp*>::const_iterator cit = simulcast_rtp_rtcp_.begin();
         cit != simulcast_rtp_rtcp_.end(); ++cit) {
      active_send_modules.push_back(*cit);
    }
    send_payload_router_->SetSendingRtpModules(active_send_modules);
    if (router_was_active)
      send_payload_router_->set_active(true);
  }
  for (RtpRtcp* rtp_rtcp : registered_modules)
    module_process_thread_.RegisterModule(rtp_rtcp);
  for (RtpRtcp* rtp_rtcp : deregistered_modules)
    module_process_thread_.DeRegisterModule(rtp_rtcp);
  return 0;
}

int32_t ViEChannel::SetReceiveCodec(const VideoCodec& video_codec) {
  DCHECK(!sender_);
  if (!vie_receiver_.SetReceiveCodec(video_codec)) {
    return -1;
  }

  if (video_codec.codecType != kVideoCodecRED &&
      video_codec.codecType != kVideoCodecULPFEC) {
    // Register codec type with VCM, but do not register RED or ULPFEC.
    if (vcm_->RegisterReceiveCodec(&video_codec, number_of_cores_,
                                  wait_for_key_frame_) != VCM_OK) {
      return -1;
    }
  }
  return 0;
}

int32_t ViEChannel::RegisterCodecObserver(ViEDecoderObserver* observer) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (observer) {
    if (codec_observer_) {
      LOG_F(LS_ERROR) << "Observer already registered.";
      return -1;
    }
    codec_observer_ = observer;
  } else {
    codec_observer_ = NULL;
  }
  return 0;
}

int32_t ViEChannel::RegisterExternalDecoder(const uint8_t pl_type,
                                            VideoDecoder* decoder,
                                            bool buffered_rendering,
                                            int32_t render_delay) {
  DCHECK(!sender_);
  int32_t result;
  result = vcm_->RegisterExternalDecoder(decoder, pl_type, buffered_rendering);
  if (result != VCM_OK) {
    return result;
  }
  return vcm_->SetRenderDelay(render_delay);
}

int32_t ViEChannel::DeRegisterExternalDecoder(const uint8_t pl_type) {
  DCHECK(!sender_);
  VideoCodec current_receive_codec;
  int32_t result = 0;
  result = vcm_->ReceiveCodec(&current_receive_codec);
  if (vcm_->RegisterExternalDecoder(NULL, pl_type, false) != VCM_OK) {
    return -1;
  }

  if (result == 0 && current_receive_codec.plType == pl_type) {
    result = vcm_->RegisterReceiveCodec(
        &current_receive_codec, number_of_cores_, wait_for_key_frame_);
  }
  return result;
}

int32_t ViEChannel::ReceiveCodecStatistics(uint32_t* num_key_frames,
                                           uint32_t* num_delta_frames) {
  CriticalSectionScoped cs(callback_cs_.get());
  *num_key_frames = receive_frame_counts_.key_frames;
  *num_delta_frames = receive_frame_counts_.delta_frames;
  return 0;
}

uint32_t ViEChannel::DiscardedPackets() const {
  return vcm_->DiscardedPackets();
}

int ViEChannel::ReceiveDelay() const {
  return vcm_->Delay();
}

int32_t ViEChannel::WaitForKeyFrame(bool wait) {
  wait_for_key_frame_ = wait;
  return 0;
}

int32_t ViEChannel::SetSignalPacketLossStatus(bool enable,
                                              bool only_key_frames) {
  if (enable) {
    if (only_key_frames) {
      vcm_->SetVideoProtection(kProtectionKeyOnLoss, false);
      if (vcm_->SetVideoProtection(kProtectionKeyOnKeyLoss, true) != VCM_OK) {
        return -1;
      }
    } else {
      vcm_->SetVideoProtection(kProtectionKeyOnKeyLoss, false);
      if (vcm_->SetVideoProtection(kProtectionKeyOnLoss, true) != VCM_OK) {
        return -1;
      }
    }
  } else {
    vcm_->SetVideoProtection(kProtectionKeyOnLoss, false);
    vcm_->SetVideoProtection(kProtectionKeyOnKeyLoss, false);
  }
  return 0;
}

void ViEChannel::SetRTCPMode(const RTCPMethod rtcp_mode) {
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetRTCPStatus(rtcp_mode);
  }
  rtp_rtcp_->SetRTCPStatus(rtcp_mode);
}

int32_t ViEChannel::SetNACKStatus(const bool enable) {
  // Update the decoding VCM.
  if (vcm_->SetVideoProtection(kProtectionNack, enable) != VCM_OK) {
    return -1;
  }
  if (enable) {
    // Disable possible FEC.
    SetFECStatus(false, 0, 0);
  }
  // Update the decoding VCM.
  if (vcm_->SetVideoProtection(kProtectionNack, enable) != VCM_OK) {
    return -1;
  }
  return ProcessNACKRequest(enable);
}

int32_t ViEChannel::ProcessNACKRequest(const bool enable) {
  if (enable) {
    // Turn on NACK.
    if (rtp_rtcp_->RTCP() == kRtcpOff) {
      return -1;
    }
    vie_receiver_.SetNackStatus(true, max_nack_reordering_threshold_);
    rtp_rtcp_->SetStorePacketsStatus(true, nack_history_size_sender_);
    vcm_->RegisterPacketRequestCallback(this);

    CriticalSectionScoped cs(rtp_rtcp_cs_.get());

    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end();
         it++) {
      RtpRtcp* rtp_rtcp = *it;
      rtp_rtcp->SetStorePacketsStatus(true, nack_history_size_sender_);
    }
    // Don't introduce errors when NACK is enabled.
    vcm_->SetDecodeErrorMode(kNoErrors);
  } else {
    CriticalSectionScoped cs(rtp_rtcp_cs_.get());
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end();
         it++) {
      RtpRtcp* rtp_rtcp = *it;
      if (paced_sender_ == NULL) {
        rtp_rtcp->SetStorePacketsStatus(false, 0);
      }
    }
    vcm_->RegisterPacketRequestCallback(NULL);
    if (paced_sender_ == NULL) {
      rtp_rtcp_->SetStorePacketsStatus(false, 0);
    }
    vie_receiver_.SetNackStatus(false, max_nack_reordering_threshold_);
    // When NACK is off, allow decoding with errors. Otherwise, the video
    // will freeze, and will only recover with a complete key frame.
    vcm_->SetDecodeErrorMode(kWithErrors);
  }
  return 0;
}

int32_t ViEChannel::SetFECStatus(const bool enable,
                                       const unsigned char payload_typeRED,
                                       const unsigned char payload_typeFEC) {
  // Disable possible NACK.
  if (enable) {
    SetNACKStatus(false);
  }

  return ProcessFECRequest(enable, payload_typeRED, payload_typeFEC);
}

bool ViEChannel::IsSendingFecEnabled() {
  bool fec_enabled = false;
  uint8_t pltype_red = 0;
  uint8_t pltype_fec = 0;
  rtp_rtcp_->GenericFECStatus(fec_enabled, pltype_red, pltype_fec);
  if (fec_enabled)
    return true;

  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (auto* module : simulcast_rtp_rtcp_) {
    module->GenericFECStatus(fec_enabled, pltype_red, pltype_fec);
    if (fec_enabled)
      return true;
  }
  return false;
}

int32_t ViEChannel::ProcessFECRequest(
    const bool enable,
    const unsigned char payload_typeRED,
    const unsigned char payload_typeFEC) {
  if (rtp_rtcp_->SetGenericFECStatus(enable, payload_typeRED,
                                    payload_typeFEC) != 0) {
    return -1;
  }
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetGenericFECStatus(enable, payload_typeRED, payload_typeFEC);
  }
  return 0;
}

int32_t ViEChannel::SetHybridNACKFECStatus(
    const bool enable,
    const unsigned char payload_typeRED,
    const unsigned char payload_typeFEC) {
  if (vcm_->SetVideoProtection(kProtectionNackFEC, enable) != VCM_OK) {
    return -1;
  }

  int32_t ret_val = 0;
  ret_val = ProcessNACKRequest(enable);
  if (ret_val < 0) {
    return ret_val;
  }
  return ProcessFECRequest(enable, payload_typeRED, payload_typeFEC);
}

int ViEChannel::SetSenderBufferingMode(int target_delay_ms) {
  if ((target_delay_ms < 0) || (target_delay_ms > kMaxTargetDelayMs)) {
    LOG(LS_ERROR) << "Invalid send buffer value.";
    return -1;
  }
  if (target_delay_ms == 0) {
    // Real-time mode.
    nack_history_size_sender_ = kSendSidePacketHistorySize;
  } else {
    nack_history_size_sender_ = GetRequiredNackListSize(target_delay_ms);
    // Don't allow a number lower than the default value.
    if (nack_history_size_sender_ < kSendSidePacketHistorySize) {
      nack_history_size_sender_ = kSendSidePacketHistorySize;
    }
  }
  rtp_rtcp_->SetStorePacketsStatus(true, nack_history_size_sender_);
  return 0;
}

int ViEChannel::SetReceiverBufferingMode(int target_delay_ms) {
  if ((target_delay_ms < 0) || (target_delay_ms > kMaxTargetDelayMs)) {
    LOG(LS_ERROR) << "Invalid receive buffer delay value.";
    return -1;
  }
  int max_nack_list_size;
  int max_incomplete_time_ms;
  if (target_delay_ms == 0) {
    // Real-time mode - restore default settings.
    max_nack_reordering_threshold_ = kMaxPacketAgeToNack;
    max_nack_list_size = kMaxNackListSize;
    max_incomplete_time_ms = 0;
  } else {
    max_nack_list_size =  3 * GetRequiredNackListSize(target_delay_ms) / 4;
    max_nack_reordering_threshold_ = max_nack_list_size;
    // Calculate the max incomplete time and round to int.
    max_incomplete_time_ms = static_cast<int>(kMaxIncompleteTimeMultiplier *
        target_delay_ms + 0.5f);
  }
  vcm_->SetNackSettings(max_nack_list_size, max_nack_reordering_threshold_,
                       max_incomplete_time_ms);
  vcm_->SetMinReceiverDelay(target_delay_ms);
  if (vie_sync_.SetTargetBufferingDelay(target_delay_ms) < 0)
    return -1;
  return 0;
}

int ViEChannel::GetRequiredNackListSize(int target_delay_ms) {
  // The max size of the nack list should be large enough to accommodate the
  // the number of packets (frames) resulting from the increased delay.
  // Roughly estimating for ~40 packets per frame @ 30fps.
  return target_delay_ms * 40 * 30 / 1000;
}

int32_t ViEChannel::SetKeyFrameRequestMethod(
    const KeyFrameRequestMethod method) {
  return rtp_rtcp_->SetKeyFrameRequestMethod(method);
}

void ViEChannel::EnableRemb(bool enable) {
  rtp_rtcp_->SetREMBStatus(enable);
}

int ViEChannel::SetSendTimestampOffsetStatus(bool enable, int id) {
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  int error = 0;
  if (enable) {
    // Enable the extension, but disable possible old id to avoid errors.
    send_timestamp_extension_id_ = id;
    rtp_rtcp_->DeregisterSendRtpHeaderExtension(
        kRtpExtensionTransmissionTimeOffset);
    error = rtp_rtcp_->RegisterSendRtpHeaderExtension(
        kRtpExtensionTransmissionTimeOffset, id);
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end(); it++) {
      (*it)->DeregisterSendRtpHeaderExtension(
          kRtpExtensionTransmissionTimeOffset);
      error |= (*it)->RegisterSendRtpHeaderExtension(
          kRtpExtensionTransmissionTimeOffset, id);
    }
  } else {
    // Disable the extension.
    send_timestamp_extension_id_ = kInvalidRtpExtensionId;
    rtp_rtcp_->DeregisterSendRtpHeaderExtension(
        kRtpExtensionTransmissionTimeOffset);
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end(); it++) {
      (*it)->DeregisterSendRtpHeaderExtension(
          kRtpExtensionTransmissionTimeOffset);
    }
  }
  return error;
}

int ViEChannel::SetReceiveTimestampOffsetStatus(bool enable, int id) {
  return vie_receiver_.SetReceiveTimestampOffsetStatus(enable, id) ? 0 : -1;
}

int ViEChannel::SetSendAbsoluteSendTimeStatus(bool enable, int id) {
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  int error = 0;
  if (enable) {
    // Enable the extension, but disable possible old id to avoid errors.
    absolute_send_time_extension_id_ = id;
    rtp_rtcp_->DeregisterSendRtpHeaderExtension(
        kRtpExtensionAbsoluteSendTime);
    error = rtp_rtcp_->RegisterSendRtpHeaderExtension(
        kRtpExtensionAbsoluteSendTime, id);
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end(); it++) {
      (*it)->DeregisterSendRtpHeaderExtension(
          kRtpExtensionAbsoluteSendTime);
      error |= (*it)->RegisterSendRtpHeaderExtension(
          kRtpExtensionAbsoluteSendTime, id);
    }
  } else {
    // Disable the extension.
    absolute_send_time_extension_id_ = kInvalidRtpExtensionId;
    rtp_rtcp_->DeregisterSendRtpHeaderExtension(
        kRtpExtensionAbsoluteSendTime);
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end(); it++) {
      (*it)->DeregisterSendRtpHeaderExtension(
          kRtpExtensionAbsoluteSendTime);
    }
  }
  return error;
}

int ViEChannel::SetReceiveAbsoluteSendTimeStatus(bool enable, int id) {
  return vie_receiver_.SetReceiveAbsoluteSendTimeStatus(enable, id) ? 0 : -1;
}

int ViEChannel::SetSendVideoRotationStatus(bool enable, int id) {
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  int error = 0;
  if (enable) {
    // Enable the extension, but disable possible old id to avoid errors.
    video_rotation_extension_id_ = id;
    rtp_rtcp_->DeregisterSendRtpHeaderExtension(kRtpExtensionVideoRotation);
    error = rtp_rtcp_->RegisterSendRtpHeaderExtension(
        kRtpExtensionVideoRotation, id);
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end(); it++) {
      (*it)->DeregisterSendRtpHeaderExtension(kRtpExtensionVideoRotation);
      error |=
          (*it)->RegisterSendRtpHeaderExtension(kRtpExtensionVideoRotation, id);
    }
  } else {
    // Disable the extension.
    video_rotation_extension_id_ = kInvalidRtpExtensionId;
    rtp_rtcp_->DeregisterSendRtpHeaderExtension(kRtpExtensionVideoRotation);
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end(); it++) {
      (*it)->DeregisterSendRtpHeaderExtension(kRtpExtensionVideoRotation);
    }
  }
  return error;
}

int ViEChannel::SetReceiveVideoRotationStatus(bool enable, int id) {
  return vie_receiver_.SetReceiveVideoRotationStatus(enable, id) ? 0 : -1;
}

void ViEChannel::SetRtcpXrRrtrStatus(bool enable) {
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  rtp_rtcp_->SetRtcpXrRrtrStatus(enable);
}

void ViEChannel::SetTransmissionSmoothingStatus(bool enable) {
  assert(paced_sender_ && "No paced sender registered.");
  paced_sender_->SetStatus(enable);
}

void ViEChannel::EnableTMMBR(bool enable) {
  rtp_rtcp_->SetTMMBRStatus(enable);
}

int32_t ViEChannel::EnableKeyFrameRequestCallback(const bool enable) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (enable && !codec_observer_) {
    LOG(LS_ERROR) << "No ViECodecObserver set.";
    return -1;
  }
  do_key_frame_callbackRequest_ = enable;
  return 0;
}

int32_t ViEChannel::SetSSRC(const uint32_t SSRC,
                            const StreamType usage,
                            const uint8_t simulcast_idx) {
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  ReserveRtpRtcpModules(simulcast_idx + 1);
  RtpRtcp* rtp_rtcp = GetRtpRtcpModule(simulcast_idx);
  if (rtp_rtcp == NULL)
    return -1;
  if (usage == kViEStreamTypeRtx) {
    rtp_rtcp->SetRtxSsrc(SSRC);
  } else {
    rtp_rtcp->SetSSRC(SSRC);
  }
  return 0;
}

int32_t ViEChannel::SetRemoteSSRCType(const StreamType usage,
                                      const uint32_t SSRC) {
  vie_receiver_.SetRtxSsrc(SSRC);
  return 0;
}

int32_t ViEChannel::GetLocalSSRC(uint8_t idx, unsigned int* ssrc) {
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  RtpRtcp* rtp_rtcp = GetRtpRtcpModule(idx);
  if (rtp_rtcp == NULL)
    return -1;
  *ssrc = rtp_rtcp->SSRC();
  return 0;
}

int32_t ViEChannel::GetRemoteSSRC(uint32_t* ssrc) {
  *ssrc = vie_receiver_.GetRemoteSsrc();
  return 0;
}

int ViEChannel::SetRtxSendPayloadType(int payload_type,
                                      int associated_payload_type) {
  rtp_rtcp_->SetRtxSendPayloadType(payload_type, associated_payload_type);
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end(); it++) {
    (*it)->SetRtxSendPayloadType(payload_type, associated_payload_type);
  }
  SetRtxSendStatus(true);
  return 0;
}

void ViEChannel::SetRtxSendStatus(bool enable) {
  int rtx_settings =
      enable ? kRtxRetransmitted | kRtxRedundantPayloads : kRtxOff;
  rtp_rtcp_->SetRtxSendStatus(rtx_settings);
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end(); it++) {
    (*it)->SetRtxSendStatus(rtx_settings);
  }
}

void ViEChannel::SetRtxReceivePayloadType(int payload_type,
                                          int associated_payload_type) {
  vie_receiver_.SetRtxPayloadType(payload_type, associated_payload_type);
}

int32_t ViEChannel::SetStartSequenceNumber(uint16_t sequence_number) {
  if (rtp_rtcp_->Sending()) {
    return -1;
  }
  rtp_rtcp_->SetSequenceNumber(sequence_number);
  return 0;
}

void ViEChannel::SetRtpStateForSsrc(uint32_t ssrc, const RtpState& rtp_state) {
  assert(!rtp_rtcp_->Sending());
  if (rtp_rtcp_->SetRtpStateForSsrc(ssrc, rtp_state))
    return;
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (auto* module : simulcast_rtp_rtcp_) {
    if (module->SetRtpStateForSsrc(ssrc, rtp_state))
      return;
  }
  for (auto* module : removed_rtp_rtcp_) {
    if (module->SetRtpStateForSsrc(ssrc, rtp_state))
      return;
  }
}

RtpState ViEChannel::GetRtpStateForSsrc(uint32_t ssrc) {
  assert(!rtp_rtcp_->Sending());

  RtpState rtp_state;
  if (rtp_rtcp_->GetRtpStateForSsrc(ssrc, &rtp_state))
    return rtp_state;

  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (auto* module : simulcast_rtp_rtcp_) {
    if (module->GetRtpStateForSsrc(ssrc, &rtp_state))
      return rtp_state;
  }
  for (auto* module : removed_rtp_rtcp_) {
    if (module->GetRtpStateForSsrc(ssrc, &rtp_state))
      return rtp_state;
  }
  LOG(LS_ERROR) << "Couldn't get RTP state for ssrc: " << ssrc;
  return rtp_state;
}

int32_t ViEChannel::SetRTCPCName(const char* rtcp_cname) {
  if (rtp_rtcp_->Sending()) {
    return -1;
  }
  return rtp_rtcp_->SetCNAME(rtcp_cname);
}

int32_t ViEChannel::GetRemoteRTCPCName(char rtcp_cname[]) {
  uint32_t remoteSSRC = vie_receiver_.GetRemoteSsrc();
  return rtp_rtcp_->RemoteCNAME(remoteSSRC, rtcp_cname);
}

int32_t ViEChannel::SendApplicationDefinedRTCPPacket(
    const uint8_t sub_type,
    uint32_t name,
    const uint8_t* data,
    uint16_t data_length_in_bytes) {
  if (!rtp_rtcp_->Sending()) {
    return -1;
  }
  if (!data) {
    LOG_F(LS_ERROR) << "Invalid input.";
    return -1;
  }
  if (data_length_in_bytes % 4 != 0) {
    LOG(LS_ERROR) << "Invalid input length.";
    return -1;
  }
  RTCPMethod rtcp_method = rtp_rtcp_->RTCP();
  if (rtcp_method == kRtcpOff) {
    LOG_F(LS_ERROR) << "RTCP not enable.";
    return -1;
  }
  // Create and send packet.
  if (rtp_rtcp_->SetRTCPApplicationSpecificData(sub_type, name, data,
                                               data_length_in_bytes) != 0) {
    return -1;
  }
  return 0;
}

int32_t ViEChannel::GetSendRtcpStatistics(uint16_t* fraction_lost,
                                          uint32_t* cumulative_lost,
                                          uint32_t* extended_max,
                                          uint32_t* jitter_samples,
                                          int64_t* rtt_ms) {
  // Aggregate the report blocks associated with streams sent on this channel.
  std::vector<RTCPReportBlock> report_blocks;
  rtp_rtcp_->RemoteRTCPStat(&report_blocks);
  {
    CriticalSectionScoped lock(rtp_rtcp_cs_.get());
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
        it != simulcast_rtp_rtcp_.end();
        ++it) {
      (*it)->RemoteRTCPStat(&report_blocks);
    }
  }

  if (report_blocks.empty())
    return -1;

  uint32_t remote_ssrc = vie_receiver_.GetRemoteSsrc();
  std::vector<RTCPReportBlock>::const_iterator it = report_blocks.begin();
  for (; it != report_blocks.end(); ++it) {
    if (it->remoteSSRC == remote_ssrc)
      break;
  }
  if (it == report_blocks.end()) {
    // We have not received packets with an SSRC matching the report blocks. To
    // have a chance of calculating an RTT we will try with the SSRC of the
    // first report block received.
    // This is very important for send-only channels where we don't know the
    // SSRC of the other end.
    remote_ssrc = report_blocks[0].remoteSSRC;
  }

  // TODO(asapersson): Change report_block_stats to not rely on
  // GetSendRtcpStatistics to be called.
  RTCPReportBlock report =
      report_block_stats_sender_->AggregateAndStore(report_blocks);
  *fraction_lost = report.fractionLost;
  *cumulative_lost = report.cumulativeLost;
  *extended_max = report.extendedHighSeqNum;
  *jitter_samples = report.jitter;

  int64_t dummy;
  int64_t rtt = 0;
  if (rtp_rtcp_->RTT(remote_ssrc, &rtt, &dummy, &dummy, &dummy) != 0) {
    return -1;
  }
  *rtt_ms = rtt;
  return 0;
}

void ViEChannel::RegisterSendChannelRtcpStatisticsCallback(
    RtcpStatisticsCallback* callback) {
  rtp_rtcp_->RegisterRtcpStatisticsCallback(callback);
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       ++it) {
    (*it)->RegisterRtcpStatisticsCallback(callback);
  }
}

void ViEChannel::RegisterReceiveChannelRtcpStatisticsCallback(
    RtcpStatisticsCallback* callback) {
  vie_receiver_.GetReceiveStatistics()->RegisterRtcpStatisticsCallback(
      callback);
  rtp_rtcp_->RegisterRtcpStatisticsCallback(callback);
}

void ViEChannel::RegisterRtcpPacketTypeCounterObserver(
    RtcpPacketTypeCounterObserver* observer) {
  rtcp_packet_type_counter_observer_.Set(observer);
}

void ViEChannel::GetSendStreamDataCounters(
    StreamDataCounters* rtp_counters,
    StreamDataCounters* rtx_counters) const {
  rtp_rtcp_->GetSendStreamDataCounters(rtp_counters, rtx_counters);
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    StreamDataCounters rtp_data;
    StreamDataCounters rtx_data;
    (*it)->GetSendStreamDataCounters(&rtp_data, &rtx_data);
    rtp_counters->Add(rtp_data);
    rtx_counters->Add(rtx_data);
  }
  for (std::list<RtpRtcp*>::const_iterator it = removed_rtp_rtcp_.begin();
       it != removed_rtp_rtcp_.end(); ++it) {
    StreamDataCounters rtp_data;
    StreamDataCounters rtx_data;
    (*it)->GetSendStreamDataCounters(&rtp_data, &rtx_data);
    rtp_counters->Add(rtp_data);
    rtx_counters->Add(rtx_data);
  }
}

void ViEChannel::GetReceiveStreamDataCounters(
    StreamDataCounters* rtp_counters,
    StreamDataCounters* rtx_counters) const {
  StreamStatistician* statistician = vie_receiver_.GetReceiveStatistics()->
      GetStatistician(vie_receiver_.GetRemoteSsrc());
  if (statistician) {
    statistician->GetReceiveStreamDataCounters(rtp_counters);
  }
  uint32_t rtx_ssrc = 0;
  if (vie_receiver_.GetRtxSsrc(&rtx_ssrc)) {
    StreamStatistician* statistician =
        vie_receiver_.GetReceiveStatistics()->GetStatistician(rtx_ssrc);
    if (statistician) {
      statistician->GetReceiveStreamDataCounters(rtx_counters);
    }
  }
}

void ViEChannel::RegisterSendChannelRtpStatisticsCallback(
      StreamDataCountersCallback* callback) {
  rtp_rtcp_->RegisterSendChannelRtpStatisticsCallback(callback);
  {
    CriticalSectionScoped cs(rtp_rtcp_cs_.get());
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end();
         it++) {
      (*it)->RegisterSendChannelRtpStatisticsCallback(callback);
    }
  }
}

void ViEChannel::RegisterReceiveChannelRtpStatisticsCallback(
    StreamDataCountersCallback* callback) {
  vie_receiver_.GetReceiveStatistics()->RegisterRtpStatisticsCallback(callback);
}

void ViEChannel::GetSendRtcpPacketTypeCounter(
    RtcpPacketTypeCounter* packet_counter) const {
  std::map<uint32_t, RtcpPacketTypeCounter> counter_map =
      rtcp_packet_type_counter_observer_.GetPacketTypeCounterMap();

  RtcpPacketTypeCounter counter;
  counter.Add(counter_map[rtp_rtcp_->SSRC()]);

  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end(); ++it) {
    counter.Add(counter_map[(*it)->SSRC()]);
  }
  for (std::list<RtpRtcp*>::const_iterator it = removed_rtp_rtcp_.begin();
       it != removed_rtp_rtcp_.end(); ++it) {
    counter.Add(counter_map[(*it)->SSRC()]);
  }
  *packet_counter = counter;
}

void ViEChannel::GetReceiveRtcpPacketTypeCounter(
    RtcpPacketTypeCounter* packet_counter) const {
  std::map<uint32_t, RtcpPacketTypeCounter> counter_map =
      rtcp_packet_type_counter_observer_.GetPacketTypeCounterMap();

  RtcpPacketTypeCounter counter;
  counter.Add(counter_map[vie_receiver_.GetRemoteSsrc()]);

  *packet_counter = counter;
}

void ViEChannel::RegisterSendSideDelayObserver(
    SendSideDelayObserver* observer) {
  send_side_delay_observer_.Set(observer);
}

void ViEChannel::RegisterSendBitrateObserver(
    BitrateStatisticsObserver* observer) {
  send_bitrate_observer_.Set(observer);
}

int32_t ViEChannel::StartSend() {
  CriticalSectionScoped cs(callback_cs_.get());
  rtp_rtcp_->SetSendingMediaStatus(true);

  if (rtp_rtcp_->Sending()) {
    return -1;
  }
  if (rtp_rtcp_->SetSendingStatus(true) != 0) {
    return -1;
  }
  CriticalSectionScoped cs_rtp(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetSendingMediaStatus(true);
    rtp_rtcp->SetSendingStatus(true);
  }
  send_payload_router_->set_active(true);
  return 0;
}

int32_t ViEChannel::StopSend() {
  send_payload_router_->set_active(false);
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  rtp_rtcp_->SetSendingMediaStatus(false);
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetSendingMediaStatus(false);
  }
  if (!rtp_rtcp_->Sending()) {
    return -1;
  }

  if (rtp_rtcp_->SetSendingStatus(false) != 0) {
    return -1;
  }
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetSendingStatus(false);
  }
  return 0;
}

bool ViEChannel::Sending() {
  return rtp_rtcp_->Sending();
}

void ViEChannel::StartReceive() {
  if (!sender_)
    StartDecodeThread();
  vie_receiver_.StartReceive();
}

void ViEChannel::StopReceive() {
  vie_receiver_.StopReceive();
  if (!sender_) {
    StopDecodeThread();
    vcm_->ResetDecoder();
  }
}

int32_t ViEChannel::ReceivedRTPPacket(const void* rtp_packet,
                                      size_t rtp_packet_length,
                                      const PacketTime& packet_time) {
  return vie_receiver_.ReceivedRTPPacket(
      rtp_packet, rtp_packet_length, packet_time);
}

int32_t ViEChannel::ReceivedRTCPPacket(const void* rtcp_packet,
                                       size_t rtcp_packet_length) {
  return vie_receiver_.ReceivedRTCPPacket(rtcp_packet, rtcp_packet_length);
}

int32_t ViEChannel::SetMTU(uint16_t mtu) {
  if (rtp_rtcp_->SetMaxTransferUnit(mtu) != 0) {
    return -1;
  }
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetMaxTransferUnit(mtu);
  }
  mtu_ = mtu;
  return 0;
}

uint16_t ViEChannel::MaxDataPayloadLength() const {
  return rtp_rtcp_->MaxDataPayloadLength();
}

RtpRtcp* ViEChannel::rtp_rtcp() {
  return rtp_rtcp_.get();
}

rtc::scoped_refptr<PayloadRouter> ViEChannel::send_payload_router() {
  return send_payload_router_;
}

VCMProtectionCallback* ViEChannel::vcm_protection_callback() {
  return vcm_protection_callback_.get();
}

CallStatsObserver* ViEChannel::GetStatsObserver() {
  return stats_observer_.get();
}

// Do not acquire the lock of |vcm_| in this function. Decode callback won't
// necessarily be called from the decoding thread. The decoding thread may have
// held the lock when calling VideoDecoder::Decode, Reset, or Release. Acquiring
// the same lock in the path of decode callback can deadlock.
int32_t ViEChannel::FrameToRender(VideoFrame& video_frame) {  // NOLINT
  CriticalSectionScoped cs(callback_cs_.get());

  if (decoder_reset_) {
    // Trigger a callback to the user if the incoming codec has changed.
    if (codec_observer_) {
      // The codec set by RegisterReceiveCodec might not be the size we're
      // actually decoding.
      receive_codec_.width = static_cast<uint16_t>(video_frame.width());
      receive_codec_.height = static_cast<uint16_t>(video_frame.height());
      codec_observer_->IncomingCodecChanged(channel_id_, receive_codec_);
    }
    decoder_reset_ = false;
  }

  if (pre_render_callback_ != NULL)
    pre_render_callback_->FrameCallback(&video_frame);

  incoming_video_stream_->RenderFrame(channel_id_, video_frame);
  return 0;
}

int32_t ViEChannel::ReceivedDecodedReferenceFrame(
  const uint64_t picture_id) {
  return rtp_rtcp_->SendRTCPReferencePictureSelection(picture_id);
}

void ViEChannel::IncomingCodecChanged(const VideoCodec& codec) {
  CriticalSectionScoped cs(callback_cs_.get());
  receive_codec_ = codec;
}

void ViEChannel::OnReceiveRatesUpdated(uint32_t bit_rate, uint32_t frame_rate) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (codec_observer_)
    codec_observer_->IncomingRate(channel_id_, frame_rate, bit_rate);
}

void ViEChannel::OnDiscardedPacketsUpdated(int discarded_packets) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (vcm_receive_stats_callback_ != NULL)
    vcm_receive_stats_callback_->OnDiscardedPacketsUpdated(discarded_packets);
}

void ViEChannel::OnFrameCountsUpdated(const FrameCounts& frame_counts) {
  CriticalSectionScoped cs(callback_cs_.get());
  receive_frame_counts_ = frame_counts;
  if (vcm_receive_stats_callback_ != NULL)
    vcm_receive_stats_callback_->OnFrameCountsUpdated(frame_counts);
}

void ViEChannel::OnDecoderTiming(int decode_ms,
                                 int max_decode_ms,
                                 int current_delay_ms,
                                 int target_delay_ms,
                                 int jitter_buffer_ms,
                                 int min_playout_delay_ms,
                                 int render_delay_ms) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (!codec_observer_)
    return;
  codec_observer_->DecoderTiming(decode_ms,
                                 max_decode_ms,
                                 current_delay_ms,
                                 target_delay_ms,
                                 jitter_buffer_ms,
                                 min_playout_delay_ms,
                                 render_delay_ms);
}

int32_t ViEChannel::RequestKeyFrame() {
  {
    CriticalSectionScoped cs(callback_cs_.get());
    if (codec_observer_ && do_key_frame_callbackRequest_) {
      codec_observer_->RequestNewKeyFrame(channel_id_);
    }
  }
  return rtp_rtcp_->RequestKeyFrame();
}

int32_t ViEChannel::SliceLossIndicationRequest(
  const uint64_t picture_id) {
  return rtp_rtcp_->SendRTCPSliceLossIndication((uint8_t) picture_id);
}

int32_t ViEChannel::ResendPackets(const uint16_t* sequence_numbers,
                                        uint16_t length) {
  return rtp_rtcp_->SendNACK(sequence_numbers, length);
}

bool ViEChannel::ChannelDecodeThreadFunction(void* obj) {
  return static_cast<ViEChannel*>(obj)->ChannelDecodeProcess();
}

bool ViEChannel::ChannelDecodeProcess() {
  vcm_->Decode(kMaxDecodeWaitTimeMs);
  return true;
}

void ViEChannel::OnRttUpdate(int64_t rtt) {
  vcm_->SetReceiveChannelParameters(rtt);
}

int ViEChannel::ProtectionRequest(const FecProtectionParams* delta_fec_params,
                                  const FecProtectionParams* key_fec_params,
                                  uint32_t* video_rate_bps,
                                  uint32_t* nack_rate_bps,
                                  uint32_t* fec_rate_bps) {
  uint32_t not_used = 0;
  rtp_rtcp_->SetFecParameters(delta_fec_params, key_fec_params);
  rtp_rtcp_->BitrateSent(&not_used, video_rate_bps, fec_rate_bps,
                         nack_rate_bps);
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (auto* module : simulcast_rtp_rtcp_) {
    uint32_t child_video_rate = 0;
    uint32_t child_fec_rate = 0;
    uint32_t child_nack_rate = 0;
    module->SetFecParameters(delta_fec_params, key_fec_params);
    module->BitrateSent(&not_used, &child_video_rate, &child_fec_rate,
                        &child_nack_rate);
    *video_rate_bps += child_video_rate;
    *nack_rate_bps += child_nack_rate;
    *fec_rate_bps += child_fec_rate;
  }
  return 0;
}

void ViEChannel::ReserveRtpRtcpModules(size_t num_modules) {
  for (size_t total_modules =
           1 + simulcast_rtp_rtcp_.size() + removed_rtp_rtcp_.size();
       total_modules < num_modules;
       ++total_modules) {
    RtpRtcp* rtp_rtcp = CreateRtpRtcpModule();
    rtp_rtcp->SetSendingStatus(false);
    rtp_rtcp->SetSendingMediaStatus(false);
    rtp_rtcp->RegisterRtcpStatisticsCallback(NULL);
    rtp_rtcp->RegisterSendChannelRtpStatisticsCallback(NULL);
    removed_rtp_rtcp_.push_back(rtp_rtcp);
  }
}

RtpRtcp* ViEChannel::GetRtpRtcpModule(size_t index) const {
  if (index == 0)
    return rtp_rtcp_.get();
  if (index <= simulcast_rtp_rtcp_.size()) {
    std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
    for (size_t i = 1; i < index; ++i) {
      ++it;
    }
    return *it;
  }

  // If the requested module exists it must be in the removed list. Index
  // translation to this list must remove the default module as well as all
  // active simulcast modules.
  size_t removed_idx = index - simulcast_rtp_rtcp_.size() - 1;
  if (removed_idx >= removed_rtp_rtcp_.size())
    return NULL;

  std::list<RtpRtcp*>::const_iterator it = removed_rtp_rtcp_.begin();
  while (removed_idx-- > 0)
    ++it;

  return *it;
}

RtpRtcp::Configuration ViEChannel::CreateRtpRtcpConfiguration() {
  RtpRtcp::Configuration configuration;
  configuration.id = ViEModuleId(engine_id_, channel_id_);
  configuration.audio = false;
  configuration.receiver_only = !sender_;
  configuration.outgoing_transport = transport_;
  if (sender_)
    configuration.intra_frame_callback = intra_frame_observer_;
  configuration.rtt_stats = rtt_stats_;
  configuration.rtcp_packet_type_counter_observer =
      &rtcp_packet_type_counter_observer_;
  configuration.paced_sender = paced_sender_;
  configuration.send_bitrate_observer = &send_bitrate_observer_;
  configuration.send_frame_count_observer = &send_frame_count_observer_;
  configuration.send_side_delay_observer = &send_side_delay_observer_;
  if (sender_)
    configuration.bandwidth_callback = bandwidth_observer_.get();

  return configuration;
}

RtpRtcp* ViEChannel::CreateRtpRtcpModule() {
  return RtpRtcp::CreateRtpRtcp(CreateRtpRtcpConfiguration());
}

void ViEChannel::StartDecodeThread() {
  DCHECK(!sender_);
  // Start the decode thread
  if (decode_thread_)
    return;
  decode_thread_ = ThreadWrapper::CreateThread(ChannelDecodeThreadFunction,
                                               this, "DecodingThread");
  decode_thread_->Start();
  decode_thread_->SetPriority(kHighestPriority);
}

void ViEChannel::StopDecodeThread() {
  if (!decode_thread_)
    return;

  vcm_->TriggerDecoderShutdown();

  decode_thread_->Stop();
  decode_thread_.reset();
}

int32_t ViEChannel::SetVoiceChannel(int32_t ve_channel_id,
                                    VoEVideoSync* ve_sync_interface) {
  return vie_sync_.ConfigureSync(ve_channel_id,
                                 ve_sync_interface,
                                 rtp_rtcp_.get(),
                                 vie_receiver_.GetRtpReceiver());
}

int32_t ViEChannel::VoiceChannel() {
  return vie_sync_.VoiceChannel();
}

void ViEChannel::RegisterPreRenderCallback(
    I420FrameCallback* pre_render_callback) {
  CriticalSectionScoped cs(callback_cs_.get());
  pre_render_callback_ = pre_render_callback;
}

void ViEChannel::RegisterPreDecodeImageCallback(
    EncodedImageCallback* pre_decode_callback) {
  vcm_->RegisterPreDecodeImageCallback(pre_decode_callback);
}

int32_t ViEChannel::OnInitializeDecoder(
    const int32_t id,
    const int8_t payload_type,
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const int frequency,
    const uint8_t channels,
    const uint32_t rate) {
  LOG(LS_INFO) << "OnInitializeDecoder " << static_cast<int>(payload_type)
               << " " << payload_name;
  vcm_->ResetDecoder();

  CriticalSectionScoped cs(callback_cs_.get());
  decoder_reset_ = true;
  return 0;
}

void ViEChannel::OnIncomingSSRCChanged(const int32_t id, const uint32_t ssrc) {
  assert(channel_id_ == ChannelId(id));
  rtp_rtcp_->SetRemoteSSRC(ssrc);

}

void ViEChannel::OnIncomingCSRCChanged(const int32_t id,
                                       const uint32_t CSRC,
                                       const bool added) {
  assert(channel_id_ == ChannelId(id));
  CriticalSectionScoped cs(callback_cs_.get());
}

void ViEChannel::RegisterSendFrameCountObserver(
    FrameCountObserver* observer) {
  send_frame_count_observer_.Set(observer);
}

void ViEChannel::RegisterReceiveStatisticsProxy(
    ReceiveStatisticsProxy* receive_statistics_proxy) {
  CriticalSectionScoped cs(callback_cs_.get());
  vcm_receive_stats_callback_ = receive_statistics_proxy;
}

void ViEChannel::SetIncomingVideoStream(
    IncomingVideoStream* incoming_video_stream) {
  CriticalSectionScoped cs(callback_cs_.get());
  incoming_video_stream_ = incoming_video_stream;
}
}  // namespace webrtc
