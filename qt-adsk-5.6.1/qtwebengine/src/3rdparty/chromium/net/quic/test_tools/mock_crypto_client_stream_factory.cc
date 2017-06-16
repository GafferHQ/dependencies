// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"

#include "base/lazy_instance.h"
#include "net/quic/quic_client_session.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_server_id.h"

using std::string;

namespace net {

MockCryptoClientStreamFactory::~MockCryptoClientStreamFactory() {
}

MockCryptoClientStreamFactory::MockCryptoClientStreamFactory()
    : handshake_mode_(MockCryptoClientStream::CONFIRM_HANDSHAKE),
      last_stream_(nullptr) {
}

QuicCryptoClientStream*
MockCryptoClientStreamFactory::CreateQuicCryptoClientStream(
    const QuicServerId& server_id,
    QuicClientSession* session,
    QuicCryptoClientConfig* crypto_config) {
  const ProofVerifyDetails* proof_verify_details = nullptr;
  if (!proof_verify_details_queue_.empty()) {
    proof_verify_details = proof_verify_details_queue_.front();
    proof_verify_details_queue_.pop();
  }
  last_stream_ =
      new MockCryptoClientStream(server_id, session, nullptr, crypto_config,
                                 handshake_mode_, proof_verify_details);
  return last_stream_;
}

}  // namespace net
