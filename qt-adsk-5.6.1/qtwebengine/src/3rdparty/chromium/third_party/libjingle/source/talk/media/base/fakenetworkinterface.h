/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#ifndef TALK_MEDIA_BASE_FAKENETWORKINTERFACE_H_
#define TALK_MEDIA_BASE_FAKENETWORKINTERFACE_H_

#include <map>
#include <vector>

#include "talk/media/base/mediachannel.h"
#include "talk/media/base/rtputils.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/byteorder.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/dscp.h"
#include "webrtc/base/messagehandler.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/thread.h"

namespace cricket {

// Fake NetworkInterface that sends/receives RTP/RTCP packets.
class FakeNetworkInterface : public MediaChannel::NetworkInterface,
                             public rtc::MessageHandler {
 public:
  FakeNetworkInterface()
      : thread_(rtc::Thread::Current()),
        dest_(NULL),
        conf_(false),
        sendbuf_size_(-1),
        recvbuf_size_(-1),
        dscp_(rtc::DSCP_NO_CHANGE) {
  }

  void SetDestination(MediaChannel* dest) { dest_ = dest; }

  // Conference mode is a mode where instead of simply forwarding the packets,
  // the transport will send multiple copies of the packet with the specified
  // SSRCs. This allows us to simulate receiving media from multiple sources.
  void SetConferenceMode(bool conf, const std::vector<uint32>& ssrcs) {
    rtc::CritScope cs(&crit_);
    conf_ = conf;
    conf_sent_ssrcs_ = ssrcs;
  }

  int NumRtpBytes() {
    rtc::CritScope cs(&crit_);
    int bytes = 0;
    for (size_t i = 0; i < rtp_packets_.size(); ++i) {
      bytes += static_cast<int>(rtp_packets_[i].size());
    }
    return bytes;
  }

  int NumRtpBytes(uint32 ssrc) {
    rtc::CritScope cs(&crit_);
    int bytes = 0;
    GetNumRtpBytesAndPackets(ssrc, &bytes, NULL);
    return bytes;
  }

  int NumRtpPackets() {
    rtc::CritScope cs(&crit_);
    return static_cast<int>(rtp_packets_.size());
  }

  int NumRtpPackets(uint32 ssrc) {
    rtc::CritScope cs(&crit_);
    int packets = 0;
    GetNumRtpBytesAndPackets(ssrc, NULL, &packets);
    return packets;
  }

  int NumSentSsrcs() {
    rtc::CritScope cs(&crit_);
    return static_cast<int>(sent_ssrcs_.size());
  }

  // Note: callers are responsible for deleting the returned buffer.
  const rtc::Buffer* GetRtpPacket(int index) {
    rtc::CritScope cs(&crit_);
    if (index >= NumRtpPackets()) {
      return NULL;
    }
    return new rtc::Buffer(rtp_packets_[index]);
  }

  int NumRtcpPackets() {
    rtc::CritScope cs(&crit_);
    return static_cast<int>(rtcp_packets_.size());
  }

  // Note: callers are responsible for deleting the returned buffer.
  const rtc::Buffer* GetRtcpPacket(int index) {
    rtc::CritScope cs(&crit_);
    if (index >= NumRtcpPackets()) {
      return NULL;
    }
    return new rtc::Buffer(rtcp_packets_[index]);
  }

  // Indicate that |n|'th packet for |ssrc| should be dropped.
  void AddPacketDrop(uint32 ssrc, uint32 n) {
    drop_map_[ssrc].insert(n);
  }

  int sendbuf_size() const { return sendbuf_size_; }
  int recvbuf_size() const { return recvbuf_size_; }
  rtc::DiffServCodePoint dscp() const { return dscp_; }

 protected:
  virtual bool SendPacket(rtc::Buffer* packet,
                          rtc::DiffServCodePoint dscp) {
    rtc::CritScope cs(&crit_);

    uint32 cur_ssrc = 0;
    if (!GetRtpSsrc(packet->data(), packet->size(), &cur_ssrc)) {
      return false;
    }
    sent_ssrcs_[cur_ssrc]++;

    // Check if we need to drop this packet.
    std::map<uint32, std::set<uint32> >::iterator itr =
      drop_map_.find(cur_ssrc);
    if (itr != drop_map_.end() &&
        itr->second.count(sent_ssrcs_[cur_ssrc]) > 0) {
        // "Drop" the packet.
        return true;
    }

    rtp_packets_.push_back(*packet);
    if (conf_) {
      rtc::Buffer buffer_copy(*packet);
      for (size_t i = 0; i < conf_sent_ssrcs_.size(); ++i) {
        if (!SetRtpSsrc(buffer_copy.data(), buffer_copy.size(),
                        conf_sent_ssrcs_[i])) {
          return false;
        }
        PostMessage(ST_RTP, buffer_copy);
      }
    } else {
      PostMessage(ST_RTP, *packet);
    }
    return true;
  }

  virtual bool SendRtcp(rtc::Buffer* packet,
                        rtc::DiffServCodePoint dscp) {
    rtc::CritScope cs(&crit_);
    rtcp_packets_.push_back(*packet);
    if (!conf_) {
      // don't worry about RTCP in conf mode for now
      PostMessage(ST_RTCP, *packet);
    }
    return true;
  }

  virtual int SetOption(SocketType type, rtc::Socket::Option opt,
                        int option) {
    if (opt == rtc::Socket::OPT_SNDBUF) {
      sendbuf_size_ = option;
    } else if (opt == rtc::Socket::OPT_RCVBUF) {
      recvbuf_size_ = option;
    } else if (opt == rtc::Socket::OPT_DSCP) {
      dscp_ = static_cast<rtc::DiffServCodePoint>(option);
    }
    return 0;
  }

  void PostMessage(int id, const rtc::Buffer& packet) {
    thread_->Post(this, id, rtc::WrapMessageData(packet));
  }

  virtual void OnMessage(rtc::Message* msg) {
    rtc::TypedMessageData<rtc::Buffer>* msg_data =
        static_cast<rtc::TypedMessageData<rtc::Buffer>*>(
            msg->pdata);
    if (dest_) {
      if (msg->message_id == ST_RTP) {
        dest_->OnPacketReceived(&msg_data->data(),
                                rtc::CreatePacketTime(0));
      } else {
        dest_->OnRtcpReceived(&msg_data->data(),
                              rtc::CreatePacketTime(0));
      }
    }
    delete msg_data;
  }

 private:
  void GetNumRtpBytesAndPackets(uint32 ssrc, int* bytes, int* packets) {
    if (bytes) {
      *bytes = 0;
    }
    if (packets) {
      *packets = 0;
    }
    uint32 cur_ssrc = 0;
    for (size_t i = 0; i < rtp_packets_.size(); ++i) {
      if (!GetRtpSsrc(rtp_packets_[i].data(), rtp_packets_[i].size(),
                      &cur_ssrc)) {
        return;
      }
      if (ssrc == cur_ssrc) {
        if (bytes) {
          *bytes += static_cast<int>(rtp_packets_[i].size());
        }
        if (packets) {
          ++(*packets);
        }
      }
    }
  }

  rtc::Thread* thread_;
  MediaChannel* dest_;
  bool conf_;
  // The ssrcs used in sending out packets in conference mode.
  std::vector<uint32> conf_sent_ssrcs_;
  // Map to track counts of packets that have been sent per ssrc.
  // This includes packets that are dropped.
  std::map<uint32, uint32> sent_ssrcs_;
  // Map to track packet-number that needs to be dropped per ssrc.
  std::map<uint32, std::set<uint32> > drop_map_;
  rtc::CriticalSection crit_;
  std::vector<rtc::Buffer> rtp_packets_;
  std::vector<rtc::Buffer> rtcp_packets_;
  int sendbuf_size_;
  int recvbuf_size_;
  rtc::DiffServCodePoint dscp_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_FAKENETWORKINTERFACE_H_
