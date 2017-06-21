// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/mock_crypto_client_stream.h"

#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/quic_client_session_base.h"
#include "net/quic/quic_server_id.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace net {

MockCryptoClientStream::MockCryptoClientStream(
    const QuicServerId& server_id,
    QuicClientSessionBase* session,
    ProofVerifyContext* verify_context,
    QuicCryptoClientConfig* crypto_config,
    HandshakeMode handshake_mode,
    const ProofVerifyDetails* proof_verify_details)
    : QuicCryptoClientStream(server_id, session, verify_context, crypto_config),
      handshake_mode_(handshake_mode),
      proof_verify_details_(proof_verify_details) {
}

MockCryptoClientStream::~MockCryptoClientStream() {
}

void MockCryptoClientStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  CloseConnection(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE);
}

void MockCryptoClientStream::CryptoConnect() {
  switch (handshake_mode_) {
    case ZERO_RTT: {
      encryption_established_ = true;
      handshake_confirmed_ = false;
      session()->connection()->SetDecrypter(ENCRYPTION_INITIAL,
                                            QuicDecrypter::Create(kNULL));
      session()->OnCryptoHandshakeEvent(
          QuicSession::ENCRYPTION_FIRST_ESTABLISHED);
      break;
    }

    case CONFIRM_HANDSHAKE: {
      encryption_established_ = true;
      handshake_confirmed_ = true;
      crypto_negotiated_params_.key_exchange = kC255;
      crypto_negotiated_params_.aead = kAESG;
      if (proof_verify_details_) {
        client_session()->OnProofVerifyDetailsAvailable(*proof_verify_details_);
      }
      SetConfigNegotiated();
      session()->connection()->SetDecrypter(ENCRYPTION_FORWARD_SECURE,
                                            QuicDecrypter::Create(kNULL));
      session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
      break;
    }

    case COLD_START: {
      handshake_confirmed_ = false;
      encryption_established_ = false;
      break;
    }
  }
}

void MockCryptoClientStream::SendOnCryptoHandshakeEvent(
    QuicSession::CryptoHandshakeEvent event) {
  encryption_established_ = true;
  if (event == QuicSession::HANDSHAKE_CONFIRMED) {
    handshake_confirmed_ = true;
    SetConfigNegotiated();
  }
  session()->OnCryptoHandshakeEvent(event);
}

void MockCryptoClientStream::SetConfigNegotiated() {
  ASSERT_FALSE(session()->config()->negotiated());
  QuicTagVector cgst;
  // TODO(rtenneti): Enable the following code after BBR code is checked in.
#if 0
  cgst.push_back(kTBBR);
#endif
  cgst.push_back(kQBIC);
  QuicConfig config;
  config.SetIdleConnectionStateLifetime(
      QuicTime::Delta::FromSeconds(2 * kMaximumIdleTimeoutSecs),
      QuicTime::Delta::FromSeconds(kMaximumIdleTimeoutSecs));
  config.SetMaxStreamsPerConnection(kDefaultMaxStreamsPerConnection / 2,
                                    kDefaultMaxStreamsPerConnection / 2);
  config.SetBytesForConnectionIdToSend(PACKET_8BYTE_CONNECTION_ID);

  CryptoHandshakeMessage msg;
  config.ToHandshakeMessage(&msg);
  string error_details;
  const QuicErrorCode error =
      session()->config()->ProcessPeerHello(msg, CLIENT, &error_details);
  ASSERT_EQ(QUIC_NO_ERROR, error);
  ASSERT_TRUE(session()->config()->negotiated());
  session()->OnConfigNegotiated();
}

QuicClientSessionBase* MockCryptoClientStream::client_session() {
  return reinterpret_cast<QuicClientSessionBase*>(session());
}

}  // namespace net
