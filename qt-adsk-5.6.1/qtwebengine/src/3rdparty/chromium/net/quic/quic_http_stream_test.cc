// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_http_stream.h"

#include <vector>

#include "base/thread_task_runner_handle.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_response_headers.h"
#include "net/http/transport_security_state.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_server_info.h"
#include "net/quic/quic_client_session.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_default_packet_writer.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_reliable_client_stream.h"
#include "net/quic/quic_write_blocked_list.h"
#include "net/quic/spdy_utils.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_test_packet_maker.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/test_task_runner.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace net {
namespace test {
namespace {

const char kUploadData[] = "Really nifty data!";
const char kDefaultServerHostName[] = "www.google.com";
const uint16 kDefaultServerPort = 80;

class TestQuicConnection : public QuicConnection {
 public:
  TestQuicConnection(const QuicVersionVector& versions,
                     QuicConnectionId connection_id,
                     IPEndPoint address,
                     QuicConnectionHelper* helper,
                     const QuicConnection::PacketWriterFactory& writer_factory)
      : QuicConnection(connection_id,
                       address,
                       helper,
                       writer_factory,
                       true /* owns_writer */,
                       Perspective::IS_CLIENT,
                       false /* is_secure */,
                       versions) {}

  void SetSendAlgorithm(SendAlgorithmInterface* send_algorithm) {
    QuicConnectionPeer::SetSendAlgorithm(this, send_algorithm);
  }
};

// Subclass of QuicHttpStream that closes itself when the first piece of data
// is received.
class AutoClosingStream : public QuicHttpStream {
 public:
  explicit AutoClosingStream(const base::WeakPtr<QuicClientSession>& session)
      : QuicHttpStream(session) {
  }

  void OnHeadersAvailable(StringPiece headers) override { Close(false); }

  int OnDataReceived(const char* data, int length) override {
    Close(false);
    return OK;
  }
};

class TestPacketWriterFactory : public QuicConnection::PacketWriterFactory {
 public:
  explicit TestPacketWriterFactory(DatagramClientSocket* socket)
      : socket_(socket) {}
  ~TestPacketWriterFactory() override {}

  QuicPacketWriter* Create(QuicConnection* connection) const override {
    return new QuicDefaultPacketWriter(socket_);
  }

 private:
  DatagramClientSocket* socket_;
};

}  // namespace

class QuicHttpStreamPeer {
 public:
  static QuicReliableClientStream* GetQuicReliableClientStream(
      QuicHttpStream* stream) {
    return stream->stream_;
  }
};

class QuicHttpStreamTest : public ::testing::TestWithParam<QuicVersion> {
 protected:
  static const bool kFin = true;
  static const bool kIncludeVersion = true;
  static const bool kIncludeCongestionFeedback = true;

  // Holds a packet to be written to the wire, and the IO mode that should
  // be used by the mock socket when performing the write.
  struct PacketToWrite {
    PacketToWrite(IoMode mode, QuicEncryptedPacket* packet)
        : mode(mode),
          packet(packet) {
    }
    IoMode mode;
    QuicEncryptedPacket* packet;
  };

  QuicHttpStreamTest()
      : net_log_(BoundNetLog()),
        use_closing_stream_(false),
        read_buffer_(new IOBufferWithSize(4096)),
        connection_id_(2),
        stream_id_(kClientDataStreamId1),
        maker_(GetParam(), connection_id_, &clock_, kDefaultServerHostName),
        random_generator_(0) {
    IPAddressNumber ip;
    CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
    peer_addr_ = IPEndPoint(ip, 443);
    self_addr_ = IPEndPoint(ip, 8435);
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(20));
  }

  ~QuicHttpStreamTest() {
    session_->CloseSessionOnError(ERR_ABORTED, QUIC_INTERNAL_ERROR);
    for (size_t i = 0; i < writes_.size(); i++) {
      delete writes_[i].packet;
    }
  }

  // Adds a packet to the list of expected writes.
  void AddWrite(scoped_ptr<QuicEncryptedPacket> packet) {
    writes_.push_back(PacketToWrite(SYNCHRONOUS, packet.release()));
  }

  // Returns the packet to be written at position |pos|.
  QuicEncryptedPacket* GetWrite(size_t pos) {
    return writes_[pos].packet;
  }

  bool AtEof() {
    return socket_data_->AllReadDataConsumed() &&
           socket_data_->AllWriteDataConsumed();
  }

  void ProcessPacket(scoped_ptr<QuicEncryptedPacket> packet) {
    connection_->ProcessUdpPacket(self_addr_, peer_addr_, *packet);
  }

  // Configures the test fixture to use the list of expected writes.
  void Initialize() {
    mock_writes_.reset(new MockWrite[writes_.size()]);
    for (size_t i = 0; i < writes_.size(); i++) {
      mock_writes_[i] = MockWrite(writes_[i].mode,
                                  writes_[i].packet->data(),
                                  writes_[i].packet->length());
    };

    socket_data_.reset(new StaticSocketDataProvider(
        nullptr, 0, mock_writes_.get(), writes_.size()));

    MockUDPClientSocket* socket = new MockUDPClientSocket(socket_data_.get(),
                                                          net_log_.net_log());
    socket->Connect(peer_addr_);
    runner_ = new TestTaskRunner(&clock_);
    send_algorithm_ = new MockSendAlgorithm();
    EXPECT_CALL(*send_algorithm_, InRecovery()).WillRepeatedly(Return(false));
    EXPECT_CALL(*send_algorithm_, InSlowStart()).WillRepeatedly(Return(false));
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, _, _, _, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*send_algorithm_, RetransmissionDelay()).WillRepeatedly(
        Return(QuicTime::Delta::Zero()));
    EXPECT_CALL(*send_algorithm_, GetCongestionWindow()).WillRepeatedly(
        Return(kMaxPacketSize));
    EXPECT_CALL(*send_algorithm_, TimeUntilSend(_, _, _)).
        WillRepeatedly(Return(QuicTime::Delta::Zero()));
    EXPECT_CALL(*send_algorithm_, BandwidthEstimate()).WillRepeatedly(
        Return(QuicBandwidth::Zero()));
    EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(AnyNumber());
    helper_.reset(new QuicConnectionHelper(runner_.get(), &clock_,
                                           &random_generator_));
    TestPacketWriterFactory writer_factory(socket);
    connection_ = new TestQuicConnection(SupportedVersions(GetParam()),
                                         connection_id_, peer_addr_,
                                         helper_.get(), writer_factory);
    connection_->set_visitor(&visitor_);
    connection_->SetSendAlgorithm(send_algorithm_);
    session_.reset(new QuicClientSession(
        connection_, scoped_ptr<DatagramClientSocket>(socket),
        /*stream_factory=*/nullptr, &crypto_client_stream_factory_,
        &transport_security_state_, make_scoped_ptr((QuicServerInfo*)nullptr),
        QuicServerId(kDefaultServerHostName, kDefaultServerPort,
                     /*is_secure=*/false, PRIVACY_MODE_DISABLED),
        /*cert_verify_flags=*/0, DefaultQuicConfig(), &crypto_config_,
        "CONNECTION_UNKNOWN", base::TimeTicks::Now(),
        base::ThreadTaskRunnerHandle::Get().get(), nullptr));
    session_->Initialize();
    session_->GetCryptoStream()->CryptoConnect();
    EXPECT_TRUE(session_->IsCryptoHandshakeConfirmed());
    stream_.reset(use_closing_stream_ ?
                  new AutoClosingStream(session_->GetWeakPtr()) :
                  new QuicHttpStream(session_->GetWeakPtr()));
  }

  void SetRequest(const std::string& method,
                  const std::string& path,
                  RequestPriority priority) {
    request_headers_ = maker_.GetRequestHeaders(method, "http", path);
  }

  void SetResponse(const std::string& status, const std::string& body) {
    response_headers_ = maker_.GetResponseHeaders(status);
    response_data_ = body;
  }

  scoped_ptr<QuicEncryptedPacket> ConstructDataPacket(
      QuicPacketSequenceNumber sequence_number,
      bool should_include_version,
      bool fin,
      QuicStreamOffset offset,
      base::StringPiece data) {
    return maker_.MakeDataPacket(sequence_number, stream_id_,
                                 should_include_version, fin, offset, data);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRequestHeadersPacket(
      QuicPacketSequenceNumber sequence_number,
      bool fin,
      RequestPriority request_priority) {
    QuicPriority priority =
        ConvertRequestPriorityToQuicPriority(request_priority);
    return maker_.MakeRequestHeadersPacket(sequence_number, stream_id_,
                                           kIncludeVersion, fin, priority,
                                           request_headers_);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructResponseHeadersPacket(
      QuicPacketSequenceNumber sequence_number,
      bool fin) {
    return maker_.MakeResponseHeadersPacket(
        sequence_number, stream_id_, !kIncludeVersion, fin, response_headers_);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRstStreamPacket(
      QuicPacketSequenceNumber sequence_number) {
    return maker_.MakeRstPacket(
        sequence_number, true, stream_id_,
        AdjustErrorForVersion(QUIC_RST_ACKNOWLEDGEMENT, GetParam()));
  }

  scoped_ptr<QuicEncryptedPacket> ConstructAckAndRstStreamPacket(
      QuicPacketSequenceNumber sequence_number) {
    return maker_.MakeAckAndRstPacket(
        sequence_number, !kIncludeVersion, stream_id_, QUIC_STREAM_CANCELLED,
        2, 1, !kIncludeCongestionFeedback);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructAckPacket(
      QuicPacketSequenceNumber sequence_number,
      QuicPacketSequenceNumber largest_received,
      QuicPacketSequenceNumber least_unacked) {
    return maker_.MakeAckPacket(sequence_number, largest_received,
                                least_unacked, !kIncludeCongestionFeedback);
  }

  BoundNetLog net_log_;
  bool use_closing_stream_;
  MockSendAlgorithm* send_algorithm_;
  scoped_refptr<TestTaskRunner> runner_;
  scoped_ptr<MockWrite[]> mock_writes_;
  MockClock clock_;
  TestQuicConnection* connection_;
  scoped_ptr<QuicConnectionHelper> helper_;
  testing::StrictMock<MockConnectionVisitor> visitor_;
  scoped_ptr<QuicHttpStream> stream_;
  TransportSecurityState transport_security_state_;
  scoped_ptr<QuicClientSession> session_;
  QuicCryptoClientConfig crypto_config_;
  TestCompletionCallback callback_;
  HttpRequestInfo request_;
  HttpRequestHeaders headers_;
  HttpResponseInfo response_;
  scoped_refptr<IOBufferWithSize> read_buffer_;
  SpdyHeaderBlock request_headers_;
  SpdyHeaderBlock response_headers_;
  std::string request_data_;
  std::string response_data_;

 private:
  const QuicConnectionId connection_id_;
  const QuicStreamId stream_id_;
  QuicTestPacketMaker maker_;
  IPEndPoint self_addr_;
  IPEndPoint peer_addr_;
  MockRandom random_generator_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  scoped_ptr<StaticSocketDataProvider> socket_data_;
  std::vector<PacketToWrite> writes_;
};

INSTANTIATE_TEST_CASE_P(Version, QuicHttpStreamTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicHttpStreamTest, RenewStreamForAuth) {
  Initialize();
  EXPECT_EQ(nullptr, stream_->RenewStreamForAuth());
}

TEST_P(QuicHttpStreamTest, CanFindEndOfResponse) {
  Initialize();
  EXPECT_TRUE(stream_->CanFindEndOfResponse());
}

TEST_P(QuicHttpStreamTest, IsConnectionReusable) {
  Initialize();
  EXPECT_FALSE(stream_->IsConnectionReusable());
}

TEST_P(QuicHttpStreamTest, GetRequest) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  AddWrite(ConstructRequestHeadersPacket(1, kFin, DEFAULT_PRIORITY));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));
  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));

  // Ack the request.
  ProcessPacket(ConstructAckPacket(1, 0, 0));

  EXPECT_EQ(ERR_IO_PENDING,
            stream_->ReadResponseHeaders(callback_.callback()));

  SetResponse("404 Not Found", std::string());
  ProcessPacket(ConstructResponseHeadersPacket(2, kFin));

  // Now that the headers have been processed, the callback will return.
  EXPECT_EQ(OK, callback_.WaitForResult());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(404, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));
  EXPECT_FALSE(response_.response_time.is_null());
  EXPECT_FALSE(response_.request_time.is_null());

  // There is no body, so this should return immediately.
  EXPECT_EQ(0, stream_->ReadResponseBody(read_buffer_.get(),
                                         read_buffer_->size(),
                                         callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

// Regression test for http://crbug.com/288128
TEST_P(QuicHttpStreamTest, GetRequestLargeResponse) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  AddWrite(ConstructRequestHeadersPacket(1, kFin, DEFAULT_PRIORITY));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));
  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));

  // Ack the request.
  ProcessPacket(ConstructAckPacket(1, 0, 0));

  EXPECT_EQ(ERR_IO_PENDING,
            stream_->ReadResponseHeaders(callback_.callback()));

  SpdyHeaderBlock headers;
  headers[":status"] = "200 OK";
  headers[":version"] = "HTTP/1.1";
  headers["content-type"] = "text/plain";
  headers["big6"] = std::string(1000, 'x');  // Lots of x's.

  response_headers_ = headers;
  ProcessPacket(ConstructResponseHeadersPacket(2, kFin));

  // Now that the headers have been processed, the callback will return.
  EXPECT_EQ(OK, callback_.WaitForResult());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // There is no body, so this should return immediately.
  EXPECT_EQ(0, stream_->ReadResponseBody(read_buffer_.get(),
                                         read_buffer_->size(),
                                         callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

// Regression test for http://crbug.com/409101
TEST_P(QuicHttpStreamTest, SessionClosedBeforeSendRequest) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));

  session_->connection()->CloseConnection(QUIC_NO_ERROR, true);

  EXPECT_EQ(ERR_CONNECTION_CLOSED,
            stream_->SendRequest(headers_, &response_,
                                 callback_.callback()));
}

// Regression test for http://crbug.com/409871
TEST_P(QuicHttpStreamTest, SessionClosedBeforeReadResponseHeaders) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  AddWrite(ConstructRequestHeadersPacket(1, kFin, DEFAULT_PRIORITY));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));

  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));

  session_->connection()->CloseConnection(QUIC_NO_ERROR, true);

  EXPECT_NE(OK, stream_->ReadResponseHeaders(callback_.callback()));
}

TEST_P(QuicHttpStreamTest, SendPostRequest) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  AddWrite(ConstructRequestHeadersPacket(1, !kFin, DEFAULT_PRIORITY));
  AddWrite(ConstructDataPacket(2, kIncludeVersion, kFin, 0, kUploadData));
  AddWrite(ConstructAckPacket(3, 3, 1));

  Initialize();

  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(
      new UploadBytesElementReader(kUploadData, strlen(kUploadData)));
  ElementsUploadDataStream upload_data_stream(element_readers.Pass(), 0);
  request_.method = "POST";
  request_.url = GURL("http://www.google.com/");
  request_.upload_data_stream = &upload_data_stream;
  ASSERT_EQ(OK, request_.upload_data_stream->Init(CompletionCallback()));

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));
  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));

  // Ack both packets in the request.
  ProcessPacket(ConstructAckPacket(1, 0, 0));

  // Send the response headers (but not the body).
  SetResponse("200 OK", std::string());
  ProcessPacket(ConstructResponseHeadersPacket(2, !kFin));

  // Since the headers have already arrived, this should return immediately.
  EXPECT_EQ(OK, stream_->ReadResponseHeaders(callback_.callback()));
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  ProcessPacket(ConstructDataPacket(3, false, kFin, 0, kResponseBody));
  // Since the body has already arrived, this should return immediately.
  EXPECT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

TEST_P(QuicHttpStreamTest, SendChunkedPostRequest) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t chunk_size = strlen(kUploadData);
  AddWrite(ConstructRequestHeadersPacket(1, !kFin, DEFAULT_PRIORITY));
  AddWrite(ConstructDataPacket(2, kIncludeVersion, !kFin, 0, kUploadData));
  AddWrite(ConstructDataPacket(3, kIncludeVersion, kFin, chunk_size,
                               kUploadData));
  AddWrite(ConstructAckPacket(4, 3, 1));
  Initialize();

  ChunkedUploadDataStream upload_data_stream(0);
  upload_data_stream.AppendData(kUploadData, chunk_size, false);

  request_.method = "POST";
  request_.url = GURL("http://www.google.com/");
  request_.upload_data_stream = &upload_data_stream;
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
      TestCompletionCallback().callback()));

  ASSERT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));
  ASSERT_EQ(ERR_IO_PENDING, stream_->SendRequest(headers_, &response_,
                                                 callback_.callback()));

  upload_data_stream.AppendData(kUploadData, chunk_size, true);

  // Ack both packets in the request.
  ProcessPacket(ConstructAckPacket(1, 0, 0));

  // Send the response headers (but not the body).
  SetResponse("200 OK", std::string());
  ProcessPacket(ConstructResponseHeadersPacket(2, !kFin));

  // Since the headers have already arrived, this should return immediately.
  ASSERT_EQ(OK, stream_->ReadResponseHeaders(callback_.callback()));
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  ProcessPacket(ConstructDataPacket(3, false, kFin, response_data_.length(),
                                    kResponseBody));

  // Since the body has already arrived, this should return immediately.
  ASSERT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

TEST_P(QuicHttpStreamTest, SendChunkedPostRequestWithFinalEmptyDataPacket) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t chunk_size = strlen(kUploadData);
  AddWrite(ConstructRequestHeadersPacket(1, !kFin, DEFAULT_PRIORITY));
  AddWrite(ConstructDataPacket(2, kIncludeVersion, !kFin, 0, kUploadData));
  AddWrite(ConstructDataPacket(3, kIncludeVersion, kFin, chunk_size, ""));
  AddWrite(ConstructAckPacket(4, 3, 1));
  Initialize();

  ChunkedUploadDataStream upload_data_stream(0);
  upload_data_stream.AppendData(kUploadData, chunk_size, false);

  request_.method = "POST";
  request_.url = GURL("http://www.google.com/");
  request_.upload_data_stream = &upload_data_stream;
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
      TestCompletionCallback().callback()));

  ASSERT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));
  ASSERT_EQ(ERR_IO_PENDING, stream_->SendRequest(headers_, &response_,
                                                 callback_.callback()));

  upload_data_stream.AppendData(nullptr, 0, true);

  ProcessPacket(ConstructAckPacket(1, 0, 0));

  // Send the response headers (but not the body).
  SetResponse("200 OK", std::string());
  ProcessPacket(ConstructResponseHeadersPacket(2, !kFin));

  // Since the headers have already arrived, this should return immediately.
  ASSERT_EQ(OK, stream_->ReadResponseHeaders(callback_.callback()));
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  ProcessPacket(ConstructDataPacket(3, false, kFin, response_data_.length(),
                                    kResponseBody));

  // Since the body has already arrived, this should return immediately.
  ASSERT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

TEST_P(QuicHttpStreamTest, SendChunkedPostRequestWithOneEmptyDataPacket) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  AddWrite(ConstructRequestHeadersPacket(1, !kFin, DEFAULT_PRIORITY));
  AddWrite(ConstructDataPacket(2, kIncludeVersion, kFin, 0, ""));
  AddWrite(ConstructAckPacket(3, 3, 1));
  Initialize();

  ChunkedUploadDataStream upload_data_stream(0);

  request_.method = "POST";
  request_.url = GURL("http://www.google.com/");
  request_.upload_data_stream = &upload_data_stream;
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
      TestCompletionCallback().callback()));

  ASSERT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));
  ASSERT_EQ(ERR_IO_PENDING, stream_->SendRequest(headers_, &response_,
                                                 callback_.callback()));

  upload_data_stream.AppendData(nullptr, 0, true);

  ProcessPacket(ConstructAckPacket(1, 0, 0));

  // Send the response headers (but not the body).
  SetResponse("200 OK", std::string());
  ProcessPacket(ConstructResponseHeadersPacket(2, !kFin));

  // Since the headers have already arrived, this should return immediately.
  ASSERT_EQ(OK, stream_->ReadResponseHeaders(callback_.callback()));
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  ProcessPacket(ConstructDataPacket(3, false, kFin, response_data_.length(),
                                    kResponseBody));

  // Since the body has already arrived, this should return immediately.
  ASSERT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

TEST_P(QuicHttpStreamTest, DestroyedEarly) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  AddWrite(ConstructRequestHeadersPacket(1, kFin, DEFAULT_PRIORITY));
  AddWrite(ConstructAckAndRstStreamPacket(2));
  use_closing_stream_ = true;
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));
  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));

  // Ack the request.
  ProcessPacket(ConstructAckPacket(1, 0, 0));
  EXPECT_EQ(ERR_IO_PENDING,
            stream_->ReadResponseHeaders(callback_.callback()));

  // Send the response with a body.
  SetResponse("404 OK", "hello world!");
  // In the course of processing this packet, the QuicHttpStream close itself.
  ProcessPacket(ConstructResponseHeadersPacket(2, kFin));

  EXPECT_TRUE(AtEof());
}

TEST_P(QuicHttpStreamTest, Priority) {
  SetRequest("GET", "/", MEDIUM);
  AddWrite(ConstructRequestHeadersPacket(1, kFin, MEDIUM));
  AddWrite(ConstructAckAndRstStreamPacket(2));
  use_closing_stream_ = true;
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, MEDIUM,
                                          net_log_, callback_.callback()));

  // Check that priority is highest.
  QuicReliableClientStream* reliable_stream =
      QuicHttpStreamPeer::GetQuicReliableClientStream(stream_.get());
  DCHECK(reliable_stream);
  DCHECK_EQ(QuicWriteBlockedList::kHighestPriority,
            reliable_stream->EffectivePriority());

  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));

  // Check that priority has now dropped back to MEDIUM.
  DCHECK_EQ(MEDIUM, ConvertQuicPriorityToRequestPriority(
      reliable_stream->EffectivePriority()));

  // Ack the request.
  ProcessPacket(ConstructAckPacket(1, 0, 0));
  EXPECT_EQ(ERR_IO_PENDING,
            stream_->ReadResponseHeaders(callback_.callback()));

  // Send the response with a body.
  SetResponse("404 OK", "hello world!");
  // In the course of processing this packet, the QuicHttpStream close itself.
  ProcessPacket(ConstructResponseHeadersPacket(2, kFin));

  EXPECT_TRUE(AtEof());
}

// Regression test for http://crbug.com/294870
TEST_P(QuicHttpStreamTest, CheckPriorityWithNoDelegate) {
  SetRequest("GET", "/", MEDIUM);
  use_closing_stream_ = true;

  AddWrite(ConstructRstStreamPacket(1));

  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, MEDIUM,
                                          net_log_, callback_.callback()));

  // Check that priority is highest.
  QuicReliableClientStream* reliable_stream =
      QuicHttpStreamPeer::GetQuicReliableClientStream(stream_.get());
  DCHECK(reliable_stream);
  QuicReliableClientStream::Delegate* delegate = reliable_stream->GetDelegate();
  DCHECK(delegate);
  DCHECK_EQ(QuicWriteBlockedList::kHighestPriority,
            reliable_stream->EffectivePriority());

  // Set Delegate to nullptr and make sure EffectivePriority returns highest
  // priority.
  reliable_stream->SetDelegate(nullptr);
  DCHECK_EQ(QuicWriteBlockedList::kHighestPriority,
            reliable_stream->EffectivePriority());
  reliable_stream->SetDelegate(delegate);
}

}  // namespace test
}  // namespace net
