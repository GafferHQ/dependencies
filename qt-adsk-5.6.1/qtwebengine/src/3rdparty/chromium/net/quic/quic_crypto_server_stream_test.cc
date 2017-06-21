// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_server_stream.h"

#include <map>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/delayed_verify_strike_register_client.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
class QuicConnection;
class ReliableQuicStream;
}  // namespace net

using std::pair;
using std::string;
using testing::_;

namespace net {
namespace test {

class QuicCryptoServerConfigPeer {
 public:
  static string GetPrimaryOrbit(const QuicCryptoServerConfig& config) {
    base::AutoLock lock(config.configs_lock_);
    CHECK(config.primary_config_.get() != nullptr);
    return string(reinterpret_cast<const char*>(config.primary_config_->orbit),
                  kOrbitSize);
  }
};

class QuicCryptoServerStreamPeer {
 public:
  static bool DoesPeerSupportStatelessRejects(
      const CryptoHandshakeMessage& message) {
    return net::QuicCryptoServerStream::DoesPeerSupportStatelessRejects(
        message);
  }
};

namespace {

const char kServerHostname[] = "test.example.com";
const uint16 kServerPort = 80;

class QuicCryptoServerStreamTest : public ::testing::TestWithParam<bool> {
 public:
  QuicCryptoServerStreamTest()
      : server_crypto_config_(QuicCryptoServerConfig::TESTING,
                              QuicRandom::GetInstance()),
        server_id_(kServerHostname, kServerPort, false, PRIVACY_MODE_DISABLED) {
    // TODO(wtc): replace this with ProofSourceForTesting() when Chromium has
    // a working ProofSourceForTesting().
    server_crypto_config_.SetProofSource(
        CryptoTestUtils::FakeProofSourceForTesting());
    server_crypto_config_.set_strike_register_no_startup_period();

    InitializeServer();

    if (AsyncStrikeRegisterVerification()) {
      string orbit =
          QuicCryptoServerConfigPeer::GetPrimaryOrbit(server_crypto_config_);
      strike_register_client_ = new DelayedVerifyStrikeRegisterClient(
          10000,  // strike_register_max_entries
          static_cast<uint32>(
              server_connection_->clock()->WallNow().ToUNIXSeconds()),
          60,  // strike_register_window_secs
          reinterpret_cast<const uint8*>(orbit.data()),
          StrikeRegister::NO_STARTUP_PERIOD_NEEDED);
      strike_register_client_->StartDelayingVerification();
      server_crypto_config_.SetStrikeRegisterClient(strike_register_client_);
    }
  }

  // Initializes the crypto server stream state for testing.  May be
  // called multiple times.
  void InitializeServer() {
    TestQuicSpdyServerSession* server_session = nullptr;
    CreateServerSessionForTest(server_id_, QuicTime::Delta::FromSeconds(100000),
                               &server_crypto_config_, &server_connection_,
                               &server_session);
    CHECK(server_session);
    server_session_.reset(server_session);
    CryptoTestUtils::SetupCryptoServerConfigForTest(
        server_connection_->clock(), server_connection_->random_generator(),
        server_session_->config(), &server_crypto_config_);
  }

  QuicCryptoServerStream* server_stream() {
    return server_session_->GetCryptoStream();
  }

  QuicCryptoClientStream* client_stream() {
    return client_session_->GetCryptoStream();
  }

  // Initializes a fake client, and all its associated state, for
  // testing.  May be called multiple times.
  void InitializeFakeClient(bool supports_stateless_rejects) {
    TestQuicSpdyClientSession* client_session = nullptr;
    CreateClientSessionForTest(server_id_, supports_stateless_rejects,
                               QuicTime::Delta::FromSeconds(100000),
                               &client_crypto_config_, &client_connection_,
                               &client_session);
    CHECK(client_session);
    client_session_.reset(client_session);
  }

  bool AsyncStrikeRegisterVerification() {
    return GetParam();
  }

  void ConstructHandshakeMessage() {
    CryptoFramer framer;
    message_data_.reset(framer.ConstructHandshakeMessage(message_));
  }

  int CompleteCryptoHandshake() {
    CHECK(server_connection_);
    CHECK(server_session_ != nullptr);
    return CryptoTestUtils::HandshakeWithFakeClient(
        server_connection_, server_stream(), client_options_);
  }

  // Performs a single round of handshake message-exchange between the
  // client and server.
  void AdvanceHandshakeWithFakeClient() {
    CHECK(server_connection_);
    CHECK(client_session_ != nullptr);

    EXPECT_CALL(*client_session_, OnProofValid(_)).Times(testing::AnyNumber());
    client_stream()->CryptoConnect();
    CryptoTestUtils::AdvanceHandshake(client_connection_, client_stream(), 0,
                                      server_connection_, server_stream(), 0);
  }

 protected:
  // Server state
  PacketSavingConnection* server_connection_;
  scoped_ptr<TestQuicSpdyServerSession> server_session_;
  QuicCryptoServerConfig server_crypto_config_;
  QuicServerId server_id_;

  // Client state
  PacketSavingConnection* client_connection_;
  QuicCryptoClientConfig client_crypto_config_;
  scoped_ptr<TestQuicSpdyClientSession> client_session_;

  CryptoHandshakeMessage message_;
  scoped_ptr<QuicData> message_data_;
  CryptoTestUtils::FakeClientOptions client_options_;
  DelayedVerifyStrikeRegisterClient* strike_register_client_;
};

INSTANTIATE_TEST_CASE_P(Tests, QuicCryptoServerStreamTest, testing::Bool());

TEST_P(QuicCryptoServerStreamTest, NotInitiallyConected) {
  EXPECT_FALSE(server_stream()->encryption_established());
  EXPECT_FALSE(server_stream()->handshake_confirmed());
}

TEST_P(QuicCryptoServerStreamTest, NotInitiallySendingStatelessRejects) {
  EXPECT_FALSE(server_stream()->use_stateless_rejects_if_peer_supported());
  EXPECT_FALSE(server_stream()->peer_supports_stateless_rejects());
}

TEST_P(QuicCryptoServerStreamTest, ConnectedAfterCHLO) {
  // CompleteCryptoHandshake returns the number of client hellos sent. This
  // test should send:
  //   * One to get a source-address token and certificates.
  //   * One to complete the handshake.
  EXPECT_EQ(2, CompleteCryptoHandshake());
  EXPECT_TRUE(server_stream()->encryption_established());
  EXPECT_TRUE(server_stream()->handshake_confirmed());
}

TEST_P(QuicCryptoServerStreamTest, StatelessRejectAfterCHLO) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stateless_reject_support,
                              true);
  server_stream()->set_use_stateless_rejects_if_peer_supported(true);

  InitializeFakeClient(/* supports_stateless_rejects= */ true);
  AdvanceHandshakeWithFakeClient();

  // Check the server to make the sure the handshake did not succeed.
  EXPECT_FALSE(server_stream()->encryption_established());
  EXPECT_FALSE(server_stream()->handshake_confirmed());

  // Check the client state to make sure that it received a server-designated
  // connection id.
  QuicCryptoClientConfig::CachedState* client_state =
      client_crypto_config_.LookupOrCreate(server_id_);

  ASSERT_TRUE(client_state->has_server_nonce());
  ASSERT_FALSE(client_state->GetNextServerNonce().empty());
  ASSERT_FALSE(client_state->has_server_nonce());

  ASSERT_TRUE(client_state->has_server_designated_connection_id());
  const QuicConnectionId server_designated_connection_id =
      client_state->GetNextServerDesignatedConnectionId();
  const QuicConnectionId expected_id =
      reinterpret_cast<MockRandom*>(server_connection_->random_generator())
          ->RandUint64();
  EXPECT_EQ(expected_id, server_designated_connection_id);
  EXPECT_FALSE(client_state->has_server_designated_connection_id());
  ASSERT_TRUE(client_state->IsComplete(QuicWallTime::FromUNIXSeconds(0)));
}

TEST_P(QuicCryptoServerStreamTest, ConnectedAfterStatelessHandshake) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stateless_reject_support,
                              true);
  server_stream()->set_use_stateless_rejects_if_peer_supported(true);

  InitializeFakeClient(/* supports_stateless_rejects= */ true);
  AdvanceHandshakeWithFakeClient();

  // On the first round, encryption will not be established.
  EXPECT_FALSE(server_stream()->encryption_established());
  EXPECT_FALSE(server_stream()->handshake_confirmed());
  EXPECT_EQ(1, server_stream()->num_handshake_messages());
  EXPECT_EQ(0, server_stream()->num_handshake_messages_with_server_nonces());

  // Now check the client state.
  QuicCryptoClientConfig::CachedState* client_state =
      client_crypto_config_.LookupOrCreate(server_id_);

  ASSERT_TRUE(client_state->has_server_designated_connection_id());
  const QuicConnectionId server_designated_connection_id =
      client_state->GetNextServerDesignatedConnectionId();
  const QuicConnectionId expected_id =
      reinterpret_cast<MockRandom*>(server_connection_->random_generator())
          ->RandUint64();
  EXPECT_EQ(expected_id, server_designated_connection_id);
  EXPECT_FALSE(client_state->has_server_designated_connection_id());
  ASSERT_TRUE(client_state->IsComplete(QuicWallTime::FromUNIXSeconds(0)));

  // Now create new client and server streams with the existing config
  // and try the handshake again (0-RTT handshake).
  InitializeServer();
  server_stream()->set_use_stateless_rejects_if_peer_supported(true);

  InitializeFakeClient(/* supports_stateless_rejects= */ true);

  client_stream()->CryptoConnect();

  // In the stateless case, the second handshake contains a server-nonce, so the
  // AsyncStrikeRegisterVerification() case will still succeed (unlike a 0-RTT
  // handshake).
  AdvanceHandshakeWithFakeClient();

  // On the second round, encryption will be established.
  EXPECT_TRUE(server_stream()->encryption_established());
  EXPECT_TRUE(server_stream()->handshake_confirmed());
  EXPECT_EQ(2, server_stream()->num_handshake_messages());
  EXPECT_EQ(1, server_stream()->num_handshake_messages_with_server_nonces());
}

TEST_P(QuicCryptoServerStreamTest, NoStatelessRejectIfNoClientSupport) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stateless_reject_support,
                              true);
  server_stream()->set_use_stateless_rejects_if_peer_supported(true);

  // The server is configured to use stateless rejects, but the client does not
  // support it.
  InitializeFakeClient(/* supports_stateless_rejects= */ false);
  AdvanceHandshakeWithFakeClient();

  // Check the server to make the sure the handshake did not succeed.
  EXPECT_FALSE(server_stream()->encryption_established());
  EXPECT_FALSE(server_stream()->handshake_confirmed());

  // Check the client state to make sure that it did not receive a
  // server-designated connection id.
  QuicCryptoClientConfig::CachedState* client_state =
      client_crypto_config_.LookupOrCreate(server_id_);

  ASSERT_FALSE(client_state->has_server_designated_connection_id());
  ASSERT_TRUE(client_state->IsComplete(QuicWallTime::FromUNIXSeconds(0)));
}

TEST_P(QuicCryptoServerStreamTest, ZeroRTT) {
  InitializeFakeClient(/* supports_stateless_rejects= */ false);

  // Do a first handshake in order to prime the client config with the server's
  // information.
  AdvanceHandshakeWithFakeClient();

  // Now do another handshake, hopefully in 0-RTT.
  DVLOG(1) << "Resetting for 0-RTT handshake attempt";
  InitializeFakeClient(/* supports_stateless_rejects= */ false);
  InitializeServer();

  client_stream()->CryptoConnect();

  if (AsyncStrikeRegisterVerification()) {
    EXPECT_FALSE(client_stream()->handshake_confirmed());
    EXPECT_FALSE(server_stream()->handshake_confirmed());

    // Advance the handshake.  Expect that the server will be stuck waiting for
    // client nonce verification to complete.
    pair<size_t, size_t> messages_moved = CryptoTestUtils::AdvanceHandshake(
        client_connection_, client_stream(), 0, server_connection_,
        server_stream(), 0);
    EXPECT_EQ(1u, messages_moved.first);
    EXPECT_EQ(0u, messages_moved.second);
    EXPECT_EQ(1, strike_register_client_->PendingVerifications());
    EXPECT_FALSE(client_stream()->handshake_confirmed());
    EXPECT_FALSE(server_stream()->handshake_confirmed());

    // The server handshake completes once the nonce verification completes.
    strike_register_client_->RunPendingVerifications();
    EXPECT_FALSE(client_stream()->handshake_confirmed());
    EXPECT_TRUE(server_stream()->handshake_confirmed());

    messages_moved = CryptoTestUtils::AdvanceHandshake(
        client_connection_, client_stream(), messages_moved.first,
        server_connection_, server_stream(), messages_moved.second);
    EXPECT_EQ(1u, messages_moved.first);
    EXPECT_EQ(1u, messages_moved.second);
    EXPECT_TRUE(client_stream()->handshake_confirmed());
    EXPECT_TRUE(server_stream()->handshake_confirmed());
  } else {
    CryptoTestUtils::CommunicateHandshakeMessages(
        client_connection_, client_stream(), server_connection_,
        server_stream());
  }

  EXPECT_EQ(1, client_stream()->num_sent_client_hellos());
}

TEST_P(QuicCryptoServerStreamTest, MessageAfterHandshake) {
  CompleteCryptoHandshake();
  EXPECT_CALL(
      *server_connection_,
      SendConnectionClose(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE));
  message_.set_tag(kCHLO);
  ConstructHandshakeMessage();
  server_stream()->ProcessRawData(message_data_->data(),
                                  message_data_->length());
}

TEST_P(QuicCryptoServerStreamTest, BadMessageType) {
  message_.set_tag(kSHLO);
  ConstructHandshakeMessage();
  EXPECT_CALL(*server_connection_,
              SendConnectionClose(QUIC_INVALID_CRYPTO_MESSAGE_TYPE));
  server_stream()->ProcessRawData(message_data_->data(),
                                  message_data_->length());
}

TEST_P(QuicCryptoServerStreamTest, WithoutCertificates) {
  server_crypto_config_.SetProofSource(nullptr);
  client_options_.dont_verify_certs = true;

  // Only 2 client hellos need to be sent in the no-certs case: one to get the
  // source-address token and the second to finish.
  EXPECT_EQ(2, CompleteCryptoHandshake());
  EXPECT_TRUE(server_stream()->encryption_established());
  EXPECT_TRUE(server_stream()->handshake_confirmed());
}

TEST_P(QuicCryptoServerStreamTest, ChannelID) {
  client_options_.channel_id_enabled = true;
  client_options_.channel_id_source_async = false;
  // CompleteCryptoHandshake verifies
  // server_stream()->crypto_negotiated_params().channel_id is correct.
  EXPECT_EQ(2, CompleteCryptoHandshake());
  EXPECT_TRUE(server_stream()->encryption_established());
  EXPECT_TRUE(server_stream()->handshake_confirmed());
}

TEST_P(QuicCryptoServerStreamTest, ChannelIDAsync) {
  client_options_.channel_id_enabled = true;
  client_options_.channel_id_source_async = true;
  // CompleteCryptoHandshake verifies
  // server_stream()->crypto_negotiated_params().channel_id is correct.
  EXPECT_EQ(2, CompleteCryptoHandshake());
  EXPECT_TRUE(server_stream()->encryption_established());
  EXPECT_TRUE(server_stream()->handshake_confirmed());
}

TEST_P(QuicCryptoServerStreamTest, OnlySendSCUPAfterHandshakeComplete) {
  // An attempt to send a SCUP before completing handshake should fail.
  server_stream()->SendServerConfigUpdate(nullptr);
  EXPECT_EQ(0, server_stream()->num_server_config_update_messages_sent());
}

TEST_P(QuicCryptoServerStreamTest, DoesPeerSupportStatelessRejects) {
  ConstructHandshakeMessage();
  QuicConfig stateless_reject_config = DefaultQuicConfigStatelessRejects();
  stateless_reject_config.ToHandshakeMessage(&message_);
  EXPECT_TRUE(
      QuicCryptoServerStreamPeer::DoesPeerSupportStatelessRejects(message_));

  message_.Clear();
  QuicConfig stateful_reject_config = DefaultQuicConfig();
  stateful_reject_config.ToHandshakeMessage(&message_);
  EXPECT_FALSE(
      QuicCryptoServerStreamPeer::DoesPeerSupportStatelessRejects(message_));
}

}  // namespace
}  // namespace test
}  // namespace net
