// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This StreamSocket implementation is to be used with servers that
// accept connections on port 443 but don't really use SSL.  For
// example, the Google Talk servers do this to bypass proxies.  (The
// connection is upgraded to TLS as part of the XMPP negotiation, so
// security is preserved.)  A "fake" SSL handshake is done immediately
// after connection to fool proxies into thinking that this is a real
// SSL connection.
//
// NOTE: This StreamSocket implementation does *not* do a real SSL
// handshake nor does it do any encryption!

#ifndef JINGLE_GLUE_FAKE_SSL_CLIENT_SOCKET_H_
#define JINGLE_GLUE_FAKE_SSL_CLIENT_SOCKET_H_

#include <cstddef>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace net {
class DrainableIOBuffer;
class SSLInfo;
}  // namespace net

namespace jingle_glue {

class FakeSSLClientSocket : public net::StreamSocket {
 public:
  explicit FakeSSLClientSocket(scoped_ptr<net::StreamSocket> transport_socket);

  ~FakeSSLClientSocket() override;

  // Exposed for testing.
  static base::StringPiece GetSslClientHello();
  static base::StringPiece GetSslServerHello();

  // net::StreamSocket implementation.
  int Read(net::IOBuffer* buf,
           int buf_len,
           const net::CompletionCallback& callback) override;
  int Write(net::IOBuffer* buf,
            int buf_len,
            const net::CompletionCallback& callback) override;
  int SetReceiveBufferSize(int32 size) override;
  int SetSendBufferSize(int32 size) override;
  int Connect(const net::CompletionCallback& callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(net::IPEndPoint* address) const override;
  int GetLocalAddress(net::IPEndPoint* address) const override;
  const net::BoundNetLog& NetLog() const override;
  void SetSubresourceSpeculation() override;
  void SetOmniboxSpeculation() override;
  bool WasEverUsed() const override;
  bool UsingTCPFastOpen() const override;
  bool WasNpnNegotiated() const override;
  net::NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(net::SSLInfo* ssl_info) override;
  void GetConnectionAttempts(net::ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override {}
  void AddConnectionAttempts(const net::ConnectionAttempts& attempts) override {
  }

 private:
  enum HandshakeState {
    STATE_NONE,
    STATE_CONNECT,
    STATE_SEND_CLIENT_HELLO,
    STATE_VERIFY_SERVER_HELLO,
  };

  int DoHandshakeLoop();
  void RunUserConnectCallback(int status);
  void DoHandshakeLoopWithUserConnectCallback();

  int DoConnect();
  void OnConnectDone(int status);
  void ProcessConnectDone();

  int DoSendClientHello();
  void OnSendClientHelloDone(int status);
  void ProcessSendClientHelloDone(size_t written);

  int DoVerifyServerHello();
  void OnVerifyServerHelloDone(int status);
  net::Error ProcessVerifyServerHelloDone(size_t read);

  scoped_ptr<net::StreamSocket> transport_socket_;

  // During the handshake process, holds a value from HandshakeState.
  // STATE_NONE otherwise.
  HandshakeState next_handshake_state_;

  // True iff we're connected and we've finished the handshake.
  bool handshake_completed_;

  // The callback passed to Connect().
  net::CompletionCallback user_connect_callback_;

  scoped_refptr<net::DrainableIOBuffer> write_buf_;
  scoped_refptr<net::DrainableIOBuffer> read_buf_;
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_FAKE_SSL_CLIENT_SOCKET_H_
