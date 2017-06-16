// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_client_session_peer.h"

#include "net/quic/quic_client_session.h"

namespace net {
namespace test {

// static
void QuicClientSessionPeer::SetMaxOpenStreams(QuicClientSession* session,
                                              size_t max_streams,
                                              size_t default_streams) {
  session->config()->SetMaxStreamsPerConnection(max_streams, default_streams);
}

// static
void QuicClientSessionPeer::SetChannelIDSent(QuicClientSession* session,
                                             bool channel_id_sent) {
  session->crypto_stream_->channel_id_sent_ = channel_id_sent;
}

}  // namespace test
}  // namespace net
