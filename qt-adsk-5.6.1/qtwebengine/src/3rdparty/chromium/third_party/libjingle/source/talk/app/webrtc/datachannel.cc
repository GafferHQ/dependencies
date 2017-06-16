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

#include "talk/app/webrtc/datachannel.h"

#include <string>

#include "talk/app/webrtc/mediastreamprovider.h"
#include "talk/app/webrtc/sctputils.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/refcount.h"

namespace webrtc {

static size_t kMaxQueuedReceivedDataBytes = 16 * 1024 * 1024;
static size_t kMaxQueuedSendDataBytes = 16 * 1024 * 1024;

enum {
  MSG_CHANNELREADY,
};

DataChannel::PacketQueue::PacketQueue() : byte_count_(0) {}

DataChannel::PacketQueue::~PacketQueue() {
  Clear();
}

bool DataChannel::PacketQueue::Empty() const {
  return packets_.empty();
}

DataBuffer* DataChannel::PacketQueue::Front() {
  return packets_.front();
}

void DataChannel::PacketQueue::Pop() {
  if (packets_.empty()) {
    return;
  }

  byte_count_ -= packets_.front()->size();
  packets_.pop_front();
}

void DataChannel::PacketQueue::Push(DataBuffer* packet) {
  byte_count_ += packet->size();
  packets_.push_back(packet);
}

void DataChannel::PacketQueue::Clear() {
  while (!packets_.empty()) {
    delete packets_.front();
    packets_.pop_front();
  }
  byte_count_ = 0;
}

void DataChannel::PacketQueue::Swap(PacketQueue* other) {
  size_t other_byte_count = other->byte_count_;
  other->byte_count_ = byte_count_;
  byte_count_ = other_byte_count;

  other->packets_.swap(packets_);
}

rtc::scoped_refptr<DataChannel> DataChannel::Create(
    DataChannelProviderInterface* provider,
    cricket::DataChannelType dct,
    const std::string& label,
    const InternalDataChannelInit& config) {
  rtc::scoped_refptr<DataChannel> channel(
      new rtc::RefCountedObject<DataChannel>(provider, dct, label));
  if (!channel->Init(config)) {
    return NULL;
  }
  return channel;
}

DataChannel::DataChannel(
    DataChannelProviderInterface* provider,
    cricket::DataChannelType dct,
    const std::string& label)
    : label_(label),
      observer_(NULL),
      state_(kConnecting),
      data_channel_type_(dct),
      provider_(provider),
      handshake_state_(kHandshakeInit),
      connected_to_provider_(false),
      send_ssrc_set_(false),
      receive_ssrc_set_(false),
      writable_(false),
      send_ssrc_(0),
      receive_ssrc_(0) {
}

bool DataChannel::Init(const InternalDataChannelInit& config) {
  if (data_channel_type_ == cricket::DCT_RTP) {
    if (config.reliable ||
        config.id != -1 ||
        config.maxRetransmits != -1 ||
        config.maxRetransmitTime != -1) {
      LOG(LS_ERROR) << "Failed to initialize the RTP data channel due to "
                    << "invalid DataChannelInit.";
      return false;
    }
    handshake_state_ = kHandshakeReady;
  } else if (data_channel_type_ == cricket::DCT_SCTP) {
    if (config.id < -1 ||
        config.maxRetransmits < -1 ||
        config.maxRetransmitTime < -1) {
      LOG(LS_ERROR) << "Failed to initialize the SCTP data channel due to "
                    << "invalid DataChannelInit.";
      return false;
    }
    if (config.maxRetransmits != -1 && config.maxRetransmitTime != -1) {
      LOG(LS_ERROR) <<
          "maxRetransmits and maxRetransmitTime should not be both set.";
      return false;
    }
    config_ = config;

    switch (config_.open_handshake_role) {
    case webrtc::InternalDataChannelInit::kNone:  // pre-negotiated
      handshake_state_ = kHandshakeReady;
      break;
    case webrtc::InternalDataChannelInit::kOpener:
      handshake_state_ = kHandshakeShouldSendOpen;
      break;
    case webrtc::InternalDataChannelInit::kAcker:
      handshake_state_ = kHandshakeShouldSendAck;
      break;
    };

    // Try to connect to the transport in case the transport channel already
    // exists.
    OnTransportChannelCreated();

    // Checks if the transport is ready to send because the initial channel
    // ready signal may have been sent before the DataChannel creation.
    // This has to be done async because the upper layer objects (e.g.
    // Chrome glue and WebKit) are not wired up properly until after this
    // function returns.
    if (provider_->ReadyToSendData()) {
      rtc::Thread::Current()->Post(this, MSG_CHANNELREADY, NULL);
    }
  }

  return true;
}

DataChannel::~DataChannel() {}

void DataChannel::RegisterObserver(DataChannelObserver* observer) {
  observer_ = observer;
  DeliverQueuedReceivedData();
}

void DataChannel::UnregisterObserver() {
  observer_ = NULL;
}

bool DataChannel::reliable() const {
  if (data_channel_type_ == cricket::DCT_RTP) {
    return false;
  } else {
    return config_.maxRetransmits == -1 &&
           config_.maxRetransmitTime == -1;
  }
}

uint64 DataChannel::buffered_amount() const {
  return queued_send_data_.byte_count();
}

void DataChannel::Close() {
  if (state_ == kClosed)
    return;
  send_ssrc_ = 0;
  send_ssrc_set_ = false;
  SetState(kClosing);
  UpdateState();
}

bool DataChannel::Send(const DataBuffer& buffer) {
  if (state_ != kOpen) {
    return false;
  }

  // TODO(jiayl): the spec is unclear about if the remote side should get the
  // onmessage event. We need to figure out the expected behavior and change the
  // code accordingly.
  if (buffer.size() == 0) {
    return true;
  }

  // If the queue is non-empty, we're waiting for SignalReadyToSend,
  // so just add to the end of the queue and keep waiting.
  if (!queued_send_data_.Empty()) {
    // Only SCTP DataChannel queues the outgoing data when the transport is
    // blocked.
    ASSERT(data_channel_type_ == cricket::DCT_SCTP);
    if (!QueueSendDataMessage(buffer)) {
      Close();
    }
    return true;
  }

  bool success = SendDataMessage(buffer, true);
  if (data_channel_type_ == cricket::DCT_RTP) {
    return success;
  }

  // Always return true for SCTP DataChannel per the spec.
  return true;
}

void DataChannel::SetReceiveSsrc(uint32 receive_ssrc) {
  ASSERT(data_channel_type_ == cricket::DCT_RTP);

  if (receive_ssrc_set_) {
    return;
  }
  receive_ssrc_ = receive_ssrc;
  receive_ssrc_set_ = true;
  UpdateState();
}

// The remote peer request that this channel shall be closed.
void DataChannel::RemotePeerRequestClose() {
  DoClose();
}

void DataChannel::SetSctpSid(int sid) {
  ASSERT(config_.id < 0 && sid >= 0 && data_channel_type_ == cricket::DCT_SCTP);
  if (config_.id == sid)
    return;

  config_.id = sid;
  provider_->AddSctpDataStream(sid);
}

void DataChannel::OnTransportChannelCreated() {
  ASSERT(data_channel_type_ == cricket::DCT_SCTP);
  if (!connected_to_provider_) {
    connected_to_provider_ = provider_->ConnectDataChannel(this);
  }
  // The sid may have been unassigned when provider_->ConnectDataChannel was
  // done. So always add the streams even if connected_to_provider_ is true.
  if (config_.id >= 0) {
    provider_->AddSctpDataStream(config_.id);
  }
}

void DataChannel::SetSendSsrc(uint32 send_ssrc) {
  ASSERT(data_channel_type_ == cricket::DCT_RTP);
  if (send_ssrc_set_) {
    return;
  }
  send_ssrc_ = send_ssrc;
  send_ssrc_set_ = true;
  UpdateState();
}

void DataChannel::OnMessage(rtc::Message* msg) {
  switch (msg->message_id) {
    case MSG_CHANNELREADY:
      OnChannelReady(true);
      break;
  }
}

// The underlaying data engine is closing.
// This function makes sure the DataChannel is disconnected and changes state to
// kClosed.
void DataChannel::OnDataEngineClose() {
  DoClose();
}

void DataChannel::OnDataReceived(cricket::DataChannel* channel,
                                 const cricket::ReceiveDataParams& params,
                                 const rtc::Buffer& payload) {
  uint32 expected_ssrc =
      (data_channel_type_ == cricket::DCT_RTP) ? receive_ssrc_ : config_.id;
  if (params.ssrc != expected_ssrc) {
    return;
  }

  if (params.type == cricket::DMT_CONTROL) {
    ASSERT(data_channel_type_ == cricket::DCT_SCTP);
    if (handshake_state_ != kHandshakeWaitingForAck) {
      // Ignore it if we are not expecting an ACK message.
      LOG(LS_WARNING) << "DataChannel received unexpected CONTROL message, "
                      << "sid = " << params.ssrc;
      return;
    }
    if (ParseDataChannelOpenAckMessage(payload)) {
      // We can send unordered as soon as we receive the ACK message.
      handshake_state_ = kHandshakeReady;
      LOG(LS_INFO) << "DataChannel received OPEN_ACK message, sid = "
                   << params.ssrc;
    } else {
      LOG(LS_WARNING) << "DataChannel failed to parse OPEN_ACK message, sid = "
                      << params.ssrc;
    }
    return;
  }

  ASSERT(params.type == cricket::DMT_BINARY ||
         params.type == cricket::DMT_TEXT);

  LOG(LS_VERBOSE) << "DataChannel received DATA message, sid = " << params.ssrc;
  // We can send unordered as soon as we receive any DATA message since the
  // remote side must have received the OPEN (and old clients do not send
  // OPEN_ACK).
  if (handshake_state_ == kHandshakeWaitingForAck) {
    handshake_state_ = kHandshakeReady;
  }

  bool binary = (params.type == cricket::DMT_BINARY);
  rtc::scoped_ptr<DataBuffer> buffer(new DataBuffer(payload, binary));
  if (state_ == kOpen && observer_) {
    observer_->OnMessage(*buffer.get());
  } else {
    if (queued_received_data_.byte_count() + payload.size() >
        kMaxQueuedReceivedDataBytes) {
      LOG(LS_ERROR) << "Queued received data exceeds the max buffer size.";

      queued_received_data_.Clear();
      if (data_channel_type_ != cricket::DCT_RTP) {
        Close();
      }

      return;
    }
    queued_received_data_.Push(buffer.release());
  }
}

void DataChannel::OnChannelReady(bool writable) {
  writable_ = writable;
  if (!writable) {
    return;
  }

  SendQueuedControlMessages();
  SendQueuedDataMessages();
  UpdateState();
}

void DataChannel::DoClose() {
  if (state_ == kClosed)
    return;

  receive_ssrc_set_ = false;
  send_ssrc_set_ = false;
  SetState(kClosing);
  UpdateState();
}

void DataChannel::UpdateState() {
  // UpdateState determines what to do from a few state variables.  Include
  // all conditions required for each state transition here for
  // clarity. OnChannelReady(true) will send any queued data and then invoke
  // UpdateState().
  switch (state_) {
    case kConnecting: {
      if (send_ssrc_set_ == receive_ssrc_set_) {
        if (data_channel_type_ == cricket::DCT_RTP && !connected_to_provider_) {
          connected_to_provider_ = provider_->ConnectDataChannel(this);
        }
        if (connected_to_provider_) {
          if (handshake_state_ == kHandshakeShouldSendOpen) {
            rtc::Buffer payload;
            WriteDataChannelOpenMessage(label_, config_, &payload);
            SendControlMessage(payload);
          } else if (handshake_state_ == kHandshakeShouldSendAck) {
            rtc::Buffer payload;
            WriteDataChannelOpenAckMessage(&payload);
            SendControlMessage(payload);
          }
          if (writable_ &&
              (handshake_state_ == kHandshakeReady ||
               handshake_state_ == kHandshakeWaitingForAck)) {
            SetState(kOpen);
            // If we have received buffers before the channel got writable.
            // Deliver them now.
            DeliverQueuedReceivedData();
          }
        }
      }
      break;
    }
    case kOpen: {
      break;
    }
    case kClosing: {
      if (queued_send_data_.Empty() && queued_control_data_.Empty()) {
        if (connected_to_provider_) {
          DisconnectFromProvider();
        }

        if (!connected_to_provider_ && !send_ssrc_set_ && !receive_ssrc_set_) {
          SetState(kClosed);
        }
      }
      break;
    }
    case kClosed:
      break;
  }
}

void DataChannel::SetState(DataState state) {
  if (state_ == state)
    return;

  state_ = state;
  if (observer_) {
    observer_->OnStateChange();
  }
}

void DataChannel::DisconnectFromProvider() {
  if (!connected_to_provider_)
    return;

  provider_->DisconnectDataChannel(this);
  connected_to_provider_ = false;

  if (data_channel_type_ == cricket::DCT_SCTP && config_.id >= 0) {
    provider_->RemoveSctpDataStream(config_.id);
  }
}

void DataChannel::DeliverQueuedReceivedData() {
  if (!observer_) {
    return;
  }

  while (!queued_received_data_.Empty()) {
    rtc::scoped_ptr<DataBuffer> buffer(queued_received_data_.Front());
    observer_->OnMessage(*buffer);
    queued_received_data_.Pop();
  }
}

void DataChannel::SendQueuedDataMessages() {
  if (queued_send_data_.Empty()) {
    return;
  }

  ASSERT(state_ == kOpen || state_ == kClosing);

  uint64 start_buffered_amount = buffered_amount();
  while (!queued_send_data_.Empty()) {
    DataBuffer* buffer = queued_send_data_.Front();
    if (!SendDataMessage(*buffer, false)) {
      // Leave the message in the queue if sending is aborted.
      break;
    }
    queued_send_data_.Pop();
    delete buffer;
  }

  if (observer_ && buffered_amount() < start_buffered_amount) {
    observer_->OnBufferedAmountChange(start_buffered_amount);
  }
}

bool DataChannel::SendDataMessage(const DataBuffer& buffer,
                                  bool queue_if_blocked) {
  cricket::SendDataParams send_params;

  if (data_channel_type_ == cricket::DCT_SCTP) {
    send_params.ordered = config_.ordered;
    // Send as ordered if it is still going through OPEN/ACK signaling.
    if (handshake_state_ != kHandshakeReady && !config_.ordered) {
      send_params.ordered = true;
      LOG(LS_VERBOSE) << "Sending data as ordered for unordered DataChannel "
                      << "because the OPEN_ACK message has not been received.";
    }

    send_params.max_rtx_count = config_.maxRetransmits;
    send_params.max_rtx_ms = config_.maxRetransmitTime;
    send_params.ssrc = config_.id;
  } else {
    send_params.ssrc = send_ssrc_;
  }
  send_params.type = buffer.binary ? cricket::DMT_BINARY : cricket::DMT_TEXT;

  cricket::SendDataResult send_result = cricket::SDR_SUCCESS;
  bool success = provider_->SendData(send_params, buffer.data, &send_result);

  if (success) {
    return true;
  }

  if (data_channel_type_ != cricket::DCT_SCTP) {
    return false;
  }

  if (send_result == cricket::SDR_BLOCK) {
    if (!queue_if_blocked || QueueSendDataMessage(buffer)) {
      return false;
    }
  }
  // Close the channel if the error is not SDR_BLOCK, or if queuing the
  // message failed.
  LOG(LS_ERROR) << "Closing the DataChannel due to a failure to send data, "
                << "send_result = " << send_result;
  Close();

  return false;
}

bool DataChannel::QueueSendDataMessage(const DataBuffer& buffer) {
  size_t start_buffered_amount = buffered_amount();
  if (start_buffered_amount >= kMaxQueuedSendDataBytes) {
    LOG(LS_ERROR) << "Can't buffer any more data for the data channel.";
    return false;
  }
  queued_send_data_.Push(new DataBuffer(buffer));

  // The buffer can have length zero, in which case there is no change.
  if (observer_ && buffered_amount() > start_buffered_amount) {
    observer_->OnBufferedAmountChange(start_buffered_amount);
  }
  return true;
}

void DataChannel::SendQueuedControlMessages() {
  PacketQueue control_packets;
  control_packets.Swap(&queued_control_data_);

  while (!control_packets.Empty()) {
    rtc::scoped_ptr<DataBuffer> buf(control_packets.Front());
    SendControlMessage(buf->data);
    control_packets.Pop();
  }
}

void DataChannel::QueueControlMessage(const rtc::Buffer& buffer) {
  queued_control_data_.Push(new DataBuffer(buffer, true));
}

bool DataChannel::SendControlMessage(const rtc::Buffer& buffer) {
  bool is_open_message = handshake_state_ == kHandshakeShouldSendOpen;

  ASSERT(data_channel_type_ == cricket::DCT_SCTP &&
         writable_ &&
         config_.id >= 0 &&
         (!is_open_message || !config_.negotiated));

  cricket::SendDataParams send_params;
  send_params.ssrc = config_.id;
  // Send data as ordered before we receive any message from the remote peer to
  // make sure the remote peer will not receive any data before it receives the
  // OPEN message.
  send_params.ordered = config_.ordered || is_open_message;
  send_params.type = cricket::DMT_CONTROL;

  cricket::SendDataResult send_result = cricket::SDR_SUCCESS;
  bool retval = provider_->SendData(send_params, buffer, &send_result);
  if (retval) {
    LOG(LS_INFO) << "Sent CONTROL message on channel " << config_.id;

    if (handshake_state_ == kHandshakeShouldSendAck) {
      handshake_state_ = kHandshakeReady;
    } else if (handshake_state_ == kHandshakeShouldSendOpen) {
      handshake_state_ = kHandshakeWaitingForAck;
    }
  } else if (send_result == cricket::SDR_BLOCK) {
    QueueControlMessage(buffer);
  } else {
    LOG(LS_ERROR) << "Closing the DataChannel due to a failure to send"
                  << " the CONTROL message, send_result = " << send_result;
    Close();
  }
  return retval;
}

}  // namespace webrtc
