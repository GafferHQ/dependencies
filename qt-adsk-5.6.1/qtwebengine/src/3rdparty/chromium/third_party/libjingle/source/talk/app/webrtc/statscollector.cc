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

#include "talk/app/webrtc/statscollector.h"

#include <utility>
#include <vector>

#include "talk/session/media/channel.h"
#include "webrtc/base/base64.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/timing.h"

using rtc::scoped_ptr;

namespace webrtc {
namespace {

// The following is the enum RTCStatsIceCandidateType from
// http://w3c.github.io/webrtc-stats/#rtcstatsicecandidatetype-enum such that
// our stats report for ice candidate type could conform to that.
const char STATSREPORT_LOCAL_PORT_TYPE[] = "host";
const char STATSREPORT_STUN_PORT_TYPE[] = "serverreflexive";
const char STATSREPORT_PRFLX_PORT_TYPE[] = "peerreflexive";
const char STATSREPORT_RELAY_PORT_TYPE[] = "relayed";

// Strings used by the stats collector to report adapter types. This fits the
// general stype of http://w3c.github.io/webrtc-stats than what
// AdapterTypeToString does.
const char* STATSREPORT_ADAPTER_TYPE_ETHERNET = "lan";
const char* STATSREPORT_ADAPTER_TYPE_WIFI = "wlan";
const char* STATSREPORT_ADAPTER_TYPE_WWAN = "wwan";
const char* STATSREPORT_ADAPTER_TYPE_VPN = "vpn";
const char* STATSREPORT_ADAPTER_TYPE_LOOPBACK = "loopback";

template<typename ValueType>
struct TypeForAdd {
  const StatsReport::StatsValueName name;
  const ValueType& value;
};

typedef TypeForAdd<bool> BoolForAdd;
typedef TypeForAdd<float> FloatForAdd;
typedef TypeForAdd<int64> Int64ForAdd;
typedef TypeForAdd<int> IntForAdd;

StatsReport::Id GetTransportIdFromProxy(const cricket::ProxyTransportMap& map,
                                        const std::string& proxy) {
  DCHECK(!proxy.empty());
  cricket::ProxyTransportMap::const_iterator found = map.find(proxy);
  if (found == map.end())
    return StatsReport::Id();

  return StatsReport::NewComponentId(
      found->second, cricket::ICE_CANDIDATE_COMPONENT_RTP);
}

StatsReport* AddTrackReport(StatsCollection* reports,
                            const std::string& track_id) {
  // Adds an empty track report.
  StatsReport::Id id(
      StatsReport::NewTypedId(StatsReport::kStatsReportTypeTrack, track_id));
  StatsReport* report = reports->ReplaceOrAddNew(id);
  report->AddString(StatsReport::kStatsValueNameTrackId, track_id);
  return report;
}

template <class TrackVector>
void CreateTrackReports(const TrackVector& tracks, StatsCollection* reports,
                        TrackIdMap& track_ids) {
  for (const auto& track : tracks) {
    const std::string& track_id = track->id();
    StatsReport* report = AddTrackReport(reports, track_id);
    DCHECK(report != nullptr);
    track_ids[track_id] = report;
  }
}

void ExtractCommonSendProperties(const cricket::MediaSenderInfo& info,
                                 StatsReport* report) {
  report->AddString(StatsReport::kStatsValueNameCodecName, info.codec_name);
  report->AddInt64(StatsReport::kStatsValueNameBytesSent, info.bytes_sent);
  report->AddInt64(StatsReport::kStatsValueNameRtt, info.rtt_ms);
}

void SetAudioProcessingStats(StatsReport* report, int signal_level,
    bool typing_noise_detected, int echo_return_loss,
    int echo_return_loss_enhancement, int echo_delay_median_ms,
    float aec_quality_min, int echo_delay_std_ms) {
  report->AddBoolean(StatsReport::kStatsValueNameTypingNoiseState,
                     typing_noise_detected);
  report->AddFloat(StatsReport::kStatsValueNameEchoCancellationQualityMin,
                   aec_quality_min);
  // Don't overwrite the previous signal level if it's not available now.
  if (signal_level >= 0)
    report->AddInt(StatsReport::kStatsValueNameAudioInputLevel, signal_level);
  const IntForAdd ints[] = {
    { StatsReport::kStatsValueNameEchoReturnLoss, echo_return_loss },
    { StatsReport::kStatsValueNameEchoReturnLossEnhancement,
      echo_return_loss_enhancement },
    { StatsReport::kStatsValueNameEchoDelayMedian, echo_delay_median_ms },
    { StatsReport::kStatsValueNameEchoDelayStdDev, echo_delay_std_ms },
  };
  for (const auto& i : ints)
    report->AddInt(i.name, i.value);
}

void ExtractStats(const cricket::VoiceReceiverInfo& info, StatsReport* report) {
  const FloatForAdd floats[] = {
    { StatsReport::kStatsValueNameExpandRate, info.expand_rate },
    { StatsReport::kStatsValueNameSecondaryDecodedRate,
      info.secondary_decoded_rate },
    { StatsReport::kStatsValueNameSpeechExpandRate, info.speech_expand_rate },
    { StatsReport::kStatsValueNameAccelerateRate, info.accelerate_rate },
    { StatsReport::kStatsValueNamePreemptiveExpandRate,
      info.preemptive_expand_rate },
  };

  const IntForAdd ints[] = {
    { StatsReport::kStatsValueNameAudioOutputLevel, info.audio_level },
    { StatsReport::kStatsValueNameCurrentDelayMs, info.delay_estimate_ms },
    { StatsReport::kStatsValueNameDecodingCNG, info.decoding_cng },
    { StatsReport::kStatsValueNameDecodingCTN, info.decoding_calls_to_neteq },
    { StatsReport::kStatsValueNameDecodingCTSG,
      info.decoding_calls_to_silence_generator },
    { StatsReport::kStatsValueNameDecodingNormal, info.decoding_normal },
    { StatsReport::kStatsValueNameDecodingPLC, info.decoding_plc },
    { StatsReport::kStatsValueNameDecodingPLCCNG, info.decoding_plc_cng },
    { StatsReport::kStatsValueNameJitterBufferMs, info.jitter_buffer_ms },
    { StatsReport::kStatsValueNameJitterReceived, info.jitter_ms },
    { StatsReport::kStatsValueNamePacketsLost, info.packets_lost },
    { StatsReport::kStatsValueNamePacketsReceived, info.packets_rcvd },
    { StatsReport::kStatsValueNamePreferredJitterBufferMs,
      info.jitter_buffer_preferred_ms },
  };

  for (const auto& f : floats)
    report->AddFloat(f.name, f.value);

  for (const auto& i : ints)
    report->AddInt(i.name, i.value);

  report->AddInt64(StatsReport::kStatsValueNameBytesReceived,
                   info.bytes_rcvd);
  report->AddInt64(StatsReport::kStatsValueNameCaptureStartNtpTimeMs,
                   info.capture_start_ntp_time_ms);

  report->AddString(StatsReport::kStatsValueNameCodecName, info.codec_name);
}

void ExtractStats(const cricket::VoiceSenderInfo& info, StatsReport* report) {
  ExtractCommonSendProperties(info, report);

  SetAudioProcessingStats(report, info.audio_level, info.typing_noise_detected,
      info.echo_return_loss, info.echo_return_loss_enhancement,
      info.echo_delay_median_ms, info.aec_quality_min, info.echo_delay_std_ms);

  const IntForAdd ints[] = {
    { StatsReport::kStatsValueNameJitterReceived, info.jitter_ms },
    { StatsReport::kStatsValueNamePacketsLost, info.packets_lost },
    { StatsReport::kStatsValueNamePacketsSent, info.packets_sent },
  };

  for (const auto& i : ints)
    report->AddInt(i.name, i.value);
}

void ExtractStats(const cricket::VideoReceiverInfo& info, StatsReport* report) {
  report->AddInt64(StatsReport::kStatsValueNameBytesReceived,
                   info.bytes_rcvd);
  report->AddInt64(StatsReport::kStatsValueNameCaptureStartNtpTimeMs,
                   info.capture_start_ntp_time_ms);
  const IntForAdd ints[] = {
    { StatsReport::kStatsValueNameCurrentDelayMs, info.current_delay_ms },
    { StatsReport::kStatsValueNameDecodeMs, info.decode_ms },
    { StatsReport::kStatsValueNameFirsSent, info.firs_sent },
    { StatsReport::kStatsValueNameFrameHeightReceived, info.frame_height },
    { StatsReport::kStatsValueNameFrameRateDecoded, info.framerate_decoded },
    { StatsReport::kStatsValueNameFrameRateOutput, info.framerate_output },
    { StatsReport::kStatsValueNameFrameRateReceived, info.framerate_rcvd },
    { StatsReport::kStatsValueNameFrameWidthReceived, info.frame_width },
    { StatsReport::kStatsValueNameJitterBufferMs, info.jitter_buffer_ms },
    { StatsReport::kStatsValueNameMaxDecodeMs, info.max_decode_ms },
    { StatsReport::kStatsValueNameMinPlayoutDelayMs,
      info.min_playout_delay_ms },
    { StatsReport::kStatsValueNameNacksSent, info.nacks_sent },
    { StatsReport::kStatsValueNamePacketsLost, info.packets_lost },
    { StatsReport::kStatsValueNamePacketsReceived, info.packets_rcvd },
    { StatsReport::kStatsValueNamePlisSent, info.plis_sent },
    { StatsReport::kStatsValueNameRenderDelayMs, info.render_delay_ms },
    { StatsReport::kStatsValueNameTargetDelayMs, info.target_delay_ms },
  };

  for (const auto& i : ints)
    report->AddInt(i.name, i.value);
}

void ExtractStats(const cricket::VideoSenderInfo& info, StatsReport* report) {
  ExtractCommonSendProperties(info, report);

  report->AddBoolean(StatsReport::kStatsValueNameBandwidthLimitedResolution,
                     (info.adapt_reason & 0x2) > 0);
  report->AddBoolean(StatsReport::kStatsValueNameCpuLimitedResolution,
                     (info.adapt_reason & 0x1) > 0);
  report->AddBoolean(StatsReport::kStatsValueNameViewLimitedResolution,
                     (info.adapt_reason & 0x4) > 0);

  const IntForAdd ints[] = {
    { StatsReport::kStatsValueNameAdaptationChanges, info.adapt_changes },
    { StatsReport::kStatsValueNameAvgEncodeMs, info.avg_encode_ms },
    { StatsReport::kStatsValueNameEncodeUsagePercent,
      info.encode_usage_percent },
    { StatsReport::kStatsValueNameFirsReceived, info.firs_rcvd },
    { StatsReport::kStatsValueNameFrameHeightInput, info.input_frame_height },
    { StatsReport::kStatsValueNameFrameHeightSent, info.send_frame_height },
    { StatsReport::kStatsValueNameFrameRateInput, info.framerate_input },
    { StatsReport::kStatsValueNameFrameRateSent, info.framerate_sent },
    { StatsReport::kStatsValueNameFrameWidthInput, info.input_frame_width },
    { StatsReport::kStatsValueNameFrameWidthSent, info.send_frame_width },
    { StatsReport::kStatsValueNameNacksReceived, info.nacks_rcvd },
    { StatsReport::kStatsValueNamePacketsLost, info.packets_lost },
    { StatsReport::kStatsValueNamePacketsSent, info.packets_sent },
    { StatsReport::kStatsValueNamePlisReceived, info.plis_rcvd },
  };

  for (const auto& i : ints)
    report->AddInt(i.name, i.value);
}

void ExtractStats(const cricket::BandwidthEstimationInfo& info,
                  double stats_gathering_started,
                  PeerConnectionInterface::StatsOutputLevel level,
                  StatsReport* report) {
  DCHECK(report->type() == StatsReport::kStatsReportTypeBwe);

  report->set_timestamp(stats_gathering_started);
  const IntForAdd ints[] = {
    { StatsReport::kStatsValueNameAvailableSendBandwidth,
      info.available_send_bandwidth },
    { StatsReport::kStatsValueNameAvailableReceiveBandwidth,
      info.available_recv_bandwidth },
    { StatsReport::kStatsValueNameTargetEncBitrate, info.target_enc_bitrate },
    { StatsReport::kStatsValueNameActualEncBitrate, info.actual_enc_bitrate },
    { StatsReport::kStatsValueNameRetransmitBitrate, info.retransmit_bitrate },
    { StatsReport::kStatsValueNameTransmitBitrate, info.transmit_bitrate },
  };
  for (const auto& i : ints)
    report->AddInt(i.name, i.value);
  report->AddInt64(StatsReport::kStatsValueNameBucketDelay, info.bucket_delay);
}

void ExtractRemoteStats(const cricket::MediaSenderInfo& info,
                        StatsReport* report) {
  report->set_timestamp(info.remote_stats[0].timestamp);
  // TODO(hta): Extract some stats here.
}

void ExtractRemoteStats(const cricket::MediaReceiverInfo& info,
                        StatsReport* report) {
  report->set_timestamp(info.remote_stats[0].timestamp);
  // TODO(hta): Extract some stats here.
}

// Template to extract stats from a data vector.
// In order to use the template, the functions that are called from it,
// ExtractStats and ExtractRemoteStats, must be defined and overloaded
// for each type.
template<typename T>
void ExtractStatsFromList(const std::vector<T>& data,
                          const StatsReport::Id& transport_id,
                          StatsCollector* collector,
                          StatsReport::Direction direction) {
  for (const auto& d : data) {
    uint32 ssrc = d.ssrc();
    // Each track can have stats for both local and remote objects.
    // TODO(hta): Handle the case of multiple SSRCs per object.
    StatsReport* report = collector->PrepareReport(true, ssrc, transport_id,
                                                   direction);
    if (report)
      ExtractStats(d, report);

    if (!d.remote_stats.empty()) {
      report = collector->PrepareReport(false, ssrc, transport_id, direction);
      if (report)
        ExtractRemoteStats(d, report);
    }
  }
}

}  // namespace

const char* IceCandidateTypeToStatsType(const std::string& candidate_type) {
  if (candidate_type == cricket::LOCAL_PORT_TYPE) {
    return STATSREPORT_LOCAL_PORT_TYPE;
  }
  if (candidate_type == cricket::STUN_PORT_TYPE) {
    return STATSREPORT_STUN_PORT_TYPE;
  }
  if (candidate_type == cricket::PRFLX_PORT_TYPE) {
    return STATSREPORT_PRFLX_PORT_TYPE;
  }
  if (candidate_type == cricket::RELAY_PORT_TYPE) {
    return STATSREPORT_RELAY_PORT_TYPE;
  }
  DCHECK(false);
  return "unknown";
}

const char* AdapterTypeToStatsType(rtc::AdapterType type) {
  switch (type) {
    case rtc::ADAPTER_TYPE_UNKNOWN:
      return "unknown";
    case rtc::ADAPTER_TYPE_ETHERNET:
      return STATSREPORT_ADAPTER_TYPE_ETHERNET;
    case rtc::ADAPTER_TYPE_WIFI:
      return STATSREPORT_ADAPTER_TYPE_WIFI;
    case rtc::ADAPTER_TYPE_CELLULAR:
      return STATSREPORT_ADAPTER_TYPE_WWAN;
    case rtc::ADAPTER_TYPE_VPN:
      return STATSREPORT_ADAPTER_TYPE_VPN;
    case rtc::ADAPTER_TYPE_LOOPBACK:
      return STATSREPORT_ADAPTER_TYPE_LOOPBACK;
    default:
      DCHECK(false);
      return "";
  }
}

StatsCollector::StatsCollector(WebRtcSession* session)
    : session_(session),
      stats_gathering_started_(0) {
  DCHECK(session_);
}

StatsCollector::~StatsCollector() {
  DCHECK(session_->signaling_thread()->IsCurrent());
}

double StatsCollector::GetTimeNow() {
  return rtc::Timing::WallTimeNow() * rtc::kNumMillisecsPerSec;
}

// Adds a MediaStream with tracks that can be used as a |selector| in a call
// to GetStats.
void StatsCollector::AddStream(MediaStreamInterface* stream) {
  DCHECK(session_->signaling_thread()->IsCurrent());
  DCHECK(stream != NULL);

  CreateTrackReports<AudioTrackVector>(stream->GetAudioTracks(),
                                       &reports_, track_ids_);
  CreateTrackReports<VideoTrackVector>(stream->GetVideoTracks(),
                                       &reports_, track_ids_);
}

void StatsCollector::AddLocalAudioTrack(AudioTrackInterface* audio_track,
                                        uint32 ssrc) {
  DCHECK(session_->signaling_thread()->IsCurrent());
  DCHECK(audio_track != NULL);
#if (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON))
  for (const auto& track : local_audio_tracks_)
    DCHECK(track.first != audio_track || track.second != ssrc);
#endif

  local_audio_tracks_.push_back(std::make_pair(audio_track, ssrc));

  // Create the kStatsReportTypeTrack report for the new track if there is no
  // report yet.
  StatsReport::Id id(StatsReport::NewTypedId(StatsReport::kStatsReportTypeTrack,
                                             audio_track->id()));
  StatsReport* report = reports_.Find(id);
  if (!report) {
    report = reports_.InsertNew(id);
    report->AddString(StatsReport::kStatsValueNameTrackId, audio_track->id());
  }
}

void StatsCollector::RemoveLocalAudioTrack(AudioTrackInterface* audio_track,
                                           uint32 ssrc) {
  DCHECK(audio_track != NULL);
  local_audio_tracks_.erase(std::remove_if(local_audio_tracks_.begin(),
      local_audio_tracks_.end(),
      [audio_track, ssrc](const LocalAudioTrackVector::value_type& track) {
        return track.first == audio_track && track.second == ssrc;
      }));
}

void StatsCollector::GetStats(MediaStreamTrackInterface* track,
                              StatsReports* reports) {
  DCHECK(session_->signaling_thread()->IsCurrent());
  DCHECK(reports != NULL);
  DCHECK(reports->empty());

  rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;

  if (!track) {
    reports->reserve(reports_.size());
    for (auto* r : reports_)
      reports->push_back(r);
    return;
  }

  StatsReport* report = reports_.Find(StatsReport::NewTypedId(
      StatsReport::kStatsReportTypeSession, session_->id()));
  if (report)
    reports->push_back(report);

  report = reports_.Find(StatsReport::NewTypedId(
      StatsReport::kStatsReportTypeTrack, track->id()));

  if (!report)
    return;

  reports->push_back(report);

  std::string track_id;
  for (const auto* r : reports_) {
    if (r->type() != StatsReport::kStatsReportTypeSsrc)
      continue;

    const StatsReport::Value* v =
        r->FindValue(StatsReport::kStatsValueNameTrackId);
    if (v && v->string_val() == track->id())
      reports->push_back(r);
  }
}

void
StatsCollector::UpdateStats(PeerConnectionInterface::StatsOutputLevel level) {
  DCHECK(session_->signaling_thread()->IsCurrent());
  double time_now = GetTimeNow();
  // Calls to UpdateStats() that occur less than kMinGatherStatsPeriod number of
  // ms apart will be ignored.
  const double kMinGatherStatsPeriod = 50;
  if (stats_gathering_started_ != 0 &&
      stats_gathering_started_ + kMinGatherStatsPeriod > time_now) {
    return;
  }
  stats_gathering_started_ = time_now;

  if (session_) {
    // TODO(tommi): All of these hop over to the worker thread to fetch
    // information.  We could use an AsyncInvoker to run all of these and post
    // the information back to the signaling thread where we can create and
    // update stats reports.  That would also clean up the threading story a bit
    // since we'd be creating/updating the stats report objects consistently on
    // the same thread (this class has no locks right now).
    ExtractSessionInfo();
    ExtractVoiceInfo();
    ExtractVideoInfo(level);
    ExtractDataInfo();
    UpdateTrackReports();
  }
}

StatsReport* StatsCollector::PrepareReport(
    bool local,
    uint32 ssrc,
    const StatsReport::Id& transport_id,
    StatsReport::Direction direction) {
  DCHECK(session_->signaling_thread()->IsCurrent());
  StatsReport::Id id(StatsReport::NewIdWithDirection(
      local ? StatsReport::kStatsReportTypeSsrc :
              StatsReport::kStatsReportTypeRemoteSsrc,
      rtc::ToString<uint32>(ssrc), direction));
  StatsReport* report = reports_.Find(id);

  // Use the ID of the track that is currently mapped to the SSRC, if any.
  std::string track_id;
  if (!GetTrackIdBySsrc(ssrc, &track_id, direction)) {
    if (!report) {
      // The ssrc is not used by any track or existing report, return NULL
      // in such case to indicate no report is prepared for the ssrc.
      return NULL;
    }

    // The ssrc is not used by any existing track. Keeps the old track id
    // since we want to report the stats for inactive ssrc.
    const StatsReport::Value* v =
        report->FindValue(StatsReport::kStatsValueNameTrackId);
    if (v)
      track_id = v->string_val();
  }

  if (!report)
    report = reports_.InsertNew(id);

  // FYI - for remote reports, the timestamp will be overwritten later.
  report->set_timestamp(stats_gathering_started_);

  report->AddInt64(StatsReport::kStatsValueNameSsrc, ssrc);
  report->AddString(StatsReport::kStatsValueNameTrackId, track_id);
  // Add the mapping of SSRC to transport.
  report->AddId(StatsReport::kStatsValueNameTransportId, transport_id);
  return report;
}

StatsReport* StatsCollector::AddOneCertificateReport(
    const rtc::SSLCertificate* cert, const StatsReport* issuer) {
  DCHECK(session_->signaling_thread()->IsCurrent());

  // TODO(bemasc): Move this computation to a helper class that caches these
  // values to reduce CPU use in GetStats.  This will require adding a fast
  // SSLCertificate::Equals() method to detect certificate changes.

  std::string digest_algorithm;
  if (!cert->GetSignatureDigestAlgorithm(&digest_algorithm))
    return nullptr;

  rtc::scoped_ptr<rtc::SSLFingerprint> ssl_fingerprint(
      rtc::SSLFingerprint::Create(digest_algorithm, cert));

  // SSLFingerprint::Create can fail if the algorithm returned by
  // SSLCertificate::GetSignatureDigestAlgorithm is not supported by the
  // implementation of SSLCertificate::ComputeDigest.  This currently happens
  // with MD5- and SHA-224-signed certificates when linked to libNSS.
  if (!ssl_fingerprint)
    return nullptr;

  std::string fingerprint = ssl_fingerprint->GetRfc4572Fingerprint();

  rtc::Buffer der_buffer;
  cert->ToDER(&der_buffer);
  std::string der_base64;
  rtc::Base64::EncodeFromArray(der_buffer.data(), der_buffer.size(),
                               &der_base64);

  StatsReport::Id id(StatsReport::NewTypedId(
      StatsReport::kStatsReportTypeCertificate, fingerprint));
  StatsReport* report = reports_.ReplaceOrAddNew(id);
  report->set_timestamp(stats_gathering_started_);
  report->AddString(StatsReport::kStatsValueNameFingerprint, fingerprint);
  report->AddString(StatsReport::kStatsValueNameFingerprintAlgorithm,
                    digest_algorithm);
  report->AddString(StatsReport::kStatsValueNameDer, der_base64);
  if (issuer)
    report->AddId(StatsReport::kStatsValueNameIssuerId, issuer->id());
  return report;
}

StatsReport* StatsCollector::AddCertificateReports(
    const rtc::SSLCertificate* cert) {
  DCHECK(session_->signaling_thread()->IsCurrent());
  // Produces a chain of StatsReports representing this certificate and the rest
  // of its chain, and adds those reports to |reports_|.  The return value is
  // the id of the leaf report.  The provided cert must be non-null, so at least
  // one report will always be provided and the returned string will never be
  // empty.
  DCHECK(cert != NULL);

  StatsReport* issuer = nullptr;
  rtc::scoped_ptr<rtc::SSLCertChain> chain;
  if (cert->GetChain(chain.accept())) {
    // This loop runs in reverse, i.e. from root to leaf, so that each
    // certificate's issuer's report ID is known before the child certificate's
    // report is generated.  The root certificate does not have an issuer ID
    // value.
    for (ptrdiff_t i = chain->GetSize() - 1; i >= 0; --i) {
      const rtc::SSLCertificate& cert_i = chain->Get(i);
      issuer = AddOneCertificateReport(&cert_i, issuer);
    }
  }
  // Add the leaf certificate.
  return AddOneCertificateReport(cert, issuer);
}

StatsReport* StatsCollector::AddConnectionInfoReport(
    const std::string& content_name, int component, int connection_id,
    const StatsReport::Id& channel_report_id,
    const cricket::ConnectionInfo& info) {
  StatsReport::Id id(StatsReport::NewCandidatePairId(content_name, component,
                                                     connection_id));
  StatsReport* report = reports_.ReplaceOrAddNew(id);
  report->set_timestamp(stats_gathering_started_);

  const BoolForAdd bools[] = {
    { StatsReport::kStatsValueNameActiveConnection, info.best_connection },
    { StatsReport::kStatsValueNameReadable, info.readable },
    { StatsReport::kStatsValueNameWritable, info.writable },
  };
  for (const auto& b : bools)
    report->AddBoolean(b.name, b.value);

  report->AddId(StatsReport::kStatsValueNameChannelId, channel_report_id);
  report->AddId(StatsReport::kStatsValueNameLocalCandidateId,
                AddCandidateReport(info.local_candidate, true)->id());
  report->AddId(StatsReport::kStatsValueNameRemoteCandidateId,
                AddCandidateReport(info.remote_candidate, false)->id());

  const Int64ForAdd int64s[] = {
    { StatsReport::kStatsValueNameBytesReceived, info.recv_total_bytes },
    { StatsReport::kStatsValueNameBytesSent, info.sent_total_bytes },
    { StatsReport::kStatsValueNamePacketsSent, info.sent_total_packets },
    { StatsReport::kStatsValueNameRtt, info.rtt },
    { StatsReport::kStatsValueNameSendPacketsDiscarded,
      info.sent_discarded_packets },
  };
  for (const auto& i : int64s)
    report->AddInt64(i.name, i.value);

  report->AddString(StatsReport::kStatsValueNameLocalAddress,
                    info.local_candidate.address().ToString());
  report->AddString(StatsReport::kStatsValueNameLocalCandidateType,
                    info.local_candidate.type());
  report->AddString(StatsReport::kStatsValueNameRemoteAddress,
                    info.remote_candidate.address().ToString());
  report->AddString(StatsReport::kStatsValueNameRemoteCandidateType,
                    info.remote_candidate.type());
  report->AddString(StatsReport::kStatsValueNameTransportType,
                    info.local_candidate.protocol());

  return report;
}

StatsReport* StatsCollector::AddCandidateReport(
    const cricket::Candidate& candidate,
    bool local) {
  StatsReport::Id id(StatsReport::NewCandidateId(local, candidate.id()));
  StatsReport* report = reports_.Find(id);
  if (!report) {
    report = reports_.InsertNew(id);
    report->set_timestamp(stats_gathering_started_);
    if (local) {
      report->AddString(StatsReport::kStatsValueNameCandidateNetworkType,
                        AdapterTypeToStatsType(candidate.network_type()));
    }
    report->AddString(StatsReport::kStatsValueNameCandidateIPAddress,
                      candidate.address().ipaddr().ToString());
    report->AddString(StatsReport::kStatsValueNameCandidatePortNumber,
                      candidate.address().PortAsString());
    report->AddInt(StatsReport::kStatsValueNameCandidatePriority,
                   candidate.priority());
    report->AddString(StatsReport::kStatsValueNameCandidateType,
                      IceCandidateTypeToStatsType(candidate.type()));
    report->AddString(StatsReport::kStatsValueNameCandidateTransportType,
                      candidate.protocol());
  }

  return report;
}

void StatsCollector::ExtractSessionInfo() {
  DCHECK(session_->signaling_thread()->IsCurrent());

  // Extract information from the base session.
  StatsReport::Id id(StatsReport::NewTypedId(
      StatsReport::kStatsReportTypeSession, session_->id()));
  StatsReport* report = reports_.ReplaceOrAddNew(id);
  report->set_timestamp(stats_gathering_started_);
  report->AddBoolean(StatsReport::kStatsValueNameInitiator,
                     session_->initiator());

  cricket::SessionStats stats;
  if (!session_->GetTransportStats(&stats)) {
    return;
  }

  // Store the proxy map away for use in SSRC reporting.
  // TODO(tommi): This shouldn't be necessary if we post the stats back to the
  // signaling thread after fetching them on the worker thread, then just use
  // the proxy map directly from the session stats.
  // As is, if GetStats() failed, we could be using old (incorrect?) proxy
  // data.
  proxy_to_transport_ = stats.proxy_to_transport;

  for (const auto& transport_iter : stats.transport_stats) {
    // Attempt to get a copy of the certificates from the transport and
    // expose them in stats reports.  All channels in a transport share the
    // same local and remote certificates.
    //
    // Note that Transport::GetIdentity and Transport::GetRemoteCertificate
    // invoke method calls on the worker thread and block this thread, but
    // messages are still processed on this thread, which may blow way the
    // existing transports. So we cannot reuse |transport| after these calls.
    StatsReport::Id local_cert_report_id, remote_cert_report_id;

    cricket::Transport* transport =
        session_->GetTransport(transport_iter.second.content_name);
    rtc::scoped_ptr<rtc::SSLIdentity> identity;
    if (transport && transport->GetIdentity(identity.accept())) {
      StatsReport* r = AddCertificateReports(&(identity->certificate()));
      if (r)
        local_cert_report_id = r->id();
    }

    transport = session_->GetTransport(transport_iter.second.content_name);
    rtc::scoped_ptr<rtc::SSLCertificate> cert;
    if (transport && transport->GetRemoteCertificate(cert.accept())) {
      StatsReport* r = AddCertificateReports(cert.get());
      if (r)
        remote_cert_report_id = r->id();
    }

    for (const auto& channel_iter : transport_iter.second.channel_stats) {
      StatsReport::Id id(StatsReport::NewComponentId(
          transport_iter.second.content_name, channel_iter.component));
      StatsReport* channel_report = reports_.ReplaceOrAddNew(id);
      channel_report->set_timestamp(stats_gathering_started_);
      channel_report->AddInt(StatsReport::kStatsValueNameComponent,
                             channel_iter.component);
      if (local_cert_report_id.get()) {
        channel_report->AddId(StatsReport::kStatsValueNameLocalCertificateId,
                              local_cert_report_id);
      }
      if (remote_cert_report_id.get()) {
        channel_report->AddId(StatsReport::kStatsValueNameRemoteCertificateId,
                              remote_cert_report_id);
      }
      const std::string& srtp_cipher = channel_iter.srtp_cipher;
      if (!srtp_cipher.empty()) {
        channel_report->AddString(StatsReport::kStatsValueNameSrtpCipher,
                                  srtp_cipher);
      }
      const std::string& ssl_cipher = channel_iter.ssl_cipher;
      if (!ssl_cipher.empty()) {
        channel_report->AddString(StatsReport::kStatsValueNameDtlsCipher,
                                  ssl_cipher);
      }

      int connection_id = 0;
      for (const cricket::ConnectionInfo& info :
               channel_iter.connection_infos) {
        StatsReport* connection_report = AddConnectionInfoReport(
            transport_iter.first, channel_iter.component, connection_id++,
            channel_report->id(), info);
        if (info.best_connection) {
          channel_report->AddId(
              StatsReport::kStatsValueNameSelectedCandidatePairId,
              connection_report->id());
        }
      }
    }
  }
}

void StatsCollector::ExtractVoiceInfo() {
  DCHECK(session_->signaling_thread()->IsCurrent());

  if (!session_->voice_channel()) {
    return;
  }
  cricket::VoiceMediaInfo voice_info;
  if (!session_->voice_channel()->GetStats(&voice_info)) {
    LOG(LS_ERROR) << "Failed to get voice channel stats.";
    return;
  }

  // TODO(tommi): The above code should run on the worker thread and post the
  // results back to the signaling thread, where we can add data to the reports.
  rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;

  StatsReport::Id transport_id(GetTransportIdFromProxy(proxy_to_transport_,
      session_->voice_channel()->content_name()));
  if (!transport_id.get()) {
    LOG(LS_ERROR) << "Failed to get transport name for proxy "
                  << session_->voice_channel()->content_name();
    return;
  }

  ExtractStatsFromList(voice_info.receivers, transport_id, this,
      StatsReport::kReceive);
  ExtractStatsFromList(voice_info.senders, transport_id, this,
      StatsReport::kSend);

  UpdateStatsFromExistingLocalAudioTracks();
}

void StatsCollector::ExtractVideoInfo(
    PeerConnectionInterface::StatsOutputLevel level) {
  DCHECK(session_->signaling_thread()->IsCurrent());

  if (!session_->video_channel())
    return;

  cricket::VideoMediaInfo video_info;
  if (!session_->video_channel()->GetStats(&video_info)) {
    LOG(LS_ERROR) << "Failed to get video channel stats.";
    return;
  }

  // TODO(tommi): The above code should run on the worker thread and post the
  // results back to the signaling thread, where we can add data to the reports.
  rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;

  StatsReport::Id transport_id(GetTransportIdFromProxy(proxy_to_transport_,
      session_->video_channel()->content_name()));
  if (!transport_id.get()) {
    LOG(LS_ERROR) << "Failed to get transport name for proxy "
                  << session_->video_channel()->content_name();
    return;
  }
  ExtractStatsFromList(video_info.receivers, transport_id, this,
      StatsReport::kReceive);
  ExtractStatsFromList(video_info.senders, transport_id, this,
      StatsReport::kSend);
  if (video_info.bw_estimations.size() != 1) {
    LOG(LS_ERROR) << "BWEs count: " << video_info.bw_estimations.size();
  } else {
    StatsReport::Id report_id(StatsReport::NewBandwidthEstimationId());
    StatsReport* report = reports_.FindOrAddNew(report_id);
    ExtractStats(
        video_info.bw_estimations[0], stats_gathering_started_, level, report);
  }
}

void StatsCollector::ExtractDataInfo() {
  DCHECK(session_->signaling_thread()->IsCurrent());

  rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;

  for (const auto& dc :
           session_->mediastream_signaling()->sctp_data_channels()) {
    StatsReport::Id id(StatsReport::NewTypedIntId(
        StatsReport::kStatsReportTypeDataChannel, dc->id()));
    StatsReport* report = reports_.ReplaceOrAddNew(id);
    report->set_timestamp(stats_gathering_started_);
    report->AddString(StatsReport::kStatsValueNameLabel, dc->label());
    report->AddInt(StatsReport::kStatsValueNameDataChannelId, dc->id());
    report->AddString(StatsReport::kStatsValueNameProtocol, dc->protocol());
    report->AddString(StatsReport::kStatsValueNameState,
                      DataChannelInterface::DataStateString(dc->state()));
  }
}

StatsReport* StatsCollector::GetReport(const StatsReport::StatsType& type,
                                       const std::string& id,
                                       StatsReport::Direction direction) {
  DCHECK(session_->signaling_thread()->IsCurrent());
  DCHECK(type == StatsReport::kStatsReportTypeSsrc ||
         type == StatsReport::kStatsReportTypeRemoteSsrc);
  return reports_.Find(StatsReport::NewIdWithDirection(type, id, direction));
}

void StatsCollector::UpdateStatsFromExistingLocalAudioTracks() {
  DCHECK(session_->signaling_thread()->IsCurrent());
  // Loop through the existing local audio tracks.
  for (const auto& it : local_audio_tracks_) {
    AudioTrackInterface* track = it.first;
    uint32 ssrc = it.second;
    StatsReport* report = GetReport(StatsReport::kStatsReportTypeSsrc,
                                    rtc::ToString<uint32>(ssrc),
                                    StatsReport::kSend);
    if (report == NULL) {
      // This can happen if a local audio track is added to a stream on the
      // fly and the report has not been set up yet. Do nothing in this case.
      LOG(LS_ERROR) << "Stats report does not exist for ssrc " << ssrc;
      continue;
    }

    // The same ssrc can be used by both local and remote audio tracks.
    const StatsReport::Value* v =
        report->FindValue(StatsReport::kStatsValueNameTrackId);
    if (!v || v->string_val() != track->id())
      continue;

    report->set_timestamp(stats_gathering_started_);
    UpdateReportFromAudioTrack(track, report);
  }
}

void StatsCollector::UpdateReportFromAudioTrack(AudioTrackInterface* track,
                                                StatsReport* report) {
  DCHECK(session_->signaling_thread()->IsCurrent());
  DCHECK(track != NULL);

  int signal_level = 0;
  if (!track->GetSignalLevel(&signal_level))
    signal_level = -1;

  rtc::scoped_refptr<AudioProcessorInterface> audio_processor(
      track->GetAudioProcessor());

  AudioProcessorInterface::AudioProcessorStats stats;
  if (audio_processor.get())
    audio_processor->GetStats(&stats);

  SetAudioProcessingStats(report, signal_level, stats.typing_noise_detected,
      stats.echo_return_loss, stats.echo_return_loss_enhancement,
      stats.echo_delay_median_ms, stats.aec_quality_min,
      stats.echo_delay_std_ms);
}

bool StatsCollector::GetTrackIdBySsrc(uint32 ssrc, std::string* track_id,
                                      StatsReport::Direction direction) {
  DCHECK(session_->signaling_thread()->IsCurrent());
  if (direction == StatsReport::kSend) {
    if (!session_->GetLocalTrackIdBySsrc(ssrc, track_id)) {
      LOG(LS_WARNING) << "The SSRC " << ssrc
                      << " is not associated with a sending track";
      return false;
    }
  } else {
    DCHECK(direction == StatsReport::kReceive);
    if (!session_->GetRemoteTrackIdBySsrc(ssrc, track_id)) {
      LOG(LS_WARNING) << "The SSRC " << ssrc
                      << " is not associated with a receiving track";
      return false;
    }
  }

  return true;
}

void StatsCollector::UpdateTrackReports() {
  DCHECK(session_->signaling_thread()->IsCurrent());

  rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;

  for (const auto& entry : track_ids_) {
    StatsReport* report = entry.second;
    report->set_timestamp(stats_gathering_started_);
  }

}

void StatsCollector::ClearUpdateStatsCacheForTest() {
  stats_gathering_started_ = 0;
}

}  // namespace webrtc
