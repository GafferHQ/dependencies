// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A toy client, which connects to a specified port and sends QUIC
// request to that endpoint.

#ifndef NET_TOOLS_QUIC_QUIC_CLIENT_H_
#define NET_TOOLS_QUIC_QUIC_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
#include "net/tools/balsa/balsa_headers.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_client_session.h"
#include "net/tools/quic/quic_spdy_client_stream.h"

namespace net {

class ProofVerifier;
class QuicServerId;

namespace tools {

class QuicEpollConnectionHelper;

namespace test {
class QuicClientPeer;
}  // namespace test

class QuicClient : public EpollCallbackInterface,
                   public QuicDataStream::Visitor {
 public:
  class ResponseListener {
   public:
    ResponseListener() {}
    virtual ~ResponseListener() {}
    virtual void OnCompleteResponse(QuicStreamId id,
                                    const BalsaHeaders& response_headers,
                                    const std::string& response_body) = 0;
  };

  // A packet writer factory that always returns the same writer.
  class DummyPacketWriterFactory : public QuicConnection::PacketWriterFactory {
   public:
    explicit DummyPacketWriterFactory(QuicPacketWriter* writer);
    ~DummyPacketWriterFactory() override;

    QuicPacketWriter* Create(QuicConnection* connection) const override;

   private:
    QuicPacketWriter* writer_;
  };

  // Create a quic client, which will have events managed by an externally owned
  // EpollServer.
  QuicClient(IPEndPoint server_address,
             const QuicServerId& server_id,
             const QuicVersionVector& supported_versions,
             EpollServer* epoll_server);
  QuicClient(IPEndPoint server_address,
             const QuicServerId& server_id,
             const QuicVersionVector& supported_versions,
             const QuicConfig& config,
             EpollServer* epoll_server);

  ~QuicClient() override;

  // Initializes the client to create a connection. Should be called exactly
  // once before calling StartConnect or Connect. Returns true if the
  // initialization succeeds, false otherwise.
  bool Initialize();

  // "Connect" to the QUIC server, including performing synchronous crypto
  // handshake.
  bool Connect();

  // Start the crypto handshake.  This can be done in place of the synchronous
  // Connect(), but callers are responsible for making sure the crypto handshake
  // completes.
  void StartConnect();

  // Returns true if the crypto handshake has yet to establish encryption.
  // Returns false if encryption is active (even if the server hasn't confirmed
  // the handshake) or if the connection has been closed.
  bool EncryptionBeingEstablished();

  // Disconnects from the QUIC server.
  void Disconnect();

  // Sends an HTTP request and does not wait for response before returning.
  void SendRequest(const BalsaHeaders& headers,
                   base::StringPiece body,
                   bool fin);

  // Sends an HTTP request and waits for response before returning.
  void SendRequestAndWaitForResponse(const BalsaHeaders& headers,
                                     base::StringPiece body,
                                     bool fin);

  // Sends a request simple GET for each URL in |args|, and then waits for
  // each to complete.
  void SendRequestsAndWaitForResponse(
      const std::vector<std::string>& url_list);

  // Returns a newly created QuicSpdyClientStream, owned by the
  // QuicClient.
  QuicSpdyClientStream* CreateReliableClientStream();

  // Wait for events until the stream with the given ID is closed.
  void WaitForStreamToClose(QuicStreamId id);

  // Wait for events until the handshake is confirmed.
  void WaitForCryptoHandshakeConfirmed();

  // Wait up to 50ms, and handle any events which occur.
  // Returns true if there are any outstanding requests.
  bool WaitForEvents();

  // From EpollCallbackInterface
  void OnRegistration(EpollServer* eps, int fd, int event_mask) override {}
  void OnModification(int fd, int event_mask) override {}
  void OnEvent(int fd, EpollEvent* event) override;
  // |fd_| can be unregistered without the client being disconnected. This
  // happens in b3m QuicProber where we unregister |fd_| to feed in events to
  // the client from the SelectServer.
  void OnUnregistration(int fd, bool replaced) override {}
  void OnShutdown(EpollServer* eps, int fd) override {}

  // QuicDataStream::Visitor
  void OnClose(QuicDataStream* stream) override;

  QuicClientSession* session() { return session_.get(); }

  bool connected() const;
  bool goaway_received() const;

  void set_bind_to_address(IPAddressNumber address) {
    bind_to_address_ = address;
  }

  IPAddressNumber bind_to_address() const { return bind_to_address_; }

  void set_local_port(int local_port) { local_port_ = local_port; }

  const IPEndPoint& server_address() const { return server_address_; }

  const IPEndPoint& client_address() const { return client_address_; }

  int fd() { return fd_; }

  const QuicServerId& server_id() const { return server_id_; }

  // This should only be set before the initial Connect()
  void set_server_id(const QuicServerId& server_id) {
    server_id_ = server_id;
  }

  void SetUserAgentID(const std::string& user_agent_id) {
    crypto_config_.set_user_agent_id(user_agent_id);
  }

  // SetProofVerifier sets the ProofVerifier that will be used to verify the
  // server's certificate and takes ownership of |verifier|.
  void SetProofVerifier(ProofVerifier* verifier) {
    // TODO(rtenneti): We should set ProofVerifier in QuicClientSession.
    crypto_config_.SetProofVerifier(verifier);
  }

  // SetChannelIDSource sets a ChannelIDSource that will be called, when the
  // server supports channel IDs, to obtain a channel ID for signing a message
  // proving possession of the channel ID. This object takes ownership of
  // |source|.
  void SetChannelIDSource(ChannelIDSource* source) {
    crypto_config_.SetChannelIDSource(source);
  }

  void SetSupportedVersions(const QuicVersionVector& versions) {
    supported_versions_ = versions;
  }

  // Takes ownership of the listener.
  void set_response_listener(ResponseListener* listener) {
    response_listener_.reset(listener);
  }

  QuicConfig* config() { return &config_; }

  void set_store_response(bool val) { store_response_ = val; }

  size_t latest_response_code() const;
  const std::string& latest_response_headers() const;
  const std::string& latest_response_body() const;

 protected:
  virtual QuicConnectionId GenerateConnectionId();
  virtual QuicEpollConnectionHelper* CreateQuicConnectionHelper();
  virtual QuicPacketWriter* CreateQuicPacketWriter();

  virtual int ReadPacket(char* buffer,
                         int buffer_len,
                         IPEndPoint* server_address,
                         IPAddressNumber* client_ip);

  virtual QuicClientSession* CreateQuicClientSession(
      const QuicConfig& config,
      QuicConnection* connection,
      const QuicServerId& server_id,
      QuicCryptoClientConfig* crypto_config);

  EpollServer* epoll_server() { return epoll_server_; }

  // If the socket has been created, then unregister and close() the FD.
  virtual void CleanUpUDPSocket();

 private:
  friend class net::tools::test::QuicClientPeer;

  // Used during initialization: creates the UDP socket FD, sets socket options,
  // and binds the socket to our address.
  bool CreateUDPSocket();

  // Actually clean up the socket.
  void CleanUpUDPSocketImpl();

  // Read a UDP packet and hand it to the framer.
  bool ReadAndProcessPacket();

  // Address of the server.
  const IPEndPoint server_address_;

  // |server_id_| is a tuple (hostname, port, is_https) of the server.
  QuicServerId server_id_;

  // config_ and crypto_config_ contain configuration and cached state about
  // servers.
  QuicConfig config_;
  QuicCryptoClientConfig crypto_config_;

  // Address of the client if the client is connected to the server.
  IPEndPoint client_address_;

  // If initialized, the address to bind to.
  IPAddressNumber bind_to_address_;
  // Local port to bind to. Initialize to 0.
  int local_port_;

  // Writer used to actually send packets to the wire. Needs to outlive
  // |session_|.
  scoped_ptr<QuicPacketWriter> writer_;

  // Session which manages streams.
  scoped_ptr<QuicClientSession> session_;
  // Listens for events on the client socket.
  EpollServer* epoll_server_;
  // UDP socket.
  int fd_;

  // Helper to be used by created connections.
  scoped_ptr<QuicEpollConnectionHelper> helper_;

  // Listens for full responses.
  scoped_ptr<ResponseListener> response_listener_;

  // Tracks if the client is initialized to connect.
  bool initialized_;

  // If overflow_supported_ is true, this will be the number of packets dropped
  // during the lifetime of the server.
  QuicPacketCount packets_dropped_;

  // True if the kernel supports SO_RXQ_OVFL, the number of packets dropped
  // because the socket would otherwise overflow.
  bool overflow_supported_;

  // This vector contains QUIC versions which we currently support.
  // This should be ordered such that the highest supported version is the first
  // element, with subsequent elements in descending order (versions can be
  // skipped as necessary). We will always pick supported_versions_[0] as the
  // initial version to use.
  QuicVersionVector supported_versions_;

  // If true, store the latest response code, headers, and body.
  bool store_response_;
  // HTTP response code from most recent response.
  size_t latest_response_code_;
  // HTTP headers from most recent response.
  std::string latest_response_headers_;
  // Body of most recent response.
  std::string latest_response_body_;

  DISALLOW_COPY_AND_ASSIGN(QuicClient);
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_CLIENT_H_
