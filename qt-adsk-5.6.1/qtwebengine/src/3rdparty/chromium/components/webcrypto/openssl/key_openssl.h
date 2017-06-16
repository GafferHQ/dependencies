// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_OPENSSL_KEY_OPENSSL_H_
#define COMPONENTS_WEBCRYPTO_OPENSSL_KEY_OPENSSL_H_

#include <openssl/ossl_typ.h>
#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "crypto/scoped_openssl_types.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

namespace webcrypto {

class CryptoData;
class AsymKeyOpenSsl;
class SymKeyOpenSsl;

// Base key class for all OpenSSL keys, used to safely cast between types. Each
// key maintains a copy of its serialized form in either 'raw', 'pkcs8', or
// 'spki' format. This is to allow structured cloning of keys synchronously from
// the target Blink thread without having to lock access to the key.
class KeyOpenSsl : public blink::WebCryptoKeyHandle {
 public:
  explicit KeyOpenSsl(const CryptoData& serialized_key_data);
  ~KeyOpenSsl() override;

  virtual SymKeyOpenSsl* AsSymKey();
  virtual AsymKeyOpenSsl* AsAsymKey();

  const std::vector<uint8_t>& serialized_key_data() const {
    return serialized_key_data_;
  }

 private:
  const std::vector<uint8_t> serialized_key_data_;
};

class SymKeyOpenSsl : public KeyOpenSsl {
 public:
  ~SymKeyOpenSsl() override;
  explicit SymKeyOpenSsl(const CryptoData& raw_key_data);

  static SymKeyOpenSsl* Cast(const blink::WebCryptoKey& key);

  SymKeyOpenSsl* AsSymKey() override;

  const std::vector<uint8_t>& raw_key_data() const {
    return serialized_key_data();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SymKeyOpenSsl);
};

class AsymKeyOpenSsl : public KeyOpenSsl {
 public:
  ~AsymKeyOpenSsl() override;
  AsymKeyOpenSsl(crypto::ScopedEVP_PKEY key,
                 const CryptoData& serialized_key_data);

  static AsymKeyOpenSsl* Cast(const blink::WebCryptoKey& key);

  AsymKeyOpenSsl* AsAsymKey() override;

  EVP_PKEY* key() { return key_.get(); }

 private:
  crypto::ScopedEVP_PKEY key_;

  DISALLOW_COPY_AND_ASSIGN(AsymKeyOpenSsl);
};

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_OPENSSL_KEY_OPENSSL_H_
