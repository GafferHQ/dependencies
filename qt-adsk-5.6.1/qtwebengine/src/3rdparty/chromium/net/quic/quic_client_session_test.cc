// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_session.h"

#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/rand_util.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_verify_result.h"
#include "net/http/transport_security_state.h"
#include "net/log/test_net_log.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_server_info.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_client_session_peer.h"
#include "net/quic/test_tools/quic_spdy_session_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_quic_framer.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_test_utils.h"
#include "net/test/cert_test_util.h"
#include "net/udp/datagram_client_socket.h"

using testing::_;

namespace net {
namespace test {
namespace {

const char kServerHostname[] = "www.example.org";
const uint16 kServerPort = 80;

class QuicClientSessionTest : public ::testing::TestWithParam<QuicVersion> {
 protected:
  QuicClientSessionTest()
      : connection_(new PacketSavingConnection(Perspective::IS_CLIENT,
                                               SupportedVersions(GetParam()))),
        session_(connection_,
                 GetSocket().Pass(),
                 /*stream_factory=*/nullptr,
                 /*crypto_client_stream_factory=*/nullptr,
                 &transport_security_state_,
                 make_scoped_ptr((QuicServerInfo*)nullptr),
                 QuicServerId(kServerHostname,
                              kServerPort,
                              /*is_secure=*/false,
                              PRIVACY_MODE_DISABLED),
                 /*cert_verify_flags=*/0,
                 DefaultQuicConfig(),
                 &crypto_config_,
                 "CONNECTION_UNKNOWN",
                 base::TimeTicks::Now(),
                 base::ThreadTaskRunnerHandle::Get().get(),
                 &net_log_) {
    session_.Initialize();
    // Advance the time, because timers do not like uninitialized times.
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
  }

  void TearDown() override {
    session_.CloseSessionOnError(ERR_ABORTED, QUIC_INTERNAL_ERROR);
  }

  scoped_ptr<DatagramClientSocket> GetSocket() {
    socket_factory_.AddSocketDataProvider(&socket_data_);
    return socket_factory_.CreateDatagramClientSocket(
        DatagramSocket::DEFAULT_BIND, base::Bind(&base::RandInt),
        &net_log_, NetLog::Source());
  }

  void CompleteCryptoHandshake() {
    ASSERT_EQ(ERR_IO_PENDING,
              session_.CryptoConnect(false, callback_.callback()));
    CryptoTestUtils::HandshakeWithFakeServer(
        connection_, session_.GetCryptoStream());
    ASSERT_EQ(OK, callback_.WaitForResult());
  }

  PacketSavingConnection* connection_;
  TestNetLog net_log_;
  MockClientSocketFactory socket_factory_;
  StaticSocketDataProvider socket_data_;
  TransportSecurityState transport_security_state_;
  QuicClientSession session_;
  MockClock clock_;
  MockRandom random_;
  QuicConnectionVisitorInterface* visitor_;
  TestCompletionCallback callback_;
  QuicCryptoClientConfig crypto_config_;
};

INSTANTIATE_TEST_CASE_P(Tests, QuicClientSessionTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicClientSessionTest, CryptoConnect) {
  CompleteCryptoHandshake();
}

TEST_P(QuicClientSessionTest, MaxNumStreams) {
  CompleteCryptoHandshake();

  std::vector<QuicReliableClientStream*> streams;
  for (size_t i = 0; i < kDefaultMaxStreamsPerConnection; i++) {
    QuicReliableClientStream* stream = session_.CreateOutgoingDynamicStream();
    EXPECT_TRUE(stream);
    streams.push_back(stream);
  }
  EXPECT_FALSE(session_.CreateOutgoingDynamicStream());

  // Close a stream and ensure I can now open a new one.
  session_.CloseStream(streams[0]->id());
  EXPECT_TRUE(session_.CreateOutgoingDynamicStream());
}

TEST_P(QuicClientSessionTest, MaxNumStreamsViaRequest) {
  CompleteCryptoHandshake();

  std::vector<QuicReliableClientStream*> streams;
  for (size_t i = 0; i < kDefaultMaxStreamsPerConnection; i++) {
    QuicReliableClientStream* stream = session_.CreateOutgoingDynamicStream();
    EXPECT_TRUE(stream);
    streams.push_back(stream);
  }

  QuicReliableClientStream* stream;
  QuicClientSession::StreamRequest stream_request;
  TestCompletionCallback callback;
  ASSERT_EQ(ERR_IO_PENDING,
            stream_request.StartRequest(session_.GetWeakPtr(), &stream,
                                        callback.callback()));

  // Close a stream and ensure I can now open a new one.
  session_.CloseStream(streams[0]->id());
  ASSERT_TRUE(callback.have_result());
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(stream != nullptr);
}

TEST_P(QuicClientSessionTest, GoAwayReceived) {
  CompleteCryptoHandshake();

  // After receiving a GoAway, I should no longer be able to create outgoing
  // streams.
  session_.OnGoAway(QuicGoAwayFrame(QUIC_PEER_GOING_AWAY, 1u, "Going away."));
  EXPECT_EQ(nullptr, session_.CreateOutgoingDynamicStream());
}

TEST_P(QuicClientSessionTest, CanPool) {
  // Load a cert that is valid for:
  //   www.example.org
  //   mail.example.org
  //   www.example.com

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  session_.OnProofVerifyDetailsAvailable(details);
  CompleteCryptoHandshake();

  EXPECT_TRUE(session_.CanPool("www.example.org", PRIVACY_MODE_DISABLED));
  EXPECT_FALSE(session_.CanPool("www.example.org", PRIVACY_MODE_ENABLED));
  EXPECT_TRUE(session_.CanPool("mail.example.org", PRIVACY_MODE_DISABLED));
  EXPECT_TRUE(session_.CanPool("mail.example.com", PRIVACY_MODE_DISABLED));
  EXPECT_FALSE(session_.CanPool("mail.google.com", PRIVACY_MODE_DISABLED));
}

TEST_P(QuicClientSessionTest, ConnectionPooledWithTlsChannelId) {
  // Load a cert that is valid for:
  //   www.example.org
  //   mail.example.org
  //   www.example.com

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  session_.OnProofVerifyDetailsAvailable(details);
  CompleteCryptoHandshake();
  QuicClientSessionPeer::SetChannelIDSent(&session_, true);

  EXPECT_TRUE(session_.CanPool("www.example.org", PRIVACY_MODE_DISABLED));
  EXPECT_TRUE(session_.CanPool("mail.example.org", PRIVACY_MODE_DISABLED));
  EXPECT_FALSE(session_.CanPool("mail.example.com", PRIVACY_MODE_DISABLED));
  EXPECT_FALSE(session_.CanPool("mail.google.com", PRIVACY_MODE_DISABLED));
}

TEST_P(QuicClientSessionTest, ConnectionNotPooledWithDifferentPin) {
  uint8 primary_pin = 1;
  uint8 backup_pin = 2;
  uint8 bad_pin = 3;
  AddPin(&transport_security_state_, "mail.example.org", primary_pin,
         backup_pin);

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.is_issued_by_known_root = true;
  details.cert_verify_result.public_key_hashes.push_back(
      GetTestHashValue(bad_pin));

  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  session_.OnProofVerifyDetailsAvailable(details);
  CompleteCryptoHandshake();
  QuicClientSessionPeer::SetChannelIDSent(&session_, true);

  EXPECT_FALSE(session_.CanPool("mail.example.org", PRIVACY_MODE_DISABLED));
}

TEST_P(QuicClientSessionTest, ConnectionPooledWithMatchingPin) {
  uint8 primary_pin = 1;
  uint8 backup_pin = 2;
  AddPin(&transport_security_state_, "mail.example.org", primary_pin,
         backup_pin);

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.is_issued_by_known_root = true;
  details.cert_verify_result.public_key_hashes.push_back(
      GetTestHashValue(primary_pin));

  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  session_.OnProofVerifyDetailsAvailable(details);
  CompleteCryptoHandshake();
  QuicClientSessionPeer::SetChannelIDSent(&session_, true);

  EXPECT_TRUE(session_.CanPool("mail.example.org", PRIVACY_MODE_DISABLED));
}

}  // namespace
}  // namespace test
}  // namespace net
