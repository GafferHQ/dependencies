// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/nss/rsa_hashed_algorithm_nss.h"

#include <secasn1.h>

#include "base/logging.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/nss/key_nss.h"
#include "components/webcrypto/nss/util_nss.h"
#include "components/webcrypto/status.h"
#include "components/webcrypto/webcrypto_util.h"
#include "crypto/scoped_nss_types.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace webcrypto {

namespace {

#if defined(USE_NSS_CERTS) && !defined(OS_CHROMEOS)
Status ErrorRsaPrivateKeyImportNotSupported() {
  return Status::ErrorUnsupported(
      "NSS version must be at least 3.16.2 for RSA private key import. See "
      "http://crbug.com/380424");
}

// Prior to NSS 3.16.2 RSA key parameters were not validated. This is
// a security problem for RSA private key import from JWK which uses a
// CKA_ID based on the public modulus to retrieve the private key.
Status NssSupportsRsaPrivateKeyImport() {
  if (!NSS_VersionCheck("3.16.2"))
    return ErrorRsaPrivateKeyImportNotSupported();

  // Also ensure that the version of Softoken is 3.16.2 or later.
  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());
  CK_SLOT_INFO info = {};
  if (PK11_GetSlotInfo(slot.get(), &info) != SECSuccess)
    return ErrorRsaPrivateKeyImportNotSupported();

  // CK_SLOT_INFO.hardwareVersion contains the major.minor
  // version info for Softoken in the corresponding .major/.minor
  // fields, and .firmwareVersion contains the patch.build
  // version info (in the .major/.minor fields)
  if ((info.hardwareVersion.major > 3) ||
      (info.hardwareVersion.major == 3 &&
       (info.hardwareVersion.minor > 16 ||
        (info.hardwareVersion.minor == 16 &&
         info.firmwareVersion.major >= 2)))) {
    return Status::Success();
  }

  return ErrorRsaPrivateKeyImportNotSupported();
}
#else
Status NssSupportsRsaPrivateKeyImport() {
  return Status::Success();
}
#endif

bool CreateRsaHashedPublicKeyAlgorithm(
    blink::WebCryptoAlgorithmId rsa_algorithm,
    blink::WebCryptoAlgorithmId hash_algorithm,
    SECKEYPublicKey* key,
    blink::WebCryptoKeyAlgorithm* key_algorithm) {
  // TODO(eroman): What about other key types rsaPss, rsaOaep.
  if (!key || key->keyType != rsaKey)
    return false;

  unsigned int modulus_length_bits = SECKEY_PublicKeyStrength(key) * 8;
  CryptoData public_exponent(key->u.rsa.publicExponent.data,
                             key->u.rsa.publicExponent.len);

  *key_algorithm = blink::WebCryptoKeyAlgorithm::createRsaHashed(
      rsa_algorithm, modulus_length_bits, public_exponent.bytes(),
      public_exponent.byte_length(), hash_algorithm);
  return true;
}

bool CreateRsaHashedPrivateKeyAlgorithm(
    blink::WebCryptoAlgorithmId rsa_algorithm,
    blink::WebCryptoAlgorithmId hash_algorithm,
    SECKEYPrivateKey* key,
    blink::WebCryptoKeyAlgorithm* key_algorithm) {
  crypto::ScopedSECKEYPublicKey public_key(SECKEY_ConvertToPublicKey(key));
  if (!public_key)
    return false;
  return CreateRsaHashedPublicKeyAlgorithm(rsa_algorithm, hash_algorithm,
                                           public_key.get(), key_algorithm);
}

// From PKCS#1 [http://tools.ietf.org/html/rfc3447]:
//
//    RSAPrivateKey ::= SEQUENCE {
//      version           Version,
//      modulus           INTEGER,  -- n
//      publicExponent    INTEGER,  -- e
//      privateExponent   INTEGER,  -- d
//      prime1            INTEGER,  -- p
//      prime2            INTEGER,  -- q
//      exponent1         INTEGER,  -- d mod (p-1)
//      exponent2         INTEGER,  -- d mod (q-1)
//      coefficient       INTEGER,  -- (inverse of q) mod p
//      otherPrimeInfos   OtherPrimeInfos OPTIONAL
//    }
//
// Note that otherPrimeInfos is only applicable for version=1. Since NSS
// doesn't use multi-prime can safely use version=0.
struct RSAPrivateKey {
  SECItem version;
  SECItem modulus;
  SECItem public_exponent;
  SECItem private_exponent;
  SECItem prime1;
  SECItem prime2;
  SECItem exponent1;
  SECItem exponent2;
  SECItem coefficient;
};

// The system NSS library doesn't have the new PK11_ExportDERPrivateKeyInfo
// function yet (https://bugzilla.mozilla.org/show_bug.cgi?id=519255). So we
// provide a fallback implementation.
#if defined(USE_NSS_CERTS)
const SEC_ASN1Template RSAPrivateKeyTemplate[] = {
    {SEC_ASN1_SEQUENCE, 0, NULL, sizeof(RSAPrivateKey)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, version)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, modulus)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, public_exponent)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, private_exponent)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, prime1)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, prime2)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, exponent1)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, exponent2)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, coefficient)},
    {0}};
#endif  // defined(USE_NSS_CERTS)

// On success |value| will be filled with data which must be freed by
// SECITEM_FreeItem(value, PR_FALSE);
bool ReadUint(SECKEYPrivateKey* key,
              CK_ATTRIBUTE_TYPE attribute,
              SECItem* value) {
  SECStatus rv = PK11_ReadRawAttribute(PK11_TypePrivKey, key, attribute, value);

  // PK11_ReadRawAttribute() returns items of type siBuffer. However in order
  // for the ASN.1 encoding to be correct, the items must be of type
  // siUnsignedInteger.
  value->type = siUnsignedInteger;

  return rv == SECSuccess;
}

// Fills |out| with the RSA private key properties. Returns true on success.
// Regardless of the return value, the caller must invoke FreeRSAPrivateKey()
// to free up any allocated memory.
//
// The passed in RSAPrivateKey must be zero-initialized.
bool InitRSAPrivateKey(SECKEYPrivateKey* key, RSAPrivateKey* out) {
  if (key->keyType != rsaKey)
    return false;

  // Everything should be zero-ed out. These are just some spot checks.
  DCHECK(!out->version.data);
  DCHECK(!out->version.len);
  DCHECK(!out->modulus.data);
  DCHECK(!out->modulus.len);

  // Always use version=0 since not using multi-prime.
  if (!SEC_ASN1EncodeInteger(NULL, &out->version, 0))
    return false;

  if (!ReadUint(key, CKA_MODULUS, &out->modulus))
    return false;
  if (!ReadUint(key, CKA_PUBLIC_EXPONENT, &out->public_exponent))
    return false;
  if (!ReadUint(key, CKA_PRIVATE_EXPONENT, &out->private_exponent))
    return false;
  if (!ReadUint(key, CKA_PRIME_1, &out->prime1))
    return false;
  if (!ReadUint(key, CKA_PRIME_2, &out->prime2))
    return false;
  if (!ReadUint(key, CKA_EXPONENT_1, &out->exponent1))
    return false;
  if (!ReadUint(key, CKA_EXPONENT_2, &out->exponent2))
    return false;
  if (!ReadUint(key, CKA_COEFFICIENT, &out->coefficient))
    return false;

  return true;
}

struct FreeRsaPrivateKey {
  void operator()(RSAPrivateKey* out) {
    SECITEM_FreeItem(&out->version, PR_FALSE);
    SECITEM_FreeItem(&out->modulus, PR_FALSE);
    SECITEM_FreeItem(&out->public_exponent, PR_FALSE);
    SECITEM_FreeItem(&out->private_exponent, PR_FALSE);
    SECITEM_FreeItem(&out->prime1, PR_FALSE);
    SECITEM_FreeItem(&out->prime2, PR_FALSE);
    SECITEM_FreeItem(&out->exponent1, PR_FALSE);
    SECITEM_FreeItem(&out->exponent2, PR_FALSE);
    SECITEM_FreeItem(&out->coefficient, PR_FALSE);
  }
};

typedef scoped_ptr<CERTSubjectPublicKeyInfo,
                   crypto::NSSDestroyer<CERTSubjectPublicKeyInfo,
                                        SECKEY_DestroySubjectPublicKeyInfo>>
    ScopedCERTSubjectPublicKeyInfo;

struct DestroyGenericObject {
  void operator()(PK11GenericObject* o) const {
    if (o)
      PK11_DestroyGenericObject(o);
  }
};

typedef scoped_ptr<PK11GenericObject, DestroyGenericObject>
    ScopedPK11GenericObject;

// Helper to add an attribute to a template.
void AddAttribute(CK_ATTRIBUTE_TYPE type,
                  void* value,
                  unsigned long length,
                  std::vector<CK_ATTRIBUTE>* templ) {
  CK_ATTRIBUTE attribute = {type, value, length};
  templ->push_back(attribute);
}

void AddAttribute(CK_ATTRIBUTE_TYPE type,
                  const CryptoData& data,
                  std::vector<CK_ATTRIBUTE>* templ) {
  CK_ATTRIBUTE attribute = {
      type, const_cast<unsigned char*>(data.bytes()), data.byte_length()};
  templ->push_back(attribute);
}

void AddAttribute(CK_ATTRIBUTE_TYPE type,
                  const std::string& data,
                  std::vector<CK_ATTRIBUTE>* templ) {
  AddAttribute(type, CryptoData(data), templ);
}

Status ExportKeyPkcs8Nss(SECKEYPrivateKey* key, std::vector<uint8_t>* buffer) {
  if (key->keyType != rsaKey)
    return Status::ErrorUnsupported();

// TODO(rsleevi): Implement OAEP support according to the spec.

#if defined(USE_NSS_CERTS)
  // PK11_ExportDERPrivateKeyInfo isn't available. Use our fallback code.
  const SECOidTag algorithm = SEC_OID_PKCS1_RSA_ENCRYPTION;
  const int kPrivateKeyInfoVersion = 0;

  SECKEYPrivateKeyInfo private_key_info = {};
  RSAPrivateKey rsa_private_key = {};
  scoped_ptr<RSAPrivateKey, FreeRsaPrivateKey> free_private_key(
      &rsa_private_key);

  // http://crbug.com/366427: the spec does not define any other failures for
  // exporting, so none of the subsequent errors are spec compliant.
  if (!InitRSAPrivateKey(key, &rsa_private_key))
    return Status::OperationError();

  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena.get())
    return Status::OperationError();

  if (!SEC_ASN1EncodeItem(arena.get(), &private_key_info.privateKey,
                          &rsa_private_key, RSAPrivateKeyTemplate)) {
    return Status::OperationError();
  }

  if (SECSuccess != SECOID_SetAlgorithmID(arena.get(),
                                          &private_key_info.algorithm,
                                          algorithm, NULL)) {
    return Status::OperationError();
  }

  if (!SEC_ASN1EncodeInteger(arena.get(), &private_key_info.version,
                             kPrivateKeyInfoVersion)) {
    return Status::OperationError();
  }

  crypto::ScopedSECItem encoded_key(
      SEC_ASN1EncodeItem(NULL, NULL, &private_key_info,
                         SEC_ASN1_GET(SECKEY_PrivateKeyInfoTemplate)));
#else   // defined(USE_NSS_CERTS)
  crypto::ScopedSECItem encoded_key(PK11_ExportDERPrivateKeyInfo(key, NULL));
#endif  // defined(USE_NSS_CERTS)

  if (!encoded_key.get())
    return Status::OperationError();

  buffer->assign(encoded_key->data, encoded_key->data + encoded_key->len);
  return Status::Success();
}

Status ImportRsaPrivateKey(const blink::WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usages,
                           const JwkRsaInfo& params,
                           blink::WebCryptoKey* key) {
  Status status = NssSupportsRsaPrivateKeyImport();
  if (status.IsError())
    return status;

  CK_OBJECT_CLASS obj_class = CKO_PRIVATE_KEY;
  CK_KEY_TYPE key_type = CKK_RSA;
  CK_BBOOL ck_false = CK_FALSE;

  std::vector<CK_ATTRIBUTE> key_template;

  AddAttribute(CKA_CLASS, &obj_class, sizeof(obj_class), &key_template);
  AddAttribute(CKA_KEY_TYPE, &key_type, sizeof(key_type), &key_template);
  AddAttribute(CKA_TOKEN, &ck_false, sizeof(ck_false), &key_template);
  AddAttribute(CKA_SENSITIVE, &ck_false, sizeof(ck_false), &key_template);
  AddAttribute(CKA_PRIVATE, &ck_false, sizeof(ck_false), &key_template);

  // Required properties by JWA.
  AddAttribute(CKA_MODULUS, params.n, &key_template);
  AddAttribute(CKA_PUBLIC_EXPONENT, params.e, &key_template);
  AddAttribute(CKA_PRIVATE_EXPONENT, params.d, &key_template);

  // Manufacture a CKA_ID so the created key can be retrieved later as a
  // SECKEYPrivateKey using FindKeyByKeyID(). Unfortunately there isn't a more
  // direct way to do this in NSS.
  //
  // For consistency with other NSS key creation methods, set the CKA_ID to
  // PK11_MakeIDFromPubKey(). There are some problems with
  // this approach:
  //
  //  (1) Prior to NSS 3.16.2, there is no parameter validation when creating
  //      private keys. It is therefore possible to construct a key using the
  //      known public modulus, and where all the other parameters are bogus.
  //      FindKeyByKeyID() returns the first key matching the ID. So this would
  //      effectively allow an attacker to retrieve a private key of their
  //      choice.
  //
  //  (2) The ID space is shared by different key types. So theoretically
  //      possible to retrieve a key of the wrong type which has a matching
  //      CKA_ID. In practice I am told this is not likely except for small key
  //      sizes, since would require constructing keys with the same public
  //      data.
  //
  //  (3) FindKeyByKeyID() doesn't necessarily return the object that was just
  //      created by CreateGenericObject. If the pre-existing key was
  //      provisioned with flags incompatible with WebCrypto (for instance
  //      marked sensitive) then this will break things.
  SECItem modulus_item = MakeSECItemForBuffer(CryptoData(params.n));
  crypto::ScopedSECItem object_id(PK11_MakeIDFromPubKey(&modulus_item));
  AddAttribute(CKA_ID, CryptoData(object_id->data, object_id->len),
               &key_template);

  // Optional properties by JWA, however guaranteed to be present by Chromium's
  // implementation.
  AddAttribute(CKA_PRIME_1, params.p, &key_template);
  AddAttribute(CKA_PRIME_2, params.q, &key_template);
  AddAttribute(CKA_EXPONENT_1, params.dp, &key_template);
  AddAttribute(CKA_EXPONENT_2, params.dq, &key_template);
  AddAttribute(CKA_COEFFICIENT, params.qi, &key_template);

  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());

  ScopedPK11GenericObject key_object(PK11_CreateGenericObject(
      slot.get(), &key_template[0], key_template.size(), PR_FALSE));

  if (!key_object)
    return Status::OperationError();

  crypto::ScopedSECKEYPrivateKey private_key_tmp(
      PK11_FindKeyByKeyID(slot.get(), object_id.get(), NULL));

  // PK11_FindKeyByKeyID() may return a handle to an existing key, rather than
  // the object created by PK11_CreateGenericObject().
  crypto::ScopedSECKEYPrivateKey private_key(
      SECKEY_CopyPrivateKey(private_key_tmp.get()));

  if (!private_key)
    return Status::OperationError();

  blink::WebCryptoKeyAlgorithm key_algorithm;
  if (!CreateRsaHashedPrivateKeyAlgorithm(
          algorithm.id(), algorithm.rsaHashedImportParams()->hash().id(),
          private_key.get(), &key_algorithm)) {
    return Status::ErrorUnexpected();
  }

  std::vector<uint8_t> pkcs8_data;
  status = ExportKeyPkcs8Nss(private_key.get(), &pkcs8_data);
  if (status.IsError())
    return status;

  scoped_ptr<PrivateKeyNss> key_handle(
      new PrivateKeyNss(private_key.Pass(), CryptoData(pkcs8_data)));

  *key = blink::WebCryptoKey::create(key_handle.release(),
                                     blink::WebCryptoKeyTypePrivate,
                                     extractable, key_algorithm, usages);
  return Status::Success();
}

Status ExportKeySpkiNss(SECKEYPublicKey* key, std::vector<uint8_t>* buffer) {
  const crypto::ScopedSECItem spki_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(key));
  if (!spki_der)
    return Status::OperationError();

  buffer->assign(spki_der->data, spki_der->data + spki_der->len);
  return Status::Success();
}

Status ImportRsaPublicKey(const blink::WebCryptoAlgorithm& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usages,
                          const CryptoData& modulus_data,
                          const CryptoData& exponent_data,
                          blink::WebCryptoKey* key) {
  if (!modulus_data.byte_length())
    return Status::ErrorImportRsaEmptyModulus();

  if (!exponent_data.byte_length())
    return Status::ErrorImportRsaEmptyExponent();

  DCHECK(modulus_data.bytes());
  DCHECK(exponent_data.bytes());

  // NSS does not provide a way to create an RSA public key directly from the
  // modulus and exponent values, but it can import an DER-encoded ASN.1 blob
  // with these values and create the public key from that. The code below
  // follows the recommendation described in
  // https://developer.mozilla.org/en-US/docs/NSS/NSS_Tech_Notes/nss_tech_note7

  // Pack the input values into a struct compatible with NSS ASN.1 encoding, and
  // set up an ASN.1 encoder template for it.
  struct RsaPublicKeyData {
    SECItem modulus;
    SECItem exponent;
  };
  const RsaPublicKeyData pubkey_in = {
      {siUnsignedInteger,
       const_cast<unsigned char*>(modulus_data.bytes()),
       modulus_data.byte_length()},
      {siUnsignedInteger,
       const_cast<unsigned char*>(exponent_data.bytes()),
       exponent_data.byte_length()}};
  const SEC_ASN1Template rsa_public_key_template[] = {
      {SEC_ASN1_SEQUENCE, 0, NULL, sizeof(RsaPublicKeyData)},
      {
       SEC_ASN1_INTEGER, offsetof(RsaPublicKeyData, modulus),
      },
      {
       SEC_ASN1_INTEGER, offsetof(RsaPublicKeyData, exponent),
      },
      {
       0, }};

  // DER-encode the public key.
  crypto::ScopedSECItem pubkey_der(
      SEC_ASN1EncodeItem(NULL, NULL, &pubkey_in, rsa_public_key_template));
  if (!pubkey_der)
    return Status::OperationError();

  // Import the DER-encoded public key to create an RSA SECKEYPublicKey.
  crypto::ScopedSECKEYPublicKey pubkey(
      SECKEY_ImportDERPublicKey(pubkey_der.get(), CKK_RSA));
  if (!pubkey)
    return Status::OperationError();

  blink::WebCryptoKeyAlgorithm key_algorithm;
  if (!CreateRsaHashedPublicKeyAlgorithm(
          algorithm.id(), algorithm.rsaHashedImportParams()->hash().id(),
          pubkey.get(), &key_algorithm)) {
    return Status::ErrorUnexpected();
  }

  std::vector<uint8_t> spki_data;
  Status status = ExportKeySpkiNss(pubkey.get(), &spki_data);
  if (status.IsError())
    return status;

  scoped_ptr<PublicKeyNss> key_handle(
      new PublicKeyNss(pubkey.Pass(), CryptoData(spki_data)));

  *key = blink::WebCryptoKey::create(key_handle.release(),
                                     blink::WebCryptoKeyTypePublic, extractable,
                                     key_algorithm, usages);
  return Status::Success();
}

}  // namespace

Status RsaHashedAlgorithm::GenerateKey(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask combined_usages,
    GenerateKeyResult* result) const {
  blink::WebCryptoKeyUsageMask public_usages = 0;
  blink::WebCryptoKeyUsageMask private_usages = 0;

  Status status = GetUsagesForGenerateAsymmetricKey(
      combined_usages, all_public_key_usages_, all_private_key_usages_,
      &public_usages, &private_usages);
  if (status.IsError())
    return status;

  unsigned int public_exponent = 0;
  unsigned int modulus_length_bits = 0;
  status = GetRsaKeyGenParameters(algorithm.rsaHashedKeyGenParams(),
                                  &public_exponent, &modulus_length_bits);
  if (status.IsError())
    return status;

  crypto::ScopedPK11Slot slot(PK11_GetInternalKeySlot());
  if (!slot)
    return Status::OperationError();

  PK11RSAGenParams rsa_gen_params;
  rsa_gen_params.keySizeInBits = modulus_length_bits;
  rsa_gen_params.pe = public_exponent;

  // The usages are enforced at the WebCrypto layer, so it isn't necessary to
  // create keys with limited usages.
  const CK_FLAGS operation_flags_mask = kAllOperationFlags;

  // The private key must be marked as insensitive and extractable, otherwise it
  // cannot later be exported in unencrypted form or structured-cloned.
  const PK11AttrFlags attribute_flags =
      PK11_ATTR_INSENSITIVE | PK11_ATTR_EXTRACTABLE;

  // Note: NSS does not generate an sec_public_key if the call below fails,
  // so there is no danger of a leaked sec_public_key.
  SECKEYPublicKey* sec_public_key;
  crypto::ScopedSECKEYPrivateKey scoped_sec_private_key(
      PK11_GenerateKeyPairWithOpFlags(slot.get(), CKM_RSA_PKCS_KEY_PAIR_GEN,
                                      &rsa_gen_params, &sec_public_key,
                                      attribute_flags, generate_flags_,
                                      operation_flags_mask, NULL));
  if (!scoped_sec_private_key)
    return Status::OperationError();

  blink::WebCryptoKeyAlgorithm key_algorithm;
  if (!CreateRsaHashedPublicKeyAlgorithm(
          algorithm.id(), algorithm.rsaHashedKeyGenParams()->hash().id(),
          sec_public_key, &key_algorithm)) {
    return Status::ErrorUnexpected();
  }

  std::vector<uint8_t> spki_data;
  status = ExportKeySpkiNss(sec_public_key, &spki_data);
  if (status.IsError())
    return status;

  scoped_ptr<PublicKeyNss> public_key_handle(new PublicKeyNss(
      crypto::ScopedSECKEYPublicKey(sec_public_key), CryptoData(spki_data)));

  std::vector<uint8_t> pkcs8_data;
  status = ExportKeyPkcs8Nss(scoped_sec_private_key.get(), &pkcs8_data);
  if (status.IsError())
    return status;

  scoped_ptr<PrivateKeyNss> private_key_handle(
      new PrivateKeyNss(scoped_sec_private_key.Pass(), CryptoData(pkcs8_data)));

  blink::WebCryptoKey public_key = blink::WebCryptoKey::create(
      public_key_handle.release(), blink::WebCryptoKeyTypePublic, true,
      key_algorithm, public_usages);

  blink::WebCryptoKey private_key = blink::WebCryptoKey::create(
      private_key_handle.release(), blink::WebCryptoKeyTypePrivate, extractable,
      key_algorithm, private_usages);

  result->AssignKeyPair(public_key, private_key);
  return Status::Success();
}

Status RsaHashedAlgorithm::VerifyKeyUsagesBeforeImportKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask usages) const {
  return VerifyUsagesBeforeImportAsymmetricKey(format, all_public_key_usages_,
                                               all_private_key_usages_, usages);
}

Status RsaHashedAlgorithm::ImportKeyPkcs8(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  Status status = NssSupportsRsaPrivateKeyImport();
  if (status.IsError())
    return status;

  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena.get())
    return Status::OperationError();

  // The binary blob 'key_data' is expected to be a DER-encoded ASN.1 PKCS#8
  // private key info object. Excess data is illegal, but NSS silently accepts
  // it, so first ensure that 'key_data' consists of a single ASN.1 element.
  SECItem key_item = MakeSECItemForBuffer(key_data);
  SECItem pki_der;
  if (SEC_QuickDERDecodeItem(arena.get(), &pki_der,
                             SEC_ASN1_GET(SEC_AnyTemplate),
                             &key_item) != SECSuccess) {
    return Status::DataError();
  }

  SECKEYPrivateKey* seckey_private_key = NULL;
  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());
  if (PK11_ImportDERPrivateKeyInfoAndReturnKey(slot.get(), &pki_der,
                                               NULL,    // nickname
                                               NULL,    // publicValue
                                               false,   // isPerm
                                               false,   // isPrivate
                                               KU_ALL,  // usage
                                               &seckey_private_key,
                                               NULL) != SECSuccess) {
    return Status::DataError();
  }
  DCHECK(seckey_private_key);
  crypto::ScopedSECKEYPrivateKey private_key(seckey_private_key);

  const KeyType sec_key_type = SECKEY_GetPrivateKeyType(private_key.get());
  if (sec_key_type != rsaKey)
    return Status::DataError();

  blink::WebCryptoKeyAlgorithm key_algorithm;
  if (!CreateRsaHashedPrivateKeyAlgorithm(
          algorithm.id(), algorithm.rsaHashedImportParams()->hash().id(),
          private_key.get(), &key_algorithm)) {
    return Status::ErrorUnexpected();
  }

  // TODO(eroman): This is probably going to be the same as the input.
  std::vector<uint8_t> pkcs8_data;
  status = ExportKeyPkcs8Nss(private_key.get(), &pkcs8_data);
  if (status.IsError())
    return status;

  scoped_ptr<PrivateKeyNss> key_handle(
      new PrivateKeyNss(private_key.Pass(), CryptoData(pkcs8_data)));

  *key = blink::WebCryptoKey::create(key_handle.release(),
                                     blink::WebCryptoKeyTypePrivate,
                                     extractable, key_algorithm, usages);

  return Status::Success();
}

Status RsaHashedAlgorithm::ImportKeySpki(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  // The binary blob 'key_data' is expected to be a DER-encoded ASN.1 Subject
  // Public Key Info. Decode this to a CERTSubjectPublicKeyInfo.
  SECItem spki_item = MakeSECItemForBuffer(key_data);
  const ScopedCERTSubjectPublicKeyInfo spki(
      SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
  if (!spki)
    return Status::DataError();

  crypto::ScopedSECKEYPublicKey sec_public_key(
      SECKEY_ExtractPublicKey(spki.get()));
  if (!sec_public_key)
    return Status::DataError();

  const KeyType sec_key_type = SECKEY_GetPublicKeyType(sec_public_key.get());
  if (sec_key_type != rsaKey)
    return Status::DataError();

  blink::WebCryptoKeyAlgorithm key_algorithm;
  if (!CreateRsaHashedPublicKeyAlgorithm(
          algorithm.id(), algorithm.rsaHashedImportParams()->hash().id(),
          sec_public_key.get(), &key_algorithm)) {
    return Status::ErrorUnexpected();
  }

  // TODO(eroman): This is probably going to be the same as the input.
  std::vector<uint8_t> spki_data;
  Status status = ExportKeySpkiNss(sec_public_key.get(), &spki_data);
  if (status.IsError())
    return status;

  scoped_ptr<PublicKeyNss> key_handle(
      new PublicKeyNss(sec_public_key.Pass(), CryptoData(spki_data)));

  *key = blink::WebCryptoKey::create(key_handle.release(),
                                     blink::WebCryptoKeyTypePublic, extractable,
                                     key_algorithm, usages);

  return Status::Success();
}

Status RsaHashedAlgorithm::ExportKeyPkcs8(const blink::WebCryptoKey& key,
                                          std::vector<uint8_t>* buffer) const {
  if (key.type() != blink::WebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();
  *buffer = PrivateKeyNss::Cast(key)->pkcs8_data();
  return Status::Success();
}

Status RsaHashedAlgorithm::ExportKeySpki(const blink::WebCryptoKey& key,
                                         std::vector<uint8_t>* buffer) const {
  if (key.type() != blink::WebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();
  *buffer = PublicKeyNss::Cast(key)->spki_data();
  return Status::Success();
}

Status RsaHashedAlgorithm::ImportKeyJwk(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  const char* jwk_algorithm =
      GetJwkAlgorithm(algorithm.rsaHashedImportParams()->hash().id());

  if (!jwk_algorithm)
    return Status::ErrorUnexpected();

  JwkRsaInfo jwk;
  Status status =
      ReadRsaKeyJwk(key_data, jwk_algorithm, extractable, usages, &jwk);
  if (status.IsError())
    return status;

  // Once the key type is known, verify the usages.
  status = CheckKeyCreationUsages(
      jwk.is_private_key ? all_private_key_usages_ : all_public_key_usages_,
      usages, !jwk.is_private_key);
  if (status.IsError())
    return status;

  return jwk.is_private_key
             ? ImportRsaPrivateKey(algorithm, extractable, usages, jwk, key)
             : ImportRsaPublicKey(algorithm, extractable, usages,
                                  CryptoData(jwk.n), CryptoData(jwk.e), key);
}

Status RsaHashedAlgorithm::ExportKeyJwk(const blink::WebCryptoKey& key,
                                        std::vector<uint8_t>* buffer) const {
  const char* jwk_algorithm =
      GetJwkAlgorithm(key.algorithm().rsaHashedParams()->hash().id());

  if (!jwk_algorithm)
    return Status::ErrorUnexpected();

  switch (key.type()) {
    case blink::WebCryptoKeyTypePublic: {
      SECKEYPublicKey* nss_key = PublicKeyNss::Cast(key)->key();
      if (nss_key->keyType != rsaKey)
        return Status::ErrorUnsupported();

      WriteRsaPublicKeyJwk(SECItemToCryptoData(nss_key->u.rsa.modulus),
                           SECItemToCryptoData(nss_key->u.rsa.publicExponent),
                           jwk_algorithm, key.extractable(), key.usages(),
                           buffer);

      return Status::Success();
    }

    case blink::WebCryptoKeyTypePrivate: {
      SECKEYPrivateKey* nss_key = PrivateKeyNss::Cast(key)->key();
      RSAPrivateKey key_props = {};
      scoped_ptr<RSAPrivateKey, FreeRsaPrivateKey> free_private_key(&key_props);

      if (!InitRSAPrivateKey(nss_key, &key_props))
        return Status::OperationError();

      WriteRsaPrivateKeyJwk(SECItemToCryptoData(key_props.modulus),
                            SECItemToCryptoData(key_props.public_exponent),
                            SECItemToCryptoData(key_props.private_exponent),
                            SECItemToCryptoData(key_props.prime1),
                            SECItemToCryptoData(key_props.prime2),
                            SECItemToCryptoData(key_props.exponent1),
                            SECItemToCryptoData(key_props.exponent2),
                            SECItemToCryptoData(key_props.coefficient),
                            jwk_algorithm, key.extractable(), key.usages(),
                            buffer);

      return Status::Success();
    }
    default:
      return Status::ErrorUnexpected();
  }
}

Status RsaHashedAlgorithm::SerializeKeyForClone(
    const blink::WebCryptoKey& key,
    blink::WebVector<uint8_t>* key_data) const {
  key_data->assign(static_cast<KeyNss*>(key.handle())->serialized_key_data());
  return Status::Success();
}

// TODO(eroman): Defer import to the crypto thread. http://crbug.com/430763
Status RsaHashedAlgorithm::DeserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    const CryptoData& key_data,
    blink::WebCryptoKey* key) const {
  blink::WebCryptoAlgorithm import_algorithm = CreateRsaHashedImportAlgorithm(
      algorithm.id(), algorithm.rsaHashedParams()->hash().id());

  Status status;

  switch (type) {
    case blink::WebCryptoKeyTypePublic:
      status =
          ImportKeySpki(key_data, import_algorithm, extractable, usages, key);
      break;
    case blink::WebCryptoKeyTypePrivate:
      status =
          ImportKeyPkcs8(key_data, import_algorithm, extractable, usages, key);
      break;
    default:
      return Status::ErrorUnexpected();
  }

  // There is some duplicated information in the serialized format used by
  // structured clone (since the KeyAlgorithm is serialized separately from the
  // key data). Use this extra information to further validate what was
  // deserialized from the key data.

  if (algorithm.id() != key->algorithm().id())
    return Status::ErrorUnexpected();

  if (key->type() != type)
    return Status::ErrorUnexpected();

  if (algorithm.rsaHashedParams()->modulusLengthBits() !=
      key->algorithm().rsaHashedParams()->modulusLengthBits()) {
    return Status::ErrorUnexpected();
  }

  if (algorithm.rsaHashedParams()->publicExponent().size() !=
          key->algorithm().rsaHashedParams()->publicExponent().size() ||
      0 !=
          memcmp(algorithm.rsaHashedParams()->publicExponent().data(),
                 key->algorithm().rsaHashedParams()->publicExponent().data(),
                 key->algorithm().rsaHashedParams()->publicExponent().size())) {
    return Status::ErrorUnexpected();
  }

  return Status::Success();
}

}  // namespace webcrypto
