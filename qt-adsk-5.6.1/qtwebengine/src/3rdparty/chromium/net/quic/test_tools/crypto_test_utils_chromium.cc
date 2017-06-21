// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/crypto_test_utils.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/test/cert_test_util.h"

namespace net {

namespace test {

namespace {

class TestProofVerifierChromium : public ProofVerifierChromium {
 public:
  TestProofVerifierChromium(CertVerifier* cert_verifier,
                            TransportSecurityState* transport_security_state,
                            const std::string& cert_file)
      : ProofVerifierChromium(cert_verifier, transport_security_state),
        cert_verifier_(cert_verifier),
        transport_security_state_(transport_security_state) {
    // Load and install the root for the validated chain.
    scoped_refptr<X509Certificate> root_cert =
        ImportCertFromFile(GetTestCertsDirectory(), cert_file);
    scoped_root_.Reset(root_cert.get());
  }
  ~TestProofVerifierChromium() override {}

 private:
  ScopedTestRoot scoped_root_;
  scoped_ptr<CertVerifier> cert_verifier_;
  scoped_ptr<TransportSecurityState> transport_security_state_;
};

const char kLeafCert[] = "leaf";
const char kIntermediateCert[] = "intermediate";
const char kSignature[] = "signature";

class FakeProofSource : public ProofSource {
 public:
  FakeProofSource() : certs_(2) {
    certs_[0] = kLeafCert;
    certs_[1] = kIntermediateCert;
  }
  ~FakeProofSource() override {}

  // ProofSource interface
  bool GetProof(const IPAddressNumber& server_ip,
                const std::string& hostname,
                const std::string& server_config,
                bool ecdsa_ok,
                const std::vector<std::string>** out_certs,
                std::string* out_signature) override {
    *out_certs = &certs_;
    *out_signature = kSignature;
    return true;
  }

 private:
  std::vector<std::string> certs_;
  DISALLOW_COPY_AND_ASSIGN(FakeProofSource);
};

class FakeProofVerifier : public ProofVerifier {
 public:
  FakeProofVerifier() {}
  ~FakeProofVerifier() override {}

  // ProofVerifier interface
  QuicAsyncStatus VerifyProof(const std::string& hostname,
                              const std::string& server_config,
                              const std::vector<std::string>& certs,
                              const std::string& signature,
                              const ProofVerifyContext* verify_context,
                              std::string* error_details,
                              scoped_ptr<ProofVerifyDetails>* verify_details,
                              ProofVerifierCallback* callback) override {
    error_details->clear();
    scoped_ptr<ProofVerifyDetailsChromium> verify_details_chromium(
        new ProofVerifyDetailsChromium);
    if (certs.size() != 2 || certs[0] != kLeafCert ||
        certs[1] != kIntermediateCert || signature != kSignature) {
      *error_details = "Invalid proof";
      verify_details_chromium->cert_verify_result.cert_status =
          CERT_STATUS_INVALID;
      *verify_details = verify_details_chromium.Pass();
      return QUIC_FAILURE;
    }
    *verify_details = verify_details_chromium.Pass();
    return QUIC_SUCCESS;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeProofVerifier);
};

}  // namespace

// static
ProofSource* CryptoTestUtils::ProofSourceForTesting() {
  return new ProofSourceChromium();
}

// static
ProofVerifier* CryptoTestUtils::ProofVerifierForTesting() {
  TestProofVerifierChromium* proof_verifier = new TestProofVerifierChromium(
      CertVerifier::CreateDefault(), new TransportSecurityState,
      "quic_root.crt");
  return proof_verifier;
}

// static
ProofVerifyContext* CryptoTestUtils::ProofVerifyContextForTesting() {
  return new ProofVerifyContextChromium(/*cert_verify_flags=*/0, BoundNetLog());
}

// static
ProofSource* CryptoTestUtils::FakeProofSourceForTesting() {
  return new FakeProofSource();
}

// static
ProofVerifier* CryptoTestUtils::FakeProofVerifierForTesting() {
  return new FakeProofVerifier();
}

// static
ProofVerifyContext* CryptoTestUtils::FakeProofVerifyContextForTesting() {
  return nullptr;
}

}  // namespace test

}  // namespace net
