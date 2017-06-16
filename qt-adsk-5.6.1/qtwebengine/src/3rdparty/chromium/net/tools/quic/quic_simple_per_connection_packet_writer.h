// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TOOLS_QUIC_SIMPLE_PER_CONNECTION_PACKET_WRITER_H_
#define NET_QUIC_TOOLS_QUIC_SIMPLE_PER_CONNECTION_PACKET_WRITER_H_

#include "base/memory/weak_ptr.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_packet_writer.h"

namespace net {
namespace tools {

class QuicSimpleServerPacketWriter;

// A connection-specific packet writer that notifies its connection when its
// writes to the shared QuicServerPacketWriter complete.
// This class is necessary because multiple connections can share the same
// QuicServerPacketWriter, so it has no way to know which connection to notify.
class QuicSimplePerConnectionPacketWriter : public QuicPacketWriter {
 public:
  // Does not take ownership of |shared_writer| or |connection|.
  QuicSimplePerConnectionPacketWriter(
      QuicSimpleServerPacketWriter* shared_writer,
      QuicConnection* connection);
  ~QuicSimplePerConnectionPacketWriter() override;

  QuicPacketWriter* shared_writer() const;
  QuicConnection* connection() const { return connection_; }

  // Default implementation of the QuicPacketWriter interface: Passes everything
  // to |shared_writer_|.
  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const IPAddressNumber& self_address,
                          const IPEndPoint& peer_address) override;
  bool IsWriteBlockedDataBuffered() const override;
  bool IsWriteBlocked() const override;
  void SetWritable() override;

 private:
  void OnWriteComplete(WriteResult result);

  QuicSimpleServerPacketWriter* shared_writer_;  // Not owned.
  QuicConnection* connection_;  // Not owned.

  base::WeakPtrFactory<QuicSimplePerConnectionPacketWriter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuicSimplePerConnectionPacketWriter);
};

}  // namespace tools
}  // namespace net

#endif  // NET_QUIC_TOOLS_QUIC_SIMPLE_PER_CONNECTION_PACKET_WRITER_H_
