// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_H_
#define NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_H_

#include <string>

#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_session.h"

namespace net {

class QuicServerId;

class MockCryptoClientStream : public QuicCryptoClientStream {
 public:
  // HandshakeMode enumerates the handshake mode MockCryptoClientStream should
  // mock in CryptoConnect.
  enum HandshakeMode {
    // CONFIRM_HANDSHAKE indicates that CryptoConnect will immediately confirm
    // the handshake and establish encryption.  This behavior will never happen
    // in the field, but is convenient for higher level tests.
    CONFIRM_HANDSHAKE,

    // ZERO_RTT indicates that CryptoConnect will establish encryption but will
    // not confirm the handshake.
    ZERO_RTT,

    // COLD_START indicates that CryptoConnect will neither establish encryption
    // nor confirm the handshake
    COLD_START,
  };

  MockCryptoClientStream(
      const QuicServerId& server_id,
      QuicClientSessionBase* session,
      ProofVerifyContext* verify_context,
      QuicCryptoClientConfig* crypto_config,
      HandshakeMode handshake_mode,
      const ProofVerifyDetails* proof_verify_details_);
  ~MockCryptoClientStream() override;

  // CryptoFramerVisitorInterface implementation.
  void OnHandshakeMessage(const CryptoHandshakeMessage& message) override;

  // QuicCryptoClientStream implementation.
  void CryptoConnect() override;

  // Invokes the sessions's CryptoHandshakeEvent method with the specified
  // event.
  void SendOnCryptoHandshakeEvent(QuicSession::CryptoHandshakeEvent event);

  HandshakeMode handshake_mode_;

 private:
  void SetConfigNegotiated();
  QuicClientSessionBase* client_session();

  const ProofVerifyDetails* proof_verify_details_;

  DISALLOW_COPY_AND_ASSIGN(MockCryptoClientStream);
};

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_H_
