// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_FRAMER_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_FRAMER_PEER_H_

#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {

namespace test {

class QuicFramerPeer {
 public:
  static QuicPacketSequenceNumber CalculatePacketSequenceNumberFromWire(
      QuicFramer* framer,
      QuicSequenceNumberLength sequence_number_length,
      QuicPacketSequenceNumber packet_sequence_number);
  static void SetLastSerializedConnectionId(QuicFramer* framer,
                                            QuicConnectionId connection_id);
  static void SetLastSequenceNumber(
      QuicFramer* framer,
      QuicPacketSequenceNumber packet_sequence_number);
  static void SetPerspective(QuicFramer* framer, Perspective perspective);

  // SwapCrypters exchanges the state of the crypters of |framer1| with
  // |framer2|.
  static void SwapCrypters(QuicFramer* framer1, QuicFramer* framer2);

  static QuicEncrypter* GetEncrypter(QuicFramer* framer, EncryptionLevel level);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicFramerPeer);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_FRAMER_PEER_H_
