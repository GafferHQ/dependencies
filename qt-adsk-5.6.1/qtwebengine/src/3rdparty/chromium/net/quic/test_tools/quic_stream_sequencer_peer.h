// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_STREAM_SEQUENCER_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_STREAM_SEQUENCER_PEER_H_

#include "base/basictypes.h"
#include "net/quic/quic_protocol.h"

namespace net {

class QuicStreamSequencer;

namespace test {

class QuicStreamSequencerPeer {
 public:
  static size_t GetNumBufferedFrames(QuicStreamSequencer* sequencer);

  static bool FrameOverlapsBufferedData(QuicStreamSequencer* sequencer,
                                        const QuicStreamFrame& frame);

  static QuicStreamOffset GetCloseOffset(QuicStreamSequencer* sequencer);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicStreamSequencerPeer);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_STREAM_SEQUENCER_PEER_H_
