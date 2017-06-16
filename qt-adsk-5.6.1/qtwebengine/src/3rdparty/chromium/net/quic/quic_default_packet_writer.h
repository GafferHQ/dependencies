// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_DEFAULT_PACKET_WRITER_H_
#define NET_QUIC_QUIC_DEFAULT_PACKET_WRITER_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_protocol.h"
#include "net/udp/datagram_client_socket.h"

namespace net {

struct WriteResult;

// Chrome specific packet writer which uses a DatagramClientSocket for writing
// data.
class NET_EXPORT_PRIVATE QuicDefaultPacketWriter : public QuicPacketWriter {
 public:
  QuicDefaultPacketWriter();
  explicit QuicDefaultPacketWriter(DatagramClientSocket* socket);
  ~QuicDefaultPacketWriter() override;

  // QuicPacketWriter
  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const IPAddressNumber& self_address,
                          const IPEndPoint& peer_address) override;
  bool IsWriteBlockedDataBuffered() const override;
  bool IsWriteBlocked() const override;
  void SetWritable() override;

  void OnWriteComplete(int rv);
  void SetConnection(QuicConnection* connection) {
    connection_ = connection;
  }

 protected:
  void set_write_blocked(bool is_blocked) {
    write_blocked_ = is_blocked;
  }

 private:
  DatagramClientSocket* socket_;
  QuicConnection* connection_;

  // Whether a write is currently in flight.
  bool write_blocked_;

  base::WeakPtrFactory<QuicDefaultPacketWriter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuicDefaultPacketWriter);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_DEFAULT_PACKET_WRITER_H_
