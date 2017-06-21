// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_verifier.h"

#include "base/logging.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/signed_tree_head.h"

namespace net {

// static
scoped_refptr<CTLogVerifier> CTLogVerifier::Create(
    const base::StringPiece& public_key,
    const base::StringPiece& description,
    const base::StringPiece& url) {
  GURL log_url(url.as_string());
  if (!log_url.is_valid())
    return nullptr;
  scoped_refptr<CTLogVerifier> result(new CTLogVerifier(description, log_url));
  if (!result->Init(public_key))
    return nullptr;
  return result;
}

CTLogVerifier::CTLogVerifier(const base::StringPiece& description,
                             const GURL& url)
    : description_(description.as_string()),
      url_(url),
      hash_algorithm_(ct::DigitallySigned::HASH_ALGO_NONE),
      signature_algorithm_(ct::DigitallySigned::SIG_ALGO_ANONYMOUS),
      public_key_(NULL) {
  DCHECK(url_.is_valid());
}

bool CTLogVerifier::Verify(const ct::LogEntry& entry,
                           const ct::SignedCertificateTimestamp& sct) {
  if (sct.log_id != key_id()) {
    DVLOG(1) << "SCT is not signed by this log.";
    return false;
  }

  if (!SignatureParametersMatch(sct.signature))
    return false;

  std::string serialized_log_entry;
  if (!ct::EncodeLogEntry(entry, &serialized_log_entry)) {
    DVLOG(1) << "Unable to serialize entry.";
    return false;
  }
  std::string serialized_data;
  if (!ct::EncodeV1SCTSignedData(sct.timestamp, serialized_log_entry,
                                 sct.extensions, &serialized_data)) {
    DVLOG(1) << "Unable to create SCT to verify.";
    return false;
  }

  return VerifySignature(serialized_data, sct.signature.signature_data);
}

bool CTLogVerifier::VerifySignedTreeHead(
    const ct::SignedTreeHead& signed_tree_head) {
  if (!SignatureParametersMatch(signed_tree_head.signature))
    return false;

  std::string serialized_data;
  ct::EncodeTreeHeadSignature(signed_tree_head, &serialized_data);
  if (VerifySignature(serialized_data,
                      signed_tree_head.signature.signature_data)) {
    return true;
  }
  return false;
}

bool CTLogVerifier::SignatureParametersMatch(
    const ct::DigitallySigned& signature) {
  if (!signature.SignatureParametersMatch(hash_algorithm_,
                                          signature_algorithm_)) {
    DVLOG(1) << "Mismatched hash or signature algorithm. Hash: "
             << hash_algorithm_ << " vs " << signature.hash_algorithm
             << " Signature: " << signature_algorithm_ << " vs "
             << signature.signature_algorithm << ".";
    return false;
  }

  return true;
}

}  // namespace net
