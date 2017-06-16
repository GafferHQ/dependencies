/*
 * libjingle
 * Copyright 2014 Google Inc.
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

#include <stdio.h>

#include "talk/app/webrtc/statscollector.h"

#include "talk/app/webrtc/mediastream.h"
#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/mediastreamsignaling.h"
#include "talk/app/webrtc/mediastreamtrack.h"
#include "talk/app/webrtc/test/fakedatachannelprovider.h"
#include "talk/app/webrtc/test/fakemediastreamsignaling.h"
#include "talk/app/webrtc/videotrack.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/devices/fakedevicemanager.h"
#include "talk/session/media/channelmanager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/base64.h"
#include "webrtc/base/fakesslidentity.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/network.h"
#include "webrtc/p2p/base/fakesession.h"

using rtc::scoped_ptr;
using testing::_;
using testing::DoAll;
using testing::Field;
using testing::Return;
using testing::ReturnNull;
using testing::SetArgPointee;
using webrtc::PeerConnectionInterface;
using webrtc::StatsReport;
using webrtc::StatsReports;

namespace cricket {

class ChannelManager;
class FakeDeviceManager;

}  // namespace cricket

namespace webrtc {

// Error return values
const char kNotFound[] = "NOT FOUND";

// Constant names for track identification.
const char kLocalTrackId[] = "local_track_id";
const char kRemoteTrackId[] = "remote_track_id";
const uint32 kSsrcOfTrack = 1234;

class MockWebRtcSession : public webrtc::WebRtcSession {
 public:
  explicit MockWebRtcSession(cricket::ChannelManager* channel_manager)
    : WebRtcSession(channel_manager, rtc::Thread::Current(),
                    rtc::Thread::Current(), NULL, NULL) {
  }
  MOCK_METHOD0(voice_channel, cricket::VoiceChannel*());
  MOCK_METHOD0(video_channel, cricket::VideoChannel*());
  MOCK_CONST_METHOD0(mediastream_signaling, const MediaStreamSignaling*());
  // Libjingle uses "local" for a outgoing track, and "remote" for a incoming
  // track.
  MOCK_METHOD2(GetLocalTrackIdBySsrc, bool(uint32, std::string*));
  MOCK_METHOD2(GetRemoteTrackIdBySsrc, bool(uint32, std::string*));
  MOCK_METHOD1(GetTransportStats, bool(cricket::SessionStats*));
  MOCK_METHOD1(GetTransport, cricket::Transport*(const std::string&));
};

class MockVideoMediaChannel : public cricket::FakeVideoMediaChannel {
 public:
  MockVideoMediaChannel() : cricket::FakeVideoMediaChannel(NULL) {}

  // MOCK_METHOD0(transport_channel, cricket::TransportChannel*());
  MOCK_METHOD1(GetStats, bool(cricket::VideoMediaInfo*));
};

class MockVoiceMediaChannel : public cricket::FakeVoiceMediaChannel {
 public:
  MockVoiceMediaChannel() : cricket::FakeVoiceMediaChannel(NULL) {
  }
  MOCK_METHOD1(GetStats, bool(cricket::VoiceMediaInfo*));
};

class FakeAudioProcessor : public webrtc::AudioProcessorInterface {
 public:
  FakeAudioProcessor() {}
  ~FakeAudioProcessor() {}

 private:
  void GetStats(AudioProcessorInterface::AudioProcessorStats* stats) override {
    stats->typing_noise_detected = true;
    stats->echo_return_loss = 2;
    stats->echo_return_loss_enhancement = 3;
    stats->echo_delay_median_ms = 4;
    stats->aec_quality_min = 5.1f;
    stats->echo_delay_std_ms = 6;
  }
};

class FakeAudioTrack
    : public webrtc::MediaStreamTrack<webrtc::AudioTrackInterface> {
 public:
  explicit FakeAudioTrack(const std::string& id)
      : webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>(id),
        processor_(new rtc::RefCountedObject<FakeAudioProcessor>()) {}
  std::string kind() const override { return "audio"; }
  webrtc::AudioSourceInterface* GetSource() const override { return NULL; }
  void AddSink(webrtc::AudioTrackSinkInterface* sink) override {}
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override {}
  bool GetSignalLevel(int* level) override {
    *level = 1;
    return true;
  }
  rtc::scoped_refptr<webrtc::AudioProcessorInterface> GetAudioProcessor()
      override {
    return processor_;
  }

 private:
  rtc::scoped_refptr<FakeAudioProcessor> processor_;
};

bool GetValue(const StatsReport* report,
              StatsReport::StatsValueName name,
              std::string* value) {
  const StatsReport::Value* v = report->FindValue(name);
  if (!v)
    return false;
  *value = v->ToString();
  return true;
}

std::string ExtractStatsValue(const StatsReport::StatsType& type,
                              const StatsReports& reports,
                              StatsReport::StatsValueName name) {
  for (const auto* r : reports) {
    std::string ret;
    if (r->type() == type && GetValue(r, name, &ret))
      return ret;
  }

  return kNotFound;
}

StatsReport::Id TypedIdFromIdString(StatsReport::StatsType type,
                                    const std::string& value) {
  EXPECT_FALSE(value.empty());
  StatsReport::Id id;
  if (value.empty())
    return id;

  // This has assumptions about how the ID is constructed.  As is, this is
  // OK since this is for testing purposes only, but if we ever need this
  // in production, we should add a generic method that does this.
  size_t index = value.find('_');
  EXPECT_NE(index, std::string::npos);
  if (index == std::string::npos || index == (value.length() - 1))
    return id;

  id = StatsReport::NewTypedId(type, value.substr(index + 1));
  EXPECT_EQ(id->ToString(), value);
  return id;
}

StatsReport::Id IdFromCertIdString(const std::string& cert_id) {
  return TypedIdFromIdString(StatsReport::kStatsReportTypeCertificate, cert_id);
}

// Finds the |n|-th report of type |type| in |reports|.
// |n| starts from 1 for finding the first report.
const StatsReport* FindNthReportByType(
    const StatsReports& reports, const StatsReport::StatsType& type, int n) {
  for (size_t i = 0; i < reports.size(); ++i) {
    if (reports[i]->type() == type) {
      n--;
      if (n == 0)
        return reports[i];
    }
  }
  return nullptr;
}

const StatsReport* FindReportById(const StatsReports& reports,
                                  const StatsReport::Id& id) {
  for (const auto* r : reports) {
    if (r->id()->Equals(id))
      return r;
  }
  return nullptr;
}

std::string ExtractSsrcStatsValue(StatsReports reports,
                                  StatsReport::StatsValueName name) {
  return ExtractStatsValue(StatsReport::kStatsReportTypeSsrc, reports, name);
}

std::string ExtractBweStatsValue(StatsReports reports,
                                 StatsReport::StatsValueName name) {
  return ExtractStatsValue(
      StatsReport::kStatsReportTypeBwe, reports, name);
}

std::string DerToPem(const std::string& der) {
  return rtc::SSLIdentity::DerToPem(
        rtc::kPemTypeCertificate,
        reinterpret_cast<const unsigned char*>(der.c_str()),
        der.length());
}

std::vector<std::string> DersToPems(
    const std::vector<std::string>& ders) {
  std::vector<std::string> pems(ders.size());
  std::transform(ders.begin(), ders.end(), pems.begin(), DerToPem);
  return pems;
}

void CheckCertChainReports(const StatsReports& reports,
                           const std::vector<std::string>& ders,
                           const StatsReport::Id& start_id) {
  StatsReport::Id cert_id;
  const StatsReport::Id* certificate_id = &start_id;
  size_t i = 0;
  while (true) {
    const StatsReport* report = FindReportById(reports, *certificate_id);
    ASSERT_TRUE(report != NULL);

    std::string der_base64;
    EXPECT_TRUE(GetValue(
        report, StatsReport::kStatsValueNameDer, &der_base64));
    std::string der = rtc::Base64::Decode(der_base64, rtc::Base64::DO_STRICT);
    EXPECT_EQ(ders[i], der);

    std::string fingerprint_algorithm;
    EXPECT_TRUE(GetValue(
        report,
        StatsReport::kStatsValueNameFingerprintAlgorithm,
        &fingerprint_algorithm));
    // The digest algorithm for a FakeSSLCertificate is always SHA-1.
    std::string sha_1_str = rtc::DIGEST_SHA_1;
    EXPECT_EQ(sha_1_str, fingerprint_algorithm);

    std::string fingerprint;
    EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameFingerprint,
                         &fingerprint));
    EXPECT_FALSE(fingerprint.empty());

    ++i;
    std::string issuer_id;
    if (!GetValue(report, StatsReport::kStatsValueNameIssuerId,
                  &issuer_id)) {
      break;
    }

    cert_id = IdFromCertIdString(issuer_id);
    certificate_id = &cert_id;
  }
  EXPECT_EQ(ders.size(), i);
}

void VerifyVoiceReceiverInfoReport(
    const StatsReport* report,
    const cricket::VoiceReceiverInfo& info) {
  std::string value_in_report;
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameAudioOutputLevel, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.audio_level), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameBytesReceived, &value_in_report));
  EXPECT_EQ(rtc::ToString<int64>(info.bytes_rcvd), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameJitterReceived, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.jitter_ms), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameJitterBufferMs, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.jitter_buffer_ms), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNamePreferredJitterBufferMs,
      &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.jitter_buffer_preferred_ms),
      value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameCurrentDelayMs, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.delay_estimate_ms), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameExpandRate, &value_in_report));
  EXPECT_EQ(rtc::ToString<float>(info.expand_rate), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameSpeechExpandRate, &value_in_report));
  EXPECT_EQ(rtc::ToString<float>(info.speech_expand_rate), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameAccelerateRate,
                       &value_in_report));
  EXPECT_EQ(rtc::ToString<float>(info.accelerate_rate), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNamePreemptiveExpandRate,
                       &value_in_report));
  EXPECT_EQ(rtc::ToString<float>(info.preemptive_expand_rate), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameSecondaryDecodedRate,
                       &value_in_report));
  EXPECT_EQ(rtc::ToString<float>(info.secondary_decoded_rate), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNamePacketsReceived, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.packets_rcvd), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameDecodingCTSG, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.decoding_calls_to_silence_generator),
      value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameDecodingCTN, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.decoding_calls_to_neteq),
      value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameDecodingNormal, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.decoding_normal), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameDecodingPLC, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.decoding_plc), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameDecodingCNG, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.decoding_cng), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameDecodingPLCCNG, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.decoding_plc_cng), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameCodecName, &value_in_report));
}


void VerifyVoiceSenderInfoReport(const StatsReport* report,
                                 const cricket::VoiceSenderInfo& sinfo) {
  std::string value_in_report;
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameCodecName, &value_in_report));
  EXPECT_EQ(sinfo.codec_name, value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameBytesSent, &value_in_report));
  EXPECT_EQ(rtc::ToString<int64>(sinfo.bytes_sent), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNamePacketsSent, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.packets_sent), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNamePacketsLost, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.packets_lost), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameRtt, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.rtt_ms), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameRtt, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.rtt_ms), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameJitterReceived, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.jitter_ms), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameEchoCancellationQualityMin,
      &value_in_report));
  EXPECT_EQ(rtc::ToString<float>(sinfo.aec_quality_min), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameEchoDelayMedian, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.echo_delay_median_ms),
            value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameEchoDelayStdDev, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.echo_delay_std_ms),
            value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameEchoReturnLoss, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.echo_return_loss),
            value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameEchoReturnLossEnhancement,
      &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.echo_return_loss_enhancement),
            value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameAudioInputLevel, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.audio_level), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameTypingNoiseState, &value_in_report));
  std::string typing_detected = sinfo.typing_noise_detected ? "true" : "false";
  EXPECT_EQ(typing_detected, value_in_report);
}

// Helper methods to avoid duplication of code.
void InitVoiceSenderInfo(cricket::VoiceSenderInfo* voice_sender_info) {
  voice_sender_info->add_ssrc(kSsrcOfTrack);
  voice_sender_info->codec_name = "fake_codec";
  voice_sender_info->bytes_sent = 100;
  voice_sender_info->packets_sent = 101;
  voice_sender_info->rtt_ms = 102;
  voice_sender_info->fraction_lost = 103;
  voice_sender_info->jitter_ms = 104;
  voice_sender_info->packets_lost = 105;
  voice_sender_info->ext_seqnum = 106;
  voice_sender_info->audio_level = 107;
  voice_sender_info->echo_return_loss = 108;
  voice_sender_info->echo_return_loss_enhancement = 109;
  voice_sender_info->echo_delay_median_ms = 110;
  voice_sender_info->echo_delay_std_ms = 111;
  voice_sender_info->aec_quality_min = 112.0f;
  voice_sender_info->typing_noise_detected = false;
}

void UpdateVoiceSenderInfoFromAudioTrack(
    FakeAudioTrack* audio_track, cricket::VoiceSenderInfo* voice_sender_info) {
  audio_track->GetSignalLevel(&voice_sender_info->audio_level);
  webrtc::AudioProcessorInterface::AudioProcessorStats audio_processor_stats;
  audio_track->GetAudioProcessor()->GetStats(&audio_processor_stats);
  voice_sender_info->typing_noise_detected =
      audio_processor_stats.typing_noise_detected;
  voice_sender_info->echo_return_loss = audio_processor_stats.echo_return_loss;
  voice_sender_info->echo_return_loss_enhancement =
      audio_processor_stats.echo_return_loss_enhancement;
  voice_sender_info->echo_delay_median_ms =
      audio_processor_stats.echo_delay_median_ms;
  voice_sender_info->aec_quality_min = audio_processor_stats.aec_quality_min;
  voice_sender_info->echo_delay_std_ms =
      audio_processor_stats.echo_delay_std_ms;
}

void InitVoiceReceiverInfo(cricket::VoiceReceiverInfo* voice_receiver_info) {
  voice_receiver_info->add_ssrc(kSsrcOfTrack);
  voice_receiver_info->bytes_rcvd = 110;
  voice_receiver_info->packets_rcvd = 111;
  voice_receiver_info->packets_lost = 112;
  voice_receiver_info->fraction_lost = 113;
  voice_receiver_info->packets_lost = 114;
  voice_receiver_info->ext_seqnum = 115;
  voice_receiver_info->jitter_ms = 116;
  voice_receiver_info->jitter_buffer_ms = 117;
  voice_receiver_info->jitter_buffer_preferred_ms = 118;
  voice_receiver_info->delay_estimate_ms = 119;
  voice_receiver_info->audio_level = 120;
  voice_receiver_info->expand_rate = 121;
  voice_receiver_info->speech_expand_rate = 122;
  voice_receiver_info->secondary_decoded_rate = 123;
  voice_receiver_info->accelerate_rate = 124;
  voice_receiver_info->preemptive_expand_rate = 125;
}

class StatsCollectorForTest : public webrtc::StatsCollector {
 public:
  explicit StatsCollectorForTest(WebRtcSession* session) :
      StatsCollector(session), time_now_(19477) {
  }

  double GetTimeNow() override {
    return time_now_;
  }

 private:
  double time_now_;
};

class StatsCollectorTest : public testing::Test {
 protected:
  StatsCollectorTest()
    : media_engine_(new cricket::FakeMediaEngine()),
      channel_manager_(
          new cricket::ChannelManager(media_engine_,
                                      new cricket::FakeDeviceManager(),
                                      rtc::Thread::Current())),
      session_(channel_manager_.get()),
      signaling_(channel_manager_.get()) {
    // By default, we ignore session GetStats calls.
    EXPECT_CALL(session_, GetTransportStats(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(session_, mediastream_signaling()).WillRepeatedly(
        Return(&signaling_));
  }

  ~StatsCollectorTest() {}

  // This creates a standard setup with a transport called "trspname"
  // having one transport channel
  // and the specified virtual connection name.
  void InitSessionStats(const std::string& vc_name) {
    const std::string kTransportName("trspname");
    cricket::TransportStats transport_stats;
    cricket::TransportChannelStats channel_stats;
    channel_stats.component = 1;
    transport_stats.content_name = kTransportName;
    transport_stats.channel_stats.push_back(channel_stats);

    session_stats_.transport_stats[kTransportName] = transport_stats;
    session_stats_.proxy_to_transport[vc_name] = kTransportName;
  }

  // Adds a outgoing video track with a given SSRC into the stats.
  void AddOutgoingVideoTrackStats() {
    stream_ = webrtc::MediaStream::Create("streamlabel");
    track_= webrtc::VideoTrack::Create(kLocalTrackId, NULL);
    stream_->AddTrack(track_);
    EXPECT_CALL(session_, GetLocalTrackIdBySsrc(kSsrcOfTrack, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(kLocalTrackId), Return(true)));
  }

  // Adds a incoming video track with a given SSRC into the stats.
  void AddIncomingVideoTrackStats() {
    stream_ = webrtc::MediaStream::Create("streamlabel");
    track_= webrtc::VideoTrack::Create(kRemoteTrackId, NULL);
    stream_->AddTrack(track_);
    EXPECT_CALL(session_, GetRemoteTrackIdBySsrc(kSsrcOfTrack, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(kRemoteTrackId), Return(true)));
    }

  // Adds a outgoing audio track with a given SSRC into the stats.
  void AddOutgoingAudioTrackStats() {
    if (stream_ == NULL)
      stream_ = webrtc::MediaStream::Create("streamlabel");

    audio_track_ = new rtc::RefCountedObject<FakeAudioTrack>(
        kLocalTrackId);
    stream_->AddTrack(audio_track_);
    EXPECT_CALL(session_, GetLocalTrackIdBySsrc(kSsrcOfTrack, _))
        .WillOnce(DoAll(SetArgPointee<1>(kLocalTrackId), Return(true)));
  }

  // Adds a incoming audio track with a given SSRC into the stats.
  void AddIncomingAudioTrackStats() {
    if (stream_ == NULL)
      stream_ = webrtc::MediaStream::Create("streamlabel");

    audio_track_ = new rtc::RefCountedObject<FakeAudioTrack>(
        kRemoteTrackId);
    stream_->AddTrack(audio_track_);
    EXPECT_CALL(session_, GetRemoteTrackIdBySsrc(kSsrcOfTrack, _))
        .WillOnce(DoAll(SetArgPointee<1>(kRemoteTrackId), Return(true)));
  }

  StatsReport* AddCandidateReport(StatsCollector* collector,
                                  const cricket::Candidate& candidate,
                                  bool local) {
    return collector->AddCandidateReport(candidate, local);
  }

  void SetupAndVerifyAudioTrackStats(
      FakeAudioTrack* audio_track,
      webrtc::MediaStream* stream,
      webrtc::StatsCollector* stats,
      cricket::VoiceChannel* voice_channel,
      const std::string& vc_name,
      MockVoiceMediaChannel* media_channel,
      cricket::VoiceSenderInfo* voice_sender_info,
      cricket::VoiceReceiverInfo* voice_receiver_info,
      cricket::VoiceMediaInfo* stats_read,
      StatsReports* reports) {
    // A track can't have both sender report and recv report at the same time
    // for now, this might change in the future though.
    ASSERT((voice_sender_info == NULL) ^ (voice_receiver_info == NULL));

    // Instruct the session to return stats containing the transport channel.
    InitSessionStats(vc_name);
    EXPECT_CALL(session_, GetTransportStats(_))
        .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats_),
                              Return(true)));

    // Constructs an ssrc stats update.
    if (voice_sender_info)
      stats_read->senders.push_back(*voice_sender_info);
    if (voice_receiver_info)
      stats_read->receivers.push_back(*voice_receiver_info);

    EXPECT_CALL(session_, voice_channel()).WillRepeatedly(
        Return(voice_channel));
    EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
    EXPECT_CALL(*media_channel, GetStats(_))
        .WillOnce(DoAll(SetArgPointee<0>(*stats_read), Return(true)));

    stats->UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
    stats->ClearUpdateStatsCacheForTest();
    stats->GetStats(NULL, reports);

    // Verify the existence of the track report.
    const StatsReport* report = FindNthReportByType(
        *reports, StatsReport::kStatsReportTypeSsrc, 1);
    EXPECT_FALSE(report == NULL);
    EXPECT_EQ(stats->GetTimeNow(), report->timestamp());
    std::string track_id = ExtractSsrcStatsValue(
        *reports, StatsReport::kStatsValueNameTrackId);
    EXPECT_EQ(audio_track->id(), track_id);
    std::string ssrc_id = ExtractSsrcStatsValue(
        *reports, StatsReport::kStatsValueNameSsrc);
    EXPECT_EQ(rtc::ToString<uint32>(kSsrcOfTrack), ssrc_id);

    // Verifies the values in the track report.
    if (voice_sender_info) {
      UpdateVoiceSenderInfoFromAudioTrack(audio_track, voice_sender_info);
      VerifyVoiceSenderInfoReport(report, *voice_sender_info);
    }
    if (voice_receiver_info) {
      VerifyVoiceReceiverInfoReport(report, *voice_receiver_info);
    }

    // Verify we get the same result by passing a track to GetStats().
    StatsReports track_reports;  // returned values.
    stats->GetStats(audio_track, &track_reports);
    const StatsReport* track_report = FindNthReportByType(
        track_reports, StatsReport::kStatsReportTypeSsrc, 1);
    EXPECT_TRUE(track_report);
    EXPECT_EQ(stats->GetTimeNow(), track_report->timestamp());
    track_id = ExtractSsrcStatsValue(track_reports,
                                     StatsReport::kStatsValueNameTrackId);
    EXPECT_EQ(audio_track->id(), track_id);
    ssrc_id = ExtractSsrcStatsValue(track_reports,
                                    StatsReport::kStatsValueNameSsrc);
    EXPECT_EQ(rtc::ToString<uint32>(kSsrcOfTrack), ssrc_id);
    if (voice_sender_info)
      VerifyVoiceSenderInfoReport(track_report, *voice_sender_info);
    if (voice_receiver_info)
    VerifyVoiceReceiverInfoReport(track_report, *voice_receiver_info);
  }

  void TestCertificateReports(const rtc::FakeSSLCertificate& local_cert,
                              const std::vector<std::string>& local_ders,
                              const rtc::FakeSSLCertificate& remote_cert,
                              const std::vector<std::string>& remote_ders) {
    StatsCollectorForTest stats(&session_);

    StatsReports reports;  // returned values.

    // Fake stats to process.
    cricket::TransportChannelStats channel_stats;
    channel_stats.component = 1;
    channel_stats.srtp_cipher = "the-srtp-cipher";
    channel_stats.ssl_cipher = "the-ssl-cipher";

    cricket::TransportStats transport_stats;
    transport_stats.content_name = "audio";
    transport_stats.channel_stats.push_back(channel_stats);

    cricket::SessionStats session_stats;
    session_stats.transport_stats[transport_stats.content_name] =
        transport_stats;

    // Fake certificates to report.
    rtc::FakeSSLIdentity local_identity(local_cert);
    rtc::scoped_ptr<rtc::FakeSSLCertificate> remote_cert_copy(
        remote_cert.GetReference());

    // Fake transport object.
    rtc::scoped_ptr<cricket::FakeTransport> transport(
        new cricket::FakeTransport(
            session_.signaling_thread(),
            session_.worker_thread(),
            transport_stats.content_name));
    transport->SetIdentity(&local_identity);
    cricket::FakeTransportChannel* channel =
        static_cast<cricket::FakeTransportChannel*>(
            transport->CreateChannel(channel_stats.component));
    EXPECT_FALSE(channel == NULL);
    channel->SetRemoteCertificate(remote_cert_copy.get());

    // Configure MockWebRtcSession
    EXPECT_CALL(session_, GetTransport(transport_stats.content_name))
      .WillRepeatedly(Return(transport.get()));
    EXPECT_CALL(session_, GetTransportStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(session_stats),
                      Return(true)));
    EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
    EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());

    stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);

    stats.GetStats(NULL, &reports);

    const StatsReport* channel_report = FindNthReportByType(
        reports, StatsReport::kStatsReportTypeComponent, 1);
    EXPECT_TRUE(channel_report != NULL);

    // Check local certificate chain.
    std::string local_certificate_id = ExtractStatsValue(
        StatsReport::kStatsReportTypeComponent,
        reports,
        StatsReport::kStatsValueNameLocalCertificateId);
    if (local_ders.size() > 0) {
      EXPECT_NE(kNotFound, local_certificate_id);
      StatsReport::Id id(IdFromCertIdString(local_certificate_id));
      CheckCertChainReports(reports, local_ders, id);
    } else {
      EXPECT_EQ(kNotFound, local_certificate_id);
    }

    // Check remote certificate chain.
    std::string remote_certificate_id = ExtractStatsValue(
        StatsReport::kStatsReportTypeComponent,
        reports,
        StatsReport::kStatsValueNameRemoteCertificateId);
    if (remote_ders.size() > 0) {
      EXPECT_NE(kNotFound, remote_certificate_id);
      StatsReport::Id id(IdFromCertIdString(remote_certificate_id));
      CheckCertChainReports(reports, remote_ders, id);
    } else {
      EXPECT_EQ(kNotFound, remote_certificate_id);
    }

    // Check negotiated ciphers.
    std::string dtls_cipher = ExtractStatsValue(
        StatsReport::kStatsReportTypeComponent,
        reports,
        StatsReport::kStatsValueNameDtlsCipher);
    EXPECT_EQ("the-ssl-cipher", dtls_cipher);
    std::string srtp_cipher = ExtractStatsValue(
        StatsReport::kStatsReportTypeComponent,
        reports,
        StatsReport::kStatsValueNameSrtpCipher);
    EXPECT_EQ("the-srtp-cipher", srtp_cipher);
  }

  cricket::FakeMediaEngine* media_engine_;
  rtc::scoped_ptr<cricket::ChannelManager> channel_manager_;
  MockWebRtcSession session_;
  FakeMediaStreamSignaling signaling_;
  FakeDataChannelProvider data_channel_provider_;
  cricket::SessionStats session_stats_;
  rtc::scoped_refptr<webrtc::MediaStream> stream_;
  rtc::scoped_refptr<webrtc::VideoTrack> track_;
  rtc::scoped_refptr<FakeAudioTrack> audio_track_;
};

// Verify that ExtractDataInfo populates reports.
TEST_F(StatsCollectorTest, ExtractDataInfo) {
  const std::string label = "hacks";
  const int id = 31337;
  const std::string state = DataChannelInterface::DataStateString(
      DataChannelInterface::DataState::kConnecting);

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());

  InternalDataChannelInit config;
  config.id = id;
  signaling_.AddDataChannel(DataChannel::Create(
      &data_channel_provider_, cricket::DCT_SCTP, label, config));
  StatsCollectorForTest stats(&session_);

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);

  StatsReports reports;
  stats.GetStats(NULL, &reports);

  const StatsReport* report =
      FindNthReportByType(reports, StatsReport::kStatsReportTypeDataChannel, 1);

  StatsReport::Id reportId = StatsReport::NewTypedIntId(
      StatsReport::kStatsReportTypeDataChannel, id);

  EXPECT_TRUE(reportId->Equals(report->id()));

  EXPECT_EQ(stats.GetTimeNow(), report->timestamp());
  EXPECT_EQ(label, ExtractStatsValue(StatsReport::kStatsReportTypeDataChannel,
                                     reports,
                                     StatsReport::kStatsValueNameLabel));
  EXPECT_EQ(rtc::ToString<int64>(id),
            ExtractStatsValue(StatsReport::kStatsReportTypeDataChannel,
                              reports,
                              StatsReport::kStatsValueNameDataChannelId));
  EXPECT_EQ(state, ExtractStatsValue(StatsReport::kStatsReportTypeDataChannel,
                                     reports,
                                     StatsReport::kStatsValueNameState));
  EXPECT_EQ("", ExtractStatsValue(StatsReport::kStatsReportTypeDataChannel,
                                  reports,
                                  StatsReport::kStatsValueNameProtocol));
}

// This test verifies that 64-bit counters are passed successfully.
TEST_F(StatsCollectorTest, BytesCounterHandles64Bits) {
  StatsCollectorForTest stats(&session_);

  const char kVideoChannelName[] = "video";

  InitSessionStats(kVideoChannelName);
  EXPECT_CALL(session_, GetTransportStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats_),
                            Return(true)));
  EXPECT_CALL(session_, GetTransport(_))
      .WillRepeatedly(Return(static_cast<cricket::Transport*>(NULL)));

  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(rtc::Thread::Current(),
      media_engine_, media_channel, NULL, kVideoChannelName, false);
  StatsReports reports;  // returned values.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  // The number of bytes must be larger than 0xFFFFFFFF for this test.
  const int64 kBytesSent = 12345678901234LL;
  const std::string kBytesSentString("12345678901234");

  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  // Construct a stats value to read.
  video_sender_info.add_ssrc(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(stats_read),
                      Return(true)));
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  std::string result = ExtractSsrcStatsValue(reports,
      StatsReport::kStatsValueNameBytesSent);
  EXPECT_EQ(kBytesSentString, result);
}

// Test that BWE information is reported via stats.
TEST_F(StatsCollectorTest, BandwidthEstimationInfoIsReported) {
  StatsCollectorForTest stats(&session_);

  const char kVideoChannelName[] = "video";

  InitSessionStats(kVideoChannelName);
  EXPECT_CALL(session_, GetTransportStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats_),
                            Return(true)));
  EXPECT_CALL(session_, GetTransport(_))
      .WillRepeatedly(Return(static_cast<cricket::Transport*>(NULL)));

  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(rtc::Thread::Current(),
      media_engine_, media_channel, NULL, kVideoChannelName, false);

  StatsReports reports;  // returned values.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  // Set up an SSRC just to test that we get both kinds of stats back: SSRC and
  // BWE.
  const int64 kBytesSent = 12345678901234LL;
  const std::string kBytesSentString("12345678901234");

  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  // Construct a stats value to read.
  video_sender_info.add_ssrc(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);
  cricket::BandwidthEstimationInfo bwe;
  const int kTargetEncBitrate = 123456;
  const std::string kTargetEncBitrateString("123456");
  bwe.target_enc_bitrate = kTargetEncBitrate;
  stats_read.bw_estimations.push_back(bwe);

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(stats_read), Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  std::string result = ExtractSsrcStatsValue(reports,
      StatsReport::kStatsValueNameBytesSent);
  EXPECT_EQ(kBytesSentString, result);
  result = ExtractBweStatsValue(reports,
      StatsReport::kStatsValueNameTargetEncBitrate);
  EXPECT_EQ(kTargetEncBitrateString, result);
}

// This test verifies that an object of type "googSession" always
// exists in the returned stats.
TEST_F(StatsCollectorTest, SessionObjectExists) {
  StatsCollectorForTest stats(&session_);

  StatsReports reports;  // returned values.
  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  const StatsReport* session_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSession, 1);
  EXPECT_FALSE(session_report == NULL);
}

// This test verifies that only one object of type "googSession" exists
// in the returned stats.
TEST_F(StatsCollectorTest, OnlyOneSessionObjectExists) {
  StatsCollectorForTest stats(&session_);

  StatsReports reports;  // returned values.
  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  const StatsReport* session_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSession, 1);
  EXPECT_FALSE(session_report == NULL);
  session_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSession, 2);
  EXPECT_EQ(NULL, session_report);
}

// This test verifies that the empty track report exists in the returned stats
// without calling StatsCollector::UpdateStats.
TEST_F(StatsCollectorTest, TrackObjectExistsWithoutUpdateStats) {
  StatsCollectorForTest stats(&session_);

  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(rtc::Thread::Current(),
      media_engine_, media_channel, NULL, "video", false);
  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  // Verfies the existence of the track report.
  StatsReports reports;
  stats.GetStats(NULL, &reports);
  EXPECT_EQ((size_t)1, reports.size());
  EXPECT_EQ(StatsReport::kStatsReportTypeTrack, reports[0]->type());
  EXPECT_EQ(0, reports[0]->timestamp());

  std::string trackValue =
      ExtractStatsValue(StatsReport::kStatsReportTypeTrack,
                        reports,
                        StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kLocalTrackId, trackValue);
}

// This test verifies that the empty track report exists in the returned stats
// when StatsCollector::UpdateStats is called with ssrc stats.
TEST_F(StatsCollectorTest, TrackAndSsrcObjectExistAfterUpdateSsrcStats) {
  StatsCollectorForTest stats(&session_);

  const char kVideoChannelName[] = "video";
  InitSessionStats(kVideoChannelName);
  EXPECT_CALL(session_, GetTransportStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats_),
                            Return(true)));
  EXPECT_CALL(session_, GetTransport(_))
      .WillRepeatedly(Return(static_cast<cricket::Transport*>(NULL)));

  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(rtc::Thread::Current(),
      media_engine_, media_channel, NULL, kVideoChannelName, false);
  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  // Constructs an ssrc stats update.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  const int64 kBytesSent = 12345678901234LL;

  // Construct a stats value to read.
  video_sender_info.add_ssrc(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
    .WillOnce(DoAll(SetArgPointee<0>(stats_read),
                    Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  StatsReports reports;
  stats.GetStats(NULL, &reports);
  // |reports| should contain at least one session report, one track report,
  // and one ssrc report.
  EXPECT_LE((size_t)3, reports.size());
  const StatsReport* track_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeTrack, 1);
  EXPECT_TRUE(track_report);

  // Get report for the specific |track|.
  reports.clear();
  stats.GetStats(track_, &reports);
  // |reports| should contain at least one session report, one track report,
  // and one ssrc report.
  EXPECT_LE((size_t)3, reports.size());
  track_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeTrack, 1);
  EXPECT_TRUE(track_report);
  EXPECT_EQ(stats.GetTimeNow(), track_report->timestamp());

  std::string ssrc_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameSsrc);
  EXPECT_EQ(rtc::ToString<uint32>(kSsrcOfTrack), ssrc_id);

  std::string track_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kLocalTrackId, track_id);
}

// This test verifies that an SSRC object has the identifier of a Transport
// stats object, and that this transport stats object exists in stats.
TEST_F(StatsCollectorTest, TransportObjectLinkedFromSsrcObject) {
  StatsCollectorForTest stats(&session_);

  // Ignore unused callback (logspam).
  EXPECT_CALL(session_, GetTransport(_))
      .WillRepeatedly(Return(static_cast<cricket::Transport*>(NULL)));
  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel();
  // The content_name known by the video channel.
  const std::string kVcName("vcname");
  cricket::VideoChannel video_channel(rtc::Thread::Current(),
      media_engine_, media_channel, NULL, kVcName, false);
  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  // Constructs an ssrc stats update.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  const int64 kBytesSent = 12345678901234LL;

  // Construct a stats value to read.
  video_sender_info.add_ssrc(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
    .WillRepeatedly(DoAll(SetArgPointee<0>(stats_read),
                          Return(true)));

  InitSessionStats(kVcName);
  EXPECT_CALL(session_, GetTransportStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats_),
                            Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  StatsReports reports;
  stats.GetStats(NULL, &reports);
  std::string transport_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeSsrc,
      reports,
      StatsReport::kStatsValueNameTransportId);
  ASSERT_NE(kNotFound, transport_id);
  // Transport id component ID will always be 1.
  // This has assumptions about how the ID is constructed.  As is, this is
  // OK since this is for testing purposes only, but if we ever need this
  // in production, we should add a generic method that does this.
  size_t index = transport_id.find('-');
  ASSERT_NE(std::string::npos, index);
  std::string content = transport_id.substr(index + 1);
  index = content.rfind('-');
  ASSERT_NE(std::string::npos, index);
  content = content.substr(0, index);
  StatsReport::Id id(StatsReport::NewComponentId(content, 1));
  ASSERT_EQ(transport_id, id->ToString());
  const StatsReport* transport_report = FindReportById(reports, id);
  ASSERT_FALSE(transport_report == NULL);
}

// This test verifies that a remote stats object will not be created for
// an outgoing SSRC where remote stats are not returned.
TEST_F(StatsCollectorTest, RemoteSsrcInfoIsAbsent) {
  StatsCollectorForTest stats(&session_);

  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel();
  // The content_name known by the video channel.
  const std::string kVcName("vcname");
  cricket::VideoChannel video_channel(rtc::Thread::Current(),
      media_engine_, media_channel, NULL, kVcName, false);
  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  StatsReports reports;
  stats.GetStats(NULL, &reports);
  const StatsReport* remote_report = FindNthReportByType(reports,
      StatsReport::kStatsReportTypeRemoteSsrc, 1);
  EXPECT_TRUE(remote_report == NULL);
}

// This test verifies that a remote stats object will be created for
// an outgoing SSRC where stats are returned.
TEST_F(StatsCollectorTest, RemoteSsrcInfoIsPresent) {
  StatsCollectorForTest stats(&session_);

  // Ignore unused callback (logspam).
  EXPECT_CALL(session_, GetTransport(_))
      .WillRepeatedly(Return(static_cast<cricket::Transport*>(NULL)));
  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel();
  // The content_name known by the video channel.
  const std::string kVcName("vcname");
  cricket::VideoChannel video_channel(rtc::Thread::Current(),
      media_engine_, media_channel, NULL, kVcName, false);
  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  // Instruct the session to return stats containing the transport channel.
  InitSessionStats(kVcName);
  EXPECT_CALL(session_, GetTransportStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats_),
                            Return(true)));

  // Constructs an ssrc stats update.
  cricket::VideoMediaInfo stats_read;

  cricket::SsrcReceiverInfo remote_ssrc_stats;
  remote_ssrc_stats.timestamp = 12345.678;
  remote_ssrc_stats.ssrc = kSsrcOfTrack;
  cricket::VideoSenderInfo video_sender_info;
  video_sender_info.add_ssrc(kSsrcOfTrack);
  video_sender_info.remote_stats.push_back(remote_ssrc_stats);
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
    .WillRepeatedly(DoAll(SetArgPointee<0>(stats_read),
                          Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  StatsReports reports;
  stats.GetStats(NULL, &reports);

  const StatsReport* remote_report = FindNthReportByType(reports,
      StatsReport::kStatsReportTypeRemoteSsrc, 1);
  EXPECT_FALSE(remote_report == NULL);
  EXPECT_EQ(12345.678, remote_report->timestamp());
}

// This test verifies that the empty track report exists in the returned stats
// when StatsCollector::UpdateStats is called with ssrc stats.
TEST_F(StatsCollectorTest, ReportsFromRemoteTrack) {
  StatsCollectorForTest stats(&session_);

  const char kVideoChannelName[] = "video";
  InitSessionStats(kVideoChannelName);
  EXPECT_CALL(session_, GetTransportStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats_),
                            Return(true)));
  EXPECT_CALL(session_, GetTransport(_))
      .WillRepeatedly(Return(static_cast<cricket::Transport*>(NULL)));

  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(rtc::Thread::Current(),
      media_engine_, media_channel, NULL, kVideoChannelName, false);
  AddIncomingVideoTrackStats();
  stats.AddStream(stream_);

  // Constructs an ssrc stats update.
  cricket::VideoReceiverInfo video_receiver_info;
  cricket::VideoMediaInfo stats_read;
  const int64 kNumOfPacketsConcealed = 54321;

  // Construct a stats value to read.
  video_receiver_info.add_ssrc(1234);
  video_receiver_info.packets_concealed = kNumOfPacketsConcealed;
  stats_read.receivers.push_back(video_receiver_info);

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(stats_read),
                      Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  StatsReports reports;
  stats.GetStats(NULL, &reports);
  // |reports| should contain at least one session report, one track report,
  // and one ssrc report.
  EXPECT_LE(static_cast<size_t>(3), reports.size());
  const StatsReport* track_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeTrack, 1);
  EXPECT_TRUE(track_report);
  EXPECT_EQ(stats.GetTimeNow(), track_report->timestamp());

  std::string ssrc_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameSsrc);
  EXPECT_EQ(rtc::ToString<uint32>(kSsrcOfTrack), ssrc_id);

  std::string track_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kRemoteTrackId, track_id);
}

// This test verifies the Ice Candidate report should contain the correct
// information from local/remote candidates.
TEST_F(StatsCollectorTest, IceCandidateReport) {
  StatsCollectorForTest stats(&session_);

  StatsReports reports;                     // returned values.

  const int local_port = 2000;
  const char local_ip[] = "192.168.0.1";
  const int remote_port = 2001;
  const char remote_ip[] = "192.168.0.2";

  rtc::SocketAddress local_address(local_ip, local_port);
  rtc::SocketAddress remote_address(remote_ip, remote_port);
  rtc::AdapterType network_type = rtc::ADAPTER_TYPE_ETHERNET;
  uint32 priority = 1000;

  cricket::Candidate c;
  ASSERT(c.id().length() > 0);
  c.set_type(cricket::LOCAL_PORT_TYPE);
  c.set_protocol(cricket::UDP_PROTOCOL_NAME);
  c.set_address(local_address);
  c.set_priority(priority);
  c.set_network_type(network_type);
  std::string report_id = AddCandidateReport(&stats, c, true)->id()->ToString();
  EXPECT_EQ("Cand-" + c.id(), report_id);

  c = cricket::Candidate();
  ASSERT(c.id().length() > 0);
  c.set_type(cricket::PRFLX_PORT_TYPE);
  c.set_protocol(cricket::UDP_PROTOCOL_NAME);
  c.set_address(remote_address);
  c.set_priority(priority);
  c.set_network_type(network_type);
  report_id = AddCandidateReport(&stats, c, false)->id()->ToString();
  EXPECT_EQ("Cand-" + c.id(), report_id);

  stats.GetStats(NULL, &reports);

  // Verify the local candidate report is populated correctly.
  EXPECT_EQ(
      local_ip,
      ExtractStatsValue(StatsReport::kStatsReportTypeIceLocalCandidate, reports,
                        StatsReport::kStatsValueNameCandidateIPAddress));
  EXPECT_EQ(
      rtc::ToString<int>(local_port),
      ExtractStatsValue(StatsReport::kStatsReportTypeIceLocalCandidate, reports,
                        StatsReport::kStatsValueNameCandidatePortNumber));
  EXPECT_EQ(
      cricket::UDP_PROTOCOL_NAME,
      ExtractStatsValue(StatsReport::kStatsReportTypeIceLocalCandidate, reports,
                        StatsReport::kStatsValueNameCandidateTransportType));
  EXPECT_EQ(
      rtc::ToString<int>(priority),
      ExtractStatsValue(StatsReport::kStatsReportTypeIceLocalCandidate, reports,
                        StatsReport::kStatsValueNameCandidatePriority));
  EXPECT_EQ(
      IceCandidateTypeToStatsType(cricket::LOCAL_PORT_TYPE),
      ExtractStatsValue(StatsReport::kStatsReportTypeIceLocalCandidate, reports,
                        StatsReport::kStatsValueNameCandidateType));
  EXPECT_EQ(
      AdapterTypeToStatsType(network_type),
      ExtractStatsValue(StatsReport::kStatsReportTypeIceLocalCandidate, reports,
                        StatsReport::kStatsValueNameCandidateNetworkType));

  // Verify the remote candidate report is populated correctly.
  EXPECT_EQ(remote_ip,
            ExtractStatsValue(StatsReport::kStatsReportTypeIceRemoteCandidate,
                              reports,
                              StatsReport::kStatsValueNameCandidateIPAddress));
  EXPECT_EQ(rtc::ToString<int>(remote_port),
            ExtractStatsValue(StatsReport::kStatsReportTypeIceRemoteCandidate,
                              reports,
                              StatsReport::kStatsValueNameCandidatePortNumber));
  EXPECT_EQ(cricket::UDP_PROTOCOL_NAME,
            ExtractStatsValue(
                StatsReport::kStatsReportTypeIceRemoteCandidate, reports,
                StatsReport::kStatsValueNameCandidateTransportType));
  EXPECT_EQ(rtc::ToString<int>(priority),
            ExtractStatsValue(StatsReport::kStatsReportTypeIceRemoteCandidate,
                              reports,
                              StatsReport::kStatsValueNameCandidatePriority));
  EXPECT_EQ(
      IceCandidateTypeToStatsType(cricket::PRFLX_PORT_TYPE),
      ExtractStatsValue(StatsReport::kStatsReportTypeIceRemoteCandidate,
                        reports, StatsReport::kStatsValueNameCandidateType));
  EXPECT_EQ(kNotFound,
            ExtractStatsValue(
                StatsReport::kStatsReportTypeIceRemoteCandidate, reports,
                StatsReport::kStatsValueNameCandidateNetworkType));
}

// This test verifies that all chained certificates are correctly
// reported
TEST_F(StatsCollectorTest, ChainedCertificateReportsCreated) {
  // Build local certificate chain.
  std::vector<std::string> local_ders(5);
  local_ders[0] = "These";
  local_ders[1] = "are";
  local_ders[2] = "some";
  local_ders[3] = "der";
  local_ders[4] = "values";
  rtc::FakeSSLCertificate local_cert(DersToPems(local_ders));

  // Build remote certificate chain
  std::vector<std::string> remote_ders(4);
  remote_ders[0] = "A";
  remote_ders[1] = "non-";
  remote_ders[2] = "intersecting";
  remote_ders[3] = "set";
  rtc::FakeSSLCertificate remote_cert(DersToPems(remote_ders));

  TestCertificateReports(local_cert, local_ders, remote_cert, remote_ders);
}

// This test verifies that all certificates without chains are correctly
// reported.
TEST_F(StatsCollectorTest, ChainlessCertificateReportsCreated) {
  // Build local certificate.
  std::string local_der = "This is the local der.";
  rtc::FakeSSLCertificate local_cert(DerToPem(local_der));

  // Build remote certificate.
  std::string remote_der = "This is somebody else's der.";
  rtc::FakeSSLCertificate remote_cert(DerToPem(remote_der));

  TestCertificateReports(local_cert, std::vector<std::string>(1, local_der),
                         remote_cert, std::vector<std::string>(1, remote_der));
}

// This test verifies that the stats are generated correctly when no
// transport is present.
TEST_F(StatsCollectorTest, NoTransport) {
  StatsCollectorForTest stats(&session_);

  StatsReports reports;  // returned values.

  // Fake stats to process.
  cricket::TransportChannelStats channel_stats;
  channel_stats.component = 1;

  cricket::TransportStats transport_stats;
  transport_stats.content_name = "audio";
  transport_stats.channel_stats.push_back(channel_stats);

  cricket::SessionStats session_stats;
  session_stats.transport_stats[transport_stats.content_name] =
      transport_stats;

  // Configure MockWebRtcSession
  EXPECT_CALL(session_, GetTransport(transport_stats.content_name))
    .WillRepeatedly(ReturnNull());
  EXPECT_CALL(session_, GetTransportStats(_))
    .WillOnce(DoAll(SetArgPointee<0>(session_stats),
                    Return(true)));

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);

  // Check that the local certificate is absent.
  std::string local_certificate_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeComponent,
      reports,
      StatsReport::kStatsValueNameLocalCertificateId);
  ASSERT_EQ(kNotFound, local_certificate_id);

  // Check that the remote certificate is absent.
  std::string remote_certificate_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeComponent,
      reports,
      StatsReport::kStatsValueNameRemoteCertificateId);
  ASSERT_EQ(kNotFound, remote_certificate_id);

  // Check that the negotiated ciphers are absent.
  std::string dtls_cipher = ExtractStatsValue(
      StatsReport::kStatsReportTypeComponent,
      reports,
      StatsReport::kStatsValueNameDtlsCipher);
  ASSERT_EQ(kNotFound, dtls_cipher);
  std::string srtp_cipher = ExtractStatsValue(
      StatsReport::kStatsReportTypeComponent,
      reports,
      StatsReport::kStatsValueNameSrtpCipher);
  ASSERT_EQ(kNotFound, srtp_cipher);
}

// This test verifies that the stats are generated correctly when the transport
// does not have any certificates.
TEST_F(StatsCollectorTest, NoCertificates) {
  StatsCollectorForTest stats(&session_);

  StatsReports reports;  // returned values.

  // Fake stats to process.
  cricket::TransportChannelStats channel_stats;
  channel_stats.component = 1;

  cricket::TransportStats transport_stats;
  transport_stats.content_name = "audio";
  transport_stats.channel_stats.push_back(channel_stats);

  cricket::SessionStats session_stats;
  session_stats.transport_stats[transport_stats.content_name] =
      transport_stats;

  // Fake transport object.
  rtc::scoped_ptr<cricket::FakeTransport> transport(
      new cricket::FakeTransport(
          session_.signaling_thread(),
          session_.worker_thread(),
          transport_stats.content_name));

  // Configure MockWebRtcSession
  EXPECT_CALL(session_, GetTransport(transport_stats.content_name))
    .WillRepeatedly(Return(transport.get()));
  EXPECT_CALL(session_, GetTransportStats(_))
    .WillOnce(DoAll(SetArgPointee<0>(session_stats),
                    Return(true)));
  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);

  // Check that the local certificate is absent.
  std::string local_certificate_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeComponent,
      reports,
      StatsReport::kStatsValueNameLocalCertificateId);
  ASSERT_EQ(kNotFound, local_certificate_id);

  // Check that the remote certificate is absent.
  std::string remote_certificate_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeComponent,
      reports,
      StatsReport::kStatsValueNameRemoteCertificateId);
  ASSERT_EQ(kNotFound, remote_certificate_id);
}

// This test verifies that a remote certificate with an unsupported digest
// algorithm is correctly ignored.
TEST_F(StatsCollectorTest, UnsupportedDigestIgnored) {
  // Build a local certificate.
  std::string local_der = "This is the local der.";
  rtc::FakeSSLCertificate local_cert(DerToPem(local_der));

  // Build a remote certificate with an unsupported digest algorithm.
  std::string remote_der = "This is somebody else's der.";
  rtc::FakeSSLCertificate remote_cert(DerToPem(remote_der));
  remote_cert.set_digest_algorithm("foobar");

  TestCertificateReports(local_cert, std::vector<std::string>(1, local_der),
                         remote_cert, std::vector<std::string>());
}

// This test verifies that a local stats object can get statistics via
// AudioTrackInterface::GetStats() method.
TEST_F(StatsCollectorTest, GetStatsFromLocalAudioTrack) {
  StatsCollectorForTest stats(&session_);

  // Ignore unused callback (logspam).
  EXPECT_CALL(session_, GetTransport(_))
      .WillRepeatedly(Return(static_cast<cricket::Transport*>(NULL)));

  MockVoiceMediaChannel* media_channel = new MockVoiceMediaChannel();
  // The content_name known by the voice channel.
  const std::string kVcName("vcname");
  cricket::VoiceChannel voice_channel(rtc::Thread::Current(),
      media_engine_, media_channel, NULL, kVcName, false);
  AddOutgoingAudioTrackStats();
  stats.AddStream(stream_);
  stats.AddLocalAudioTrack(audio_track_, kSsrcOfTrack);

  cricket::VoiceSenderInfo voice_sender_info;
  InitVoiceSenderInfo(&voice_sender_info);

  cricket::VoiceMediaInfo stats_read;
  StatsReports reports;  // returned values.
  SetupAndVerifyAudioTrackStats(
      audio_track_.get(), stream_.get(), &stats, &voice_channel, kVcName,
      media_channel, &voice_sender_info, NULL, &stats_read, &reports);

  // Verify that there is no remote report for the local audio track because
  // we did not set it up.
  const StatsReport* remote_report = FindNthReportByType(reports,
      StatsReport::kStatsReportTypeRemoteSsrc, 1);
  EXPECT_TRUE(remote_report == NULL);
}

// This test verifies that audio receive streams populate stats reports
// correctly.
TEST_F(StatsCollectorTest, GetStatsFromRemoteStream) {
  StatsCollectorForTest stats(&session_);

  // Ignore unused callback (logspam).
  EXPECT_CALL(session_, GetTransport(_))
      .WillRepeatedly(Return(static_cast<cricket::Transport*>(NULL)));
  MockVoiceMediaChannel* media_channel = new MockVoiceMediaChannel();
  // The content_name known by the voice channel.
  const std::string kVcName("vcname");
  cricket::VoiceChannel voice_channel(rtc::Thread::Current(),
      media_engine_, media_channel, NULL, kVcName, false);
  AddIncomingAudioTrackStats();
  stats.AddStream(stream_);

  cricket::VoiceReceiverInfo voice_receiver_info;
  InitVoiceReceiverInfo(&voice_receiver_info);
  voice_receiver_info.codec_name = "fake_codec";

  cricket::VoiceMediaInfo stats_read;
  StatsReports reports;  // returned values.
  SetupAndVerifyAudioTrackStats(
      audio_track_.get(), stream_.get(), &stats, &voice_channel, kVcName,
      media_channel, NULL, &voice_receiver_info, &stats_read, &reports);
}

// This test verifies that a local stats object won't update its statistics
// after a RemoveLocalAudioTrack() call.
TEST_F(StatsCollectorTest, GetStatsAfterRemoveAudioStream) {
  StatsCollectorForTest stats(&session_);

  // Ignore unused callback (logspam).
  EXPECT_CALL(session_, GetTransport(_))
      .WillRepeatedly(Return(static_cast<cricket::Transport*>(NULL)));
  MockVoiceMediaChannel* media_channel = new MockVoiceMediaChannel();
  // The content_name known by the voice channel.
  const std::string kVcName("vcname");
  cricket::VoiceChannel voice_channel(rtc::Thread::Current(),
      media_engine_, media_channel, NULL, kVcName, false);
  AddOutgoingAudioTrackStats();
  stats.AddStream(stream_);
  stats.AddLocalAudioTrack(audio_track_.get(), kSsrcOfTrack);

  // Instruct the session to return stats containing the transport channel.
  InitSessionStats(kVcName);
  EXPECT_CALL(session_, GetTransportStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats_),
                            Return(true)));

  stats.RemoveLocalAudioTrack(audio_track_.get(), kSsrcOfTrack);
  cricket::VoiceSenderInfo voice_sender_info;
  InitVoiceSenderInfo(&voice_sender_info);

  // Constructs an ssrc stats update.
  cricket::VoiceMediaInfo stats_read;
  stats_read.senders.push_back(voice_sender_info);

  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(Return(&voice_channel));
  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(stats_read),
                            Return(true)));

  StatsReports reports;  // returned values.
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);

  // The report will exist since we don't remove them in RemoveStream().
  const StatsReport* report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSsrc, 1);
  EXPECT_FALSE(report == NULL);
  EXPECT_EQ(stats.GetTimeNow(), report->timestamp());
  std::string track_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kLocalTrackId, track_id);
  std::string ssrc_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameSsrc);
  EXPECT_EQ(rtc::ToString<uint32>(kSsrcOfTrack), ssrc_id);

  // Verifies the values in the track report, no value will be changed by the
  // AudioTrackInterface::GetSignalValue() and
  // AudioProcessorInterface::AudioProcessorStats::GetStats();
  VerifyVoiceSenderInfoReport(report, voice_sender_info);
}

// This test verifies that when ongoing and incoming audio tracks are using
// the same ssrc, they populate stats reports correctly.
TEST_F(StatsCollectorTest, LocalAndRemoteTracksWithSameSsrc) {
  StatsCollectorForTest stats(&session_);

  // Ignore unused callback (logspam).
  EXPECT_CALL(session_, GetTransport(_))
      .WillRepeatedly(Return(static_cast<cricket::Transport*>(NULL)));
  MockVoiceMediaChannel* media_channel = new MockVoiceMediaChannel();
  // The content_name known by the voice channel.
  const std::string kVcName("vcname");
  cricket::VoiceChannel voice_channel(rtc::Thread::Current(),
      media_engine_, media_channel, NULL, kVcName, false);

  // Create a local stream with a local audio track and adds it to the stats.
  AddOutgoingAudioTrackStats();
  stats.AddStream(stream_);
  stats.AddLocalAudioTrack(audio_track_.get(), kSsrcOfTrack);

  // Create a remote stream with a remote audio track and adds it to the stats.
  rtc::scoped_refptr<webrtc::MediaStream> remote_stream(
      webrtc::MediaStream::Create("remotestreamlabel"));
  rtc::scoped_refptr<FakeAudioTrack> remote_track(
      new rtc::RefCountedObject<FakeAudioTrack>(kRemoteTrackId));
  EXPECT_CALL(session_, GetRemoteTrackIdBySsrc(kSsrcOfTrack, _))
      .WillOnce(DoAll(SetArgPointee<1>(kRemoteTrackId), Return(true)));
  remote_stream->AddTrack(remote_track);
  stats.AddStream(remote_stream);

  // Instruct the session to return stats containing the transport channel.
  InitSessionStats(kVcName);
  EXPECT_CALL(session_, GetTransportStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats_),
                            Return(true)));

  cricket::VoiceSenderInfo voice_sender_info;
  InitVoiceSenderInfo(&voice_sender_info);

  // Some of the contents in |voice_sender_info| needs to be updated from the
  // |audio_track_|.
  UpdateVoiceSenderInfoFromAudioTrack(audio_track_.get(), &voice_sender_info);

  cricket::VoiceReceiverInfo voice_receiver_info;
  InitVoiceReceiverInfo(&voice_receiver_info);

  // Constructs an ssrc stats update.
  cricket::VoiceMediaInfo stats_read;
  stats_read.senders.push_back(voice_sender_info);
  stats_read.receivers.push_back(voice_receiver_info);

  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(Return(&voice_channel));
  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(stats_read),
                            Return(true)));

  StatsReports reports;  // returned values.
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);

  // Get stats for the local track.
  stats.GetStats(audio_track_.get(), &reports);
  const StatsReport* track_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSsrc, 1);
  EXPECT_TRUE(track_report);
  EXPECT_EQ(stats.GetTimeNow(), track_report->timestamp());
  std::string track_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kLocalTrackId, track_id);
  VerifyVoiceSenderInfoReport(track_report, voice_sender_info);

  // Get stats for the remote track.
  reports.clear();
  stats.GetStats(remote_track.get(), &reports);
  track_report = FindNthReportByType(reports,
                                     StatsReport::kStatsReportTypeSsrc, 1);
  EXPECT_TRUE(track_report);
  EXPECT_EQ(stats.GetTimeNow(), track_report->timestamp());
  track_id = ExtractSsrcStatsValue(reports,
                                   StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kRemoteTrackId, track_id);
  VerifyVoiceReceiverInfoReport(track_report, voice_receiver_info);
}

// This test verifies that when two outgoing audio tracks are using the same
// ssrc at different times, they populate stats reports correctly.
// TODO(xians): Figure out if it is possible to encapsulate the setup and
// avoid duplication of code in test cases.
TEST_F(StatsCollectorTest, TwoLocalTracksWithSameSsrc) {
  StatsCollectorForTest stats(&session_);

  // Ignore unused callback (logspam).
  EXPECT_CALL(session_, GetTransport(_))
      .WillRepeatedly(Return(static_cast<cricket::Transport*>(NULL)));
  MockVoiceMediaChannel* media_channel = new MockVoiceMediaChannel();
  // The content_name known by the voice channel.
  const std::string kVcName("vcname");
  cricket::VoiceChannel voice_channel(rtc::Thread::Current(),
      media_engine_, media_channel, NULL, kVcName, false);

  // Create a local stream with a local audio track and adds it to the stats.
  AddOutgoingAudioTrackStats();
  stats.AddStream(stream_);
  stats.AddLocalAudioTrack(audio_track_, kSsrcOfTrack);

  cricket::VoiceSenderInfo voice_sender_info;
  voice_sender_info.add_ssrc(kSsrcOfTrack);

  cricket::VoiceMediaInfo stats_read;
  StatsReports reports;  // returned values.
  SetupAndVerifyAudioTrackStats(
      audio_track_.get(), stream_.get(), &stats, &voice_channel, kVcName,
      media_channel, &voice_sender_info, NULL, &stats_read, &reports);

  // Remove the previous audio track from the stream.
  stream_->RemoveTrack(audio_track_.get());
  stats.RemoveLocalAudioTrack(audio_track_.get(), kSsrcOfTrack);

  // Create a new audio track and adds it to the stream and stats.
  static const std::string kNewTrackId = "new_track_id";
  rtc::scoped_refptr<FakeAudioTrack> new_audio_track(
      new rtc::RefCountedObject<FakeAudioTrack>(kNewTrackId));
  EXPECT_CALL(session_, GetLocalTrackIdBySsrc(kSsrcOfTrack, _))
      .WillOnce(DoAll(SetArgPointee<1>(kNewTrackId), Return(true)));
  stream_->AddTrack(new_audio_track);

  stats.AddLocalAudioTrack(new_audio_track, kSsrcOfTrack);
  stats.ClearUpdateStatsCacheForTest();
  cricket::VoiceSenderInfo new_voice_sender_info;
  InitVoiceSenderInfo(&new_voice_sender_info);
  cricket::VoiceMediaInfo new_stats_read;
  reports.clear();
  SetupAndVerifyAudioTrackStats(
      new_audio_track.get(), stream_.get(), &stats, &voice_channel, kVcName,
      media_channel, &new_voice_sender_info, NULL, &new_stats_read, &reports);
}

}  // namespace webrtc
