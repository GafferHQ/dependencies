// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_session_peer.h"

#include "base/stl_util.h"
#include "net/quic/quic_session.h"
#include "net/quic/reliable_quic_stream.h"

using std::map;

namespace net {
namespace test {

// static
void QuicSessionPeer::SetNextStreamId(QuicSession* session, QuicStreamId id) {
  session->next_stream_id_ = id;
}

// static
void QuicSessionPeer::SetMaxOpenStreams(QuicSession* session,
                                        uint32 max_streams) {
  session->max_open_streams_ = max_streams;
}

// static
QuicCryptoStream* QuicSessionPeer::GetCryptoStream(QuicSession* session) {
  return session->GetCryptoStream();
}

// static
QuicWriteBlockedList* QuicSessionPeer::GetWriteBlockedStreams(
    QuicSession* session) {
  return &session->write_blocked_streams_;
}

// static
ReliableQuicStream* QuicSessionPeer::GetIncomingDynamicStream(
    QuicSession* session,
    QuicStreamId stream_id) {
  return session->GetIncomingDynamicStream(stream_id);
}

// static
map<QuicStreamId, QuicStreamOffset>&
QuicSessionPeer::GetLocallyClosedStreamsHighestOffset(QuicSession* session) {
  return session->locally_closed_streams_highest_offset_;
}

// static
base::hash_set<QuicStreamId>* QuicSessionPeer::GetDrainingStreams(
    QuicSession* session) {
  return &session->draining_streams_;
}

// static
bool QuicSessionPeer::IsStreamClosed(QuicSession* session, QuicStreamId id) {
  DCHECK_NE(0u, id);
  return session->IsClosedStream(id);
}

// static
bool QuicSessionPeer::IsStreamCreated(QuicSession* session, QuicStreamId id) {
  DCHECK_NE(0u, id);
  return ContainsKey(session->dynamic_streams(), id);
}

// static
bool QuicSessionPeer::IsStreamImplicitlyCreated(QuicSession* session,
                                                QuicStreamId id) {
  DCHECK_NE(0u, id);
  return ContainsKey(session->implicitly_created_streams_, id);
}

// static
bool QuicSessionPeer::IsStreamUncreated(QuicSession* session, QuicStreamId id) {
  DCHECK_NE(0u, id);
  if (id % 2 == session->next_stream_id_ % 2) {
    // locally-created stream.
    return id >= session->next_stream_id_;
  } else {
    // peer-created stream.
    return id > session->largest_peer_created_stream_id_;
  }
}

}  // namespace test
}  // namespace net
