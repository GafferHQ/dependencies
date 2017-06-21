// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_certificate.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/sha1.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "crypto/capi_util.h"
#include "crypto/scoped_capi_types.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"

// Implement CalculateChainFingerprint() with our native crypto library.
#if defined(USE_OPENSSL)
#include <openssl/sha.h>
#else
#include <blapi.h>
#endif

#pragma comment(lib, "crypt32.lib")

using base::Time;

namespace net {

namespace {

typedef crypto::ScopedCAPIHandle<
    HCERTSTORE,
    crypto::CAPIDestroyerWithFlags<HCERTSTORE,
                                   CertCloseStore, 0> > ScopedHCERTSTORE;

void ExplodedTimeToSystemTime(const base::Time::Exploded& exploded,
                              SYSTEMTIME* system_time) {
  system_time->wYear = static_cast<WORD>(exploded.year);
  system_time->wMonth = static_cast<WORD>(exploded.month);
  system_time->wDayOfWeek = static_cast<WORD>(exploded.day_of_week);
  system_time->wDay = static_cast<WORD>(exploded.day_of_month);
  system_time->wHour = static_cast<WORD>(exploded.hour);
  system_time->wMinute = static_cast<WORD>(exploded.minute);
  system_time->wSecond = static_cast<WORD>(exploded.second);
  system_time->wMilliseconds = static_cast<WORD>(exploded.millisecond);
}

//-----------------------------------------------------------------------------

// Decodes the cert's subjectAltName extension into a CERT_ALT_NAME_INFO
// structure and stores it in *output.
void GetCertSubjectAltName(
    PCCERT_CONTEXT cert,
    scoped_ptr<CERT_ALT_NAME_INFO, base::FreeDeleter>* output) {
  PCERT_EXTENSION extension = CertFindExtension(szOID_SUBJECT_ALT_NAME2,
                                                cert->pCertInfo->cExtension,
                                                cert->pCertInfo->rgExtension);
  if (!extension)
    return;

  CRYPT_DECODE_PARA decode_para;
  decode_para.cbSize = sizeof(decode_para);
  decode_para.pfnAlloc = crypto::CryptAlloc;
  decode_para.pfnFree = crypto::CryptFree;
  CERT_ALT_NAME_INFO* alt_name_info = NULL;
  DWORD alt_name_info_size = 0;
  BOOL rv;
  rv = CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           szOID_SUBJECT_ALT_NAME2,
                           extension->Value.pbData,
                           extension->Value.cbData,
                           CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG,
                           &decode_para,
                           &alt_name_info,
                           &alt_name_info_size);
  if (rv)
    output->reset(alt_name_info);
}

void AddCertsFromStore(HCERTSTORE store,
                       X509Certificate::OSCertHandles* results) {
  PCCERT_CONTEXT cert = NULL;

  while ((cert = CertEnumCertificatesInStore(store, cert)) != NULL) {
    PCCERT_CONTEXT to_add = NULL;
    if (CertAddCertificateContextToStore(
        NULL,  // The cert won't be persisted in any cert store. This breaks
               // any association the context currently has to |store|, which
               // allows us, the caller, to safely close |store| without
               // releasing the cert handles.
        cert,
        CERT_STORE_ADD_USE_EXISTING,
        &to_add) && to_add != NULL) {
      // When processing stores generated from PKCS#7/PKCS#12 files, it
      // appears that the order returned is the inverse of the order that it
      // appeared in the file.
      // TODO(rsleevi): Ensure this order is consistent across all Win
      // versions
      results->insert(results->begin(), to_add);
    }
  }
}

X509Certificate::OSCertHandles ParsePKCS7(const char* data, size_t length) {
  X509Certificate::OSCertHandles results;
  CERT_BLOB data_blob;
  data_blob.cbData = length;
  data_blob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(data));

  HCERTSTORE out_store = NULL;

  DWORD expected_types = CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED |
                         CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED |
                         CERT_QUERY_CONTENT_FLAG_PKCS7_UNSIGNED;

  if (!CryptQueryObject(CERT_QUERY_OBJECT_BLOB, &data_blob, expected_types,
                        CERT_QUERY_FORMAT_FLAG_BINARY, 0, NULL, NULL, NULL,
                        &out_store, NULL, NULL) || out_store == NULL) {
    return results;
  }

  AddCertsFromStore(out_store, &results);
  CertCloseStore(out_store, CERT_CLOSE_STORE_CHECK_FLAG);

  return results;
}

// Given a CERT_NAME_BLOB, returns true if it appears in a given list,
// formatted as a vector of strings holding DER-encoded X.509
// DistinguishedName entries.
bool IsCertNameBlobInIssuerList(
    CERT_NAME_BLOB* name_blob,
    const std::vector<std::string>& issuer_names) {
  for (std::vector<std::string>::const_iterator it = issuer_names.begin();
       it != issuer_names.end(); ++it) {
    CERT_NAME_BLOB issuer_blob;
    issuer_blob.pbData =
        reinterpret_cast<BYTE*>(const_cast<char*>(it->data()));
    issuer_blob.cbData = static_cast<DWORD>(it->length());

    BOOL rb = CertCompareCertificateName(
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, &issuer_blob, name_blob);
    if (rb)
      return true;
  }
  return false;
}

}  // namespace

void X509Certificate::Initialize() {
  DCHECK(cert_handle_);
  subject_.ParseDistinguishedName(cert_handle_->pCertInfo->Subject.pbData,
                                  cert_handle_->pCertInfo->Subject.cbData);
  issuer_.ParseDistinguishedName(cert_handle_->pCertInfo->Issuer.pbData,
                                 cert_handle_->pCertInfo->Issuer.cbData);

  valid_start_ = Time::FromFileTime(cert_handle_->pCertInfo->NotBefore);
  valid_expiry_ = Time::FromFileTime(cert_handle_->pCertInfo->NotAfter);

  fingerprint_ = CalculateFingerprint(cert_handle_);
  ca_fingerprint_ = CalculateCAFingerprint(intermediate_ca_certs_);

  const CRYPT_INTEGER_BLOB* serial = &cert_handle_->pCertInfo->SerialNumber;
  scoped_ptr<uint8_t[]> serial_bytes(new uint8_t[serial->cbData]);
  for (unsigned i = 0; i < serial->cbData; i++)
    serial_bytes[i] = serial->pbData[serial->cbData - i - 1];
  serial_number_ = std::string(
      reinterpret_cast<char*>(serial_bytes.get()), serial->cbData);
}

void X509Certificate::GetSubjectAltName(
    std::vector<std::string>* dns_names,
    std::vector<std::string>* ip_addrs) const {
  if (dns_names)
    dns_names->clear();
  if (ip_addrs)
    ip_addrs->clear();

  if (!cert_handle_)
    return;

  scoped_ptr<CERT_ALT_NAME_INFO, base::FreeDeleter> alt_name_info;
  GetCertSubjectAltName(cert_handle_, &alt_name_info);
  CERT_ALT_NAME_INFO* alt_name = alt_name_info.get();
  if (alt_name) {
    int num_entries = alt_name->cAltEntry;
    for (int i = 0; i < num_entries; i++) {
      // dNSName is an ASN.1 IA5String representing a string of ASCII
      // characters, so we can use UTF16ToASCII here.
      const CERT_ALT_NAME_ENTRY& entry = alt_name->rgAltEntry[i];

      if (dns_names && entry.dwAltNameChoice == CERT_ALT_NAME_DNS_NAME) {
        dns_names->push_back(base::UTF16ToASCII(entry.pwszDNSName));
      } else if (ip_addrs &&
                 entry.dwAltNameChoice == CERT_ALT_NAME_IP_ADDRESS) {
        ip_addrs->push_back(std::string(
            reinterpret_cast<const char*>(entry.IPAddress.pbData),
            entry.IPAddress.cbData));
      }
    }
  }
}

PCCERT_CONTEXT X509Certificate::CreateOSCertChainForCert() const {
  // Create an in-memory certificate store to hold this certificate and
  // any intermediate certificates in |intermediate_ca_certs_|. The store
  // will be referenced in the returned PCCERT_CONTEXT, and will not be freed
  // until the PCCERT_CONTEXT is freed.
  ScopedHCERTSTORE store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, 0, NULL,
      CERT_STORE_DEFER_CLOSE_UNTIL_LAST_FREE_FLAG, NULL));
  if (!store.get())
    return NULL;

  // NOTE: This preserves all of the properties of |os_cert_handle()| except
  // for CERT_KEY_PROV_HANDLE_PROP_ID and CERT_KEY_CONTEXT_PROP_ID - the two
  // properties that hold access to already-opened private keys. If a handle
  // has already been unlocked (eg: PIN prompt), then the first time that the
  // identity is used for client auth, it may prompt the user again.
  PCCERT_CONTEXT primary_cert;
  BOOL ok = CertAddCertificateContextToStore(store.get(), os_cert_handle(),
                                             CERT_STORE_ADD_ALWAYS,
                                             &primary_cert);
  if (!ok || !primary_cert)
    return NULL;

  for (size_t i = 0; i < intermediate_ca_certs_.size(); ++i) {
    CertAddCertificateContextToStore(store.get(), intermediate_ca_certs_[i],
                                     CERT_STORE_ADD_ALWAYS, NULL);
  }

  // Note: |store| is explicitly not released, as the call to CertCloseStore()
  // when |store| goes out of scope will not actually free the store. Instead,
  // the store will be freed when |primary_cert| is freed.
  return primary_cert;
}

// static
bool X509Certificate::GetDEREncoded(X509Certificate::OSCertHandle cert_handle,
                                    std::string* encoded) {
  if (!cert_handle || !cert_handle->pbCertEncoded ||
      !cert_handle->cbCertEncoded) {
    return false;
  }
  encoded->assign(reinterpret_cast<char*>(cert_handle->pbCertEncoded),
                  cert_handle->cbCertEncoded);
  return true;
}

// static
bool X509Certificate::IsSameOSCert(X509Certificate::OSCertHandle a,
                                   X509Certificate::OSCertHandle b) {
  DCHECK(a && b);
  if (a == b)
    return true;
  return a->cbCertEncoded == b->cbCertEncoded &&
      memcmp(a->pbCertEncoded, b->pbCertEncoded, a->cbCertEncoded) == 0;
}

// static
X509Certificate::OSCertHandle X509Certificate::CreateOSCertHandleFromBytes(
    const char* data, int length) {
  OSCertHandle cert_handle = NULL;
  if (!CertAddEncodedCertificateToStore(
      NULL, X509_ASN_ENCODING, reinterpret_cast<const BYTE*>(data),
      length, CERT_STORE_ADD_USE_EXISTING, &cert_handle))
    return NULL;

  return cert_handle;
}

X509Certificate::OSCertHandles X509Certificate::CreateOSCertHandlesFromBytes(
    const char* data, int length, Format format) {
  OSCertHandles results;
  switch (format) {
    case FORMAT_SINGLE_CERTIFICATE: {
      OSCertHandle handle = CreateOSCertHandleFromBytes(data, length);
      if (handle != NULL)
        results.push_back(handle);
      break;
    }
    case FORMAT_PKCS7:
      results = ParsePKCS7(data, length);
      break;
    default:
      NOTREACHED() << "Certificate format " << format << " unimplemented";
      break;
  }

  return results;
}

// static
X509Certificate::OSCertHandle X509Certificate::DupOSCertHandle(
    OSCertHandle cert_handle) {
  return CertDuplicateCertificateContext(cert_handle);
}

// static
void X509Certificate::FreeOSCertHandle(OSCertHandle cert_handle) {
  CertFreeCertificateContext(cert_handle);
}

// static
SHA1HashValue X509Certificate::CalculateFingerprint(
    OSCertHandle cert) {
  DCHECK(NULL != cert->pbCertEncoded);
  DCHECK_NE(static_cast<DWORD>(0), cert->cbCertEncoded);

  BOOL rv;
  SHA1HashValue sha1;
  DWORD sha1_size = sizeof(sha1.data);
  rv = CryptHashCertificate(NULL, CALG_SHA1, 0, cert->pbCertEncoded,
                            cert->cbCertEncoded, sha1.data, &sha1_size);
  DCHECK(rv && sha1_size == sizeof(sha1.data));
  if (!rv)
    memset(sha1.data, 0, sizeof(sha1.data));
  return sha1;
}

// static
SHA256HashValue X509Certificate::CalculateFingerprint256(OSCertHandle cert) {
  DCHECK(NULL != cert->pbCertEncoded);
  DCHECK_NE(0u, cert->cbCertEncoded);

  SHA256HashValue sha256;
  size_t sha256_size = sizeof(sha256.data);

  // Use crypto::SHA256HashString for two reasons:
  // * < Windows Vista does not have universal SHA-256 support.
  // * More efficient on Windows > Vista (less overhead since non-default CSP
  // is not needed).
  base::StringPiece der_cert(reinterpret_cast<const char*>(cert->pbCertEncoded),
                             cert->cbCertEncoded);
  crypto::SHA256HashString(der_cert, sha256.data, sha256_size);
  return sha256;
}

SHA1HashValue X509Certificate::CalculateCAFingerprint(
    const OSCertHandles& intermediates) {
  SHA1HashValue sha1;
  memset(sha1.data, 0, sizeof(sha1.data));

#if defined(USE_OPENSSL)
  SHA_CTX ctx;
  if (!SHA1_Init(&ctx))
    return sha1;
  for (size_t i = 0; i < intermediates.size(); ++i) {
    PCCERT_CONTEXT ca_cert = intermediates[i];
    if (!SHA1_Update(&ctx, ca_cert->pbCertEncoded, ca_cert->cbCertEncoded))
      return sha1;
  }
  SHA1_Final(sha1.data, &ctx);
#else  // !USE_OPENSSL
  SHA1Context* sha1_ctx = SHA1_NewContext();
  if (!sha1_ctx)
    return sha1;
  SHA1_Begin(sha1_ctx);
  for (size_t i = 0; i < intermediates.size(); ++i) {
    PCCERT_CONTEXT ca_cert = intermediates[i];
    SHA1_Update(sha1_ctx, ca_cert->pbCertEncoded, ca_cert->cbCertEncoded);
  }
  unsigned int result_len;
  SHA1_End(sha1_ctx, sha1.data, &result_len, SHA1_LENGTH);
  SHA1_DestroyContext(sha1_ctx, PR_TRUE);
#endif  // USE_OPENSSL

  return sha1;
}

// static
X509Certificate::OSCertHandle X509Certificate::ReadOSCertHandleFromPickle(
    base::PickleIterator* pickle_iter) {
  const char* data;
  int length;
  if (!pickle_iter->ReadData(&data, &length))
    return NULL;

  // Legacy serialized certificates were serialized with extended attributes,
  // rather than as DER only. As a result, these serialized certificates are
  // not portable across platforms and may have side-effects on Windows due
  // to extended attributes being serialized/deserialized -
  // http://crbug.com/118706. To avoid deserializing these attributes, write
  // the deserialized cert into a temporary cert store and then create a new
  // cert from the DER - that is, without attributes.
  ScopedHCERTSTORE store(
      CertOpenStore(CERT_STORE_PROV_MEMORY, 0, NULL, 0, NULL));
  if (!store.get())
    return NULL;

  OSCertHandle cert_handle = NULL;
  if (!CertAddSerializedElementToStore(
          store.get(), reinterpret_cast<const BYTE*>(data), length,
          CERT_STORE_ADD_NEW, 0, CERT_STORE_CERTIFICATE_CONTEXT_FLAG,
          NULL, reinterpret_cast<const void **>(&cert_handle))) {
    return NULL;
  }

  std::string encoded;
  bool ok = GetDEREncoded(cert_handle, &encoded);
  FreeOSCertHandle(cert_handle);
  cert_handle = NULL;

  if (ok)
    cert_handle = CreateOSCertHandleFromBytes(encoded.data(), encoded.size());
  return cert_handle;
}

// static
bool X509Certificate::WriteOSCertHandleToPickle(OSCertHandle cert_handle,
                                                base::Pickle* pickle) {
  return pickle->WriteData(
      reinterpret_cast<char*>(cert_handle->pbCertEncoded),
      cert_handle->cbCertEncoded);
}

// static
void X509Certificate::GetPublicKeyInfo(OSCertHandle cert_handle,
                                       size_t* size_bits,
                                       PublicKeyType* type) {
  *type = kPublicKeyTypeUnknown;
  *size_bits = 0;

  PCCRYPT_OID_INFO oid_info = CryptFindOIDInfo(
      CRYPT_OID_INFO_OID_KEY,
      cert_handle->pCertInfo->SubjectPublicKeyInfo.Algorithm.pszObjId,
      CRYPT_PUBKEY_ALG_OID_GROUP_ID);
  if (!oid_info)
    return;

  CHECK_EQ(oid_info->dwGroupId,
           static_cast<DWORD>(CRYPT_PUBKEY_ALG_OID_GROUP_ID));

  *size_bits = CertGetPublicKeyLength(
      X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
      &cert_handle->pCertInfo->SubjectPublicKeyInfo);

  if (IS_SPECIAL_OID_INFO_ALGID(oid_info->Algid)) {
    // For an EC public key, oid_info->Algid is CALG_OID_INFO_PARAMETERS
    // (0xFFFFFFFE). Need to handle it as a special case.
    if (strcmp(oid_info->pszOID, szOID_ECC_PUBLIC_KEY) == 0) {
      *type = kPublicKeyTypeECDSA;
    } else {
      NOTREACHED();
    }
    return;
  }
  switch (oid_info->Algid) {
    case CALG_RSA_SIGN:
    case CALG_RSA_KEYX:
      *type = kPublicKeyTypeRSA;
      break;
    case CALG_DSS_SIGN:
      *type = kPublicKeyTypeDSA;
      break;
    case CALG_ECDSA:
      *type = kPublicKeyTypeECDSA;
      break;
    case CALG_ECDH:
      *type = kPublicKeyTypeECDH;
      break;
  }
}

bool X509Certificate::IsIssuedByEncoded(
    const std::vector<std::string>& valid_issuers) {

  // If the certificate's issuer in the list?
  if (IsCertNameBlobInIssuerList(&cert_handle_->pCertInfo->Issuer,
                                 valid_issuers)) {
    return true;
  }
  // Otherwise, is any of the intermediate CA subjects in the list?
  for (OSCertHandles::iterator it = intermediate_ca_certs_.begin();
       it != intermediate_ca_certs_.end(); ++it) {
    if (IsCertNameBlobInIssuerList(&(*it)->pCertInfo->Issuer,
                                   valid_issuers)) {
      return true;
    }
  }

  return false;
}

// static
bool X509Certificate::IsSelfSigned(OSCertHandle cert_handle) {
  return !!CryptVerifyCertificateSignatureEx(
      NULL,
      X509_ASN_ENCODING,
      CRYPT_VERIFY_CERT_SIGN_SUBJECT_CERT,
      reinterpret_cast<void*>(const_cast<PCERT_CONTEXT>(cert_handle)),
      CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT,
      reinterpret_cast<void*>(const_cast<PCERT_CONTEXT>(cert_handle)),
      0,
      NULL);
}

}  // namespace net
