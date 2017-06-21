// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket_pool.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_delegate.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/http/http_response_headers.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_test_util_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;
const char * const kAuthHeaders[] = {
  "proxy-authorization", "Basic Zm9vOmJhcg=="
};
const int kAuthHeadersSize = arraysize(kAuthHeaders) / 2;

enum HttpProxyType {
  HTTP,
  HTTPS,
  SPDY
};

struct HttpProxyClientSocketPoolTestParams {
  HttpProxyClientSocketPoolTestParams()
      : proxy_type(HTTP),
        protocol(kProtoSPDY31) {}

  HttpProxyClientSocketPoolTestParams(
      HttpProxyType proxy_type,
      NextProto protocol)
      : proxy_type(proxy_type),
        protocol(protocol) {}

  HttpProxyType proxy_type;
  NextProto protocol;
};

typedef ::testing::TestWithParam<HttpProxyType> TestWithHttpParam;

const char kHttpProxyHost[] = "httpproxy.example.com";
const char kHttpsProxyHost[] = "httpsproxy.example.com";

class TestProxyDelegate : public ProxyDelegate {
 public:
  TestProxyDelegate()
      : on_before_tunnel_request_called_(false),
        on_tunnel_request_completed_called_(false),
        on_tunnel_headers_received_called_(false) {
  }

  ~TestProxyDelegate() override {}

  bool on_before_tunnel_request_called() const {
    return on_before_tunnel_request_called_;
  }

  bool on_tunnel_request_completed_called() const {
    return on_tunnel_request_completed_called_;
  }

  bool on_tunnel_headers_received_called() const {
    return on_tunnel_headers_received_called_;
  }

  void VerifyOnTunnelRequestCompleted(const std::string& endpoint,
                                      const std::string& proxy_server) const {
    EXPECT_TRUE(on_tunnel_request_completed_called_);
    EXPECT_TRUE(HostPortPair::FromString(endpoint).Equals(
        on_tunnel_request_completed_endpoint_));
    EXPECT_TRUE(HostPortPair::FromString(proxy_server).Equals(
        on_tunnel_request_completed_proxy_server_));
  }

  void VerifyOnTunnelHeadersReceived(const std::string& origin,
                                     const std::string& proxy_server,
                                     const std::string& status_line) const {
    EXPECT_TRUE(on_tunnel_headers_received_called_);
    EXPECT_TRUE(HostPortPair::FromString(origin).Equals(
                    on_tunnel_headers_received_origin_));
    EXPECT_TRUE(HostPortPair::FromString(proxy_server).Equals(
                    on_tunnel_headers_received_proxy_server_));
    EXPECT_EQ(status_line, on_tunnel_headers_received_status_line_);
  }

  // ProxyDelegate:
  void OnResolveProxy(const GURL& url,
                      int load_flags,
                      const ProxyService& proxy_service,
                      ProxyInfo* result) override {}

  void OnTunnelConnectCompleted(const HostPortPair& endpoint,
                                const HostPortPair& proxy_server,
                                int net_error) override {
    on_tunnel_request_completed_called_ = true;
    on_tunnel_request_completed_endpoint_ = endpoint;
    on_tunnel_request_completed_proxy_server_ = proxy_server;
  }

  void OnFallback(const ProxyServer& bad_proxy, int net_error) override {}

  void OnBeforeSendHeaders(URLRequest* request,
                           const ProxyInfo& proxy_info,
                           HttpRequestHeaders* headers) override {}

  void OnBeforeTunnelRequest(const HostPortPair& proxy_server,
                             HttpRequestHeaders* extra_headers) override {
    on_before_tunnel_request_called_ = true;
    if (extra_headers) {
      extra_headers->SetHeader("Foo", proxy_server.ToString());
    }
  }

  void OnTunnelHeadersReceived(
      const HostPortPair& origin,
      const HostPortPair& proxy_server,
      const HttpResponseHeaders& response_headers) override {
    on_tunnel_headers_received_called_ = true;
    on_tunnel_headers_received_origin_ = origin;
    on_tunnel_headers_received_proxy_server_ = proxy_server;
    on_tunnel_headers_received_status_line_ = response_headers.GetStatusLine();
  }

 private:
  bool on_before_tunnel_request_called_;
  bool on_tunnel_request_completed_called_;
  bool on_tunnel_headers_received_called_;
  HostPortPair on_tunnel_request_completed_endpoint_;
  HostPortPair on_tunnel_request_completed_proxy_server_;
  HostPortPair on_tunnel_headers_received_origin_;
  HostPortPair on_tunnel_headers_received_proxy_server_;
  std::string on_tunnel_headers_received_status_line_;
};


class HttpProxyClientSocketPoolTest
    : public ::testing::TestWithParam<HttpProxyClientSocketPoolTestParams> {
 protected:
  HttpProxyClientSocketPoolTest()
      : session_deps_(GetParam().protocol),
        transport_socket_pool_(kMaxSockets,
                               kMaxSocketsPerGroup,
                               session_deps_.socket_factory.get()),
        ssl_socket_pool_(kMaxSockets,
                         kMaxSocketsPerGroup,
                         session_deps_.cert_verifier.get(),
                         NULL /* channel_id_store */,
                         NULL /* transport_security_state */,
                         NULL /* cert_transparency_verifier */,
                         NULL /* cert_policy_enforcer */,
                         std::string() /* ssl_session_cache_shard */,
                         session_deps_.socket_factory.get(),
                         &transport_socket_pool_,
                         NULL,
                         NULL,
                         session_deps_.ssl_config_service.get(),
                         BoundNetLog().net_log()),
        session_(CreateNetworkSession()),
        spdy_util_(GetParam().protocol),
        pool_(kMaxSockets,
              kMaxSocketsPerGroup,
              &transport_socket_pool_,
              &ssl_socket_pool_,
              NULL) {}

  virtual ~HttpProxyClientSocketPoolTest() {
  }

  void AddAuthToCache() {
    const base::string16 kFoo(base::ASCIIToUTF16("foo"));
    const base::string16 kBar(base::ASCIIToUTF16("bar"));
    GURL proxy_url(GetParam().proxy_type == HTTP ?
                   (std::string("http://") + kHttpProxyHost) :
                   (std::string("https://") + kHttpsProxyHost));
    session_->http_auth_cache()->Add(proxy_url,
                                     "MyRealm1",
                                     HttpAuth::AUTH_SCHEME_BASIC,
                                     "Basic realm=MyRealm1",
                                     AuthCredentials(kFoo, kBar),
                                     "/");
  }

  scoped_refptr<TransportSocketParams> CreateHttpProxyParams() const {
    if (GetParam().proxy_type != HTTP)
      return NULL;
    return new TransportSocketParams(
        HostPortPair(kHttpProxyHost, 80),
        false,
        false,
        OnHostResolutionCallback(),
        TransportSocketParams::COMBINE_CONNECT_AND_WRITE_DEFAULT);
  }

  scoped_refptr<SSLSocketParams> CreateHttpsProxyParams() const {
    if (GetParam().proxy_type == HTTP)
      return NULL;
    return new SSLSocketParams(
        new TransportSocketParams(
            HostPortPair(kHttpsProxyHost, 443),
            false,
            false,
            OnHostResolutionCallback(),
            TransportSocketParams::COMBINE_CONNECT_AND_WRITE_DEFAULT),
        NULL,
        NULL,
        HostPortPair(kHttpsProxyHost, 443),
        SSLConfig(),
        PRIVACY_MODE_DISABLED,
        0,
        false);
  }

  // Returns the a correctly constructed HttpProxyParms
  // for the HTTP or HTTPS proxy.
  scoped_refptr<HttpProxySocketParams> CreateParams(
      bool tunnel,
      ProxyDelegate* proxy_delegate) {
    return scoped_refptr<HttpProxySocketParams>(new HttpProxySocketParams(
        CreateHttpProxyParams(),
        CreateHttpsProxyParams(),
        std::string(),
        HostPortPair("www.google.com", tunnel ? 443 : 80),
        session_->http_auth_cache(),
        session_->http_auth_handler_factory(),
        session_->spdy_session_pool(),
        tunnel,
        proxy_delegate));
  }

  scoped_refptr<HttpProxySocketParams> CreateTunnelParams(
      ProxyDelegate* proxy_delegate) {
    return CreateParams(true, proxy_delegate);
  }

  scoped_refptr<HttpProxySocketParams> CreateNoTunnelParams(
      ProxyDelegate* proxy_delegate) {
    return CreateParams(false, proxy_delegate);
  }

  MockClientSocketFactory* socket_factory() {
    return session_deps_.socket_factory.get();
  }

  void Initialize(MockRead* reads, size_t reads_count,
                  MockWrite* writes, size_t writes_count,
                  MockRead* spdy_reads, size_t spdy_reads_count,
                  MockWrite* spdy_writes, size_t spdy_writes_count) {
    if (GetParam().proxy_type == SPDY) {
      data_.reset(new SequencedSocketData(spdy_reads, spdy_reads_count,
                                          spdy_writes, spdy_writes_count));
    } else {
      data_.reset(
          new SequencedSocketData(reads, reads_count, writes, writes_count));
    }

    data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));

    socket_factory()->AddSocketDataProvider(data_.get());

    if (GetParam().proxy_type != HTTP) {
      ssl_data_.reset(new SSLSocketDataProvider(SYNCHRONOUS, OK));
      if (GetParam().proxy_type == SPDY) {
        InitializeSpdySsl();
      }
      socket_factory()->AddSSLSocketDataProvider(ssl_data_.get());
    }
  }

  void InitializeSpdySsl() {
    ssl_data_->SetNextProto(GetParam().protocol);
  }

  HttpNetworkSession* CreateNetworkSession() {
    return SpdySessionDependencies::SpdyCreateSession(&session_deps_);
  }

  RequestPriority GetLastTransportRequestPriority() const {
    return transport_socket_pool_.last_request_priority();
  }

 private:
  SpdySessionDependencies session_deps_;

  MockTransportClientSocketPool transport_socket_pool_;
  MockHostResolver host_resolver_;
  scoped_ptr<CertVerifier> cert_verifier_;
  SSLClientSocketPool ssl_socket_pool_;

  const scoped_refptr<HttpNetworkSession> session_;

 protected:
  SpdyTestUtil spdy_util_;
  scoped_ptr<SSLSocketDataProvider> ssl_data_;
  scoped_ptr<SequencedSocketData> data_;
  HttpProxyClientSocketPool pool_;
  ClientSocketHandle handle_;
  TestCompletionCallback callback_;
};

//-----------------------------------------------------------------------------
// All tests are run with three different proxy types: HTTP, HTTPS (non-SPDY)
// and SPDY.
//
// TODO(akalin): Use ::testing::Combine() when we are able to use
// <tr1/tuple>.
INSTANTIATE_TEST_CASE_P(
    HttpProxyClientSocketPoolTests,
    HttpProxyClientSocketPoolTest,
    ::testing::Values(HttpProxyClientSocketPoolTestParams(HTTP, kProtoSPDY31),
                      HttpProxyClientSocketPoolTestParams(HTTPS, kProtoSPDY31),
                      HttpProxyClientSocketPoolTestParams(SPDY, kProtoSPDY31),
                      HttpProxyClientSocketPoolTestParams(HTTP, kProtoHTTP2_14),
                      HttpProxyClientSocketPoolTestParams(HTTPS,
                                                          kProtoHTTP2_14),
                      HttpProxyClientSocketPoolTestParams(SPDY, kProtoHTTP2_14),
                      HttpProxyClientSocketPoolTestParams(HTTP, kProtoHTTP2),
                      HttpProxyClientSocketPoolTestParams(HTTPS, kProtoHTTP2),
                      HttpProxyClientSocketPoolTestParams(SPDY, kProtoHTTP2)));

TEST_P(HttpProxyClientSocketPoolTest, NoTunnel) {
  Initialize(NULL, 0, NULL, 0, NULL, 0, NULL, 0);

  scoped_ptr<TestProxyDelegate> proxy_delegate(new TestProxyDelegate());
  int rv = handle_.Init("a", CreateNoTunnelParams(proxy_delegate.get()), LOW,
                        CompletionCallback(), &pool_, BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle_.is_initialized());
  ASSERT_TRUE(handle_.socket());
  HttpProxyClientSocket* tunnel_socket =
          static_cast<HttpProxyClientSocket*>(handle_.socket());
  EXPECT_TRUE(tunnel_socket->IsConnected());
  EXPECT_FALSE(proxy_delegate->on_before_tunnel_request_called());
  EXPECT_FALSE(proxy_delegate->on_tunnel_headers_received_called());
  EXPECT_TRUE(proxy_delegate->on_tunnel_request_completed_called());
}

// Make sure that HttpProxyConnectJob passes on its priority to its
// (non-SSL) socket request on Init.
TEST_P(HttpProxyClientSocketPoolTest, SetSocketRequestPriorityOnInit) {
  Initialize(NULL, 0, NULL, 0, NULL, 0, NULL, 0);
  EXPECT_EQ(OK,
            handle_.Init("a", CreateNoTunnelParams(NULL), HIGHEST,
                         CompletionCallback(), &pool_, BoundNetLog()));
  EXPECT_EQ(HIGHEST, GetLastTransportRequestPriority());
}

TEST_P(HttpProxyClientSocketPoolTest, NeedAuth) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    // No credentials.
    MockRead(ASYNC, 1, "HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead(ASYNC, 2, "Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead(ASYNC, 3, "Content-Length: 10\r\n\r\n"),
    MockRead(ASYNC, 4, "0123456789"),
  };
  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyConnect(
      NULL, 0, 1, LOW, HostPortPair("www.google.com", 443)));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*req, 0, ASYNC),
    CreateMockWrite(*rst, 2, ASYNC),
  };
  SpdyHeaderBlock resp_block;
  resp_block[spdy_util_.GetStatusKey()] = "407";
  resp_block["proxy-authenticate"] = "Basic realm=\"MyRealm1\"";
  spdy_util_.MaybeAddVersionHeader(&resp_block);

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyReply(1, resp_block));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 3)
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes),
             spdy_reads, arraysize(spdy_reads), spdy_writes,
             arraysize(spdy_writes));

  int rv = handle_.Init("a", CreateTunnelParams(NULL), LOW,
                        callback_.callback(), &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  rv = callback_.WaitForResult();
  EXPECT_EQ(ERR_PROXY_AUTH_REQUESTED, rv);
  EXPECT_TRUE(handle_.is_initialized());
  ASSERT_TRUE(handle_.socket());
  ProxyClientSocket* tunnel_socket =
      static_cast<ProxyClientSocket*>(handle_.socket());
  if (GetParam().proxy_type == SPDY) {
    EXPECT_TRUE(tunnel_socket->IsConnected());
    EXPECT_TRUE(tunnel_socket->IsUsingSpdy());
  } else {
    EXPECT_FALSE(tunnel_socket->IsConnected());
    EXPECT_FALSE(tunnel_socket->IsUsingSpdy());
  }
}

TEST_P(HttpProxyClientSocketPoolTest, HaveAuth) {
  // It's pretty much impossible to make the SPDY case behave synchronously
  // so we skip this test for SPDY
  if (GetParam().proxy_type == SPDY)
    return;
  std::string proxy_host_port =
      GetParam().proxy_type == HTTP ?
          (kHttpProxyHost + std::string(":80")) :
          (kHttpsProxyHost + std::string(":443"));
  std::string request =
      "CONNECT www.google.com:443 HTTP/1.1\r\n"
      "Host: www.google.com\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n"
      "Foo: " + proxy_host_port + "\r\n\r\n";
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, request.c_str()),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes), NULL, 0,
             NULL, 0);
  AddAuthToCache();

  scoped_ptr<TestProxyDelegate> proxy_delegate(new TestProxyDelegate());
  int rv = handle_.Init("a", CreateTunnelParams(proxy_delegate.get()), LOW,
                        callback_.callback(), &pool_, BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle_.is_initialized());
  ASSERT_TRUE(handle_.socket());
  HttpProxyClientSocket* tunnel_socket =
          static_cast<HttpProxyClientSocket*>(handle_.socket());
  EXPECT_TRUE(tunnel_socket->IsConnected());
  proxy_delegate->VerifyOnTunnelHeadersReceived(
      "www.google.com:443",
      proxy_host_port.c_str(),
      "HTTP/1.1 200 Connection Established");
  proxy_delegate->VerifyOnTunnelRequestCompleted(
      "www.google.com:443",
      proxy_host_port.c_str());
}

TEST_P(HttpProxyClientSocketPoolTest, AsyncHaveAuth) {
  std::string proxy_host_port =
      GetParam().proxy_type == HTTP ?
          (kHttpProxyHost + std::string(":80")) :
          (kHttpsProxyHost + std::string(":443"));
  std::string request =
      "CONNECT www.google.com:443 HTTP/1.1\r\n"
      "Host: www.google.com\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n"
      "Foo: " + proxy_host_port + "\r\n\r\n";
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, request.c_str()),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyConnect(kAuthHeaders, kAuthHeadersSize, 1, LOW,
                                      HostPortPair("www.google.com", 443)));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*req, 0, ASYNC)
  };
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead spdy_reads[] = {
      CreateMockRead(*resp, 1, ASYNC),
      // Connection stays open.
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes),
             spdy_reads, arraysize(spdy_reads), spdy_writes,
             arraysize(spdy_writes));
  AddAuthToCache();

  scoped_ptr<TestProxyDelegate> proxy_delegate(new TestProxyDelegate());
  int rv = handle_.Init("a", CreateTunnelParams(proxy_delegate.get()), LOW,
                        callback_.callback(), &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  EXPECT_EQ(OK, callback_.WaitForResult());
  EXPECT_TRUE(handle_.is_initialized());
  ASSERT_TRUE(handle_.socket());
  EXPECT_TRUE(handle_.socket()->IsConnected());
  proxy_delegate->VerifyOnTunnelRequestCompleted(
      "www.google.com:443",
      proxy_host_port.c_str());
}

// Make sure that HttpProxyConnectJob passes on its priority to its
// SPDY session's socket request on Init (if applicable).
TEST_P(HttpProxyClientSocketPoolTest,
       SetSpdySessionSocketRequestPriorityOnInit) {
  if (GetParam().proxy_type != SPDY)
    return;

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyConnect(kAuthHeaders, kAuthHeadersSize, 1, MEDIUM,
                                      HostPortPair("www.google.com", 443)));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*req, 0, ASYNC)
  };
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 2)
  };

  Initialize(NULL, 0, NULL, 0,
             spdy_reads, arraysize(spdy_reads),
             spdy_writes, arraysize(spdy_writes));
  AddAuthToCache();

  EXPECT_EQ(ERR_IO_PENDING,
            handle_.Init("a", CreateTunnelParams(NULL), MEDIUM,
                         callback_.callback(), &pool_, BoundNetLog()));
  EXPECT_EQ(MEDIUM, GetLastTransportRequestPriority());

  EXPECT_EQ(OK, callback_.WaitForResult());
}

TEST_P(HttpProxyClientSocketPoolTest, TCPError) {
  if (GetParam().proxy_type == SPDY) return;
  data_.reset(new SequencedSocketData(NULL, 0, NULL, 0));
  data_->set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_CLOSED));

  socket_factory()->AddSocketDataProvider(data_.get());

  int rv = handle_.Init("a", CreateTunnelParams(NULL), LOW,
                        callback_.callback(), &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, callback_.WaitForResult());

  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());
}

TEST_P(HttpProxyClientSocketPoolTest, SSLError) {
  if (GetParam().proxy_type == HTTP) return;
  data_.reset(new SequencedSocketData(NULL, 0, NULL, 0));
  data_->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data_.get());

  ssl_data_.reset(new SSLSocketDataProvider(ASYNC,
                                            ERR_CERT_AUTHORITY_INVALID));
  if (GetParam().proxy_type == SPDY) {
    InitializeSpdySsl();
  }
  socket_factory()->AddSSLSocketDataProvider(ssl_data_.get());

  int rv = handle_.Init("a", CreateTunnelParams(NULL), LOW,
                        callback_.callback(), &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  EXPECT_EQ(ERR_PROXY_CERTIFICATE_INVALID, callback_.WaitForResult());

  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());
}

TEST_P(HttpProxyClientSocketPoolTest, SslClientAuth) {
  if (GetParam().proxy_type == HTTP) return;
  data_.reset(new SequencedSocketData(NULL, 0, NULL, 0));
  data_->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data_.get());

  ssl_data_.reset(new SSLSocketDataProvider(ASYNC,
                                            ERR_SSL_CLIENT_AUTH_CERT_NEEDED));
  if (GetParam().proxy_type == SPDY) {
    InitializeSpdySsl();
  }
  socket_factory()->AddSSLSocketDataProvider(ssl_data_.get());

  int rv = handle_.Init("a", CreateTunnelParams(NULL), LOW,
                        callback_.callback(), &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  EXPECT_EQ(ERR_SSL_CLIENT_AUTH_CERT_NEEDED, callback_.WaitForResult());

  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());
}

TEST_P(HttpProxyClientSocketPoolTest, TunnelUnexpectedClose) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0,
              "CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 Conn"),
    MockRead(ASYNC, ERR_CONNECTION_CLOSED, 2),
  };
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyConnect(kAuthHeaders, kAuthHeadersSize, 1, LOW,
                                      HostPortPair("www.google.com", 443)));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*req, 0, ASYNC)
  };
  MockRead spdy_reads[] = {
    MockRead(ASYNC, ERR_CONNECTION_CLOSED, 1),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes),
             spdy_reads, arraysize(spdy_reads), spdy_writes,
             arraysize(spdy_writes));
  AddAuthToCache();

  int rv = handle_.Init("a", CreateTunnelParams(NULL), LOW,
                        callback_.callback(), &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  if (GetParam().proxy_type == SPDY) {
    // SPDY cannot process a headers block unless it's complete and so it
    // returns ERR_CONNECTION_CLOSED in this case.
    EXPECT_EQ(ERR_CONNECTION_CLOSED, callback_.WaitForResult());
  } else {
    EXPECT_EQ(ERR_RESPONSE_HEADERS_TRUNCATED, callback_.WaitForResult());
  }
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());
}

TEST_P(HttpProxyClientSocketPoolTest, Tunnel1xxResponse) {
  // Tests that 1xx responses are rejected for a CONNECT request.
  if (GetParam().proxy_type == SPDY) {
    // SPDY doesn't have 1xx responses.
    return;
  }

  MockWrite writes[] = {
    MockWrite(ASYNC, 0,
              "CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 100 Continue\r\n\r\n"),
    MockRead(ASYNC, 2, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes),
             NULL, 0, NULL, 0);

  int rv = handle_.Init("a", CreateTunnelParams(NULL), LOW,
                        callback_.callback(), &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, callback_.WaitForResult());
}

TEST_P(HttpProxyClientSocketPoolTest, TunnelSetupError) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0,
              "CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 304 Not Modified\r\n\r\n"),
  };
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyConnect(kAuthHeaders, kAuthHeadersSize, 1, LOW,
                                      HostPortPair("www.google.com", 443)));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*req, 0, ASYNC),
    CreateMockWrite(*rst, 2, ASYNC),
  };
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdySynReplyError(1));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 3),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes),
             spdy_reads, arraysize(spdy_reads), spdy_writes,
             arraysize(spdy_writes));
  AddAuthToCache();

  int rv = handle_.Init("a", CreateTunnelParams(NULL), LOW,
                        callback_.callback(), &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  rv = callback_.WaitForResult();
  // All Proxy CONNECT responses are not trustworthy
  EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());
}

TEST_P(HttpProxyClientSocketPoolTest, TunnelSetupRedirect) {
  const std::string redirectTarget = "https://foo.google.com/";

  const std::string responseText = "HTTP/1.1 302 Found\r\n"
                                   "Location: " + redirectTarget + "\r\n"
                                   "Set-Cookie: foo=bar\r\n"
                                   "\r\n";
  MockWrite writes[] = {
    MockWrite(ASYNC, 0,
              "CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, responseText.c_str()),
  };
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyConnect(kAuthHeaders, kAuthHeadersSize, 1, LOW,
                                      HostPortPair("www.google.com", 443)));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));

  MockWrite spdy_writes[] = {
    CreateMockWrite(*req, 0, ASYNC),
    CreateMockWrite(*rst, 3, ASYNC),
  };

  const char* const responseHeaders[] = {
    "location", redirectTarget.c_str(),
    "set-cookie", "foo=bar",
  };
  const int responseHeadersSize = arraysize(responseHeaders) / 2;
  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdySynReplyError(
          "302 Found",
          responseHeaders, responseHeadersSize,
          1));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 2),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes),
             spdy_reads, arraysize(spdy_reads), spdy_writes,
             arraysize(spdy_writes));
  AddAuthToCache();

  int rv = handle_.Init("a", CreateTunnelParams(NULL), LOW,
                        callback_.callback(), &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  rv = callback_.WaitForResult();

  if (GetParam().proxy_type == HTTP) {
    // We don't trust 302 responses to CONNECT from HTTP proxies.
    EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, rv);
    EXPECT_FALSE(handle_.is_initialized());
    EXPECT_FALSE(handle_.socket());
  } else {
    // Expect ProxyClientSocket to return the proxy's response, sanitized.
    EXPECT_EQ(ERR_HTTPS_PROXY_TUNNEL_RESPONSE, rv);
    EXPECT_TRUE(handle_.is_initialized());
    ASSERT_TRUE(handle_.socket());

    const ProxyClientSocket* tunnel_socket =
        static_cast<ProxyClientSocket*>(handle_.socket());
    const HttpResponseInfo* response = tunnel_socket->GetConnectResponseInfo();
    const HttpResponseHeaders* headers = response->headers.get();

    // Make sure Set-Cookie header was stripped.
    EXPECT_FALSE(headers->HasHeader("set-cookie"));

    // Make sure Content-Length: 0 header was added.
    EXPECT_TRUE(headers->HasHeaderValue("content-length", "0"));

    // Make sure Location header was included and correct.
    std::string location;
    EXPECT_TRUE(headers->IsRedirect(&location));
    EXPECT_EQ(location, redirectTarget);
  }
}

// It would be nice to also test the timeouts in HttpProxyClientSocketPool.

}  // namespace

}  // namespace net
