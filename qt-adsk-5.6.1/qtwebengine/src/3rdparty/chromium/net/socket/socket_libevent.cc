// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_libevent.h"

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

namespace net {

namespace {

int MapAcceptError(int os_error) {
  switch (os_error) {
    // If the client aborts the connection before the server calls accept,
    // POSIX specifies accept should fail with ECONNABORTED. The server can
    // ignore the error and just call accept again, so we map the error to
    // ERR_IO_PENDING. See UNIX Network Programming, Vol. 1, 3rd Ed., Sec.
    // 5.11, "Connection Abort before accept Returns".
    case ECONNABORTED:
      return ERR_IO_PENDING;
    default:
      return MapSystemError(os_error);
  }
}

int MapConnectError(int os_error) {
  switch (os_error) {
    case EINPROGRESS:
      return ERR_IO_PENDING;
    case EACCES:
      return ERR_NETWORK_ACCESS_DENIED;
    case ETIMEDOUT:
      return ERR_CONNECTION_TIMED_OUT;
    default: {
      int net_error = MapSystemError(os_error);
      if (net_error == ERR_FAILED)
        return ERR_CONNECTION_FAILED;  // More specific than ERR_FAILED.
      return net_error;
    }
  }
}

}  // namespace

SocketLibevent::SocketLibevent()
    : socket_fd_(kInvalidSocket),
      read_buf_len_(0),
      write_buf_len_(0),
      waiting_connect_(false) {
}

SocketLibevent::~SocketLibevent() {
  Close();
}

int SocketLibevent::Open(int address_family) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(kInvalidSocket, socket_fd_);
  DCHECK(address_family == AF_INET ||
         address_family == AF_INET6 ||
         address_family == AF_UNIX);

  socket_fd_ = CreatePlatformSocket(
      address_family,
      SOCK_STREAM,
      address_family == AF_UNIX ? 0 : IPPROTO_TCP);
  if (socket_fd_ < 0) {
    PLOG(ERROR) << "CreatePlatformSocket() returned an error, errno=" << errno;
    return MapSystemError(errno);
  }

  if (SetNonBlocking(socket_fd_)) {
    int rv = MapSystemError(errno);
    Close();
    return rv;
  }

  return OK;
}

int SocketLibevent::AdoptConnectedSocket(SocketDescriptor socket,
                                         const SockaddrStorage& address) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(kInvalidSocket, socket_fd_);

  socket_fd_ = socket;

  if (SetNonBlocking(socket_fd_)) {
    int rv = MapSystemError(errno);
    Close();
    return rv;
  }

  SetPeerAddress(address);
  return OK;
}

SocketDescriptor SocketLibevent::ReleaseConnectedSocket() {
  StopWatchingAndCleanUp();
  SocketDescriptor socket_fd = socket_fd_;
  socket_fd_ = kInvalidSocket;
  return socket_fd;
}

int SocketLibevent::Bind(const SockaddrStorage& address) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_fd_);

  int rv = bind(socket_fd_, address.addr, address.addr_len);
  if (rv < 0) {
    PLOG(ERROR) << "bind() returned an error, errno=" << errno;
    return MapSystemError(errno);
  }

  return OK;
}

int SocketLibevent::Listen(int backlog) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_fd_);
  DCHECK_LT(0, backlog);

  int rv = listen(socket_fd_, backlog);
  if (rv < 0) {
    PLOG(ERROR) << "listen() returned an error, errno=" << errno;
    return MapSystemError(errno);
  }

  return OK;
}

int SocketLibevent::Accept(scoped_ptr<SocketLibevent>* socket,
                           const CompletionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_fd_);
  DCHECK(accept_callback_.is_null());
  DCHECK(socket);
  DCHECK(!callback.is_null());

  int rv = DoAccept(socket);
  if (rv != ERR_IO_PENDING)
    return rv;

  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          socket_fd_, true, base::MessageLoopForIO::WATCH_READ,
          &accept_socket_watcher_, this)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on accept, errno " << errno;
    return MapSystemError(errno);
  }

  accept_socket_ = socket;
  accept_callback_ = callback;
  return ERR_IO_PENDING;
}

int SocketLibevent::Connect(const SockaddrStorage& address,
                            const CompletionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_fd_);
  DCHECK(!waiting_connect_);
  DCHECK(!callback.is_null());

  SetPeerAddress(address);

  int rv = DoConnect();
  if (rv != ERR_IO_PENDING)
    return rv;

  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          socket_fd_, true, base::MessageLoopForIO::WATCH_WRITE,
          &write_socket_watcher_, this)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on connect, errno " << errno;
    return MapSystemError(errno);
  }

  write_callback_ = callback;
  waiting_connect_ = true;
  return ERR_IO_PENDING;
}

bool SocketLibevent::IsConnected() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (socket_fd_ == kInvalidSocket || waiting_connect_)
    return false;

  // Checks if connection is alive.
  char c;
  int rv = HANDLE_EINTR(recv(socket_fd_, &c, 1, MSG_PEEK));
  if (rv == 0)
    return false;
  if (rv == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    return false;

  return true;
}

bool SocketLibevent::IsConnectedAndIdle() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (socket_fd_ == kInvalidSocket || waiting_connect_)
    return false;

  // Check if connection is alive and we haven't received any data
  // unexpectedly.
  char c;
  int rv = HANDLE_EINTR(recv(socket_fd_, &c, 1, MSG_PEEK));
  if (rv >= 0)
    return false;
  if (errno != EAGAIN && errno != EWOULDBLOCK)
    return false;

  return true;
}

int SocketLibevent::Read(IOBuffer* buf,
                         int buf_len,
                         const CompletionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_fd_);
  DCHECK(!waiting_connect_);
  CHECK(read_callback_.is_null());
  // Synchronous operation not supported
  DCHECK(!callback.is_null());
  DCHECK_LT(0, buf_len);

  int rv = DoRead(buf, buf_len);
  if (rv != ERR_IO_PENDING)
    return rv;

  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          socket_fd_, true, base::MessageLoopForIO::WATCH_READ,
          &read_socket_watcher_, this)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on read, errno " << errno;
    return MapSystemError(errno);
  }

  read_buf_ = buf;
  read_buf_len_ = buf_len;
  read_callback_ = callback;
  return ERR_IO_PENDING;
}

int SocketLibevent::Write(IOBuffer* buf,
                          int buf_len,
                          const CompletionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_fd_);
  DCHECK(!waiting_connect_);
  CHECK(write_callback_.is_null());
  // Synchronous operation not supported
  DCHECK(!callback.is_null());
  DCHECK_LT(0, buf_len);

  int rv = DoWrite(buf, buf_len);
  if (rv == ERR_IO_PENDING)
    rv = WaitForWrite(buf, buf_len, callback);
  return rv;
}

int SocketLibevent::WaitForWrite(IOBuffer* buf,
                                 int buf_len,
                                 const CompletionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_fd_);
  DCHECK(write_callback_.is_null());
  // Synchronous operation not supported
  DCHECK(!callback.is_null());
  DCHECK_LT(0, buf_len);

  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          socket_fd_, true, base::MessageLoopForIO::WATCH_WRITE,
          &write_socket_watcher_, this)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on write, errno " << errno;
    return MapSystemError(errno);
  }

  write_buf_ = buf;
  write_buf_len_ = buf_len;
  write_callback_ = callback;
  return ERR_IO_PENDING;
}

int SocketLibevent::GetLocalAddress(SockaddrStorage* address) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(address);

  if (getsockname(socket_fd_, address->addr, &address->addr_len) < 0)
    return MapSystemError(errno);
  return OK;
}

int SocketLibevent::GetPeerAddress(SockaddrStorage* address) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(address);

  if (!HasPeerAddress())
    return ERR_SOCKET_NOT_CONNECTED;

  *address = *peer_address_;
  return OK;
}

void SocketLibevent::SetPeerAddress(const SockaddrStorage& address) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // |peer_address_| will be non-NULL if Connect() has been called. Unless
  // Close() is called to reset the internal state, a second call to Connect()
  // is not allowed.
  // Please note that we don't allow a second Connect() even if the previous
  // Connect() has failed. Connecting the same |socket_| again after a
  // connection attempt failed results in unspecified behavior according to
  // POSIX.
  DCHECK(!peer_address_);
  peer_address_.reset(new SockaddrStorage(address));
}

bool SocketLibevent::HasPeerAddress() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return peer_address_ != NULL;
}

void SocketLibevent::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());

  StopWatchingAndCleanUp();

  if (socket_fd_ != kInvalidSocket) {
    if (IGNORE_EINTR(close(socket_fd_)) < 0)
      PLOG(ERROR) << "close() returned an error, errno=" << errno;
    socket_fd_ = kInvalidSocket;
  }
}

void SocketLibevent::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(!accept_callback_.is_null() || !read_callback_.is_null());
  if (!accept_callback_.is_null()) {
    AcceptCompleted();
  } else {  // !read_callback_.is_null()
    ReadCompleted();
  }
}

void SocketLibevent::OnFileCanWriteWithoutBlocking(int fd) {
  DCHECK(!write_callback_.is_null());
  if (waiting_connect_) {
    ConnectCompleted();
  } else {
    WriteCompleted();
  }
}

int SocketLibevent::DoAccept(scoped_ptr<SocketLibevent>* socket) {
  SockaddrStorage new_peer_address;
  int new_socket = HANDLE_EINTR(accept(socket_fd_,
                                       new_peer_address.addr,
                                       &new_peer_address.addr_len));
  if (new_socket < 0)
    return MapAcceptError(errno);

  scoped_ptr<SocketLibevent> accepted_socket(new SocketLibevent);
  int rv = accepted_socket->AdoptConnectedSocket(new_socket, new_peer_address);
  if (rv != OK)
    return rv;

  *socket = accepted_socket.Pass();
  return OK;
}

void SocketLibevent::AcceptCompleted() {
  DCHECK(accept_socket_);
  int rv = DoAccept(accept_socket_);
  if (rv == ERR_IO_PENDING)
    return;

  bool ok = accept_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  accept_socket_ = NULL;
  base::ResetAndReturn(&accept_callback_).Run(rv);
}

int SocketLibevent::DoConnect() {
  int rv = HANDLE_EINTR(connect(socket_fd_,
                                peer_address_->addr,
                                peer_address_->addr_len));
  DCHECK_GE(0, rv);
  return rv == 0 ? OK : MapConnectError(errno);
}

void SocketLibevent::ConnectCompleted() {
  // Get the error that connect() completed with.
  int os_error = 0;
  socklen_t len = sizeof(os_error);
  if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &os_error, &len) == 0) {
    // TCPSocketLibevent expects errno to be set.
    errno = os_error;
  }

  int rv = MapConnectError(errno);
  if (rv == ERR_IO_PENDING)
    return;

  bool ok = write_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  waiting_connect_ = false;
  base::ResetAndReturn(&write_callback_).Run(rv);
}

int SocketLibevent::DoRead(IOBuffer* buf, int buf_len) {
  int rv = HANDLE_EINTR(read(socket_fd_, buf->data(), buf_len));
  return rv >= 0 ? rv : MapSystemError(errno);
}

void SocketLibevent::ReadCompleted() {
  int rv = DoRead(read_buf_.get(), read_buf_len_);
  if (rv == ERR_IO_PENDING)
    return;

  bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  read_buf_ = NULL;
  read_buf_len_ = 0;
  base::ResetAndReturn(&read_callback_).Run(rv);
}

int SocketLibevent::DoWrite(IOBuffer* buf, int buf_len) {
  int rv = HANDLE_EINTR(write(socket_fd_, buf->data(), buf_len));
  return rv >= 0 ? rv : MapSystemError(errno);
}

void SocketLibevent::WriteCompleted() {
  int rv = DoWrite(write_buf_.get(), write_buf_len_);
  if (rv == ERR_IO_PENDING)
    return;

  bool ok = write_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  write_buf_ = NULL;
  write_buf_len_ = 0;
  base::ResetAndReturn(&write_callback_).Run(rv);
}

void SocketLibevent::StopWatchingAndCleanUp() {
  bool ok = accept_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  ok = read_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  ok = write_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);

  if (!accept_callback_.is_null()) {
    accept_socket_ = NULL;
    accept_callback_.Reset();
  }

  if (!read_callback_.is_null()) {
    read_buf_ = NULL;
    read_buf_len_ = 0;
    read_callback_.Reset();
  }

  if (!write_callback_.is_null()) {
    write_buf_ = NULL;
    write_buf_len_ = 0;
    write_callback_.Reset();
  }

  waiting_connect_ = false;
  peer_address_.reset();
}

}  // namespace net
