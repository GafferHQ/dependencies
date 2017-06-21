// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/quic_crypto_client_config.h"

#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/quic_server_id.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;
using std::vector;

namespace net {
namespace test {
namespace {

class TestProofVerifyDetails : public ProofVerifyDetails {
  ~TestProofVerifyDetails() override {}

  // ProofVerifyDetails implementation
  ProofVerifyDetails* Clone() const override {
    return new TestProofVerifyDetails;
  }
};

}  // namespace

TEST(QuicCryptoClientConfigTest, CachedState_IsEmpty) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_TRUE(state.IsEmpty());
}

TEST(QuicCryptoClientConfigTest, CachedState_IsComplete) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_FALSE(state.IsComplete(QuicWallTime::FromUNIXSeconds(0)));
}

TEST(QuicCryptoClientConfigTest, CachedState_GenerationCounter) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_EQ(0u, state.generation_counter());
  state.SetProofInvalid();
  EXPECT_EQ(1u, state.generation_counter());
}

TEST(QuicCryptoClientConfigTest, CachedState_SetProofVerifyDetails) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_TRUE(state.proof_verify_details() == nullptr);
  ProofVerifyDetails* details = new TestProofVerifyDetails;
  state.SetProofVerifyDetails(details);
  EXPECT_EQ(details, state.proof_verify_details());
}

TEST(QuicCryptoClientConfigTest, CachedState_ServerDesignatedConnectionId) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_FALSE(state.has_server_designated_connection_id());

  QuicConnectionId connection_id = 1234;
  state.add_server_designated_connection_id(connection_id);
  EXPECT_TRUE(state.has_server_designated_connection_id());
  EXPECT_EQ(connection_id, state.GetNextServerDesignatedConnectionId());
  EXPECT_FALSE(state.has_server_designated_connection_id());

  // Allow the ID to be set multiple times.  It's unusual that this would
  // happen, but not impossible.
  ++connection_id;
  state.add_server_designated_connection_id(connection_id);
  EXPECT_TRUE(state.has_server_designated_connection_id());
  EXPECT_EQ(connection_id, state.GetNextServerDesignatedConnectionId());
  ++connection_id;
  state.add_server_designated_connection_id(connection_id);
  EXPECT_EQ(connection_id, state.GetNextServerDesignatedConnectionId());
  EXPECT_FALSE(state.has_server_designated_connection_id());

  // Test FIFO behavior.
  const QuicConnectionId first_cid = 0xdeadbeef;
  const QuicConnectionId second_cid = 0xfeedbead;
  state.add_server_designated_connection_id(first_cid);
  state.add_server_designated_connection_id(second_cid);
  EXPECT_TRUE(state.has_server_designated_connection_id());
  EXPECT_EQ(first_cid, state.GetNextServerDesignatedConnectionId());
  EXPECT_EQ(second_cid, state.GetNextServerDesignatedConnectionId());
}

TEST(QuicCryptoClientConfigTest, CachedState_ServerIdConsumedBeforeSet) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_FALSE(state.has_server_designated_connection_id());
#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
  EXPECT_DEBUG_DEATH(state.GetNextServerDesignatedConnectionId(),
                     "Attempting to consume a connection id "
                     "that was never designated.");
#endif  // GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
}

TEST(QuicCryptoClientConfigTest, CachedState_ServerNonce) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_FALSE(state.has_server_nonce());

  string server_nonce = "nonce_1";
  state.add_server_nonce(server_nonce);
  EXPECT_TRUE(state.has_server_nonce());
  EXPECT_EQ(server_nonce, state.GetNextServerNonce());
  EXPECT_FALSE(state.has_server_nonce());

  // Allow the ID to be set multiple times.  It's unusual that this would
  // happen, but not impossible.
  server_nonce = "nonce_2";
  state.add_server_nonce(server_nonce);
  EXPECT_TRUE(state.has_server_nonce());
  EXPECT_EQ(server_nonce, state.GetNextServerNonce());
  server_nonce = "nonce_3";
  state.add_server_nonce(server_nonce);
  EXPECT_EQ(server_nonce, state.GetNextServerNonce());
  EXPECT_FALSE(state.has_server_nonce());

  // Test FIFO behavior.
  const string first_nonce = "first_nonce";
  const string second_nonce = "second_nonce";
  state.add_server_nonce(first_nonce);
  state.add_server_nonce(second_nonce);
  EXPECT_TRUE(state.has_server_nonce());
  EXPECT_EQ(first_nonce, state.GetNextServerNonce());
  EXPECT_EQ(second_nonce, state.GetNextServerNonce());
}

TEST(QuicCryptoClientConfigTest, CachedState_ServerNonceConsumedBeforeSet) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_FALSE(state.has_server_nonce());
#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
  EXPECT_DEBUG_DEATH(state.GetNextServerNonce(),
                     "Attempting to consume a server nonce "
                     "that was never designated.");
#endif  // GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
}

TEST(QuicCryptoClientConfigTest, CachedState_InitializeFrom) {
  QuicCryptoClientConfig::CachedState state;
  QuicCryptoClientConfig::CachedState other;
  state.set_source_address_token("TOKEN");
  // TODO(rch): Populate other fields of |state|.
  other.InitializeFrom(state);
  EXPECT_EQ(state.server_config(), other.server_config());
  EXPECT_EQ(state.source_address_token(), other.source_address_token());
  EXPECT_EQ(state.certs(), other.certs());
  EXPECT_EQ(1u, other.generation_counter());
  EXPECT_FALSE(state.has_server_designated_connection_id());
  EXPECT_FALSE(state.has_server_nonce());
}

TEST(QuicCryptoClientConfigTest, InchoateChlo) {
  QuicCryptoClientConfig::CachedState state;
  QuicCryptoClientConfig config;
  QuicCryptoNegotiatedParameters params;
  CryptoHandshakeMessage msg;
  QuicServerId server_id("www.google.com", 80, false, PRIVACY_MODE_DISABLED);
  config.FillInchoateClientHello(server_id, QuicVersionMax(), &state,
                                 &params, &msg);

  QuicTag cver;
  EXPECT_EQ(QUIC_NO_ERROR, msg.GetUint32(kVER, &cver));
  EXPECT_EQ(QuicVersionToQuicTag(QuicVersionMax()), cver);
}

TEST(QuicCryptoClientConfigTest, PreferAesGcm) {
  QuicCryptoClientConfig config;
  if (config.aead.size() > 1)
    EXPECT_NE(kAESG, config.aead[0]);
  config.PreferAesGcm();
  EXPECT_EQ(kAESG, config.aead[0]);
}

TEST(QuicCryptoClientConfigTest, InchoateChloSecure) {
  QuicCryptoClientConfig::CachedState state;
  QuicCryptoClientConfig config;
  QuicCryptoNegotiatedParameters params;
  CryptoHandshakeMessage msg;
  QuicServerId server_id("www.google.com", 443, true, PRIVACY_MODE_DISABLED);
  config.FillInchoateClientHello(server_id, QuicVersionMax(), &state,
                                 &params, &msg);

  QuicTag pdmd;
  EXPECT_EQ(QUIC_NO_ERROR, msg.GetUint32(kPDMD, &pdmd));
  EXPECT_EQ(kX509, pdmd);
}

TEST(QuicCryptoClientConfigTest, InchoateChloSecureNoEcdsa) {
  QuicCryptoClientConfig::CachedState state;
  QuicCryptoClientConfig config;
  config.DisableEcdsa();
  QuicCryptoNegotiatedParameters params;
  CryptoHandshakeMessage msg;
  QuicServerId server_id("www.google.com", 443, true, PRIVACY_MODE_DISABLED);
  config.FillInchoateClientHello(server_id, QuicVersionMax(), &state,
                                 &params, &msg);

  QuicTag pdmd;
  EXPECT_EQ(QUIC_NO_ERROR, msg.GetUint32(kPDMD, &pdmd));
  EXPECT_EQ(kX59R, pdmd);
}

TEST(QuicCryptoClientConfigTest, FillClientHello) {
  QuicCryptoClientConfig::CachedState state;
  QuicCryptoClientConfig config;
  QuicCryptoNegotiatedParameters params;
  QuicConnectionId kConnectionId = 1234;
  string error_details;
  MockRandom rand;
  CryptoHandshakeMessage chlo;
  QuicServerId server_id("www.google.com", 80, false, PRIVACY_MODE_DISABLED);
  config.FillClientHello(server_id,
                         kConnectionId,
                         QuicVersionMax(),
                         &state,
                         QuicWallTime::Zero(),
                         &rand,
                         nullptr,  // channel_id_key
                         &params,
                         &chlo,
                         &error_details);

  // Verify that certain QuicTags have been set correctly in the CHLO.
  QuicTag cver;
  EXPECT_EQ(QUIC_NO_ERROR, chlo.GetUint32(kVER, &cver));
  EXPECT_EQ(QuicVersionToQuicTag(QuicVersionMax()), cver);
}

TEST(QuicCryptoClientConfigTest, ProcessServerDowngradeAttack) {
  QuicVersionVector supported_versions = QuicSupportedVersions();
  if (supported_versions.size() == 1) {
    // No downgrade attack is possible if the client only supports one version.
    return;
  }
  QuicTagVector supported_version_tags;
  for (size_t i = supported_versions.size(); i > 0; --i) {
    supported_version_tags.push_back(
        QuicVersionToQuicTag(supported_versions[i - 1]));
  }
  CryptoHandshakeMessage msg;
  msg.set_tag(kSHLO);
  msg.SetVector(kVER, supported_version_tags);

  QuicCryptoClientConfig::CachedState cached;
  QuicCryptoNegotiatedParameters out_params;
  string error;
  QuicCryptoClientConfig config;
  EXPECT_EQ(QUIC_VERSION_NEGOTIATION_MISMATCH,
            config.ProcessServerHello(msg, 0, supported_versions,
                                      &cached, &out_params, &error));
  EXPECT_EQ("Downgrade attack detected", error);
}

TEST(QuicCryptoClientConfigTest, InitializeFrom) {
  QuicCryptoClientConfig config;
  QuicServerId canonical_server_id("www.google.com", 80, false,
                                   PRIVACY_MODE_DISABLED);
  QuicCryptoClientConfig::CachedState* state =
      config.LookupOrCreate(canonical_server_id);
  // TODO(rch): Populate other fields of |state|.
  state->set_source_address_token("TOKEN");
  state->SetProofValid();

  QuicServerId other_server_id("mail.google.com", 80, false,
                               PRIVACY_MODE_DISABLED);
  config.InitializeFrom(other_server_id, canonical_server_id, &config);
  QuicCryptoClientConfig::CachedState* other =
      config.LookupOrCreate(other_server_id);

  EXPECT_EQ(state->server_config(), other->server_config());
  EXPECT_EQ(state->source_address_token(), other->source_address_token());
  EXPECT_EQ(state->certs(), other->certs());
  EXPECT_EQ(1u, other->generation_counter());
}

TEST(QuicCryptoClientConfigTest, Canonical) {
  QuicCryptoClientConfig config;
  config.AddCanonicalSuffix(".google.com");
  QuicServerId canonical_id1("www.google.com", 80, false,
                             PRIVACY_MODE_DISABLED);
  QuicServerId canonical_id2("mail.google.com", 80, false,
                             PRIVACY_MODE_DISABLED);
  QuicCryptoClientConfig::CachedState* state =
      config.LookupOrCreate(canonical_id1);
  // TODO(rch): Populate other fields of |state|.
  state->set_source_address_token("TOKEN");
  state->SetProofValid();

  QuicCryptoClientConfig::CachedState* other =
      config.LookupOrCreate(canonical_id2);

  EXPECT_TRUE(state->IsEmpty());
  EXPECT_EQ(state->server_config(), other->server_config());
  EXPECT_EQ(state->source_address_token(), other->source_address_token());
  EXPECT_EQ(state->certs(), other->certs());
  EXPECT_EQ(1u, other->generation_counter());

  QuicServerId different_id("mail.google.org", 80, false,
                            PRIVACY_MODE_DISABLED);
  EXPECT_TRUE(config.LookupOrCreate(different_id)->IsEmpty());
}

TEST(QuicCryptoClientConfigTest, CanonicalNotUsedIfNotValid) {
  QuicCryptoClientConfig config;
  config.AddCanonicalSuffix(".google.com");
  QuicServerId canonical_id1("www.google.com", 80, false,
                             PRIVACY_MODE_DISABLED);
  QuicServerId canonical_id2("mail.google.com", 80, false,
                             PRIVACY_MODE_DISABLED);
  QuicCryptoClientConfig::CachedState* state =
      config.LookupOrCreate(canonical_id1);
  // TODO(rch): Populate other fields of |state|.
  state->set_source_address_token("TOKEN");

  // Do not set the proof as valid, and check that it is not used
  // as a canonical entry.
  EXPECT_TRUE(config.LookupOrCreate(canonical_id2)->IsEmpty());
}

TEST(QuicCryptoClientConfigTest, ClearCachedStates) {
  QuicCryptoClientConfig config;
  QuicServerId server_id("www.google.com", 80, false, PRIVACY_MODE_DISABLED);
  QuicCryptoClientConfig::CachedState* state = config.LookupOrCreate(server_id);
  // TODO(rch): Populate other fields of |state|.
  vector<string> certs(1);
  certs[0] = "Hello Cert";
  state->SetProof(certs, "signature");
  state->set_source_address_token("TOKEN");
  state->SetProofValid();
  EXPECT_EQ(1u, state->generation_counter());

  // Verify LookupOrCreate returns the same data.
  QuicCryptoClientConfig::CachedState* other = config.LookupOrCreate(server_id);

  EXPECT_EQ(state, other);
  EXPECT_EQ(1u, other->generation_counter());

  // Clear the cached states.
  config.ClearCachedStates();

  // Verify LookupOrCreate doesn't have any data.
  QuicCryptoClientConfig::CachedState* cleared_cache =
      config.LookupOrCreate(server_id);

  EXPECT_EQ(state, cleared_cache);
  EXPECT_FALSE(cleared_cache->proof_valid());
  EXPECT_TRUE(cleared_cache->server_config().empty());
  EXPECT_TRUE(cleared_cache->certs().empty());
  EXPECT_TRUE(cleared_cache->signature().empty());
  EXPECT_EQ(2u, cleared_cache->generation_counter());
}

// Creates a minimal dummy reject message that will pass the client-config
// validation tests.
void FillInDummyReject(CryptoHandshakeMessage* rej, bool reject_is_stateless) {
  if (reject_is_stateless) {
    rej->set_tag(kSREJ);
  } else {
    rej->set_tag(kREJ);
  }

  // Minimum SCFG that passes config validation checks.
  // clang-format off
  unsigned char scfg[] = {
    // SCFG
    0x53, 0x43, 0x46, 0x47,
    // num entries
    0x01, 0x00,
    // padding
    0x00, 0x00,
    // EXPY
    0x45, 0x58, 0x50, 0x59,
    // EXPY end offset
    0x08, 0x00, 0x00, 0x00,
    // Value
    '1',  '2',  '3',  '4',
    '5',  '6',  '7',  '8'
  };
  // clang-format on
  rej->SetValue(kSCFG, scfg);
  rej->SetStringPiece(kServerNonceTag, "SERVER_NONCE");
  vector<QuicTag> reject_reasons;
  reject_reasons.push_back(CLIENT_NONCE_INVALID_FAILURE);
  rej->SetVector(kRREJ, reject_reasons);
}

TEST(QuicCryptoClientConfigTest, ProcessReject) {
  CryptoHandshakeMessage rej;
  FillInDummyReject(&rej, /* stateless */ false);

  // Now process the rejection.
  QuicCryptoClientConfig::CachedState cached;
  QuicCryptoNegotiatedParameters out_params;
  string error;
  QuicCryptoClientConfig config;
  EXPECT_EQ(QUIC_NO_ERROR, config.ProcessRejection(
                               rej, QuicWallTime::FromUNIXSeconds(0), &cached,
                               true,  // is_https
                               &out_params, &error));
  EXPECT_FALSE(cached.has_server_designated_connection_id());
  EXPECT_FALSE(cached.has_server_nonce());
}

TEST(QuicCryptoClientConfigTest, ProcessStatelessReject) {
  // Create a dummy reject message and mark it as stateless.
  CryptoHandshakeMessage rej;
  FillInDummyReject(&rej, /* stateless */ true);
  const QuicConnectionId kConnectionId = 0xdeadbeef;
  const string server_nonce = "SERVER_NONCE";
  rej.SetValue(kRCID, kConnectionId);
  rej.SetStringPiece(kServerNonceTag, server_nonce);

  // Now process the rejection.
  QuicCryptoClientConfig::CachedState cached;
  QuicCryptoNegotiatedParameters out_params;
  string error;
  QuicCryptoClientConfig config;
  EXPECT_EQ(QUIC_NO_ERROR, config.ProcessRejection(
                               rej, QuicWallTime::FromUNIXSeconds(0), &cached,
                               true,  // is_https
                               &out_params, &error));
  EXPECT_TRUE(cached.has_server_designated_connection_id());
  EXPECT_EQ(kConnectionId, cached.GetNextServerDesignatedConnectionId());
  EXPECT_EQ(server_nonce, cached.GetNextServerNonce());
}

TEST(QuicCryptoClientConfigTest, BadlyFormattedStatelessReject) {
  // Create a dummy reject message and mark it as stateless.  Do not
  // add an server-designated connection-id.
  CryptoHandshakeMessage rej;
  FillInDummyReject(&rej, /* stateless */ true);

  // Now process the rejection.
  QuicCryptoClientConfig::CachedState cached;
  QuicCryptoNegotiatedParameters out_params;
  string error;
  QuicCryptoClientConfig config;
  EXPECT_EQ(
      QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND,
      config.ProcessRejection(rej, QuicWallTime::FromUNIXSeconds(0), &cached,
                              true,  // is_https
                              &out_params, &error));
  EXPECT_FALSE(cached.has_server_designated_connection_id());
  EXPECT_EQ("Missing kRCID", error);
}

}  // namespace test
}  // namespace net
