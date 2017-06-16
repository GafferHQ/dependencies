// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_TRANSPORT_SECURITY_STATE_H_
#define NET_HTTP_TRANSPORT_SECURITY_STATE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"

namespace net {

class SSLInfo;

// Tracks which hosts have enabled strict transport security and/or public
// key pins.
//
// This object manages the in-memory store. Register a Delegate with
// |SetDelegate| to persist the state to disk.
//
// HTTP strict transport security (HSTS) is defined in
// http://tools.ietf.org/html/ietf-websec-strict-transport-sec, and
// HTTP-based dynamic public key pinning (HPKP) is defined in
// http://tools.ietf.org/html/ietf-websec-key-pinning.
class NET_EXPORT TransportSecurityState
    : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  class NET_EXPORT Delegate {
   public:
    // This function may not block and may be called with internal locks held.
    // Thus it must not reenter the TransportSecurityState object.
    virtual void StateIsDirty(TransportSecurityState* state) = 0;

   protected:
    virtual ~Delegate() {}
  };

  TransportSecurityState();
  ~TransportSecurityState();

  // A STSState describes the strict transport security state (required
  // upgrade to HTTPS).
  class NET_EXPORT STSState {
   public:
    enum UpgradeMode {
      // These numbers must match those in hsts_view.js, function modeToString.
      MODE_FORCE_HTTPS = 0,
      MODE_DEFAULT = 1,
    };

    STSState();
    ~STSState();

    // The absolute time (UTC) when the |upgrade_mode| (and other state) was
    // observed.
    base::Time last_observed;

    // The absolute time (UTC) when |upgrade_mode| (and other state)
    // expires.
    base::Time expiry;

    UpgradeMode upgrade_mode;

    // Are subdomains subject to this policy state?
    bool include_subdomains;

    // The domain which matched during a search for this STSState entry.
    // Updated by |GetDynamicSTSState| and |GetStaticDomainState|.
    std::string domain;

    // ShouldUpgradeToSSL returns true iff HTTP requests should be internally
    // redirected to HTTPS (also if WS should be upgraded to WSS).
    bool ShouldUpgradeToSSL() const;

    // ShouldSSLErrorsBeFatal returns true iff HTTPS errors should cause
    // hard-fail behavior (e.g. if HSTS is set for the domain).
    bool ShouldSSLErrorsBeFatal() const;
  };

  class NET_EXPORT STSStateIterator {
   public:
    explicit STSStateIterator(const TransportSecurityState& state);
    ~STSStateIterator();

    bool HasNext() const { return iterator_ != end_; }
    void Advance() { ++iterator_; }
    const std::string& hostname() const { return iterator_->first; }
    const STSState& domain_state() const { return iterator_->second; }

   private:
    std::map<std::string, STSState>::const_iterator iterator_;
    std::map<std::string, STSState>::const_iterator end_;
  };

  // A PKPState describes the public key pinning state.
  class NET_EXPORT PKPState {
   public:
    PKPState();
    ~PKPState();

    // The absolute time (UTC) when the |spki_hashes| (and other state) were
    // observed.
    base::Time last_observed;

    // The absolute time (UTC) when the |spki_hashes| expire.
    base::Time expiry;

    // Optional; hashes of pinned SubjectPublicKeyInfos.
    HashValueVector spki_hashes;

    // Optional; hashes of static known-bad SubjectPublicKeyInfos which MUST
    // NOT intersect with the set of SPKIs in the TLS server's certificate
    // chain.
    HashValueVector bad_spki_hashes;

    // Are subdomains subject to this policy state?
    bool include_subdomains;

    // The domain which matched during a search for this DomainState entry.
    // Updated by |GetDynamicPKPState| and |GetStaticDomainState|.
    std::string domain;

    // Takes a set of SubjectPublicKeyInfo |hashes| and returns true if:
    //   1) |bad_static_spki_hashes| does not intersect |hashes|; AND
    //   2) Both |static_spki_hashes| and |dynamic_spki_hashes| are empty
    //      or at least one of them intersects |hashes|.
    //
    // |{dynamic,static}_spki_hashes| contain trustworthy public key hashes,
    // any one of which is sufficient to validate the certificate chain in
    // question. The public keys could be of a root CA, intermediate CA, or
    // leaf certificate, depending on the security vs. disaster recovery
    // tradeoff selected. (Pinning only to leaf certifiates increases
    // security because you no longer trust any CAs, but it hampers disaster
    // recovery because you can't just get a new certificate signed by the
    // CA.)
    //
    // |bad_static_spki_hashes| contains public keys that we don't want to
    // trust.
    bool CheckPublicKeyPins(const HashValueVector& hashes,
                            std::string* failure_log) const;

    // Returns true if any of the HashValueVectors |static_spki_hashes|,
    // |bad_static_spki_hashes|, or |dynamic_spki_hashes| contains any
    // items.
    bool HasPublicKeyPins() const;

    // ShouldSSLErrorsBeFatal returns true iff HTTPS errors should cause
    // hard-fail behavior (e.g. if HSTS is set for the domain).
    bool ShouldSSLErrorsBeFatal() const;
  };

  class NET_EXPORT PKPStateIterator {
   public:
    explicit PKPStateIterator(const TransportSecurityState& state);
    ~PKPStateIterator();

    bool HasNext() const { return iterator_ != end_; }
    void Advance() { ++iterator_; }
    const std::string& hostname() const { return iterator_->first; }
    const PKPState& domain_state() const { return iterator_->second; }

   private:
    std::map<std::string, PKPState>::const_iterator iterator_;
    std::map<std::string, PKPState>::const_iterator end_;
  };

  // These functions search for static and dynamic STS and PKP states, and
  // invoke the
  // functions of the same name on them. These functions are the primary public
  // interface; direct access to STS and PKP states is best left to tests.
  bool ShouldSSLErrorsBeFatal(const std::string& host);
  bool ShouldUpgradeToSSL(const std::string& host);
  bool CheckPublicKeyPins(const std::string& host,
                          bool is_issued_by_known_root,
                          const HashValueVector& hashes,
                          std::string* failure_log);
  bool HasPublicKeyPins(const std::string& host);

  // Assign a |Delegate| for persisting the transport security state. If
  // |NULL|, state will not be persisted. The caller retains
  // ownership of |delegate|.
  // Note: This is only used for serializing/deserializing the
  // TransportSecurityState.
  void SetDelegate(Delegate* delegate);

  // Clears all dynamic data (e.g. HSTS and HPKP data).
  //
  // Does NOT persist changes using the Delegate, as this function is only
  // used to clear any dynamic data prior to re-loading it from a file.
  // Note: This is only used for serializing/deserializing the
  // TransportSecurityState.
  void ClearDynamicData();

  // Inserts |state| into |enabled_sts_hosts_| under the key |hashed_host|.
  // |hashed_host| is already in the internal representation.
  // Note: This is only used for serializing/deserializing the
  // TransportSecurityState.
  void AddOrUpdateEnabledSTSHosts(const std::string& hashed_host,
                                  const STSState& state);

  // Inserts |state| into |enabled_pkp_hosts_| under the key |hashed_host|.
  // |hashed_host| is already in the internal representation.
  // Note: This is only used for serializing/deserializing the
  // TransportSecurityState.
  void AddOrUpdateEnabledPKPHosts(const std::string& hashed_host,
                                  const PKPState& state);

  // Deletes all dynamic data (e.g. HSTS or HPKP data) created since a given
  // time.
  //
  // If any entries are deleted, the new state will be persisted through
  // the Delegate (if any).
  void DeleteAllDynamicDataSince(const base::Time& time);

  // Deletes any dynamic data stored for |host| (e.g. HSTS or HPKP data).
  // If |host| doesn't have an exact entry then no action is taken. Does
  // not delete static (i.e. preloaded) data.  Returns true iff an entry
  // was deleted.
  //
  // If an entry is deleted, the new state will be persisted through
  // the Delegate (if any).
  bool DeleteDynamicDataForHost(const std::string& host);

  // Returns true and updates |*sts_result| and |*pkp_result| iff there is a
  // static (built-in) state for |host|. If multiple entries match |host|,
  // the most specific match determines the return value.
  bool GetStaticDomainState(const std::string& host,
                            STSState* sts_result,
                            PKPState* pkp_result) const;

  // Returns true and updates |*result| iff |host| has HSTS (respectively, HPKP)
  // state. If multiple HSTS (respectively, HPKP) entries match |host|,  the
  // most specific match determines the HSTS (respectively, HPKP) return value.
  //
  // Note that these methods are not const because they opportunistically remove
  // entries that have expired.
  bool GetDynamicSTSState(const std::string& host, STSState* result);
  bool GetDynamicPKPState(const std::string& host, PKPState* result);

  // Processes an HSTS header value from the host, adding entries to
  // dynamic state if necessary.
  bool AddHSTSHeader(const std::string& host, const std::string& value);

  // Processes an HPKP header value from the host, adding entries to
  // dynamic state if necessary.  ssl_info is used to check that
  // the specified pins overlap with the certificate chain.
  bool AddHPKPHeader(const std::string& host, const std::string& value,
                     const SSLInfo& ssl_info);

  // Adds explicitly-specified data as if it was processed from an
  // HSTS header (used for net-internals and unit tests).
  void AddHSTS(const std::string& host,
               const base::Time& expiry,
               bool include_subdomains);

  // Adds explicitly-specified data as if it was processed from an
  // HPKP header (used for net-internals and unit tests).
  void AddHPKP(const std::string& host,
               const base::Time& expiry,
               bool include_subdomains,
               const HashValueVector& hashes);

  // Returns true iff we have any static public key pins for the |host| and
  // iff its set of required pins is the set we expect for Google
  // properties.
  //
  // If |host| matches both an exact entry and is a subdomain of another
  // entry, the exact match determines the return value.
  static bool IsGooglePinnedProperty(const std::string& host);

  // The maximum number of seconds for which we'll cache an HSTS request.
  static const long int kMaxHSTSAgeSecs;

 private:
  friend class TransportSecurityStateTest;
  FRIEND_TEST_ALL_PREFIXES(HttpSecurityHeadersTest, UpdateDynamicPKPOnly);
  FRIEND_TEST_ALL_PREFIXES(HttpSecurityHeadersTest, UpdateDynamicPKPMaxAge0);
  FRIEND_TEST_ALL_PREFIXES(HttpSecurityHeadersTest, NoClobberPins);

  typedef std::map<std::string, STSState> STSStateMap;
  typedef std::map<std::string, PKPState> PKPStateMap;

  // Send an UMA report on pin validation failure, if the host is in a
  // statically-defined list of domains.
  //
  // TODO(palmer): This doesn't really belong here, and should be moved into
  // the exactly one call site. This requires unifying |struct HSTSPreload|
  // (an implementation detail of this class) with a more generic
  // representation of first-class DomainStates, and exposing the preloads
  // to the caller with |GetStaticDomainState|.
  static void ReportUMAOnPinFailure(const std::string& host);

  // IsBuildTimely returns true if the current build is new enough ensure that
  // built in security information (i.e. HSTS preloading and pinning
  // information) is timely.
  static bool IsBuildTimely();

  // Helper method for actually checking pins.
  bool CheckPublicKeyPinsImpl(const std::string& host,
                              const HashValueVector& hashes,
                              std::string* failure_log);

  // If a Delegate is present, notify it that the internal state has
  // changed.
  void DirtyNotify();

  // Adds HSTS state to |host|.
  void AddHSTSInternal(const std::string& host,
                       STSState::UpgradeMode upgrade_mode,
                       const base::Time& expiry,
                       bool include_subdomains);

  // Adds HPKP state to |host|.
  void AddHPKPInternal(const std::string& host,
                       const base::Time& last_observed,
                       const base::Time& expiry,
                       bool include_subdomains,
                       const HashValueVector& hashes);

  // Enable TransportSecurity for |host|. |state| supercedes any previous
  // state for the |host|, including static entries.
  //
  // The new state for |host| is persisted using the Delegate (if any).
  void EnableSTSHost(const std::string& host, const STSState& state);
  void EnablePKPHost(const std::string& host, const PKPState& state);

  // The sets of hosts that have enabled TransportSecurity. |domain| will always
  // be empty for a STSState or PKPState in these maps; the domain
  // comes from the map keys instead. In addition, |upgrade_mode| in the
  // STSState is never MODE_DEFAULT and |HasPublicKeyPins| in the PKPState
  // always returns true.
  STSStateMap enabled_sts_hosts_;
  PKPStateMap enabled_pkp_hosts_;

  Delegate* delegate_;

  // True if static pins should be used.
  bool enable_static_pins_;

  DISALLOW_COPY_AND_ASSIGN(TransportSecurityState);
};

}  // namespace net

#endif  // NET_HTTP_TRANSPORT_SECURITY_STATE_H_
