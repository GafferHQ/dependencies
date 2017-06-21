// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/openssl/rsa_hashed_algorithm_openssl.h"
#include "components/webcrypto/openssl/rsa_sign_openssl.h"
#include "components/webcrypto/status.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace webcrypto {

namespace {

class RsaPssImplementation : public RsaHashedAlgorithm {
 public:
  RsaPssImplementation()
      : RsaHashedAlgorithm(blink::WebCryptoKeyUsageVerify,
                           blink::WebCryptoKeyUsageSign) {}

  const char* GetJwkAlgorithm(
      const blink::WebCryptoAlgorithmId hash) const override {
    switch (hash) {
      case blink::WebCryptoAlgorithmIdSha1:
        return "PS1";
      case blink::WebCryptoAlgorithmIdSha256:
        return "PS256";
      case blink::WebCryptoAlgorithmIdSha384:
        return "PS384";
      case blink::WebCryptoAlgorithmIdSha512:
        return "PS512";
      default:
        return NULL;
    }
  }

  Status Sign(const blink::WebCryptoAlgorithm& algorithm,
              const blink::WebCryptoKey& key,
              const CryptoData& data,
              std::vector<uint8_t>* buffer) const override {
    return RsaSign(key, algorithm.rsaPssParams()->saltLengthBytes(), data,
                   buffer);
  }

  Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                const blink::WebCryptoKey& key,
                const CryptoData& signature,
                const CryptoData& data,
                bool* signature_match) const override {
    return RsaVerify(key, algorithm.rsaPssParams()->saltLengthBytes(),
                     signature, data, signature_match);
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformRsaPssImplementation() {
  return new RsaPssImplementation;
}

}  // namespace webcrypto
