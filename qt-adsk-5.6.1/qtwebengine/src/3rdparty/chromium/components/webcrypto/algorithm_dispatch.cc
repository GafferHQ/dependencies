// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithm_dispatch.h"

#include "base/logging.h"
#include "components/webcrypto/algorithm_implementation.h"
#include "components/webcrypto/algorithm_registry.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/platform_crypto.h"
#include "components/webcrypto/status.h"
#include "components/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace webcrypto {

namespace {

Status DecryptDontCheckKeyUsage(const blink::WebCryptoAlgorithm& algorithm,
                                const blink::WebCryptoKey& key,
                                const CryptoData& data,
                                std::vector<uint8_t>* buffer) {
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  return impl->Decrypt(algorithm, key, data, buffer);
}

Status EncryptDontCheckUsage(const blink::WebCryptoAlgorithm& algorithm,
                             const blink::WebCryptoKey& key,
                             const CryptoData& data,
                             std::vector<uint8_t>* buffer) {
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  return impl->Encrypt(algorithm, key, data, buffer);
}

Status ExportKeyDontCheckExtractability(blink::WebCryptoKeyFormat format,
                                        const blink::WebCryptoKey& key,
                                        std::vector<uint8_t>* buffer) {
  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(key.algorithm().id(), &impl);
  if (status.IsError())
    return status;

  return impl->ExportKey(format, key, buffer);
}

}  // namespace

Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               const CryptoData& data,
               std::vector<uint8_t>* buffer) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageEncrypt))
    return Status::ErrorUnexpected();
  return EncryptDontCheckUsage(algorithm, key, data, buffer);
}

Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               const CryptoData& data,
               std::vector<uint8_t>* buffer) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageDecrypt))
    return Status::ErrorUnexpected();
  return DecryptDontCheckKeyUsage(algorithm, key, data, buffer);
}

Status Digest(const blink::WebCryptoAlgorithm& algorithm,
              const CryptoData& data,
              std::vector<uint8_t>* buffer) {
  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  return impl->Digest(algorithm, data, buffer);
}

Status GenerateKey(const blink::WebCryptoAlgorithm& algorithm,
                   bool extractable,
                   blink::WebCryptoKeyUsageMask usages,
                   GenerateKeyResult* result) {
  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  status = impl->GenerateKey(algorithm, extractable, usages, result);
  if (status.IsError())
    return status;

  const blink::WebCryptoKey* key = NULL;
  if (result->type() == GenerateKeyResult::TYPE_SECRET_KEY)
    key = &result->secret_key();
  if (result->type() == GenerateKeyResult::TYPE_PUBLIC_PRIVATE_KEY_PAIR)
    key = &result->private_key();
  if (key == NULL)
    return Status::ErrorUnexpected();

  // This should only fail if an algorithm is implemented incorrectly and
  // does not do its own check of the usages.
  if (key->usages() == 0) {
    DCHECK(false) << "Key usages for generateKey() must not be empty";
    return Status::ErrorCreateKeyEmptyUsages();
  }
  return status;
}

Status ImportKey(blink::WebCryptoKeyFormat format,
                 const CryptoData& key_data,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoKey* key) {
  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  status = impl->VerifyKeyUsagesBeforeImportKey(format, usages);
  if (status.IsError())
    return status;

  return impl->ImportKey(format, key_data, algorithm, extractable, usages, key);
}

Status ExportKey(blink::WebCryptoKeyFormat format,
                 const blink::WebCryptoKey& key,
                 std::vector<uint8_t>* buffer) {
  if (!key.extractable())
    return Status::ErrorKeyNotExtractable();
  return ExportKeyDontCheckExtractability(format, key, buffer);
}

Status Sign(const blink::WebCryptoAlgorithm& algorithm,
            const blink::WebCryptoKey& key,
            const CryptoData& data,
            std::vector<uint8_t>* buffer) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageSign))
    return Status::ErrorUnexpected();
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  return impl->Sign(algorithm, key, data, buffer);
}

Status Verify(const blink::WebCryptoAlgorithm& algorithm,
              const blink::WebCryptoKey& key,
              const CryptoData& signature,
              const CryptoData& data,
              bool* signature_match) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageVerify))
    return Status::ErrorUnexpected();
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  return impl->Verify(algorithm, key, signature, data, signature_match);
}

Status WrapKey(blink::WebCryptoKeyFormat format,
               const blink::WebCryptoKey& key_to_wrap,
               const blink::WebCryptoKey& wrapping_key,
               const blink::WebCryptoAlgorithm& wrapping_algorithm,
               std::vector<uint8_t>* buffer) {
  if (!KeyUsageAllows(wrapping_key, blink::WebCryptoKeyUsageWrapKey))
    return Status::ErrorUnexpected();

  std::vector<uint8_t> exported_data;
  Status status = ExportKey(format, key_to_wrap, &exported_data);
  if (status.IsError())
    return status;
  return EncryptDontCheckUsage(wrapping_algorithm, wrapping_key,
                               CryptoData(exported_data), buffer);
}

Status UnwrapKey(blink::WebCryptoKeyFormat format,
                 const CryptoData& wrapped_key_data,
                 const blink::WebCryptoKey& wrapping_key,
                 const blink::WebCryptoAlgorithm& wrapping_algorithm,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoKey* key) {
  if (!KeyUsageAllows(wrapping_key, blink::WebCryptoKeyUsageUnwrapKey))
    return Status::ErrorUnexpected();
  if (wrapping_algorithm.id() != wrapping_key.algorithm().id())
    return Status::ErrorUnexpected();

  // Fail fast if the import is doomed to fail.
  const AlgorithmImplementation* import_impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &import_impl);
  if (status.IsError())
    return status;

  status = import_impl->VerifyKeyUsagesBeforeImportKey(format, usages);
  if (status.IsError())
    return status;

  std::vector<uint8_t> buffer;
  status = DecryptDontCheckKeyUsage(wrapping_algorithm, wrapping_key,
                                    wrapped_key_data, &buffer);
  if (status.IsError())
    return status;

  // NOTE that returning the details of ImportKey() failures may leak
  // information about the plaintext of the encrypted key (for instance the JWK
  // key_ops). As long as the ImportKey error messages don't describe actual
  // key bytes however this should be OK. For more discussion see
  // http://crubg.com/372040
  return ImportKey(format, CryptoData(buffer), algorithm, extractable, usages,
                   key);
}

Status DeriveBits(const blink::WebCryptoAlgorithm& algorithm,
                  const blink::WebCryptoKey& base_key,
                  unsigned int length_bits,
                  std::vector<uint8_t>* derived_bytes) {
  if (!KeyUsageAllows(base_key, blink::WebCryptoKeyUsageDeriveBits))
    return Status::ErrorUnexpected();

  if (algorithm.id() != base_key.algorithm().id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  return impl->DeriveBits(algorithm, base_key, true, length_bits,
                          derived_bytes);
}

Status DeriveKey(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& base_key,
                 const blink::WebCryptoAlgorithm& import_algorithm,
                 const blink::WebCryptoAlgorithm& key_length_algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoKey* derived_key) {
  if (!KeyUsageAllows(base_key, blink::WebCryptoKeyUsageDeriveKey))
    return Status::ErrorUnexpected();

  if (algorithm.id() != base_key.algorithm().id())
    return Status::ErrorUnexpected();

  if (import_algorithm.id() != key_length_algorithm.id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* import_impl = NULL;
  Status status =
      GetAlgorithmImplementation(import_algorithm.id(), &import_impl);
  if (status.IsError())
    return status;

  // Fail fast if the requested key usages are incorect.
  status = import_impl->VerifyKeyUsagesBeforeImportKey(
      blink::WebCryptoKeyFormatRaw, usages);
  if (status.IsError())
    return status;

  // Determine how many bits long the derived key should be.
  unsigned int length_bits = 0;
  bool has_length_bits = false;
  status = import_impl->GetKeyLength(key_length_algorithm, &has_length_bits,
                                     &length_bits);
  if (status.IsError())
    return status;

  // Derive the key bytes.
  const AlgorithmImplementation* derive_impl = NULL;
  status = GetAlgorithmImplementation(algorithm.id(), &derive_impl);
  if (status.IsError())
    return status;

  std::vector<uint8_t> derived_bytes;
  status = derive_impl->DeriveBits(algorithm, base_key, has_length_bits,
                                   length_bits, &derived_bytes);
  if (status.IsError())
    return status;

  // Create the key using the derived bytes.
  return ImportKey(blink::WebCryptoKeyFormatRaw, CryptoData(derived_bytes),
                   import_algorithm, extractable, usages, derived_key);
}

scoped_ptr<blink::WebCryptoDigestor> CreateDigestor(
    blink::WebCryptoAlgorithmId algorithm) {
  PlatformInit();
  return CreatePlatformDigestor(algorithm);
}

bool SerializeKeyForClone(const blink::WebCryptoKey& key,
                          blink::WebVector<uint8_t>* key_data) {
  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(key.algorithm().id(), &impl);
  if (status.IsError())
    return false;

  status = impl->SerializeKeyForClone(key, key_data);
  return status.IsSuccess();
}

bool DeserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                            blink::WebCryptoKeyType type,
                            bool extractable,
                            blink::WebCryptoKeyUsageMask usages,
                            const CryptoData& key_data,
                            blink::WebCryptoKey* key) {
  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return false;

  status = impl->DeserializeKeyForClone(algorithm, type, extractable, usages,
                                        key_data, key);
  return status.IsSuccess();
}

}  // namespace webcrypto
