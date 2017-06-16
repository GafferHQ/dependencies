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

// This file contains mock implementations of observers used in PeerConnection.

#ifndef TALK_APP_WEBRTC_TEST_MOCKPEERCONNECTIONOBSERVERS_H_
#define TALK_APP_WEBRTC_TEST_MOCKPEERCONNECTIONOBSERVERS_H_

#include <string>

#include "talk/app/webrtc/datachannelinterface.h"

namespace webrtc {

class MockCreateSessionDescriptionObserver
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  MockCreateSessionDescriptionObserver()
      : called_(false),
        result_(false) {}
  virtual ~MockCreateSessionDescriptionObserver() {}
  virtual void OnSuccess(SessionDescriptionInterface* desc) {
    called_ = true;
    result_ = true;
    desc_.reset(desc);
  }
  virtual void OnFailure(const std::string& error) {
    called_ = true;
    result_ = false;
  }
  bool called() const { return called_; }
  bool result() const { return result_; }
  SessionDescriptionInterface* release_desc() {
    return desc_.release();
  }

 private:
  bool called_;
  bool result_;
  rtc::scoped_ptr<SessionDescriptionInterface> desc_;
};

class MockSetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  MockSetSessionDescriptionObserver()
      : called_(false),
        result_(false) {}
  virtual ~MockSetSessionDescriptionObserver() {}
  virtual void OnSuccess() {
    called_ = true;
    result_ = true;
  }
  virtual void OnFailure(const std::string& error) {
    called_ = true;
    result_ = false;
  }
  bool called() const { return called_; }
  bool result() const { return result_; }

 private:
  bool called_;
  bool result_;
};

class MockDataChannelObserver : public webrtc::DataChannelObserver {
 public:
  explicit MockDataChannelObserver(webrtc::DataChannelInterface* channel)
     : channel_(channel), received_message_count_(0) {
    channel_->RegisterObserver(this);
    state_ = channel_->state();
  }
  virtual ~MockDataChannelObserver() {
    channel_->UnregisterObserver();
  }

  void OnBufferedAmountChange(uint64 previous_amount) override {}

  void OnStateChange() override { state_ = channel_->state(); }
  void OnMessage(const DataBuffer& buffer) override {
    last_message_.assign(buffer.data.data<char>(), buffer.data.size());
    ++received_message_count_;
  }

  bool IsOpen() const { return state_ == DataChannelInterface::kOpen; }
  const std::string& last_message() const { return last_message_; }
  size_t received_message_count() const { return received_message_count_; }

 private:
  rtc::scoped_refptr<webrtc::DataChannelInterface> channel_;
  DataChannelInterface::DataState state_;
  std::string last_message_;
  size_t received_message_count_;
};

class MockStatsObserver : public webrtc::StatsObserver {
 public:
  MockStatsObserver() : called_(false), stats_() {}
  virtual ~MockStatsObserver() {}

  virtual void OnComplete(const StatsReports& reports) {
    ASSERT(!called_);
    called_ = true;
    stats_.Clear();
    stats_.number_of_reports = reports.size();
    for (const auto* r : reports) {
      if (r->type() == StatsReport::kStatsReportTypeSsrc) {
        stats_.timestamp = r->timestamp();
        GetIntValue(r, StatsReport::kStatsValueNameAudioOutputLevel,
            &stats_.audio_output_level);
        GetIntValue(r, StatsReport::kStatsValueNameAudioInputLevel,
            &stats_.audio_input_level);
        GetIntValue(r, StatsReport::kStatsValueNameBytesReceived,
            &stats_.bytes_received);
        GetIntValue(r, StatsReport::kStatsValueNameBytesSent,
            &stats_.bytes_sent);
      } else if (r->type() == StatsReport::kStatsReportTypeBwe) {
        stats_.timestamp = r->timestamp();
        GetIntValue(r, StatsReport::kStatsValueNameAvailableReceiveBandwidth,
            &stats_.available_receive_bandwidth);
      } else if (r->type() == StatsReport::kStatsReportTypeComponent) {
        stats_.timestamp = r->timestamp();
        GetStringValue(r, StatsReport::kStatsValueNameDtlsCipher,
            &stats_.dtls_cipher);
        GetStringValue(r, StatsReport::kStatsValueNameSrtpCipher,
            &stats_.srtp_cipher);
      }
    }
  }

  bool called() const { return called_; }
  size_t number_of_reports() const { return stats_.number_of_reports; }
  double timestamp() const { return stats_.timestamp; }

  int AudioOutputLevel() const {
    ASSERT(called_);
    return stats_.audio_output_level;
  }

  int AudioInputLevel() const {
    ASSERT(called_);
    return stats_.audio_input_level;
  }

  int BytesReceived() const {
    ASSERT(called_);
    return stats_.bytes_received;
  }

  int BytesSent() const {
    ASSERT(called_);
    return stats_.bytes_sent;
  }

  int AvailableReceiveBandwidth() const {
    ASSERT(called_);
    return stats_.available_receive_bandwidth;
  }

  std::string DtlsCipher() const {
    ASSERT(called_);
    return stats_.dtls_cipher;
  }

  std::string SrtpCipher() const {
    ASSERT(called_);
    return stats_.srtp_cipher;
  }

 private:
  bool GetIntValue(const StatsReport* report,
                   StatsReport::StatsValueName name,
                   int* value) {
    const StatsReport::Value* v = report->FindValue(name);
    if (v) {
      // TODO(tommi): We should really just be using an int here :-/
      *value = rtc::FromString<int>(v->ToString());
    }
    return v != nullptr;
  }

  bool GetStringValue(const StatsReport* report,
                      StatsReport::StatsValueName name,
                      std::string* value) {
    const StatsReport::Value* v = report->FindValue(name);
    if (v)
      *value = v->ToString();
    return v != nullptr;
  }

  bool called_;
  struct {
    void Clear() {
      number_of_reports = 0;
      timestamp = 0;
      audio_output_level = 0;
      audio_input_level = 0;
      bytes_received = 0;
      bytes_sent = 0;
      available_receive_bandwidth = 0;
      dtls_cipher.clear();
      srtp_cipher.clear();
    }

    size_t number_of_reports;
    double timestamp;
    int audio_output_level;
    int audio_input_level;
    int bytes_received;
    int bytes_sent;
    int available_receive_bandwidth;
    std::string dtls_cipher;
    std::string srtp_cipher;
  } stats_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_TEST_MOCKPEERCONNECTIONOBSERVERS_H_
