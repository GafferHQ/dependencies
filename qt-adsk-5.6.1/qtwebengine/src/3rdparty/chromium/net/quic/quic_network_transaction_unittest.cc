// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/transport_security_state.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_entry.h"
#include "net/log/test_net_log_util.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_test_packet_maker.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/mock_client_socket_pool_manager.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {
namespace test {

namespace {

// This is the expected return from a current server advertising QUIC.
static const char kQuicAlternateProtocolHttpHeader[] =
    "Alternate-Protocol: 80:quic\r\n\r\n";
static const char kQuicAlternateProtocol50pctHttpHeader[] =
    "Alternate-Protocol: 80:quic,p=.5\r\n\r\n";
static const char kQuicAlternateProtocolDifferentPortHttpHeader[] =
    "Alternate-Protocol: 137:quic\r\n\r\n";
static const char kQuicAlternateProtocolHttpsHeader[] =
    "Alternate-Protocol: 443:quic\r\n\r\n";

const char kDefaultServerHostName[] = "www.google.com";

}  // namespace

// Helper class to encapsulate MockReads and MockWrites for QUIC.
// Simplify ownership issues and the interaction with the MockSocketFactory.
class MockQuicData {
 public:
  MockQuicData() : sequence_number_(0) {}

  ~MockQuicData() {
    STLDeleteElements(&packets_);
  }

  void AddSynchronousRead(scoped_ptr<QuicEncryptedPacket> packet) {
    reads_.push_back(MockRead(SYNCHRONOUS, packet->data(), packet->length(),
                              sequence_number_++));
    packets_.push_back(packet.release());
  }

  void AddRead(scoped_ptr<QuicEncryptedPacket> packet) {
    reads_.push_back(
        MockRead(ASYNC, packet->data(), packet->length(), sequence_number_++));
    packets_.push_back(packet.release());
  }

  void AddRead(IoMode mode, int rv) {
    reads_.push_back(MockRead(mode, rv, sequence_number_++));
  }

  void AddWrite(scoped_ptr<QuicEncryptedPacket> packet) {
    writes_.push_back(MockWrite(SYNCHRONOUS, packet->data(), packet->length(),
                                sequence_number_++));
    packets_.push_back(packet.release());
  }

  void AddSocketDataToFactory(MockClientSocketFactory* factory) {
    MockRead* reads = reads_.empty() ? nullptr  : &reads_[0];
    MockWrite* writes = writes_.empty() ? nullptr  : &writes_[0];
    socket_data_.reset(
        new SequencedSocketData(reads, reads_.size(), writes, writes_.size()));
    factory->AddSocketDataProvider(socket_data_.get());
  }

 private:
  std::vector<QuicEncryptedPacket*> packets_;
  std::vector<MockWrite> writes_;
  std::vector<MockRead> reads_;
  size_t sequence_number_;
  scoped_ptr<SocketDataProvider> socket_data_;
};

class ProxyHeadersHandler {
 public:
  ProxyHeadersHandler() : was_called_(false) {}

  bool was_called() { return was_called_; }

  void OnBeforeProxyHeadersSent(const ProxyInfo& proxy_info,
                                HttpRequestHeaders* request_headers) {
    was_called_ = true;
  }

 private:
  bool was_called_;
};

class QuicNetworkTransactionTest
    : public PlatformTest,
      public ::testing::WithParamInterface<QuicVersion> {
 protected:
  QuicNetworkTransactionTest()
      : clock_(new MockClock),
        maker_(GetParam(), 0, clock_, kDefaultServerHostName),
        ssl_config_service_(new SSLConfigServiceDefaults),
        proxy_service_(ProxyService::CreateDirect()),
        auth_handler_factory_(
            HttpAuthHandlerFactory::CreateDefault(&host_resolver_)),
        random_generator_(0),
        hanging_data_(nullptr, 0, nullptr, 0) {
    request_.method = "GET";
    std::string url("http://");
    url.append(kDefaultServerHostName);
    request_.url = GURL(url);
    request_.load_flags = 0;
    clock_->AdvanceTime(QuicTime::Delta::FromMilliseconds(20));
  }

  void SetUp() override {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::MessageLoop::current()->RunUntilIdle();
  }

  void TearDown() override {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    // Empty the current queue.
    base::MessageLoop::current()->RunUntilIdle();
    PlatformTest::TearDown();
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::MessageLoop::current()->RunUntilIdle();
  }

  scoped_ptr<QuicEncryptedPacket> ConstructConnectionClosePacket(
      QuicPacketSequenceNumber num) {
    return maker_.MakeConnectionClosePacket(num);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructAckPacket(
      QuicPacketSequenceNumber largest_received,
      QuicPacketSequenceNumber least_unacked) {
    return maker_.MakeAckPacket(2, largest_received, least_unacked, true);
  }

  SpdyHeaderBlock GetRequestHeaders(const std::string& method,
                                    const std::string& scheme,
                                    const std::string& path) {
    return maker_.GetRequestHeaders(method, scheme, path);
  }

  SpdyHeaderBlock GetResponseHeaders(const std::string& status) {
    return maker_.GetResponseHeaders(status);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructDataPacket(
      QuicPacketSequenceNumber sequence_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      QuicStreamOffset offset,
      base::StringPiece data) {
    return maker_.MakeDataPacket(
        sequence_number, stream_id, should_include_version, fin, offset, data);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRequestHeadersPacket(
      QuicPacketSequenceNumber sequence_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const SpdyHeaderBlock& headers) {
    QuicPriority priority =
        ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
    return maker_.MakeRequestHeadersPacket(sequence_number, stream_id,
                                           should_include_version, fin,
                                           priority, headers);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructResponseHeadersPacket(
      QuicPacketSequenceNumber sequence_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const SpdyHeaderBlock& headers) {
    return maker_.MakeResponseHeadersPacket(
        sequence_number, stream_id, should_include_version, fin, headers);
  }

  void CreateSession() {
    CreateSessionWithFactory(&socket_factory_, false);
  }

  void CreateSessionWithNextProtos() {
    CreateSessionWithFactory(&socket_factory_, true);
  }

  // If |use_next_protos| is true, enables SPDY and QUIC.
  void CreateSessionWithFactory(ClientSocketFactory* socket_factory,
                                bool use_next_protos) {
    params_.enable_quic = true;
    params_.quic_clock = clock_;
    params_.quic_random = &random_generator_;
    params_.client_socket_factory = socket_factory;
    params_.quic_crypto_client_stream_factory = &crypto_client_stream_factory_;
    params_.host_resolver = &host_resolver_;
    params_.cert_verifier = &cert_verifier_;
    params_.transport_security_state = &transport_security_state_;
    params_.proxy_service = proxy_service_.get();
    params_.ssl_config_service = ssl_config_service_.get();
    params_.http_auth_handler_factory = auth_handler_factory_.get();
    params_.http_server_properties = http_server_properties_.GetWeakPtr();
    params_.quic_supported_versions = SupportedVersions(GetParam());

    if (use_next_protos) {
      params_.use_alternate_protocols = true;
      params_.next_protos = NextProtosWithSpdyAndQuic(true, true);
    }

    session_ = new HttpNetworkSession(params_);
    session_->quic_stream_factory()->set_require_confirmation(false);
    ASSERT_EQ(params_.quic_socket_receive_buffer_size,
              session_->quic_stream_factory()->socket_receive_buffer_size());
  }

  void CheckWasQuicResponse(const scoped_ptr<HttpNetworkTransaction>& trans) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    ASSERT_TRUE(response->headers.get() != nullptr);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    EXPECT_TRUE(response->was_fetched_via_spdy);
    EXPECT_TRUE(response->was_npn_negotiated);
    EXPECT_EQ(HttpResponseInfo::CONNECTION_INFO_QUIC1_SPDY3,
              response->connection_info);
  }

  void CheckResponsePort(const scoped_ptr<HttpNetworkTransaction>& trans,
                         uint16 port) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    EXPECT_EQ(port, response->socket_address.port());
  }

  void CheckWasHttpResponse(const scoped_ptr<HttpNetworkTransaction>& trans) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    ASSERT_TRUE(response->headers.get() != nullptr);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    EXPECT_FALSE(response->was_fetched_via_spdy);
    EXPECT_FALSE(response->was_npn_negotiated);
    EXPECT_EQ(HttpResponseInfo::CONNECTION_INFO_HTTP1,
              response->connection_info);
  }

  void CheckResponseData(const scoped_ptr<HttpNetworkTransaction>& trans,
                         const std::string& expected) {
    std::string response_data;
    ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
    EXPECT_EQ(expected, response_data);
  }

  void RunTransaction(const scoped_ptr<HttpNetworkTransaction>& trans) {
    TestCompletionCallback callback;
    int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_EQ(OK, callback.WaitForResult());
  }

  void SendRequestAndExpectHttpResponse(const std::string& expected) {
    scoped_ptr<HttpNetworkTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
    RunTransaction(trans);
    CheckWasHttpResponse(trans);
    CheckResponseData(trans, expected);
  }

  void SendRequestAndExpectQuicResponse(const std::string& expected) {
    SendRequestAndExpectQuicResponseMaybeFromProxy(expected, false, 80);
  }

  void SendRequestAndExpectQuicResponseOnPort(const std::string& expected,
                                              uint16 port) {
    SendRequestAndExpectQuicResponseMaybeFromProxy(expected, false, port);
  }

  void SendRequestAndExpectQuicResponseFromProxyOnPort(
      const std::string& expected,
      uint16 port) {
    SendRequestAndExpectQuicResponseMaybeFromProxy(expected, true, port);
  }

  void AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::HandshakeMode handshake_mode) {
    crypto_client_stream_factory_.set_handshake_mode(handshake_mode);
    HostPortPair host_port_pair = HostPortPair::FromURL(request_.url);
    AlternativeService alternative_service(QUIC, host_port_pair.host(), 80);
    http_server_properties_.SetAlternativeService(host_port_pair,
                                                  alternative_service, 1.0);
  }

  void ExpectBrokenAlternateProtocolMapping() {
    const HostPortPair origin = HostPortPair::FromURL(request_.url);
    const AlternativeServiceVector alternative_service_vector =
        http_server_properties_.GetAlternativeServices(origin);
    EXPECT_EQ(1u, alternative_service_vector.size());
    EXPECT_TRUE(http_server_properties_.IsAlternativeServiceBroken(
        alternative_service_vector[0]));
  }

  void ExpectQuicAlternateProtocolMapping() {
    const HostPortPair origin = HostPortPair::FromURL(request_.url);
    const AlternativeServiceVector alternative_service_vector =
        http_server_properties_.GetAlternativeServices(origin);
    EXPECT_EQ(1u, alternative_service_vector.size());
    EXPECT_EQ(QUIC, alternative_service_vector[0].protocol);
  }

  void AddHangingNonAlternateProtocolSocketData() {
    MockConnect hanging_connect(SYNCHRONOUS, ERR_IO_PENDING);
    hanging_data_.set_connect_data(hanging_connect);
    socket_factory_.AddSocketDataProvider(&hanging_data_);
  }

  MockClock* clock_;  // Owned by QuicStreamFactory after CreateSession.
  QuicTestPacketMaker maker_;
  scoped_refptr<HttpNetworkSession> session_;
  MockClientSocketFactory socket_factory_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  MockHostResolver host_resolver_;
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  scoped_refptr<SSLConfigServiceDefaults> ssl_config_service_;
  scoped_ptr<ProxyService> proxy_service_;
  scoped_ptr<HttpAuthHandlerFactory> auth_handler_factory_;
  MockRandom random_generator_;
  HttpServerPropertiesImpl http_server_properties_;
  HttpNetworkSession::Params params_;
  HttpRequestInfo request_;
  BoundTestNetLog net_log_;
  StaticSocketDataProvider hanging_data_;

 private:
  void SendRequestAndExpectQuicResponseMaybeFromProxy(
      const std::string& expected,
      bool used_proxy,
      uint16 port) {
    scoped_ptr<HttpNetworkTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
    ProxyHeadersHandler proxy_headers_handler;
    trans->SetBeforeProxyHeadersSentCallback(
        base::Bind(&ProxyHeadersHandler::OnBeforeProxyHeadersSent,
                   base::Unretained(&proxy_headers_handler)));
    RunTransaction(trans);
    CheckWasQuicResponse(trans);
    CheckResponsePort(trans, port);
    CheckResponseData(trans, expected);
    EXPECT_EQ(used_proxy, proxy_headers_handler.was_called());
  }
};

INSTANTIATE_TEST_CASE_P(Version, QuicNetworkTransactionTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicNetworkTransactionTest, ForceQuic) {
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("www.google.com:80");

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();

  SendRequestAndExpectQuicResponse("hello!");

  // Check that the NetLog was filled reasonably.
  TestNetLogEntry::List entries;
  net_log_.GetEntries(&entries);
  EXPECT_LT(0u, entries.size());

  // Check that we logged a QUIC_SESSION_PACKET_RECEIVED.
  int pos = ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_QUIC_SESSION_PACKET_RECEIVED,
      NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  // ... and also a TYPE_QUIC_SESSION_PACKET_HEADER_RECEIVED.
  pos = ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_QUIC_SESSION_PACKET_HEADER_RECEIVED,
      NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  std::string packet_sequence_number;
  ASSERT_TRUE(entries[pos].GetStringValue(
      "packet_sequence_number", &packet_sequence_number));
  EXPECT_EQ("1", packet_sequence_number);

  // ... and also a QUIC_SESSION_STREAM_FRAME_RECEIVED.
  pos = ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_QUIC_SESSION_STREAM_FRAME_RECEIVED,
      NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  int log_stream_id;
  ASSERT_TRUE(entries[pos].GetIntegerValue("stream_id", &log_stream_id));
  EXPECT_EQ(3, log_stream_id);
}

TEST_P(QuicNetworkTransactionTest, QuicProxy) {
  params_.enable_quic_for_proxies = true;
  proxy_service_.reset(
      ProxyService::CreateFixedFromPacResult("QUIC myproxy:70"));

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // There is no need to set up an alternate protocol job, because
  // no attempt will be made to speak to the proxy over TCP.

  CreateSession();

  SendRequestAndExpectQuicResponseFromProxyOnPort("hello!", 70);
}

// Regression test for https://crbug.com/492458.  Test that for an HTTP
// connection through a QUIC proxy, the certificate exhibited by the proxy is
// checked against the proxy hostname, not the origin hostname.
TEST_P(QuicNetworkTransactionTest, QuicProxyWithCert) {
  const std::string origin_host = "news.example.com";
  const std::string proxy_host = "www.example.org";

  params_.enable_quic_for_proxies = true;
  proxy_service_.reset(
      ProxyService::CreateFixedFromPacResult("QUIC " + proxy_host + ":70"));

  maker_.set_hostname(origin_host);
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem"));
  ASSERT_TRUE(cert.get());
  // This certificate is valid for the proxy, but not for the origin.
  bool common_name_fallback_used;
  EXPECT_TRUE(cert->VerifyNameMatch(proxy_host, &common_name_fallback_used));
  EXPECT_FALSE(cert->VerifyNameMatch(origin_host, &common_name_fallback_used));
  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  request_.url = GURL("http://" + origin_host);
  AddHangingNonAlternateProtocolSocketData();
  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  SendRequestAndExpectQuicResponseFromProxyOnPort("hello!", 70);
}

TEST_P(QuicNetworkTransactionTest, ForceQuicWithErrorConnecting) {
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("www.google.com:80");

  MockQuicData mock_quic_data;
  mock_quic_data.AddRead(ASYNC, ERR_SOCKET_NOT_CONNECTED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_CONNECTION_CLOSED, callback.WaitForResult());
}

TEST_P(QuicNetworkTransactionTest, DoNotForceQuicForHttps) {
  // Attempt to "force" quic on 443, which will not be honored.
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("www.google.com:443");

  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider data(http_reads, arraysize(http_reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreateSession();

  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest, UseAlternateProtocolForQuic) {
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(kQuicAlternateProtocolHttpHeader),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, AlternateProtocolDifferentPort) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(kQuicAlternateProtocolDifferentPortHttpHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponseOnPort("hello!", 137);
}

TEST_P(QuicNetworkTransactionTest, ConfirmAlternativeService) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(kQuicAlternateProtocolHttpHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();

  AlternativeService alternative_service(QUIC,
                                         HostPortPair::FromURL(request_.url));
  http_server_properties_.MarkAlternativeServiceRecentlyBroken(
      alternative_service);
  EXPECT_TRUE(http_server_properties_.WasAlternativeServiceRecentlyBroken(
      alternative_service));

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");

  EXPECT_FALSE(http_server_properties_.WasAlternativeServiceRecentlyBroken(
      alternative_service));
}

TEST_P(QuicNetworkTransactionTest, UseAlternateProtocolProbabilityForQuic) {
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(kQuicAlternateProtocol50pctHttpHeader),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  params_.alternative_service_probability_threshold = .25;
  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, DontUseAlternateProtocolProbabilityForQuic) {
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(kQuicAlternateProtocol50pctHttpHeader),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSocketDataProvider(&http_data);

  params_.alternative_service_probability_threshold = .75;
  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest, DontUseAlternateProtocolForInsecureQuic) {
  MockRead http_reads[] = {MockRead("HTTP/1.1 200 OK\r\n"),
                           MockRead("Content-length: 11\r\n"),
                           MockRead("Alternate-Protocol: 443:quic\r\n\r\n"),
                           MockRead("hello world"),
                           MockRead("HTTP/1.1 200 OK\r\n"),
                           MockRead("Content-length: 11\r\n"),
                           MockRead("Alternate-Protocol: 443:quic\r\n\r\n"),
                           MockRead("hello world"),
                           MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSocketDataProvider(&http_data);

  params_.disable_insecure_quic = true;
  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest,
       DontUseAlternateProtocolWithBadProbabilityForQuic) {
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Alternate-Protocol: 443:quic,p=2\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSocketDataProvider(&http_data);

  params_.alternative_service_probability_threshold = .75;
  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest, UseAlternateProtocolForQuicForHttps) {
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("www.google.com:443");

  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(kQuicAlternateProtocolHttpsHeader),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();

  // TODO(rtenneti): Test QUIC over HTTPS, GetSSLInfo().
  SendRequestAndExpectHttpResponse("hello world");
}

class QuicAltSvcCertificateVerificationTest
    : public QuicNetworkTransactionTest {
 public:
  void Run(bool valid) {
    HostPortPair origin(valid ? "mail.example.org" : "invalid.example.org",
                        443);
    HostPortPair alternative("www.example.org", 443);
    std::string url("https://");
    url.append(origin.host());
    url.append(":443");
    request_.url = GURL(url);

    maker_.set_hostname(origin.host());
    MockQuicData mock_quic_data;
    mock_quic_data.AddWrite(
        ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                      GetRequestHeaders("GET", "https", "/")));
    mock_quic_data.AddRead(ConstructResponseHeadersPacket(
        1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
    mock_quic_data.AddRead(
        ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
    mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
    mock_quic_data.AddRead(SYNCHRONOUS, 0);
    mock_quic_data.AddSocketDataToFactory(&socket_factory_);

    scoped_refptr<X509Certificate> cert(
        ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem"));
    ASSERT_TRUE(cert.get());
    bool common_name_fallback_used;
    EXPECT_EQ(valid,
              cert->VerifyNameMatch(origin.host(), &common_name_fallback_used));
    EXPECT_TRUE(
        cert->VerifyNameMatch(alternative.host(), &common_name_fallback_used));
    ProofVerifyDetailsChromium verify_details;
    verify_details.cert_verify_result.verified_cert = cert;
    verify_details.cert_verify_result.is_issued_by_known_root = true;
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
    crypto_client_stream_factory_.set_handshake_mode(
        MockCryptoClientStream::CONFIRM_HANDSHAKE);

    // Connection to |origin| fails, so that success of |request| depends on
    // connection to |alternate| only.
    MockConnect refused_connect(ASYNC, ERR_CONNECTION_REFUSED);
    StaticSocketDataProvider refused_data;
    refused_data.set_connect_data(refused_connect);
    socket_factory_.AddSocketDataProvider(&refused_data);

    CreateSessionWithNextProtos();
    AlternativeService alternative_service(QUIC, alternative);
    session_->http_server_properties()->SetAlternativeService(
        origin, alternative_service, 1.0);
    scoped_ptr<HttpNetworkTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
    TestCompletionCallback callback;
    int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    rv = callback.WaitForResult();
    if (valid) {
      EXPECT_EQ(OK, rv);
      CheckWasQuicResponse(trans);
      CheckResponsePort(trans, 443);
      CheckResponseData(trans, "hello!");
    } else {
      EXPECT_EQ(ERR_CONNECTION_REFUSED, rv);
    }
  }
};

INSTANTIATE_TEST_CASE_P(Version,
                        QuicAltSvcCertificateVerificationTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicAltSvcCertificateVerificationTest,
       RequestSucceedsWithValidCertificate) {
  Run(true);
}

TEST_P(QuicAltSvcCertificateVerificationTest,
       RequestFailsWithInvalidCertificate) {
  Run(false);
}

TEST_P(QuicNetworkTransactionTest, HungAlternateProtocol) {
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  MockWrite http_writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
    MockWrite(SYNCHRONOUS, 1, "Host: www.google.com\r\n"),
    MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")
  };

  MockRead http_reads[] = {
    MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 4, kQuicAlternateProtocolHttpHeader),
    MockRead(SYNCHRONOUS, 5, "hello world"),
    MockRead(SYNCHRONOUS, OK, 6)
  };

  DeterministicMockClientSocketFactory socket_factory;

  DeterministicSocketData http_data(http_reads, arraysize(http_reads),
                                    http_writes, arraysize(http_writes));
  socket_factory.AddSocketDataProvider(&http_data);

  // The QUIC transaction will not be allowed to complete.
  MockWrite quic_writes[] = {
    MockWrite(ASYNC, ERR_IO_PENDING, 0)
  };
  MockRead quic_reads[] = {
    MockRead(ASYNC, ERR_IO_PENDING, 1),
  };
  DeterministicSocketData quic_data(quic_reads, arraysize(quic_reads),
                                    quic_writes, arraysize(quic_writes));
  socket_factory.AddSocketDataProvider(&quic_data);

  // The HTTP transaction will complete.
  DeterministicSocketData http_data2(http_reads, arraysize(http_reads),
                                     http_writes, arraysize(http_writes));
  socket_factory.AddSocketDataProvider(&http_data2);

  CreateSessionWithFactory(&socket_factory, true);

  // Run the first request.
  http_data.StopAfter(arraysize(http_reads) + arraysize(http_writes));
  SendRequestAndExpectHttpResponse("hello world");
  ASSERT_TRUE(http_data.AllReadDataConsumed());
  ASSERT_TRUE(http_data.AllWriteDataConsumed());

  // Now run the second request in which the QUIC socket hangs,
  // and verify the the transaction continues over HTTP.
  http_data2.StopAfter(arraysize(http_reads) + arraysize(http_writes));
  SendRequestAndExpectHttpResponse("hello world");

  ASSERT_TRUE(http_data2.AllReadDataConsumed());
  ASSERT_TRUE(http_data2.AllWriteDataConsumed());
  ASSERT_TRUE(!quic_data.AllReadDataConsumed());
  ASSERT_TRUE(!quic_data.AllWriteDataConsumed());
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithHttpRace) {
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithNoHttpRace) {
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("www.google.com", "192.168.0.1", "");
  HostResolver::RequestInfo info(HostPortPair("www.google.com", 80));
  AddressList address;
  host_resolver_.Resolve(info,
                         DEFAULT_PRIORITY,
                         &address,
                         CompletionCallback(),
                         nullptr,
                         net_log_.bound());

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithProxy) {
  proxy_service_.reset(
      ProxyService::CreateFixedFromPacResult("PROXY myproxy:70"));

  // Since we are using a proxy, the QUIC job will not succeed.
  MockWrite http_writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET http://www.google.com/ HTTP/1.1\r\n"),
    MockWrite(SYNCHRONOUS, 1, "Host: www.google.com\r\n"),
    MockWrite(SYNCHRONOUS, 2, "Proxy-Connection: keep-alive\r\n\r\n")
  };

  MockRead http_reads[] = {
    MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 4, kQuicAlternateProtocolHttpHeader),
    MockRead(SYNCHRONOUS, 5, "hello world"),
    MockRead(SYNCHRONOUS, OK, 6)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     http_writes, arraysize(http_writes));
  socket_factory_.AddSocketDataProvider(&http_data);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("www.google.com", "192.168.0.1", "");
  HostResolver::RequestInfo info(HostPortPair("www.google.com", 80));
  AddressList address;
  host_resolver_.Resolve(info,
                         DEFAULT_PRIORITY,
                         &address,
                         CompletionCallback(),
                         nullptr,
                         net_log_.bound());

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithConfirmationRequired) {
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("www.google.com", "192.168.0.1", "");
  HostResolver::RequestInfo info(HostPortPair("www.google.com", 80));
  AddressList address;
  host_resolver_.Resolve(info, DEFAULT_PRIORITY, &address,
                         CompletionCallback(), nullptr, net_log_.bound());

  CreateSessionWithNextProtos();
  session_->quic_stream_factory()->set_require_confirmation(true);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      QuicSession::HANDSHAKE_CONFIRMED);
  EXPECT_EQ(OK, callback.WaitForResult());
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocol) {
  // Alternate-protocol job
  scoped_ptr<QuicEncryptedPacket> close(ConstructConnectionClosePacket(1));
  MockRead quic_reads[] = {
    MockRead(ASYNC, close->data(), close->length()),
    MockRead(ASYNC, OK),  // EOF
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");
  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocolReadError) {
  // Alternate-protocol job
  MockRead quic_reads[] = {
    MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSessionWithNextProtos();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");
  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, NoBrokenAlternateProtocolIfTcpFails) {
  // Alternate-protocol job will fail when the session attempts to read.
  MockRead quic_reads[] = {
    MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job will also fail.
  MockRead http_reads[] = {
    MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     nullptr, 0);
  http_data.set_connect_data(MockConnect(ASYNC, ERR_SOCKET_NOT_CONNECTED));
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSessionWithNextProtos();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, callback.WaitForResult());
  ExpectQuicAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, FailedZeroRttBrokenAlternateProtocol) {
  // Alternate-protocol job
  MockRead quic_reads[] = {
    MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  AddHangingNonAlternateProtocolSocketData();

  // Second Alternate-protocol job which will race with the TCP job.
  StaticSocketDataProvider quic_data2(quic_reads, arraysize(quic_reads),
                                      nullptr, 0);
  socket_factory_.AddSocketDataProvider(&quic_data2);

  // Final job that will proceed when the QUIC job fails.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSessionWithNextProtos();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  SendRequestAndExpectHttpResponse("hello from http");

  ExpectBrokenAlternateProtocolMapping();

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicNetworkTransactionTest, DISABLED_HangingZeroRttFallback) {
  // Alternate-protocol job
  MockRead quic_reads[] = {
    MockRead(ASYNC, ERR_IO_PENDING),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job that will proceed when the QUIC job fails.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSessionWithNextProtos();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  SendRequestAndExpectHttpResponse("hello from http");
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocolOnConnectFailure) {
  // Alternate-protocol job will fail before creating a QUIC session.
  StaticSocketDataProvider quic_data(nullptr, 0, nullptr, 0);
  quic_data.set_connect_data(MockConnect(SYNCHRONOUS,
                                         ERR_INTERNET_DISCONNECTED));
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");

  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, ConnectionCloseDuringConnect) {
  MockQuicData mock_quic_data;
  mock_quic_data.AddSynchronousRead(ConstructConnectionClosePacket(1));
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // When the QUIC connection fails, we will try the request again over HTTP.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(kQuicAlternateProtocolHttpHeader),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     nullptr, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("www.google.com", "192.168.0.1", "");
  HostResolver::RequestInfo info(HostPortPair("www.google.com", 80));
  AddressList address;
  host_resolver_.Resolve(info,
                         DEFAULT_PRIORITY,
                         &address,
                         CompletionCallback(),
                         nullptr,
                         net_log_.bound());

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectHttpResponse("hello world");
}

// Test that a secure request over an insecure QUIC connection fails with
// the appropriate error code.  Note that this never happens in production,
// because the handshake (which this test mocks) would fail in this scenario.
TEST_P(QuicNetworkTransactionTest, SecureResourceOverInsecureQuic) {
  maker_.set_hostname("www.example.org");
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  request_.url = GURL("https://www.example.org:443");
  AddHangingNonAlternateProtocolSocketData();
  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_REQUEST_FOR_SECURE_RESOURCE_OVER_INSECURE_QUIC,
            callback.GetResult(rv));
}

TEST_P(QuicNetworkTransactionTest, SecureResourceOverSecureQuic) {
  maker_.set_hostname("www.example.org");
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem"));
  ASSERT_TRUE(cert.get());
  bool common_name_fallback_used;
  EXPECT_TRUE(
      cert->VerifyNameMatch("www.example.org", &common_name_fallback_used));
  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  request_.url = GURL("https://www.example.org:443");
  AddHangingNonAlternateProtocolSocketData();
  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  SendRequestAndExpectQuicResponse("hello!");
}

}  // namespace test
}  // namespace net
