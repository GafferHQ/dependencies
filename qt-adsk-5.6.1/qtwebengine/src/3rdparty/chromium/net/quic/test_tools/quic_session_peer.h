// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_SESSION_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_SESSION_PEER_H_

#include <map>

#include "base/containers/hash_tables.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_write_blocked_list.h"

namespace net {

class QuicCryptoStream;
class QuicDataStream;
class QuicHeadersStream;
class QuicSession;
class ReliableQuicStream;

namespace test {

class QuicSessionPeer {
 public:
  static void SetNextStreamId(QuicSession* session, QuicStreamId id);
  static void SetMaxOpenStreams(QuicSession* session, uint32 max_streams);
  static QuicCryptoStream* GetCryptoStream(QuicSession* session);
  static QuicWriteBlockedList* GetWriteBlockedStreams(QuicSession* session);
  static ReliableQuicStream* GetIncomingDynamicStream(QuicSession* session,
                                                      QuicStreamId stream_id);
  static std::map<QuicStreamId, QuicStreamOffset>&
  GetLocallyClosedStreamsHighestOffset(QuicSession* session);
  static base::hash_set<QuicStreamId>* GetDrainingStreams(QuicSession* session);

  // Discern the state of a stream.  Exactly one of these should be true at a
  // time for any stream id > 0 (other than the special streams 1 and 3).
  static bool IsStreamClosed(QuicSession* session, QuicStreamId id);
  static bool IsStreamCreated(QuicSession* session, QuicStreamId id);
  static bool IsStreamImplicitlyCreated(QuicSession* session, QuicStreamId id);
  static bool IsStreamUncreated(QuicSession* session, QuicStreamId id);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicSessionPeer);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_SESSION_PEER_H_
