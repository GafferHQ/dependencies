// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_default_packet_writer.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

QuicDefaultPacketWriter::QuicDefaultPacketWriter() : weak_factory_(this) {
}

QuicDefaultPacketWriter::QuicDefaultPacketWriter(DatagramClientSocket* socket)
    : socket_(socket),
      write_blocked_(false),
      weak_factory_(this) {
}

QuicDefaultPacketWriter::~QuicDefaultPacketWriter() {}

WriteResult QuicDefaultPacketWriter::WritePacket(
    const char* buffer,
    size_t buf_len,
    const IPAddressNumber& self_address,
    const IPEndPoint& peer_address) {
  scoped_refptr<StringIOBuffer> buf(
      new StringIOBuffer(std::string(buffer, buf_len)));
  DCHECK(!IsWriteBlocked());
  base::TimeTicks now = base::TimeTicks::Now();
  int rv = socket_->Write(buf.get(),
                          buf_len,
                          base::Bind(&QuicDefaultPacketWriter::OnWriteComplete,
                                     weak_factory_.GetWeakPtr()));
  WriteStatus status = WRITE_STATUS_OK;
  if (rv < 0) {
    if (rv != ERR_IO_PENDING) {
      UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.WriteError", -rv);
      status = WRITE_STATUS_ERROR;
    } else {
      status = WRITE_STATUS_BLOCKED;
      write_blocked_ = true;
    }
  }

  base::TimeDelta delta = base::TimeTicks::Now() - now;
  if (status == WRITE_STATUS_OK) {
    UMA_HISTOGRAM_TIMES("Net.QuicSession.PacketWriteTime.Synchronous", delta);
  } else if (status == WRITE_STATUS_BLOCKED) {
    UMA_HISTOGRAM_TIMES("Net.QuicSession.PacketWriteTime.Asynchronous", delta);
  }

  return WriteResult(status, rv);
}

bool QuicDefaultPacketWriter::IsWriteBlockedDataBuffered() const {
  // Chrome sockets' Write() methods buffer the data until the Write is
  // permitted.
  return true;
}

bool QuicDefaultPacketWriter::IsWriteBlocked() const {
  return write_blocked_;
}

void QuicDefaultPacketWriter::SetWritable() {
  write_blocked_ = false;
}

void QuicDefaultPacketWriter::OnWriteComplete(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  write_blocked_ = false;
  if (rv < 0) {
    connection_->OnWriteError(rv);
  }
  connection_->OnCanWrite();
}

}  // namespace net
