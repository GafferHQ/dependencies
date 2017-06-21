// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_data_channel_handler.h"

#include <limits>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"

namespace content {

namespace {

enum DataChannelCounters {
  CHANNEL_CREATED,
  CHANNEL_OPENED,
  CHANNEL_RELIABLE,
  CHANNEL_ORDERED,
  CHANNEL_NEGOTIATED,
  CHANNEL_BOUNDARY
};

void IncrementCounter(DataChannelCounters counter) {
  UMA_HISTOGRAM_ENUMERATION("WebRTC.DataChannelCounters",
                            counter,
                            CHANNEL_BOUNDARY);
}

}  // namespace

// Implementation of DataChannelObserver that receives events on libjingle's
// signaling thread and forwards them over to the main thread for handling.
// Since the handler's lifetime is scoped potentially narrower than what
// the callbacks allow for, we use reference counting here to make sure
// all callbacks have a valid pointer but won't do anything if the handler
// has gone away.
RtcDataChannelHandler::Observer::Observer(
    RtcDataChannelHandler* handler,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    webrtc::DataChannelInterface* channel)
    : handler_(handler), main_thread_(main_thread), channel_(channel) {
  channel_->RegisterObserver(this);
}

RtcDataChannelHandler::Observer::~Observer() {
  DVLOG(3) << "dtor";
  DCHECK(!channel_.get()) << "Unregister hasn't been called.";
}

const scoped_refptr<base::SingleThreadTaskRunner>&
RtcDataChannelHandler::Observer::main_thread() const {
  return main_thread_;
}

const scoped_refptr<webrtc::DataChannelInterface>&
RtcDataChannelHandler::Observer::channel() const {
  DCHECK(main_thread_->BelongsToCurrentThread());
  return channel_;
}

void RtcDataChannelHandler::Observer::Unregister() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  handler_ = nullptr;
  if (channel_.get()) {
    channel_->UnregisterObserver();
    // Now that we're guaranteed to not get further OnStateChange callbacks,
    // it's safe to release our reference to the channel.
    channel_ = nullptr;
  }
}

void RtcDataChannelHandler::Observer::OnStateChange() {
  main_thread_->PostTask(FROM_HERE, base::Bind(
      &RtcDataChannelHandler::Observer::OnStateChangeImpl, this,
      channel_->state()));
}

void RtcDataChannelHandler::Observer::OnMessage(
    const webrtc::DataBuffer& buffer) {
  // TODO(tommi): Figure out a way to transfer ownership of the buffer without
  // having to create a copy.  See webrtc bug 3967.
  scoped_ptr<webrtc::DataBuffer> new_buffer(new webrtc::DataBuffer(buffer));
  main_thread_->PostTask(FROM_HERE,
      base::Bind(&RtcDataChannelHandler::Observer::OnMessageImpl, this,
      base::Passed(&new_buffer)));
}

void RtcDataChannelHandler::Observer::OnStateChangeImpl(
    webrtc::DataChannelInterface::DataState state) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (handler_)
    handler_->OnStateChange(state);
}

void RtcDataChannelHandler::Observer::OnMessageImpl(
    scoped_ptr<webrtc::DataBuffer> buffer) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (handler_)
    handler_->OnMessage(buffer.Pass());
}

RtcDataChannelHandler::RtcDataChannelHandler(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    webrtc::DataChannelInterface* channel)
    : observer_(new Observer(this, main_thread, channel)),
      webkit_client_(NULL) {
  DVLOG(1) << "RtcDataChannelHandler " << channel->label();

  // Detach from the ctor thread since we can be constructed on either the main
  // or signaling threads.
  thread_checker_.DetachFromThread();

  IncrementCounter(CHANNEL_CREATED);
  if (channel->reliable())
    IncrementCounter(CHANNEL_RELIABLE);
  if (channel->ordered())
    IncrementCounter(CHANNEL_ORDERED);
  if (channel->negotiated())
    IncrementCounter(CHANNEL_NEGOTIATED);

  UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.DataChannelMaxRetransmits",
                              channel->maxRetransmits(), 0,
                              std::numeric_limits<unsigned short>::max(), 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.DataChannelMaxRetransmitTime",
                              channel->maxRetransmitTime(), 0,
                              std::numeric_limits<unsigned short>::max(), 50);
}

RtcDataChannelHandler::~RtcDataChannelHandler() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "::dtor";
  // setClient might not have been called at all if the data channel was not
  // passed to Blink.  So, we call it here explicitly just to make sure the
  // observer gets properly unregistered.
  // See RTCPeerConnectionHandler::OnDataChannel for more.
  setClient(nullptr);
}

void RtcDataChannelHandler::setClient(
    blink::WebRTCDataChannelHandlerClient* client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << "setClient " << client;
  webkit_client_ = client;
  if (!client && observer_.get()) {
    observer_->Unregister();
    observer_ = nullptr;
  }
}

blink::WebString RtcDataChannelHandler::label() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::UTF8ToUTF16(channel()->label());
}

bool RtcDataChannelHandler::isReliable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return channel()->reliable();
}

bool RtcDataChannelHandler::ordered() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return channel()->ordered();
}

unsigned short RtcDataChannelHandler::maxRetransmitTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return channel()->maxRetransmitTime();
}

unsigned short RtcDataChannelHandler::maxRetransmits() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return channel()->maxRetransmits();
}

blink::WebString RtcDataChannelHandler::protocol() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::UTF8ToUTF16(channel()->protocol());
}

bool RtcDataChannelHandler::negotiated() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return channel()->negotiated();
}

unsigned short RtcDataChannelHandler::id() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return channel()->id();
}

blink::WebRTCDataChannelHandlerClient::ReadyState convertReadyState(
    webrtc::DataChannelInterface::DataState state) {
  switch (state) {
    case webrtc::DataChannelInterface::kConnecting:
      return blink::WebRTCDataChannelHandlerClient::ReadyStateConnecting;
      break;
    case webrtc::DataChannelInterface::kOpen:
      return blink::WebRTCDataChannelHandlerClient::ReadyStateOpen;
      break;
    case webrtc::DataChannelInterface::kClosing:
      return blink::WebRTCDataChannelHandlerClient::ReadyStateClosing;
      break;
    case webrtc::DataChannelInterface::kClosed:
      return blink::WebRTCDataChannelHandlerClient::ReadyStateClosed;
      break;
    default:
      NOTREACHED();
      // MSVC does not respect |NOTREACHED()|, so we need a return value.
      return blink::WebRTCDataChannelHandlerClient::ReadyStateClosed;
  }
}

blink::WebRTCDataChannelHandlerClient::ReadyState
    RtcDataChannelHandler::state() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!observer_.get()) {
    return blink::WebRTCDataChannelHandlerClient::ReadyStateConnecting;
  } else {
    return convertReadyState(observer_->channel()->state());
  }
}

unsigned long RtcDataChannelHandler::bufferedAmount() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return channel()->buffered_amount();
}

bool RtcDataChannelHandler::sendStringData(const blink::WebString& data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string utf8_buffer = base::UTF16ToUTF8(data);
  rtc::Buffer buffer(utf8_buffer.c_str(), utf8_buffer.length());
  webrtc::DataBuffer data_buffer(buffer, false);
  RecordMessageSent(data_buffer.size());
  return channel()->Send(data_buffer);
}

bool RtcDataChannelHandler::sendRawData(const char* data, size_t length) {
  DCHECK(thread_checker_.CalledOnValidThread());
  rtc::Buffer buffer(data, length);
  webrtc::DataBuffer data_buffer(buffer, true);
  RecordMessageSent(data_buffer.size());
  return channel()->Send(data_buffer);
}

void RtcDataChannelHandler::close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  channel()->Close();
  // Note that even though Close() will run synchronously, the readyState has
  // not changed yet since the state changes that occured on the signaling
  // thread have been posted to this thread and will be delivered later.
  // To work around this, we could have a nested loop here and deliver the
  // callbacks before running from this function, but doing so can cause
  // undesired side effects in webkit, so we don't, and instead rely on the
  // user of the API handling readyState notifications.
}

const scoped_refptr<webrtc::DataChannelInterface>&
RtcDataChannelHandler::channel() const {
  return observer_->channel();
}

void RtcDataChannelHandler::OnStateChange(
    webrtc::DataChannelInterface::DataState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "OnStateChange " << state;

  if (!webkit_client_) {
    // If this happens, the web application will not get notified of changes.
    NOTREACHED() << "WebRTCDataChannelHandlerClient not set.";
    return;
  }

  if (state == webrtc::DataChannelInterface::kOpen)
    IncrementCounter(CHANNEL_OPENED);

  webkit_client_->didChangeReadyState(convertReadyState(state));
}

void RtcDataChannelHandler::OnMessage(scoped_ptr<webrtc::DataBuffer> buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!webkit_client_) {
    // If this happens, the web application will not get notified of changes.
    NOTREACHED() << "WebRTCDataChannelHandlerClient not set.";
    return;
  }

  if (buffer->binary) {
    webkit_client_->didReceiveRawData(buffer->data.data<char>(),
                                      buffer->data.size());
  } else {
    base::string16 utf16;
    if (!base::UTF8ToUTF16(buffer->data.data<char>(), buffer->data.size(),
                           &utf16)) {
      LOG(ERROR) << "Failed convert received data to UTF16";
      return;
    }
    webkit_client_->didReceiveStringData(utf16);
  }
}

void RtcDataChannelHandler::RecordMessageSent(size_t num_bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Currently, messages are capped at some fairly low limit (16 Kb?)
  // but we may allow unlimited-size messages at some point, so making
  // the histogram maximum quite large (100 Mb) to have some
  // granularity at the higher end in that eventuality. The histogram
  // buckets are exponentially growing in size, so we'll still have
  // good granularity at the low end.

  // This makes the last bucket in the histogram count messages from
  // 100 Mb to infinity.
  const int kMaxBucketSize = 100 * 1024 * 1024;
  const int kNumBuckets = 50;

  if (channel()->reliable()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.ReliableDataChannelMessageSize",
                                num_bytes,
                                1, kMaxBucketSize, kNumBuckets);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.UnreliableDataChannelMessageSize",
                                num_bytes,
                                1, kMaxBucketSize, kNumBuckets);
  }
}

}  // namespace content
