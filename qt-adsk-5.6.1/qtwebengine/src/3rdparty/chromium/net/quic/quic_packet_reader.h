// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef NET_QUIC_QUIC_PACKET_READER_H_
#define NET_QUIC_QUIC_PACKET_READER_H_

#include "base/memory/weak_ptr.h"
#include "net/base/io_buffer.h"
#include "net/base/net_export.h"
#include "net/log/net_log.h"
#include "net/quic/quic_protocol.h"
#include "net/udp/datagram_client_socket.h"

namespace net {

class NET_EXPORT_PRIVATE QuicPacketReader {
 public:
  class NET_EXPORT_PRIVATE Visitor {
   public:
    virtual ~Visitor() {};
    virtual void OnReadError(int result) = 0;
    virtual bool OnPacket(const QuicEncryptedPacket& packet,
                          IPEndPoint local_address,
                          IPEndPoint peer_address) = 0;
  };

  QuicPacketReader(DatagramClientSocket* socket,
                   Visitor* visitor,
                   const BoundNetLog& net_log);
  virtual ~QuicPacketReader();

  // Causes the QuicConnectionHelper to start reading from the socket
  // and passing the data along to the QuicConnection.
  void StartReading();

 private:
  // A completion callback invoked when a read completes.
  void OnReadComplete(int result);

  DatagramClientSocket* socket_;
  Visitor* visitor_;
  bool read_pending_;
  int num_packets_read_;
  scoped_refptr<IOBufferWithSize> read_buffer_;
  BoundNetLog net_log_;

  base::WeakPtrFactory<QuicPacketReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuicPacketReader);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PACKET_READER_H_
