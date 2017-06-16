// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/udp/udp_server_socket.h"

#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"

namespace net {

UDPServerSocket::UDPServerSocket(net::NetLog* net_log,
                                 const net::NetLog::Source& source)
    : socket_(DatagramSocket::DEFAULT_BIND,
              RandIntCallback(),
              net_log,
              source),
      allow_address_reuse_(false),
      allow_broadcast_(false) {
}

UDPServerSocket::~UDPServerSocket() {
}

int UDPServerSocket::Listen(const IPEndPoint& address) {
  int rv = socket_.Open(address.GetFamily());
  if (rv != OK)
    return rv;

  if (allow_address_reuse_) {
    rv = socket_.AllowAddressReuse();
    if (rv != OK) {
      socket_.Close();
      return rv;
    }
  }

  if (allow_broadcast_) {
    rv = socket_.SetBroadcast(true);
    if (rv != OK) {
      socket_.Close();
      return rv;
    }
  }

  return socket_.Bind(address);
}

int UDPServerSocket::RecvFrom(IOBuffer* buf,
                              int buf_len,
                              IPEndPoint* address,
                              const CompletionCallback& callback) {
  return socket_.RecvFrom(buf, buf_len, address, callback);
}

int UDPServerSocket::SendTo(IOBuffer* buf,
                            int buf_len,
                            const IPEndPoint& address,
                            const CompletionCallback& callback) {
  return socket_.SendTo(buf, buf_len, address, callback);
}

int UDPServerSocket::SetReceiveBufferSize(int32 size) {
  return socket_.SetReceiveBufferSize(size);
}

int UDPServerSocket::SetSendBufferSize(int32 size) {
  return socket_.SetSendBufferSize(size);
}

void UDPServerSocket::Close() {
  socket_.Close();
}

int UDPServerSocket::GetPeerAddress(IPEndPoint* address) const {
  return socket_.GetPeerAddress(address);
}

int UDPServerSocket::GetLocalAddress(IPEndPoint* address) const {
  return socket_.GetLocalAddress(address);
}

const BoundNetLog& UDPServerSocket::NetLog() const {
  return socket_.NetLog();
}

void UDPServerSocket::AllowAddressReuse() {
  allow_address_reuse_ = true;
}

void UDPServerSocket::AllowBroadcast() {
  allow_broadcast_ = true;
}

int UDPServerSocket::JoinGroup(const IPAddressNumber& group_address) const {
  return socket_.JoinGroup(group_address);
}

int UDPServerSocket::LeaveGroup(const IPAddressNumber& group_address) const {
  return socket_.LeaveGroup(group_address);
}

int UDPServerSocket::SetMulticastInterface(uint32 interface_index) {
  return socket_.SetMulticastInterface(interface_index);
}

int UDPServerSocket::SetMulticastTimeToLive(int time_to_live) {
  return socket_.SetMulticastTimeToLive(time_to_live);
}

int UDPServerSocket::SetMulticastLoopbackMode(bool loopback) {
  return socket_.SetMulticastLoopbackMode(loopback);
}

int UDPServerSocket::SetDiffServCodePoint(DiffServCodePoint dscp) {
  return socket_.SetDiffServCodePoint(dscp);
}

void UDPServerSocket::DetachFromThread() {
  socket_.DetachFromThread();
}

#if defined(OS_WIN)
void UDPServerSocket::UseNonBlockingIO() {
  socket_.UseNonBlockingIO();
}
#endif

}  // namespace net
