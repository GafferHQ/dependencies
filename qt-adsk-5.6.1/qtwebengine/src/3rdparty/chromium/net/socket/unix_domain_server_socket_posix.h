// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UNIX_DOMAIN_SERVER_SOCKET_POSIX_H_
#define NET_SOCKET_UNIX_DOMAIN_SERVER_SOCKET_POSIX_H_

#include <sys/types.h>

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/socket/server_socket.h"
#include "net/socket/socket_descriptor.h"

namespace net {

class SocketLibevent;

// Unix Domain Server Socket Implementation. Supports abstract namespaces on
// Linux and Android.
class NET_EXPORT UnixDomainServerSocket : public ServerSocket {
 public:
  // Credentials of a peer process connected to the socket.
  struct NET_EXPORT Credentials {
#if defined(OS_LINUX) || defined(OS_ANDROID)
    // Linux/Android API provides more information about the connected peer
    // than Windows/OS X. It's useful for permission-based authorization on
    // Android.
    pid_t process_id;
#endif
    uid_t user_id;
    gid_t group_id;
  };

  // Callback that returns whether the already connected client, identified by
  // its credentials, is allowed to keep the connection open. Note that
  // the socket is closed immediately in case the callback returns false.
  typedef base::Callback<bool (const Credentials&)> AuthCallback;

  UnixDomainServerSocket(const AuthCallback& auth_callack,
                         bool use_abstract_namespace);
  ~UnixDomainServerSocket() override;

  // Gets credentials of peer to check permissions.
  static bool GetPeerCredentials(SocketDescriptor socket_fd,
                                 Credentials* credentials);

  // ServerSocket implementation.
  int Listen(const IPEndPoint& address, int backlog) override;
  int ListenWithAddressAndPort(const std::string& unix_domain_path,
                               uint16 port_unused,
                               int backlog) override;
  int GetLocalAddress(IPEndPoint* address) const override;
  int Accept(scoped_ptr<StreamSocket>* socket,
             const CompletionCallback& callback) override;

  // Accepts an incoming connection on |listen_socket_|, but passes back
  // a raw SocketDescriptor instead of a StreamSocket.
  int AcceptSocketDescriptor(SocketDescriptor* socket_descriptor,
                             const CompletionCallback& callback);

 private:
  // A callback to wrap the setting of the out-parameter to Accept().
  // This allows the internal machinery of that call to be implemented in
  // a manner that's agnostic to the caller's desired output.
  typedef base::Callback<void(scoped_ptr<SocketLibevent>)> SetterCallback;

  int DoAccept(const SetterCallback& setter_callback,
               const CompletionCallback& callback);
  void AcceptCompleted(const SetterCallback& setter_callback,
                       const CompletionCallback& callback,
                       int rv);
  bool AuthenticateAndGetStreamSocket(const SetterCallback& setter_callback);

  scoped_ptr<SocketLibevent> listen_socket_;
  const AuthCallback auth_callback_;
  const bool use_abstract_namespace_;

  scoped_ptr<SocketLibevent> accept_socket_;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainServerSocket);
};

}  // namespace net

#endif  // NET_SOCKET_UNIX_DOMAIN_SOCKET_POSIX_H_
