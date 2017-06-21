// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_platform_key.h"

#include <windows.h>
#include <NCrypt.h>

#include <algorithm>
#include <string>
#include <vector>

#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/win/windows_version.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_capi_types.h"
#include "crypto/wincrypt_shim.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/scoped_openssl_types.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/threaded_ssl_private_key.h"

namespace net {

namespace {

using NCryptFreeObjectFunc = SECURITY_STATUS(WINAPI*)(NCRYPT_HANDLE);
using NCryptSignHashFunc = SECURITY_STATUS(WINAPI*)(NCRYPT_KEY_HANDLE,  // hKey
                                                    VOID*,   // pPaddingInfo
                                                    BYTE*,   // pbHashValue
                                                    DWORD,   // cbHashValue
                                                    BYTE*,   // pbSignature
                                                    DWORD,   // cbSignature
                                                    DWORD*,  // pcbResult
                                                    DWORD);  // dwFlags

class CNGFunctions {
 public:
  CNGFunctions() : ncrypt_free_object_(nullptr), ncrypt_sign_hash_(nullptr) {
    HMODULE ncrypt = GetModuleHandle(L"ncrypt.dll");
    if (ncrypt != nullptr) {
      ncrypt_free_object_ = reinterpret_cast<NCryptFreeObjectFunc>(
          GetProcAddress(ncrypt, "NCryptFreeObject"));
      ncrypt_sign_hash_ = reinterpret_cast<NCryptSignHashFunc>(
          GetProcAddress(ncrypt, "NCryptSignHash"));
    }
  }

  NCryptFreeObjectFunc ncrypt_free_object() const {
    return ncrypt_free_object_;
  }

  NCryptSignHashFunc ncrypt_sign_hash() const { return ncrypt_sign_hash_; }

 private:
  NCryptFreeObjectFunc ncrypt_free_object_;
  NCryptSignHashFunc ncrypt_sign_hash_;
};

base::LazyInstance<CNGFunctions>::Leaky g_cng_functions =
    LAZY_INSTANCE_INITIALIZER;

class SSLPlatformKeyCAPI : public ThreadedSSLPrivateKey::Delegate {
 public:
  // Takes ownership of |provider|.
  SSLPlatformKeyCAPI(HCRYPTPROV provider, DWORD key_spec, size_t max_length)
      : provider_(provider), key_spec_(key_spec), max_length_(max_length) {}

  ~SSLPlatformKeyCAPI() override {}

  SSLPrivateKey::Type GetType() override { return SSLPrivateKey::Type::RSA; }

  bool SupportsHash(SSLPrivateKey::Hash hash) override {
    // If the key is in CAPI, assume conservatively that the CAPI service
    // provider may only be able to sign pre-TLS-1.2 and SHA-1 hashes.
    return hash == SSLPrivateKey::Hash::MD5_SHA1 ||
           hash == SSLPrivateKey::Hash::SHA1;
  }

  size_t GetMaxSignatureLengthInBytes() override { return max_length_; }

  Error SignDigest(SSLPrivateKey::Hash hash,
                   const base::StringPiece& input,
                   std::vector<uint8_t>* signature) override {
    ALG_ID hash_alg = 0;
    switch (hash) {
      case SSLPrivateKey::Hash::MD5_SHA1:
        hash_alg = CALG_SSL3_SHAMD5;
        break;
      case SSLPrivateKey::Hash::SHA1:
        hash_alg = CALG_SHA1;
        break;
      case SSLPrivateKey::Hash::SHA256:
        hash_alg = CALG_SHA_256;
        break;
      case SSLPrivateKey::Hash::SHA384:
        hash_alg = CALG_SHA_384;
        break;
      case SSLPrivateKey::Hash::SHA512:
        hash_alg = CALG_SHA_512;
        break;
    }
    DCHECK_NE(static_cast<ALG_ID>(0), hash_alg);

    crypto::ScopedHCRYPTHASH hash_handle;
    if (!CryptCreateHash(provider_, hash_alg, 0, 0, hash_handle.receive())) {
      PLOG(ERROR) << "CreateCreateHash failed";
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    DWORD hash_len;
    DWORD arg_len = sizeof(hash_len);
    if (!CryptGetHashParam(hash_handle.get(), HP_HASHSIZE,
                           reinterpret_cast<BYTE*>(&hash_len), &arg_len, 0)) {
      PLOG(ERROR) << "CryptGetHashParam HP_HASHSIZE failed";
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    if (hash_len != input.size())
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    if (!CryptSetHashParam(
            hash_handle.get(), HP_HASHVAL,
            const_cast<BYTE*>(reinterpret_cast<const BYTE*>(input.data())),
            0)) {
      PLOG(ERROR) << "CryptSetHashParam HP_HASHVAL failed";
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    DWORD signature_len = 0;
    if (!CryptSignHash(hash_handle.get(), key_spec_, nullptr, 0, nullptr,
                       &signature_len)) {
      PLOG(ERROR) << "CryptSignHash failed";
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    signature->resize(signature_len);
    if (!CryptSignHash(hash_handle.get(), key_spec_, nullptr, 0,
                       vector_as_array(signature), &signature_len)) {
      PLOG(ERROR) << "CryptSignHash failed";
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    signature->resize(signature_len);

    // CryptoAPI signs in little-endian, so reverse it.
    std::reverse(signature->begin(), signature->end());
    return OK;
  }

 private:
  crypto::ScopedHCRYPTPROV provider_;
  DWORD key_spec_;
  size_t max_length_;

  DISALLOW_COPY_AND_ASSIGN(SSLPlatformKeyCAPI);
};

class SSLPlatformKeyCNG : public ThreadedSSLPrivateKey::Delegate {
 public:
  // Takes ownership of |key|.
  SSLPlatformKeyCNG(NCRYPT_KEY_HANDLE key,
                    SSLPrivateKey::Type type,
                    size_t max_length)
      : key_(key), type_(type), max_length_(max_length) {}

  ~SSLPlatformKeyCNG() override {
    g_cng_functions.Get().ncrypt_free_object()(key_);
  }

  SSLPrivateKey::Type GetType() override { return type_; }

  bool SupportsHash(SSLPrivateKey::Hash hash) override {
    // If the key is a 1024-bit RSA, assume conservatively that it may only be
    // able to sign SHA-1 hashes. This is the case for older Estonian ID cards
    // that have 1024-bit RSA keys. (For an RSA key, the maximum signature
    // length is the size of the modulus in bytes.)
    //
    // CNG does provide NCryptIsAlgSupported and NCryptEnumAlgorithms functions,
    // however they seem to both return NTE_NOT_SUPPORTED when querying the
    // NCRYPT_PROV_HANDLE at the key's NCRYPT_PROVIDER_HANDLE_PROPERTY.
    if (type_ == SSLPrivateKey::Type::RSA && max_length_ <= 1024 / 8) {
      return hash == SSLPrivateKey::Hash::MD5_SHA1 ||
             hash == SSLPrivateKey::Hash::SHA1;
    }
    return true;
  }

  size_t GetMaxSignatureLengthInBytes() override { return max_length_; }

  Error SignDigest(SSLPrivateKey::Hash hash,
                   const base::StringPiece& input,
                   std::vector<uint8_t>* signature) override {
    crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

    BCRYPT_PKCS1_PADDING_INFO rsa_padding_info = {0};
    void* padding_info = nullptr;
    DWORD flags = 0;
    if (type_ == SSLPrivateKey::Type::RSA) {
      switch (hash) {
        case SSLPrivateKey::Hash::MD5_SHA1:
          rsa_padding_info.pszAlgId = nullptr;
          break;
        case SSLPrivateKey::Hash::SHA1:
          rsa_padding_info.pszAlgId = BCRYPT_SHA1_ALGORITHM;
          break;
        case SSLPrivateKey::Hash::SHA256:
          rsa_padding_info.pszAlgId = BCRYPT_SHA256_ALGORITHM;
          break;
        case SSLPrivateKey::Hash::SHA384:
          rsa_padding_info.pszAlgId = BCRYPT_SHA384_ALGORITHM;
          break;
        case SSLPrivateKey::Hash::SHA512:
          rsa_padding_info.pszAlgId = BCRYPT_SHA512_ALGORITHM;
          break;
      }
      padding_info = &rsa_padding_info;
      flags |= BCRYPT_PAD_PKCS1;
    }

    DWORD signature_len;
    SECURITY_STATUS status = g_cng_functions.Get().ncrypt_sign_hash()(
        key_, padding_info,
        const_cast<BYTE*>(reinterpret_cast<const BYTE*>(input.data())),
        input.size(), nullptr, 0, &signature_len, flags);
    if (FAILED(status)) {
      LOG(ERROR) << "NCryptSignHash failed: " << status;
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    signature->resize(signature_len);
    status = g_cng_functions.Get().ncrypt_sign_hash()(
        key_, padding_info,
        const_cast<BYTE*>(reinterpret_cast<const BYTE*>(input.data())),
        input.size(), vector_as_array(signature), signature_len, &signature_len,
        flags);
    if (FAILED(status)) {
      LOG(ERROR) << "NCryptSignHash failed: " << status;
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    signature->resize(signature_len);

    // CNG emits raw ECDSA signatures, but BoringSSL expects a DER-encoded
    // ECDSA-Sig-Value.
    if (type_ == SSLPrivateKey::Type::ECDSA) {
      if (signature->size() % 2 != 0) {
        LOG(ERROR) << "Bad signature length";
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      }
      size_t order_len = signature->size() / 2;

      // Convert the RAW ECDSA signature to a DER-encoded ECDSA-Sig-Value.
      crypto::ScopedECDSA_SIG sig(ECDSA_SIG_new());
      if (!sig || !BN_bin2bn(vector_as_array(signature), order_len, sig->r) ||
          !BN_bin2bn(vector_as_array(signature) + order_len, order_len,
                     sig->s)) {
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      }

      int len = i2d_ECDSA_SIG(sig.get(), nullptr);
      if (len <= 0)
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      signature->resize(len);
      uint8_t* ptr = vector_as_array(signature);
      len = i2d_ECDSA_SIG(sig.get(), &ptr);
      if (len <= 0)
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      signature->resize(len);
    }

    return OK;
  }

 private:
  NCRYPT_KEY_HANDLE key_;
  SSLPrivateKey::Type type_;
  size_t max_length_;

  DISALLOW_COPY_AND_ASSIGN(SSLPlatformKeyCNG);
};

// Determines the key type and maximum signature length of |certificate|'s
// public key.
bool GetKeyInfo(const X509Certificate* certificate,
                SSLPrivateKey::Type* out_type,
                size_t* out_max_length) {
  crypto::OpenSSLErrStackTracer tracker(FROM_HERE);

  std::string der_encoded;
  if (!X509Certificate::GetDEREncoded(certificate->os_cert_handle(),
                                      &der_encoded))
    return false;
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(der_encoded.data());
  ScopedX509 x509(d2i_X509(nullptr, &bytes, der_encoded.size()));
  if (!x509)
    return false;
  crypto::ScopedEVP_PKEY key(X509_get_pubkey(x509.get()));
  if (!key)
    return false;
  switch (EVP_PKEY_id(key.get())) {
    case EVP_PKEY_RSA:
      *out_type = SSLPrivateKey::Type::RSA;
      break;
    case EVP_PKEY_EC:
      *out_type = SSLPrivateKey::Type::ECDSA;
      break;
    default:
      return false;
  }
  *out_max_length = EVP_PKEY_size(key.get());
  return true;
}

}  // namespace

scoped_ptr<SSLPrivateKey> FetchClientCertPrivateKey(
    X509Certificate* certificate,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  // Rather than query the private key for metadata, extract the public key from
  // the certificate without using Windows APIs. CAPI and CNG do not
  // consistently work depending on the system. See https://crbug.com/468345.
  SSLPrivateKey::Type key_type;
  size_t max_length;
  if (!GetKeyInfo(certificate, &key_type, &max_length))
    return nullptr;

  PCCERT_CONTEXT cert_context = certificate->os_cert_handle();

  HCRYPTPROV_OR_NCRYPT_KEY_HANDLE prov_or_key = 0;
  DWORD key_spec = 0;
  BOOL must_free = FALSE;
  DWORD flags = 0;
  if (base::win::GetVersion() >= base::win::VERSION_VISTA)
    flags |= CRYPT_ACQUIRE_PREFER_NCRYPT_KEY_FLAG;

  if (!CryptAcquireCertificatePrivateKey(cert_context, flags, nullptr,
                                         &prov_or_key, &key_spec, &must_free)) {
    PLOG(WARNING) << "Could not acquire private key";
    return nullptr;
  }

  // Should never get a cached handle back - ownership must always be
  // transferred.
  CHECK_EQ(must_free, TRUE);

  scoped_ptr<ThreadedSSLPrivateKey::Delegate> delegate;
  if (key_spec == CERT_NCRYPT_KEY_SPEC) {
    delegate.reset(new SSLPlatformKeyCNG(prov_or_key, key_type, max_length));
  } else {
    DCHECK(SSLPrivateKey::Type::RSA == key_type);
    delegate.reset(new SSLPlatformKeyCAPI(prov_or_key, key_spec, max_length));
  }
  return make_scoped_ptr(
      new ThreadedSSLPrivateKey(delegate.Pass(), task_runner.Pass()));
}

}  // namespace net
