// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/crypto_test_utils.h"

#include "net/quic/crypto/channel_id.h"
#include "net/quic/crypto/common_cert_set.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_crypto_server_stream.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/quic_server_id.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_framer_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_quic_framer.h"

using base::StringPiece;
using std::make_pair;
using std::pair;
using std::string;
using std::vector;

namespace net {
namespace test {

namespace {

const char kServerHostname[] = "test.example.com";
const uint16 kServerPort = 80;

// CryptoFramerVisitor is a framer visitor that records handshake messages.
class CryptoFramerVisitor : public CryptoFramerVisitorInterface {
 public:
  CryptoFramerVisitor()
      : error_(false) {
  }

  void OnError(CryptoFramer* framer) override { error_ = true; }

  void OnHandshakeMessage(const CryptoHandshakeMessage& message) override {
    messages_.push_back(message);
  }

  bool error() const {
    return error_;
  }

  const vector<CryptoHandshakeMessage>& messages() const {
    return messages_;
  }

 private:
  bool error_;
  vector<CryptoHandshakeMessage> messages_;
};

// MovePackets parses crypto handshake messages from packet number
// |*inout_packet_index| through to the last packet (or until a packet fails to
// decrypt) and has |dest_stream| process them. |*inout_packet_index| is updated
// with an index one greater than the last packet processed.
void MovePackets(PacketSavingConnection* source_conn,
                 size_t *inout_packet_index,
                 QuicCryptoStream* dest_stream,
                 PacketSavingConnection* dest_conn) {
  SimpleQuicFramer framer(source_conn->supported_versions());
  CryptoFramer crypto_framer;
  CryptoFramerVisitor crypto_visitor;

  // In order to properly test the code we need to perform encryption and
  // decryption so that the crypters latch when expected. The crypters are in
  // |dest_conn|, but we don't want to try and use them there. Instead we swap
  // them into |framer|, perform the decryption with them, and then swap them
  // back.
  QuicConnectionPeer::SwapCrypters(dest_conn, framer.framer());

  crypto_framer.set_visitor(&crypto_visitor);

  size_t index = *inout_packet_index;
  for (; index < source_conn->encrypted_packets_.size(); index++) {
    if (!framer.ProcessPacket(*source_conn->encrypted_packets_[index])) {
      // The framer will be unable to decrypt forward-secure packets sent after
      // the handshake is complete. Don't treat them as handshake packets.
      break;
    }

    for (const QuicStreamFrame& stream_frame : framer.stream_frames()) {
      ASSERT_TRUE(crypto_framer.ProcessInput(stream_frame.data));
      ASSERT_FALSE(crypto_visitor.error());
    }
  }
  *inout_packet_index = index;

  QuicConnectionPeer::SwapCrypters(dest_conn, framer.framer());

  ASSERT_EQ(0u, crypto_framer.InputBytesRemaining());

  for (const CryptoHandshakeMessage& message : crypto_visitor.messages()) {
    dest_stream->OnHandshakeMessage(message);
  }
}

// HexChar parses |c| as a hex character. If valid, it sets |*value| to the
// value of the hex character and returns true. Otherwise it returns false.
bool HexChar(char c, uint8* value) {
  if (c >= '0' && c <= '9') {
    *value = c - '0';
    return true;
  }
  if (c >= 'a' && c <= 'f') {
    *value = c - 'a' + 10;
    return true;
  }
  if (c >= 'A' && c <= 'F') {
    *value = c - 'A' + 10;
    return true;
  }
  return false;
}

// A ChannelIDSource that works in asynchronous mode unless the |callback|
// argument to GetChannelIDKey is nullptr.
class AsyncTestChannelIDSource : public ChannelIDSource,
                                 public CryptoTestUtils::CallbackSource {
 public:
  // Takes ownership of |sync_source|, a synchronous ChannelIDSource.
  explicit AsyncTestChannelIDSource(ChannelIDSource* sync_source)
      : sync_source_(sync_source) {}
  ~AsyncTestChannelIDSource() override {}

  // ChannelIDSource implementation.
  QuicAsyncStatus GetChannelIDKey(const string& hostname,
                                  scoped_ptr<ChannelIDKey>* channel_id_key,
                                  ChannelIDSourceCallback* callback) override {
    // Synchronous mode.
    if (!callback) {
      return sync_source_->GetChannelIDKey(hostname, channel_id_key, nullptr);
    }

    // Asynchronous mode.
    QuicAsyncStatus status =
        sync_source_->GetChannelIDKey(hostname, &channel_id_key_, nullptr);
    if (status != QUIC_SUCCESS) {
      return QUIC_FAILURE;
    }
    callback_.reset(callback);
    return QUIC_PENDING;
  }

  // CallbackSource implementation.
  void RunPendingCallbacks() override {
    if (callback_.get()) {
      callback_->Run(&channel_id_key_);
      callback_.reset();
    }
  }

 private:
  scoped_ptr<ChannelIDSource> sync_source_;
  scoped_ptr<ChannelIDSourceCallback> callback_;
  scoped_ptr<ChannelIDKey> channel_id_key_;
};

}  // anonymous namespace

CryptoTestUtils::FakeClientOptions::FakeClientOptions()
    : dont_verify_certs(false),
      channel_id_enabled(false),
      channel_id_source_async(false) {
}

// static
int CryptoTestUtils::HandshakeWithFakeServer(
    PacketSavingConnection* client_conn,
    QuicCryptoClientStream* client) {
  PacketSavingConnection* server_conn = new PacketSavingConnection(
      Perspective::IS_SERVER, client_conn->supported_versions());

  QuicConfig config = DefaultQuicConfig();
  QuicCryptoServerConfig crypto_config(QuicCryptoServerConfig::TESTING,
                                       QuicRandom::GetInstance());
  SetupCryptoServerConfigForTest(server_conn->clock(),
                                 server_conn->random_generator(), &config,
                                 &crypto_config);

  TestQuicSpdyServerSession server_session(server_conn, config, &crypto_config);

  // The client's handshake must have been started already.
  CHECK_NE(0u, client_conn->encrypted_packets_.size());

  CommunicateHandshakeMessages(client_conn, client, server_conn,
                               server_session.GetCryptoStream());

  CompareClientAndServerKeys(client, server_session.GetCryptoStream());

  return client->num_sent_client_hellos();
}

// static
int CryptoTestUtils::HandshakeWithFakeClient(
    PacketSavingConnection* server_conn,
    QuicCryptoServerStream* server,
    const FakeClientOptions& options) {
  PacketSavingConnection* client_conn =
      new PacketSavingConnection(Perspective::IS_CLIENT);
  // Advance the time, because timers do not like uninitialized times.
  client_conn->AdvanceTime(QuicTime::Delta::FromSeconds(1));

  QuicCryptoClientConfig crypto_config;
  bool is_https = false;
  AsyncTestChannelIDSource* async_channel_id_source = nullptr;
  if (options.channel_id_enabled) {
    is_https = true;

    ChannelIDSource* source = ChannelIDSourceForTesting();
    if (options.channel_id_source_async) {
      async_channel_id_source = new AsyncTestChannelIDSource(source);
      source = async_channel_id_source;
    }
    crypto_config.SetChannelIDSource(source);
  }
  QuicServerId server_id(kServerHostname, kServerPort, is_https,
                         PRIVACY_MODE_DISABLED);
  if (!options.dont_verify_certs) {
    // TODO(wtc): replace this with ProofVerifierForTesting() when we have
    // a working ProofSourceForTesting().
    crypto_config.SetProofVerifier(FakeProofVerifierForTesting());
  }
  TestQuicSpdyClientSession client_session(client_conn, DefaultQuicConfig(),
                                           server_id, &crypto_config);

  client_session.GetCryptoStream()->CryptoConnect();
  CHECK_EQ(1u, client_conn->encrypted_packets_.size());

  CommunicateHandshakeMessagesAndRunCallbacks(
      client_conn, client_session.GetCryptoStream(), server_conn, server,
      async_channel_id_source);

  CompareClientAndServerKeys(client_session.GetCryptoStream(), server);

  if (options.channel_id_enabled) {
    scoped_ptr<ChannelIDKey> channel_id_key;
    QuicAsyncStatus status = crypto_config.channel_id_source()->GetChannelIDKey(
        kServerHostname, &channel_id_key, nullptr);
    EXPECT_EQ(QUIC_SUCCESS, status);
    EXPECT_EQ(channel_id_key->SerializeKey(),
              server->crypto_negotiated_params().channel_id);
    EXPECT_EQ(
        options.channel_id_source_async,
        client_session.GetCryptoStream()->WasChannelIDSourceCallbackRun());
  }

  return client_session.GetCryptoStream()->num_sent_client_hellos();
}

// static
void CryptoTestUtils::SetupCryptoServerConfigForTest(
    const QuicClock* clock,
    QuicRandom* rand,
    QuicConfig* config,
    QuicCryptoServerConfig* crypto_config) {
  QuicCryptoServerConfig::ConfigOptions options;
  options.channel_id_enabled = true;
  scoped_ptr<CryptoHandshakeMessage> scfg(
      crypto_config->AddDefaultConfig(rand, clock, options));
}

// static
void CryptoTestUtils::CommunicateHandshakeMessages(
    PacketSavingConnection* a_conn,
    QuicCryptoStream* a,
    PacketSavingConnection* b_conn,
    QuicCryptoStream* b) {
  CommunicateHandshakeMessagesAndRunCallbacks(a_conn, a, b_conn, b, nullptr);
}

// static
void CryptoTestUtils::CommunicateHandshakeMessagesAndRunCallbacks(
    PacketSavingConnection* a_conn,
    QuicCryptoStream* a,
    PacketSavingConnection* b_conn,
    QuicCryptoStream* b,
    CallbackSource* callback_source) {
  size_t a_i = 0, b_i = 0;
  while (!a->handshake_confirmed()) {
    ASSERT_GT(a_conn->encrypted_packets_.size(), a_i);
    VLOG(1) << "Processing " << a_conn->encrypted_packets_.size() - a_i
            << " packets a->b";
    MovePackets(a_conn, &a_i, b, b_conn);
    if (callback_source) {
      callback_source->RunPendingCallbacks();
    }

    ASSERT_GT(b_conn->encrypted_packets_.size(), b_i);
    VLOG(1) << "Processing " << b_conn->encrypted_packets_.size() - b_i
            << " packets b->a";
    MovePackets(b_conn, &b_i, a, a_conn);
    if (callback_source) {
      callback_source->RunPendingCallbacks();
    }
  }
}

// static
pair<size_t, size_t> CryptoTestUtils::AdvanceHandshake(
    PacketSavingConnection* a_conn,
    QuicCryptoStream* a,
    size_t a_i,
    PacketSavingConnection* b_conn,
    QuicCryptoStream* b,
    size_t b_i) {
  VLOG(1) << "Processing " << a_conn->encrypted_packets_.size() - a_i
          << " packets a->b";
  MovePackets(a_conn, &a_i, b, b_conn);

  VLOG(1) << "Processing " << b_conn->encrypted_packets_.size() - b_i
          << " packets b->a";
  if (b_conn->encrypted_packets_.size() - b_i == 2) {
    VLOG(1) << "here";
  }
  MovePackets(b_conn, &b_i, a, a_conn);

  return std::make_pair(a_i, b_i);
}

// static
string CryptoTestUtils::GetValueForTag(const CryptoHandshakeMessage& message,
                                       QuicTag tag) {
  QuicTagValueMap::const_iterator it = message.tag_value_map().find(tag);
  if (it == message.tag_value_map().end()) {
    return string();
  }
  return it->second;
}

class MockCommonCertSets : public CommonCertSets {
 public:
  MockCommonCertSets(StringPiece cert, uint64 hash, uint32 index)
      : cert_(cert.as_string()),
        hash_(hash),
        index_(index) {
  }

  StringPiece GetCommonHashes() const override {
    CHECK(false) << "not implemented";
    return StringPiece();
  }

  StringPiece GetCert(uint64 hash, uint32 index) const override {
    if (hash == hash_ && index == index_) {
      return cert_;
    }
    return StringPiece();
  }

  bool MatchCert(StringPiece cert,
                 StringPiece common_set_hashes,
                 uint64* out_hash,
                 uint32* out_index) const override {
    if (cert != cert_) {
      return false;
    }

    if (common_set_hashes.size() % sizeof(uint64) != 0) {
      return false;
    }
    bool client_has_set = false;
    for (size_t i = 0; i < common_set_hashes.size(); i += sizeof(uint64)) {
      uint64 hash;
      memcpy(&hash, common_set_hashes.data() + i, sizeof(hash));
      if (hash == hash_) {
        client_has_set = true;
        break;
      }
    }

    if (!client_has_set) {
      return false;
    }

    *out_hash = hash_;
    *out_index = index_;
    return true;
  }

 private:
  const string cert_;
  const uint64 hash_;
  const uint32 index_;
};

CommonCertSets* CryptoTestUtils::MockCommonCertSets(StringPiece cert,
                                                    uint64 hash,
                                                    uint32 index) {
  return new class MockCommonCertSets(cert, hash, index);
}

void CryptoTestUtils::CompareClientAndServerKeys(
    QuicCryptoClientStream* client,
    QuicCryptoServerStream* server) {
  QuicFramer* client_framer =
      QuicConnectionPeer::GetFramer(client->session()->connection());
  QuicFramer* server_framer =
      QuicConnectionPeer::GetFramer(server->session()->connection());
  const QuicEncrypter* client_encrypter(
      QuicFramerPeer::GetEncrypter(client_framer, ENCRYPTION_INITIAL));
  const QuicDecrypter* client_decrypter(
      client->session()->connection()->decrypter());
  const QuicEncrypter* client_forward_secure_encrypter(
      QuicFramerPeer::GetEncrypter(client_framer, ENCRYPTION_FORWARD_SECURE));
  const QuicDecrypter* client_forward_secure_decrypter(
      client->session()->connection()->alternative_decrypter());
  const QuicEncrypter* server_encrypter(
      QuicFramerPeer::GetEncrypter(server_framer, ENCRYPTION_INITIAL));
  const QuicDecrypter* server_decrypter(
      server->session()->connection()->decrypter());
  const QuicEncrypter* server_forward_secure_encrypter(
      QuicFramerPeer::GetEncrypter(server_framer, ENCRYPTION_FORWARD_SECURE));
  const QuicDecrypter* server_forward_secure_decrypter(
      server->session()->connection()->alternative_decrypter());

  StringPiece client_encrypter_key = client_encrypter->GetKey();
  StringPiece client_encrypter_iv = client_encrypter->GetNoncePrefix();
  StringPiece client_decrypter_key = client_decrypter->GetKey();
  StringPiece client_decrypter_iv = client_decrypter->GetNoncePrefix();
  StringPiece client_forward_secure_encrypter_key =
      client_forward_secure_encrypter->GetKey();
  StringPiece client_forward_secure_encrypter_iv =
      client_forward_secure_encrypter->GetNoncePrefix();
  StringPiece client_forward_secure_decrypter_key =
      client_forward_secure_decrypter->GetKey();
  StringPiece client_forward_secure_decrypter_iv =
      client_forward_secure_decrypter->GetNoncePrefix();
  StringPiece server_encrypter_key = server_encrypter->GetKey();
  StringPiece server_encrypter_iv = server_encrypter->GetNoncePrefix();
  StringPiece server_decrypter_key = server_decrypter->GetKey();
  StringPiece server_decrypter_iv = server_decrypter->GetNoncePrefix();
  StringPiece server_forward_secure_encrypter_key =
      server_forward_secure_encrypter->GetKey();
  StringPiece server_forward_secure_encrypter_iv =
      server_forward_secure_encrypter->GetNoncePrefix();
  StringPiece server_forward_secure_decrypter_key =
      server_forward_secure_decrypter->GetKey();
  StringPiece server_forward_secure_decrypter_iv =
      server_forward_secure_decrypter->GetNoncePrefix();

  StringPiece client_subkey_secret =
      client->crypto_negotiated_params().subkey_secret;
  StringPiece server_subkey_secret =
      server->crypto_negotiated_params().subkey_secret;


  const char kSampleLabel[] = "label";
  const char kSampleContext[] = "context";
  const size_t kSampleOutputLength = 32;
  string client_key_extraction;
  string server_key_extraction;
  EXPECT_TRUE(client->ExportKeyingMaterial(kSampleLabel,
                                           kSampleContext,
                                           kSampleOutputLength,
                                           &client_key_extraction));
  EXPECT_TRUE(server->ExportKeyingMaterial(kSampleLabel,
                                           kSampleContext,
                                           kSampleOutputLength,
                                           &server_key_extraction));

  CompareCharArraysWithHexError("client write key",
                                client_encrypter_key.data(),
                                client_encrypter_key.length(),
                                server_decrypter_key.data(),
                                server_decrypter_key.length());
  CompareCharArraysWithHexError("client write IV",
                                client_encrypter_iv.data(),
                                client_encrypter_iv.length(),
                                server_decrypter_iv.data(),
                                server_decrypter_iv.length());
  CompareCharArraysWithHexError("server write key",
                                server_encrypter_key.data(),
                                server_encrypter_key.length(),
                                client_decrypter_key.data(),
                                client_decrypter_key.length());
  CompareCharArraysWithHexError("server write IV",
                                server_encrypter_iv.data(),
                                server_encrypter_iv.length(),
                                client_decrypter_iv.data(),
                                client_decrypter_iv.length());
  CompareCharArraysWithHexError("client forward secure write key",
                                client_forward_secure_encrypter_key.data(),
                                client_forward_secure_encrypter_key.length(),
                                server_forward_secure_decrypter_key.data(),
                                server_forward_secure_decrypter_key.length());
  CompareCharArraysWithHexError("client forward secure write IV",
                                client_forward_secure_encrypter_iv.data(),
                                client_forward_secure_encrypter_iv.length(),
                                server_forward_secure_decrypter_iv.data(),
                                server_forward_secure_decrypter_iv.length());
  CompareCharArraysWithHexError("server forward secure write key",
                                server_forward_secure_encrypter_key.data(),
                                server_forward_secure_encrypter_key.length(),
                                client_forward_secure_decrypter_key.data(),
                                client_forward_secure_decrypter_key.length());
  CompareCharArraysWithHexError("server forward secure write IV",
                                server_forward_secure_encrypter_iv.data(),
                                server_forward_secure_encrypter_iv.length(),
                                client_forward_secure_decrypter_iv.data(),
                                client_forward_secure_decrypter_iv.length());
  CompareCharArraysWithHexError("subkey secret",
                                client_subkey_secret.data(),
                                client_subkey_secret.length(),
                                server_subkey_secret.data(),
                                server_subkey_secret.length());
  CompareCharArraysWithHexError("sample key extraction",
                                client_key_extraction.data(),
                                client_key_extraction.length(),
                                server_key_extraction.data(),
                                server_key_extraction.length());
}

// static
QuicTag CryptoTestUtils::ParseTag(const char* tagstr) {
  const size_t len = strlen(tagstr);
  CHECK_NE(0u, len);

  QuicTag tag = 0;

  if (tagstr[0] == '#') {
    CHECK_EQ(static_cast<size_t>(1 + 2*4), len);
    tagstr++;

    for (size_t i = 0; i < 8; i++) {
      tag <<= 4;

      uint8 v = 0;
      CHECK(HexChar(tagstr[i], &v));
      tag |= v;
    }

    return tag;
  }

  CHECK_LE(len, 4u);
  for (size_t i = 0; i < 4; i++) {
    tag >>= 8;
    if (i < len) {
      tag |= static_cast<uint32>(tagstr[i]) << 24;
    }
  }

  return tag;
}

// static
CryptoHandshakeMessage CryptoTestUtils::Message(const char* message_tag, ...) {
  va_list ap;
  va_start(ap, message_tag);

  CryptoHandshakeMessage message = BuildMessage(message_tag, ap);
  va_end(ap);
  return message;
}

// static
CryptoHandshakeMessage CryptoTestUtils::BuildMessage(const char* message_tag,
                                                     va_list ap) {
  CryptoHandshakeMessage msg;
  msg.set_tag(ParseTag(message_tag));

  for (;;) {
    const char* tagstr = va_arg(ap, const char*);
    if (tagstr == nullptr) {
      break;
    }

    if (tagstr[0] == '$') {
      // Special value.
      const char* const special = tagstr + 1;
      if (strcmp(special, "padding") == 0) {
        const int min_bytes = va_arg(ap, int);
        msg.set_minimum_size(min_bytes);
      } else {
        CHECK(false) << "Unknown special value: " << special;
      }

      continue;
    }

    const QuicTag tag = ParseTag(tagstr);
    const char* valuestr = va_arg(ap, const char*);

    size_t len = strlen(valuestr);
    if (len > 0 && valuestr[0] == '#') {
      valuestr++;
      len--;

      CHECK_EQ(0u, len % 2);
      scoped_ptr<uint8[]> buf(new uint8[len/2]);

      for (size_t i = 0; i < len/2; i++) {
        uint8 v = 0;
        CHECK(HexChar(valuestr[i*2], &v));
        buf[i] = v << 4;
        CHECK(HexChar(valuestr[i*2 + 1], &v));
        buf[i] |= v;
      }

      msg.SetStringPiece(
          tag, StringPiece(reinterpret_cast<char*>(buf.get()), len/2));
      continue;
    }

    msg.SetStringPiece(tag, valuestr);
  }

  // The CryptoHandshakeMessage needs to be serialized and parsed to ensure
  // that any padding is included.
  scoped_ptr<QuicData> bytes(CryptoFramer::ConstructHandshakeMessage(msg));
  scoped_ptr<CryptoHandshakeMessage> parsed(
      CryptoFramer::ParseMessage(bytes->AsStringPiece()));
  CHECK(parsed.get());

  return *parsed;
}

}  // namespace test
}  // namespace net
