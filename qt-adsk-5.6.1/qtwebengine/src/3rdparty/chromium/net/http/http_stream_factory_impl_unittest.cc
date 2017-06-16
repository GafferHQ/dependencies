// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_session_peer.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_request_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_stream.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/mock_client_socket_pool_manager.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
// This file can be included from net/http even though
// it is in net/websockets because it doesn't
// introduce any link dependency to net/websockets.
#include "net/websockets/websocket_handshake_stream_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class MockWebSocketHandshakeStream : public WebSocketHandshakeStreamBase {
 public:
  enum StreamType {
    kStreamTypeBasic,
    kStreamTypeSpdy,
  };

  explicit MockWebSocketHandshakeStream(StreamType type) : type_(type) {}

  ~MockWebSocketHandshakeStream() override {}

  StreamType type() const {
    return type_;
  }

  // HttpStream methods
  int InitializeStream(const HttpRequestInfo* request_info,
                       RequestPriority priority,
                       const BoundNetLog& net_log,
                       const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }
  int SendRequest(const HttpRequestHeaders& request_headers,
                  HttpResponseInfo* response,
                  const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }
  int ReadResponseHeaders(const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }
  int ReadResponseBody(IOBuffer* buf,
                       int buf_len,
                       const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }
  void Close(bool not_reusable) override {}
  bool IsResponseBodyComplete() const override { return false; }
  bool CanFindEndOfResponse() const override { return false; }
  bool IsConnectionReused() const override { return false; }
  void SetConnectionReused() override {}
  bool IsConnectionReusable() const override { return false; }
  int64 GetTotalReceivedBytes() const override { return 0; }
  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override {
    return false;
  }
  void GetSSLInfo(SSLInfo* ssl_info) override {}
  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) override {}
  bool IsSpdyHttpStream() const override { return false; }
  void Drain(HttpNetworkSession* session) override {}
  void SetPriority(RequestPriority priority) override {}
  UploadProgress GetUploadProgress() const override { return UploadProgress(); }
  HttpStream* RenewStreamForAuth() override { return nullptr; }

  scoped_ptr<WebSocketStream> Upgrade() override {
    return scoped_ptr<WebSocketStream>();
  }

 private:
  const StreamType type_;
};

// HttpStreamFactoryImpl subclass that can wait until a preconnect is complete.
class MockHttpStreamFactoryImplForPreconnect : public HttpStreamFactoryImpl {
 public:
  MockHttpStreamFactoryImplForPreconnect(HttpNetworkSession* session,
                                         bool for_websockets)
      : HttpStreamFactoryImpl(session, for_websockets),
        preconnect_done_(false),
        waiting_for_preconnect_(false) {}


  void WaitForPreconnects() {
    while (!preconnect_done_) {
      waiting_for_preconnect_ = true;
      base::MessageLoop::current()->Run();
      waiting_for_preconnect_ = false;
    }
  }

 private:
  // HttpStreamFactoryImpl methods.
  void OnPreconnectsCompleteInternal() override {
    preconnect_done_ = true;
    if (waiting_for_preconnect_)
      base::MessageLoop::current()->Quit();
  }

  bool preconnect_done_;
  bool waiting_for_preconnect_;
};

class StreamRequestWaiter : public HttpStreamRequest::Delegate {
 public:
  StreamRequestWaiter()
      : waiting_for_stream_(false),
        stream_done_(false) {}

  // HttpStreamRequest::Delegate

  void OnStreamReady(const SSLConfig& used_ssl_config,
                     const ProxyInfo& used_proxy_info,
                     HttpStream* stream) override {
    stream_done_ = true;
    if (waiting_for_stream_)
      base::MessageLoop::current()->Quit();
    stream_.reset(stream);
    used_ssl_config_ = used_ssl_config;
    used_proxy_info_ = used_proxy_info;
  }

  void OnWebSocketHandshakeStreamReady(
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      WebSocketHandshakeStreamBase* stream) override {
    stream_done_ = true;
    if (waiting_for_stream_)
      base::MessageLoop::current()->Quit();
    websocket_stream_.reset(stream);
    used_ssl_config_ = used_ssl_config;
    used_proxy_info_ = used_proxy_info;
  }

  void OnStreamFailed(int status,
                      const SSLConfig& used_ssl_config,
                      SSLFailureState ssl_failure_state) override {}

  void OnCertificateError(int status,
                          const SSLConfig& used_ssl_config,
                          const SSLInfo& ssl_info) override {}

  void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                        const SSLConfig& used_ssl_config,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override {}

  void OnNeedsClientAuth(const SSLConfig& used_ssl_config,
                         SSLCertRequestInfo* cert_info) override {}

  void OnHttpsProxyTunnelResponse(const HttpResponseInfo& response_info,
                                  const SSLConfig& used_ssl_config,
                                  const ProxyInfo& used_proxy_info,
                                  HttpStream* stream) override {}

  void WaitForStream() {
    while (!stream_done_) {
      waiting_for_stream_ = true;
      base::MessageLoop::current()->Run();
      waiting_for_stream_ = false;
    }
  }

  const SSLConfig& used_ssl_config() const {
    return used_ssl_config_;
  }

  const ProxyInfo& used_proxy_info() const {
    return used_proxy_info_;
  }

  HttpStream* stream() {
    return stream_.get();
  }

  MockWebSocketHandshakeStream* websocket_stream() {
    return static_cast<MockWebSocketHandshakeStream*>(websocket_stream_.get());
  }

  bool stream_done() const { return stream_done_; }

 private:
  bool waiting_for_stream_;
  bool stream_done_;
  scoped_ptr<HttpStream> stream_;
  scoped_ptr<WebSocketHandshakeStreamBase> websocket_stream_;
  SSLConfig used_ssl_config_;
  ProxyInfo used_proxy_info_;

  DISALLOW_COPY_AND_ASSIGN(StreamRequestWaiter);
};

class WebSocketSpdyHandshakeStream : public MockWebSocketHandshakeStream {
 public:
  explicit WebSocketSpdyHandshakeStream(
      const base::WeakPtr<SpdySession>& spdy_session)
      : MockWebSocketHandshakeStream(kStreamTypeSpdy),
        spdy_session_(spdy_session) {}

  ~WebSocketSpdyHandshakeStream() override {}

  SpdySession* spdy_session() { return spdy_session_.get(); }

 private:
  base::WeakPtr<SpdySession> spdy_session_;
};

class WebSocketBasicHandshakeStream : public MockWebSocketHandshakeStream {
 public:
  explicit WebSocketBasicHandshakeStream(
      scoped_ptr<ClientSocketHandle> connection)
      : MockWebSocketHandshakeStream(kStreamTypeBasic),
        connection_(connection.Pass()) {}

  ~WebSocketBasicHandshakeStream() override {
    connection_->socket()->Disconnect();
  }

  ClientSocketHandle* connection() { return connection_.get(); }

 private:
  scoped_ptr<ClientSocketHandle> connection_;
};

class WebSocketStreamCreateHelper
    : public WebSocketHandshakeStreamBase::CreateHelper {
 public:
  ~WebSocketStreamCreateHelper() override {}

  WebSocketHandshakeStreamBase* CreateBasicStream(
      scoped_ptr<ClientSocketHandle> connection,
      bool using_proxy) override {
    return new WebSocketBasicHandshakeStream(connection.Pass());
  }

  WebSocketHandshakeStreamBase* CreateSpdyStream(
      const base::WeakPtr<SpdySession>& spdy_session,
      bool use_relative_url) override {
    return new WebSocketSpdyHandshakeStream(spdy_session);
  }
};

struct TestCase {
  int num_streams;
  bool ssl;
};

TestCase kTests[] = {
  { 1, false },
  { 2, false },
  { 1, true},
  { 2, true},
};

void PreconnectHelperForURL(int num_streams,
                            const GURL& url,
                            HttpNetworkSession* session) {
  HttpNetworkSessionPeer peer(session);
  MockHttpStreamFactoryImplForPreconnect* mock_factory =
      new MockHttpStreamFactoryImplForPreconnect(session, false);
  peer.SetHttpStreamFactory(scoped_ptr<HttpStreamFactory>(mock_factory));
  SSLConfig ssl_config;
  session->ssl_config_service()->GetSSLConfig(&ssl_config);

  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.load_flags = 0;

  session->http_stream_factory()->PreconnectStreams(
      num_streams, request, DEFAULT_PRIORITY, ssl_config, ssl_config);
  mock_factory->WaitForPreconnects();
};

void PreconnectHelper(const TestCase& test,
                      HttpNetworkSession* session) {
  GURL url = test.ssl ? GURL("https://www.google.com") :
      GURL("http://www.google.com");
  PreconnectHelperForURL(test.num_streams, url, session);
};

template<typename ParentPool>
class CapturePreconnectsSocketPool : public ParentPool {
 public:
  CapturePreconnectsSocketPool(HostResolver* host_resolver,
                               CertVerifier* cert_verifier);

  int last_num_streams() const {
    return last_num_streams_;
  }

  int RequestSocket(const std::string& group_name,
                    const void* socket_params,
                    RequestPriority priority,
                    ClientSocketHandle* handle,
                    const CompletionCallback& callback,
                    const BoundNetLog& net_log) override {
    ADD_FAILURE();
    return ERR_UNEXPECTED;
  }

  void RequestSockets(const std::string& group_name,
                      const void* socket_params,
                      int num_sockets,
                      const BoundNetLog& net_log) override {
    last_num_streams_ = num_sockets;
  }

  void CancelRequest(const std::string& group_name,
                     ClientSocketHandle* handle) override {
    ADD_FAILURE();
  }
  void ReleaseSocket(const std::string& group_name,
                     scoped_ptr<StreamSocket> socket,
                     int id) override {
    ADD_FAILURE();
  }
  void CloseIdleSockets() override { ADD_FAILURE(); }
  int IdleSocketCount() const override {
    ADD_FAILURE();
    return 0;
  }
  int IdleSocketCountInGroup(const std::string& group_name) const override {
    ADD_FAILURE();
    return 0;
  }
  LoadState GetLoadState(const std::string& group_name,
                         const ClientSocketHandle* handle) const override {
    ADD_FAILURE();
    return LOAD_STATE_IDLE;
  }
  base::TimeDelta ConnectionTimeout() const override {
    return base::TimeDelta();
  }

 private:
  int last_num_streams_;
};

typedef CapturePreconnectsSocketPool<TransportClientSocketPool>
CapturePreconnectsTransportSocketPool;
typedef CapturePreconnectsSocketPool<HttpProxyClientSocketPool>
CapturePreconnectsHttpProxySocketPool;
typedef CapturePreconnectsSocketPool<SOCKSClientSocketPool>
CapturePreconnectsSOCKSSocketPool;
typedef CapturePreconnectsSocketPool<SSLClientSocketPool>
CapturePreconnectsSSLSocketPool;

template <typename ParentPool>
CapturePreconnectsSocketPool<ParentPool>::CapturePreconnectsSocketPool(
    HostResolver* host_resolver,
    CertVerifier* /* cert_verifier */)
    : ParentPool(0, 0, host_resolver, nullptr, nullptr), last_num_streams_(-1) {
}

template <>
CapturePreconnectsHttpProxySocketPool::CapturePreconnectsSocketPool(
    HostResolver* /* host_resolver */,
    CertVerifier* /* cert_verifier */)
    : HttpProxyClientSocketPool(0, 0, nullptr, nullptr, nullptr),
      last_num_streams_(-1) {
}

template <>
CapturePreconnectsSSLSocketPool::CapturePreconnectsSocketPool(
    HostResolver* /* host_resolver */,
    CertVerifier* cert_verifier)
    : SSLClientSocketPool(0,
                          0,
                          cert_verifier,
                          nullptr,        // channel_id_store
                          nullptr,        // transport_security_state
                          nullptr,        // cert_transparency_verifier
                          nullptr,        // cert_policy_enforcer
                          std::string(),  // ssl_session_cache_shard
                          nullptr,        // deterministic_socket_factory
                          nullptr,        // transport_socket_pool
                          nullptr,
                          nullptr,
                          nullptr,   // ssl_config_service
                          nullptr),  // net_log
      last_num_streams_(-1) {
}

class HttpStreamFactoryTest : public ::testing::Test,
                              public ::testing::WithParamInterface<NextProto> {
};

INSTANTIATE_TEST_CASE_P(NextProto,
                        HttpStreamFactoryTest,
                        testing::Values(kProtoSPDY31,
                                        kProtoHTTP2_14,
                                        kProtoHTTP2));

TEST_P(HttpStreamFactoryTest, PreconnectDirect) {
  for (size_t i = 0; i < arraysize(kTests); ++i) {
    SpdySessionDependencies session_deps(
        GetParam(), ProxyService::CreateDirect());
    scoped_refptr<HttpNetworkSession> session(
        SpdySessionDependencies::SpdyCreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session);
    CapturePreconnectsTransportSocketPool* transport_conn_pool =
        new CapturePreconnectsTransportSocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    CapturePreconnectsSSLSocketPool* ssl_conn_pool =
        new CapturePreconnectsSSLSocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    scoped_ptr<MockClientSocketPoolManager> mock_pool_manager(
        new MockClientSocketPoolManager);
    mock_pool_manager->SetTransportSocketPool(transport_conn_pool);
    mock_pool_manager->SetSSLSocketPool(ssl_conn_pool);
    peer.SetClientSocketPoolManager(mock_pool_manager.Pass());
    PreconnectHelper(kTests[i], session.get());
    if (kTests[i].ssl)
      EXPECT_EQ(kTests[i].num_streams, ssl_conn_pool->last_num_streams());
    else
      EXPECT_EQ(kTests[i].num_streams, transport_conn_pool->last_num_streams());
  }
}

TEST_P(HttpStreamFactoryTest, PreconnectHttpProxy) {
  for (size_t i = 0; i < arraysize(kTests); ++i) {
    SpdySessionDependencies session_deps(
        GetParam(), ProxyService::CreateFixed("http_proxy"));
    scoped_refptr<HttpNetworkSession> session(
        SpdySessionDependencies::SpdyCreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session);
    HostPortPair proxy_host("http_proxy", 80);
    CapturePreconnectsHttpProxySocketPool* http_proxy_pool =
        new CapturePreconnectsHttpProxySocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    CapturePreconnectsSSLSocketPool* ssl_conn_pool =
        new CapturePreconnectsSSLSocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    scoped_ptr<MockClientSocketPoolManager> mock_pool_manager(
        new MockClientSocketPoolManager);
    mock_pool_manager->SetSocketPoolForHTTPProxy(proxy_host, http_proxy_pool);
    mock_pool_manager->SetSocketPoolForSSLWithProxy(proxy_host, ssl_conn_pool);
    peer.SetClientSocketPoolManager(mock_pool_manager.Pass());
    PreconnectHelper(kTests[i], session.get());
    if (kTests[i].ssl)
      EXPECT_EQ(kTests[i].num_streams, ssl_conn_pool->last_num_streams());
    else
      EXPECT_EQ(kTests[i].num_streams, http_proxy_pool->last_num_streams());
  }
}

TEST_P(HttpStreamFactoryTest, PreconnectSocksProxy) {
  for (size_t i = 0; i < arraysize(kTests); ++i) {
    SpdySessionDependencies session_deps(
        GetParam(), ProxyService::CreateFixed("socks4://socks_proxy:1080"));
    scoped_refptr<HttpNetworkSession> session(
        SpdySessionDependencies::SpdyCreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session);
    HostPortPair proxy_host("socks_proxy", 1080);
    CapturePreconnectsSOCKSSocketPool* socks_proxy_pool =
        new CapturePreconnectsSOCKSSocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    CapturePreconnectsSSLSocketPool* ssl_conn_pool =
        new CapturePreconnectsSSLSocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    scoped_ptr<MockClientSocketPoolManager> mock_pool_manager(
        new MockClientSocketPoolManager);
    mock_pool_manager->SetSocketPoolForSOCKSProxy(proxy_host, socks_proxy_pool);
    mock_pool_manager->SetSocketPoolForSSLWithProxy(proxy_host, ssl_conn_pool);
    peer.SetClientSocketPoolManager(mock_pool_manager.Pass());
    PreconnectHelper(kTests[i], session.get());
    if (kTests[i].ssl)
      EXPECT_EQ(kTests[i].num_streams, ssl_conn_pool->last_num_streams());
    else
      EXPECT_EQ(kTests[i].num_streams, socks_proxy_pool->last_num_streams());
  }
}

TEST_P(HttpStreamFactoryTest, PreconnectDirectWithExistingSpdySession) {
  for (size_t i = 0; i < arraysize(kTests); ++i) {
    SpdySessionDependencies session_deps(
        GetParam(), ProxyService::CreateDirect());
    scoped_refptr<HttpNetworkSession> session(
        SpdySessionDependencies::SpdyCreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session);

    // Put a SpdySession in the pool.
    HostPortPair host_port_pair("www.google.com", 443);
    SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                       PRIVACY_MODE_DISABLED);
    ignore_result(CreateFakeSpdySession(session->spdy_session_pool(), key));

    CapturePreconnectsTransportSocketPool* transport_conn_pool =
        new CapturePreconnectsTransportSocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    CapturePreconnectsSSLSocketPool* ssl_conn_pool =
        new CapturePreconnectsSSLSocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    scoped_ptr<MockClientSocketPoolManager> mock_pool_manager(
        new MockClientSocketPoolManager);
    mock_pool_manager->SetTransportSocketPool(transport_conn_pool);
    mock_pool_manager->SetSSLSocketPool(ssl_conn_pool);
    peer.SetClientSocketPoolManager(mock_pool_manager.Pass());
    PreconnectHelper(kTests[i], session.get());
    // We shouldn't be preconnecting if we have an existing session, which is
    // the case for https://www.google.com.
    if (kTests[i].ssl)
      EXPECT_EQ(-1, ssl_conn_pool->last_num_streams());
    else
      EXPECT_EQ(kTests[i].num_streams,
                transport_conn_pool->last_num_streams());
  }
}

// Verify that preconnects to unsafe ports are cancelled before they reach
// the SocketPool.
TEST_P(HttpStreamFactoryTest, PreconnectUnsafePort) {
  ASSERT_FALSE(IsPortAllowedForScheme(7, "http"));

  SpdySessionDependencies session_deps(
      GetParam(), ProxyService::CreateDirect());
  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));
  HttpNetworkSessionPeer peer(session);
  CapturePreconnectsTransportSocketPool* transport_conn_pool =
      new CapturePreconnectsTransportSocketPool(
          session_deps.host_resolver.get(),
          session_deps.cert_verifier.get());
  scoped_ptr<MockClientSocketPoolManager> mock_pool_manager(
      new MockClientSocketPoolManager);
  mock_pool_manager->SetTransportSocketPool(transport_conn_pool);
  peer.SetClientSocketPoolManager(mock_pool_manager.Pass());

  PreconnectHelperForURL(1, GURL("http://www.google.com:7"), session.get());
  EXPECT_EQ(-1, transport_conn_pool->last_num_streams());
}

TEST_P(HttpStreamFactoryTest, JobNotifiesProxy) {
  const char* kProxyString = "PROXY bad:99; PROXY maybe:80; DIRECT";
  SpdySessionDependencies session_deps(
      GetParam(), ProxyService::CreateFixedFromPacResult(kProxyString));

  // First connection attempt fails
  StaticSocketDataProvider socket_data1;
  socket_data1.set_connect_data(MockConnect(ASYNC, ERR_ADDRESS_UNREACHABLE));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data1);

  // Second connection attempt succeeds
  StaticSocketDataProvider socket_data2;
  socket_data2.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data2);

  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream. It should succeed using the second proxy in the
  // list.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, ssl_config, ssl_config,
          &waiter, BoundNetLog()));
  waiter.WaitForStream();

  // The proxy that failed should now be known to the proxy_service as bad.
  const ProxyRetryInfoMap& retry_info =
      session->proxy_service()->proxy_retry_info();
  EXPECT_EQ(1u, retry_info.size());
  ProxyRetryInfoMap::const_iterator iter = retry_info.find("bad:99");
  EXPECT_TRUE(iter != retry_info.end());
}

TEST_P(HttpStreamFactoryTest, UnreachableQuicProxyMarkedAsBad) {
  for (int i = 1; i <= 2; i++) {
    int mock_error =
        i == 1 ? ERR_QUIC_PROTOCOL_ERROR : ERR_QUIC_HANDSHAKE_FAILED;

    scoped_ptr<ProxyService> proxy_service;
    proxy_service.reset(
        ProxyService::CreateFixedFromPacResult("QUIC bad:99; DIRECT"));

    HttpNetworkSession::Params params;
    params.enable_quic = true;
    params.enable_quic_for_proxies = true;
    scoped_refptr<SSLConfigServiceDefaults> ssl_config_service(
        new SSLConfigServiceDefaults);
    HttpServerPropertiesImpl http_server_properties;
    MockClientSocketFactory socket_factory;
    params.client_socket_factory = &socket_factory;
    MockHostResolver host_resolver;
    params.host_resolver = &host_resolver;
    TransportSecurityState transport_security_state;
    params.transport_security_state = &transport_security_state;
    params.proxy_service = proxy_service.get();
    params.ssl_config_service = ssl_config_service.get();
    params.http_server_properties = http_server_properties.GetWeakPtr();

    scoped_refptr<HttpNetworkSession> session;
    session = new HttpNetworkSession(params);
    session->quic_stream_factory()->set_require_confirmation(false);

    StaticSocketDataProvider socket_data1;
    socket_data1.set_connect_data(MockConnect(ASYNC, mock_error));
    socket_factory.AddSocketDataProvider(&socket_data1);

    // Second connection attempt succeeds.
    StaticSocketDataProvider socket_data2;
    socket_data2.set_connect_data(MockConnect(ASYNC, OK));
    socket_factory.AddSocketDataProvider(&socket_data2);

    // Now request a stream. It should succeed using the second proxy in the
    // list.
    HttpRequestInfo request_info;
    request_info.method = "GET";
    request_info.url = GURL("http://www.google.com");

    SSLConfig ssl_config;
    StreamRequestWaiter waiter;
    scoped_ptr<HttpStreamRequest> request(
        session->http_stream_factory()->RequestStream(
            request_info, DEFAULT_PRIORITY, ssl_config, ssl_config, &waiter,
            BoundNetLog()));
    waiter.WaitForStream();

    // The proxy that failed should now be known to the proxy_service as bad.
    const ProxyRetryInfoMap& retry_info =
        session->proxy_service()->proxy_retry_info();
    EXPECT_EQ(1u, retry_info.size()) << i;
    EXPECT_TRUE(waiter.used_proxy_info().is_direct());

    ProxyRetryInfoMap::const_iterator iter = retry_info.find("quic://bad:99");
    EXPECT_TRUE(iter != retry_info.end()) << i;
  }
}

}  // namespace

TEST_P(HttpStreamFactoryTest, QuicLossyProxyMarkedAsBad) {
  // Checks if a
  scoped_ptr<ProxyService> proxy_service;
  proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("QUIC bad:99; DIRECT"));

  HttpNetworkSession::Params params;
  params.enable_quic = true;
  params.enable_quic_for_proxies = true;
  scoped_refptr<SSLConfigServiceDefaults> ssl_config_service(
      new SSLConfigServiceDefaults);
  HttpServerPropertiesImpl http_server_properties;
  MockClientSocketFactory socket_factory;
  params.client_socket_factory = &socket_factory;
  MockHostResolver host_resolver;
  params.host_resolver = &host_resolver;
  TransportSecurityState transport_security_state;
  params.transport_security_state = &transport_security_state;
  params.proxy_service = proxy_service.get();
  params.ssl_config_service = ssl_config_service.get();
  params.http_server_properties = http_server_properties.GetWeakPtr();
  params.quic_max_number_of_lossy_connections = 2;

  scoped_refptr<HttpNetworkSession> session;
  session = new HttpNetworkSession(params);
  session->quic_stream_factory()->set_require_confirmation(false);

  session->quic_stream_factory()->number_of_lossy_connections_[99] =
      params.quic_max_number_of_lossy_connections;

  StaticSocketDataProvider socket_data2;
  socket_data2.set_connect_data(MockConnect(ASYNC, OK));
  socket_factory.AddSocketDataProvider(&socket_data2);

  // Now request a stream. It should succeed using the second proxy in the
  // list.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, ssl_config, ssl_config, &waiter,
          BoundNetLog()));
  waiter.WaitForStream();

  // The proxy that failed should now be known to the proxy_service as bad.
  const ProxyRetryInfoMap& retry_info =
      session->proxy_service()->proxy_retry_info();
  EXPECT_EQ(1u, retry_info.size());
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());

  ProxyRetryInfoMap::const_iterator iter = retry_info.find("quic://bad:99");
  EXPECT_TRUE(iter != retry_info.end());
}

namespace {

TEST_P(HttpStreamFactoryTest, PrivacyModeDisablesChannelId) {
  SpdySessionDependencies session_deps(
      GetParam(), ProxyService::CreateDirect());

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Set an existing SpdySession in the pool.
  HostPortPair host_port_pair("www.google.com", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_ENABLED);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.privacy_mode = PRIVACY_MODE_DISABLED;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, ssl_config, ssl_config,
          &waiter, BoundNetLog()));
  waiter.WaitForStream();

  // The stream shouldn't come from spdy as we are using different privacy mode
  EXPECT_FALSE(request->using_spdy());

  SSLConfig used_ssl_config = waiter.used_ssl_config();
  EXPECT_EQ(used_ssl_config.channel_id_enabled, ssl_config.channel_id_enabled);
}

namespace {
// Return count of distinct groups in given socket pool.
int GetSocketPoolGroupCount(ClientSocketPool* pool) {
  int count = 0;
  scoped_ptr<base::DictionaryValue> dict(pool->GetInfoAsValue("", "", false));
  EXPECT_TRUE(dict != nullptr);
  base::DictionaryValue* groups = nullptr;
  if (dict->GetDictionary("groups", &groups) && (groups != nullptr)) {
    count = static_cast<int>(groups->size());
  }
  return count;
}
}  // namespace

TEST_P(HttpStreamFactoryTest, PrivacyModeUsesDifferentSocketPoolGroup) {
  SpdySessionDependencies session_deps(
      GetParam(), ProxyService::CreateDirect());

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));
  SSLClientSocketPool* ssl_pool = session->GetSSLSocketPool(
      HttpNetworkSession::NORMAL_SOCKET_POOL);

  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 0);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.privacy_mode = PRIVACY_MODE_DISABLED;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;

  scoped_ptr<HttpStreamRequest> request1(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, ssl_config, ssl_config,
          &waiter, BoundNetLog()));
  waiter.WaitForStream();

  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 1);

  scoped_ptr<HttpStreamRequest> request2(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, ssl_config, ssl_config,
          &waiter, BoundNetLog()));
  waiter.WaitForStream();

  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 1);

  request_info.privacy_mode = PRIVACY_MODE_ENABLED;
  scoped_ptr<HttpStreamRequest> request3(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, ssl_config, ssl_config,
          &waiter, BoundNetLog()));
  waiter.WaitForStream();

  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 2);
}

TEST_P(HttpStreamFactoryTest, GetLoadState) {
  SpdySessionDependencies session_deps(
      GetParam(), ProxyService::CreateDirect());

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, ssl_config, ssl_config,
          &waiter, BoundNetLog()));

  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, request->GetLoadState());

  waiter.WaitForStream();
}

TEST_P(HttpStreamFactoryTest, RequestHttpStream) {
  SpdySessionDependencies session_deps(
      GetParam(), ProxyService::CreateDirect());

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.  It should succeed using the second proxy in the
  // list.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info,
          DEFAULT_PRIORITY,
          ssl_config,
          ssl_config,
          &waiter,
          BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  ASSERT_TRUE(nullptr != waiter.stream());
  EXPECT_TRUE(nullptr == waiter.websocket_stream());
  EXPECT_FALSE(waiter.stream()->IsSpdyHttpStream());

  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSSLSocketPool(
      HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(
          HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSSLSocketPool(
      HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

TEST_P(HttpStreamFactoryTest, RequestHttpStreamOverSSL) {
  SpdySessionDependencies session_deps(
      GetParam(), ProxyService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  StaticSocketDataProvider socket_data(&mock_read, 1, nullptr, 0);
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info,
          DEFAULT_PRIORITY,
          ssl_config,
          ssl_config,
          &waiter,
          BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  ASSERT_TRUE(nullptr != waiter.stream());
  EXPECT_TRUE(nullptr == waiter.websocket_stream());
  EXPECT_FALSE(waiter.stream()->IsSpdyHttpStream());
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(
          HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSSLSocketPool(
      HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

TEST_P(HttpStreamFactoryTest, RequestHttpStreamOverProxy) {
  SpdySessionDependencies session_deps(
      GetParam(), ProxyService::CreateFixed("myproxy:8888"));

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.  It should succeed using the second proxy in the
  // list.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info,
          DEFAULT_PRIORITY,
          ssl_config,
          ssl_config,
          &waiter,
          BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  ASSERT_TRUE(nullptr != waiter.stream());
  EXPECT_TRUE(nullptr == waiter.websocket_stream());
  EXPECT_FALSE(waiter.stream()->IsSpdyHttpStream());
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(session->GetSocketPoolForHTTPProxy(
      HttpNetworkSession::NORMAL_SOCKET_POOL,
      HostPortPair("myproxy", 8888))));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSocketPoolForSSLWithProxy(
      HttpNetworkSession::NORMAL_SOCKET_POOL,
      HostPortPair("myproxy", 8888))));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSocketPoolForHTTPProxy(
      HttpNetworkSession::WEBSOCKET_SOCKET_POOL,
      HostPortPair("myproxy", 8888))));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSocketPoolForSSLWithProxy(
      HttpNetworkSession::WEBSOCKET_SOCKET_POOL,
      HostPortPair("myproxy", 8888))));
  EXPECT_FALSE(waiter.used_proxy_info().is_direct());
}

TEST_P(HttpStreamFactoryTest, RequestWebSocketBasicHandshakeStream) {
  SpdySessionDependencies session_deps(
      GetParam(), ProxyService::CreateDirect());

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("ws://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  WebSocketStreamCreateHelper create_helper;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory_for_websocket()
          ->RequestWebSocketHandshakeStream(request_info,
                                            DEFAULT_PRIORITY,
                                            ssl_config,
                                            ssl_config,
                                            &waiter,
                                            &create_helper,
                                            BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(nullptr == waiter.stream());
  ASSERT_TRUE(nullptr != waiter.websocket_stream());
  EXPECT_EQ(MockWebSocketHandshakeStream::kStreamTypeBasic,
            waiter.websocket_stream()->type());
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

TEST_P(HttpStreamFactoryTest, RequestWebSocketBasicHandshakeStreamOverSSL) {
  SpdySessionDependencies session_deps(
      GetParam(), ProxyService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  StaticSocketDataProvider socket_data(&mock_read, 1, nullptr, 0);
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("wss://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  WebSocketStreamCreateHelper create_helper;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory_for_websocket()
          ->RequestWebSocketHandshakeStream(request_info,
                                            DEFAULT_PRIORITY,
                                            ssl_config,
                                            ssl_config,
                                            &waiter,
                                            &create_helper,
                                            BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(nullptr == waiter.stream());
  ASSERT_TRUE(nullptr != waiter.websocket_stream());
  EXPECT_EQ(MockWebSocketHandshakeStream::kStreamTypeBasic,
            waiter.websocket_stream()->type());
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

TEST_P(HttpStreamFactoryTest, RequestWebSocketBasicHandshakeStreamOverProxy) {
  SpdySessionDependencies session_deps(
      GetParam(), ProxyService::CreateFixed("myproxy:8888"));

  MockRead read(SYNCHRONOUS, "HTTP/1.0 200 Connection established\r\n\r\n");
  StaticSocketDataProvider socket_data(&read, 1, 0, 0);
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("ws://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  WebSocketStreamCreateHelper create_helper;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory_for_websocket()
          ->RequestWebSocketHandshakeStream(request_info,
                                            DEFAULT_PRIORITY,
                                            ssl_config,
                                            ssl_config,
                                            &waiter,
                                            &create_helper,
                                            BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(nullptr == waiter.stream());
  ASSERT_TRUE(nullptr != waiter.websocket_stream());
  EXPECT_EQ(MockWebSocketHandshakeStream::kStreamTypeBasic,
            waiter.websocket_stream()->type());
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(
          HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSocketPoolForHTTPProxy(
      HttpNetworkSession::NORMAL_SOCKET_POOL,
      HostPortPair("myproxy", 8888))));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSocketPoolForSSLWithProxy(
      HttpNetworkSession::NORMAL_SOCKET_POOL,
      HostPortPair("myproxy", 8888))));
  EXPECT_EQ(1, GetSocketPoolGroupCount(session->GetSocketPoolForHTTPProxy(
      HttpNetworkSession::WEBSOCKET_SOCKET_POOL,
      HostPortPair("myproxy", 8888))));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSocketPoolForSSLWithProxy(
      HttpNetworkSession::WEBSOCKET_SOCKET_POOL,
      HostPortPair("myproxy", 8888))));
  EXPECT_FALSE(waiter.used_proxy_info().is_direct());
}

TEST_P(HttpStreamFactoryTest, RequestSpdyHttpStream) {
  SpdySessionDependencies session_deps(GetParam(),
                                       ProxyService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  SequencedSocketData socket_data(&mock_read, 1, nullptr, 0);
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  ssl_socket_data.SetNextProto(GetParam());
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  HostPortPair host_port_pair("www.google.com", 443);
  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info,
          DEFAULT_PRIORITY,
          ssl_config,
          ssl_config,
          &waiter,
          BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(nullptr == waiter.websocket_stream());
  ASSERT_TRUE(nullptr != waiter.stream());
  EXPECT_TRUE(waiter.stream()->IsSpdyHttpStream());
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(
          HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

// TODO(ricea): This test can be removed once the new WebSocket stack supports
// SPDY. Currently, even if we connect to a SPDY-supporting server, we need to
// use plain SSL.
TEST_P(HttpStreamFactoryTest, RequestWebSocketSpdyHandshakeStreamButGetSSL) {
  SpdySessionDependencies session_deps(GetParam(),
                                       ProxyService::CreateDirect());

  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  StaticSocketDataProvider socket_data(&mock_read, 1, nullptr, 0);
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  HostPortPair host_port_pair("www.google.com", 80);
  scoped_refptr<HttpNetworkSession>
      session(SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("wss://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter1;
  WebSocketStreamCreateHelper create_helper;
  scoped_ptr<HttpStreamRequest> request1(
      session->http_stream_factory_for_websocket()
          ->RequestWebSocketHandshakeStream(request_info,
                                            DEFAULT_PRIORITY,
                                            ssl_config,
                                            ssl_config,
                                            &waiter1,
                                            &create_helper,
                                            BoundNetLog()));
  waiter1.WaitForStream();
  EXPECT_TRUE(waiter1.stream_done());
  ASSERT_TRUE(nullptr != waiter1.websocket_stream());
  EXPECT_EQ(MockWebSocketHandshakeStream::kStreamTypeBasic,
            waiter1.websocket_stream()->type());
  EXPECT_TRUE(nullptr == waiter1.stream());

  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_TRUE(waiter1.used_proxy_info().is_direct());
}

// TODO(ricea): Re-enable once WebSocket-over-SPDY is implemented.
TEST_P(HttpStreamFactoryTest, DISABLED_RequestWebSocketSpdyHandshakeStream) {
  SpdySessionDependencies session_deps(GetParam(),
                                       ProxyService::CreateDirect());

  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  StaticSocketDataProvider socket_data(&mock_read, 1, nullptr, 0);
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  ssl_socket_data.SetNextProto(GetParam());
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  HostPortPair host_port_pair("www.google.com", 80);
  scoped_refptr<HttpNetworkSession>
      session(SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("wss://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter1;
  WebSocketStreamCreateHelper create_helper;
  scoped_ptr<HttpStreamRequest> request1(
      session->http_stream_factory_for_websocket()
          ->RequestWebSocketHandshakeStream(request_info,
                                            DEFAULT_PRIORITY,
                                            ssl_config,
                                            ssl_config,
                                            &waiter1,
                                            &create_helper,
                                            BoundNetLog()));
  waiter1.WaitForStream();
  EXPECT_TRUE(waiter1.stream_done());
  ASSERT_TRUE(nullptr != waiter1.websocket_stream());
  EXPECT_EQ(MockWebSocketHandshakeStream::kStreamTypeSpdy,
            waiter1.websocket_stream()->type());
  EXPECT_TRUE(nullptr == waiter1.stream());

  StreamRequestWaiter waiter2;
  scoped_ptr<HttpStreamRequest> request2(
      session->http_stream_factory_for_websocket()
          ->RequestWebSocketHandshakeStream(request_info,
                                            DEFAULT_PRIORITY,
                                            ssl_config,
                                            ssl_config,
                                            &waiter2,
                                            &create_helper,
                                            BoundNetLog()));
  waiter2.WaitForStream();
  EXPECT_TRUE(waiter2.stream_done());
  ASSERT_TRUE(nullptr != waiter2.websocket_stream());
  EXPECT_EQ(MockWebSocketHandshakeStream::kStreamTypeSpdy,
            waiter2.websocket_stream()->type());
  EXPECT_TRUE(nullptr == waiter2.stream());
  EXPECT_NE(waiter2.websocket_stream(), waiter1.websocket_stream());
  EXPECT_EQ(static_cast<WebSocketSpdyHandshakeStream*>(
                waiter2.websocket_stream())->spdy_session(),
            static_cast<WebSocketSpdyHandshakeStream*>(
                waiter1.websocket_stream())->spdy_session());

  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(
          HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_TRUE(waiter1.used_proxy_info().is_direct());
}

// TODO(ricea): Re-enable once WebSocket over SPDY is implemented.
TEST_P(HttpStreamFactoryTest, DISABLED_OrphanedWebSocketStream) {
  SpdySessionDependencies session_deps(GetParam(),
                                       ProxyService::CreateDirect());
  session_deps.use_alternate_protocols = true;

  MockRead mock_read(ASYNC, OK);
  SequencedSocketData socket_data(&mock_read, 1, nullptr, 0);
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  MockRead mock_read2(ASYNC, OK);
  SequencedSocketData socket_data2(&mock_read2, 1, nullptr, 0);
  socket_data2.set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data2);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  ssl_socket_data.SetNextProto(GetParam());
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("ws://www.google.com:8888");
  request_info.load_flags = 0;

  session->http_server_properties()->SetAlternativeService(
      HostPortPair("www.google.com", 8888),
      AlternativeService(NPN_HTTP_2, "www.google.com", 9999), 1.0);

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  WebSocketStreamCreateHelper create_helper;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory_for_websocket()
          ->RequestWebSocketHandshakeStream(request_info,
                                            DEFAULT_PRIORITY,
                                            ssl_config,
                                            ssl_config,
                                            &waiter,
                                            &create_helper,
                                            BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(nullptr == waiter.stream());
  ASSERT_TRUE(nullptr != waiter.websocket_stream());
  EXPECT_EQ(MockWebSocketHandshakeStream::kStreamTypeSpdy,
            waiter.websocket_stream()->type());

  // Make sure that there was an alternative connection
  // which consumes extra connections.
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(2, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(
          HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::WEBSOCKET_SOCKET_POOL)));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());

  // Make sure there is no orphaned job. it is already canceled.
  ASSERT_EQ(0u, static_cast<HttpStreamFactoryImpl*>(
      session->http_stream_factory_for_websocket())->num_orphaned_jobs());
}

}  // namespace

}  // namespace net
