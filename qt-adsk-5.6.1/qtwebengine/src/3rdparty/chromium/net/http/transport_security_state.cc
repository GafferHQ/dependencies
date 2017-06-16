// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_state.h"

#if defined(USE_OPENSSL)
#include <openssl/ecdsa.h>
#include <openssl/ssl.h>
#else  // !defined(USE_OPENSSL)
#include <cryptohi.h>
#include <hasht.h>
#include <keyhi.h>
#include <nspr.h>
#include <pk11pub.h>
#endif

#include <algorithm>

#include "base/base64.h"
#include "base/build_time.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "net/base/dns_util.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_security_headers.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

#if defined(USE_OPENSSL)
#include "crypto/openssl_util.h"
#endif

namespace net {

namespace {

#include "net/http/transport_security_state_static.h"

std::string HashesToBase64String(const HashValueVector& hashes) {
  std::string str;
  for (size_t i = 0; i != hashes.size(); ++i) {
    if (i != 0)
      str += ",";
    str += hashes[i].ToString();
  }
  return str;
}

std::string HashHost(const std::string& canonicalized_host) {
  char hashed[crypto::kSHA256Length];
  crypto::SHA256HashString(canonicalized_host, hashed, sizeof(hashed));
  return std::string(hashed, sizeof(hashed));
}

// Returns true if the intersection of |a| and |b| is not empty. If either
// |a| or |b| is empty, returns false.
bool HashesIntersect(const HashValueVector& a,
                     const HashValueVector& b) {
  for (HashValueVector::const_iterator i = a.begin(); i != a.end(); ++i) {
    HashValueVector::const_iterator j =
        std::find_if(b.begin(), b.end(), HashValuesEqual(*i));
    if (j != b.end())
      return true;
  }
  return false;
}

bool AddHash(const char* sha1_hash,
             HashValueVector* out) {
  HashValue hash(HASH_VALUE_SHA1);
  memcpy(hash.data(), sha1_hash, hash.size());
  out->push_back(hash);
  return true;
}

// Converts |hostname| from dotted form ("www.google.com") to the form
// used in DNS: "\x03www\x06google\x03com", lowercases that, and returns
// the result.
std::string CanonicalizeHost(const std::string& host) {
  // We cannot perform the operations as detailed in the spec here as |host|
  // has already undergone IDN processing before it reached us. Thus, we check
  // that there are no invalid characters in the host and lowercase the result.
  std::string new_host;
  if (!DNSDomainFromDot(host, &new_host)) {
    // DNSDomainFromDot can fail if any label is > 63 bytes or if the whole
    // name is >255 bytes. However, search terms can have those properties.
    return std::string();
  }

  for (size_t i = 0; new_host[i]; i += new_host[i] + 1) {
    const unsigned label_length = static_cast<unsigned>(new_host[i]);
    if (!label_length)
      break;

    for (size_t j = 0; j < label_length; ++j) {
      new_host[i + 1 + j] = static_cast<char>(tolower(new_host[i + 1 + j]));
    }
  }

  return new_host;
}

// BitReader is a class that allows a bytestring to be read bit-by-bit.
class BitReader {
 public:
  BitReader(const uint8* bytes, size_t num_bits)
      : bytes_(bytes),
        num_bits_(num_bits),
        num_bytes_((num_bits + 7) / 8),
        current_byte_index_(0),
        num_bits_used_(8) {}

  // Next sets |*out| to the next bit from the input. It returns false if no
  // more bits are available or true otherwise.
  bool Next(bool* out) {
    if (num_bits_used_ == 8) {
      if (current_byte_index_ >= num_bytes_) {
        return false;
      }
      current_byte_ = bytes_[current_byte_index_++];
      num_bits_used_ = 0;
    }

    *out = 1 & (current_byte_ >> (7 - num_bits_used_));
    num_bits_used_++;
    return true;
  }

  // Read sets the |num_bits| least-significant bits of |*out| to the value of
  // the next |num_bits| bits from the input. It returns false if there are
  // insufficient bits in the input or true otherwise.
  bool Read(unsigned num_bits, uint32* out) {
    DCHECK_LE(num_bits, 32u);

    uint32 ret = 0;
    for (unsigned i = 0; i < num_bits; ++i) {
      bool bit;
      if (!Next(&bit)) {
        return false;
      }
      ret |= static_cast<uint32>(bit) << (num_bits - 1 - i);
    }

    *out = ret;
    return true;
  }

  // Unary sets |*out| to the result of decoding a unary value from the input.
  // It returns false if there were insufficient bits in the input and true
  // otherwise.
  bool Unary(size_t* out) {
    size_t ret = 0;

    for (;;) {
      bool bit;
      if (!Next(&bit)) {
        return false;
      }
      if (!bit) {
        break;
      }
      ret++;
    }

    *out = ret;
    return true;
  }

  // Seek sets the current offest in the input to bit number |offset|. It
  // returns true if |offset| is within the range of the input and false
  // otherwise.
  bool Seek(size_t offset) {
    if (offset >= num_bits_) {
      return false;
    }
    current_byte_index_ = offset / 8;
    current_byte_ = bytes_[current_byte_index_++];
    num_bits_used_ = offset % 8;
    return true;
  }

 private:
  const uint8* const bytes_;
  const size_t num_bits_;
  const size_t num_bytes_;
  // current_byte_index_ contains the current byte offset in |bytes_|.
  size_t current_byte_index_;
  // current_byte_ contains the current byte of the input.
  uint8 current_byte_;
  // num_bits_used_ contains the number of bits of |current_byte_| that have
  // been read.
  unsigned num_bits_used_;
};

// HuffmanDecoder is a very simple Huffman reader. The input Huffman tree is
// simply encoded as a series of two-byte structures. The first byte determines
// the "0" pointer for that node and the second the "1" pointer. Each byte
// either has the MSB set, in which case the bottom 7 bits are the value for
// that position, or else the bottom seven bits contain the index of a node.
//
// The tree is decoded by walking rather than a table-driven approach.
class HuffmanDecoder {
 public:
  HuffmanDecoder(const uint8* tree, size_t tree_bytes)
      : tree_(tree), tree_bytes_(tree_bytes) {}

  bool Decode(BitReader* reader, char* out) {
    const uint8* current = &tree_[tree_bytes_ - 2];

    for (;;) {
      bool bit;
      if (!reader->Next(&bit)) {
        return false;
      }

      uint8 b = current[bit];
      if (b & 0x80) {
        *out = static_cast<char>(b & 0x7f);
        return true;
      }

      unsigned offset = static_cast<unsigned>(b) * 2;
      DCHECK_LT(offset, tree_bytes_);
      if (offset >= tree_bytes_) {
        return false;
      }

      current = &tree_[offset];
    }
  }

 private:
  const uint8* const tree_;
  const size_t tree_bytes_;
};

// PreloadResult is the result of resolving a specific name in the preloaded
// data.
struct PreloadResult {
  uint32 pinset_id;
  uint32 domain_id;
  // hostname_offset contains the number of bytes from the start of the given
  // hostname where the name of the matching entry starts.
  size_t hostname_offset;
  bool sts_include_subdomains;
  bool pkp_include_subdomains;
  bool force_https;
  bool has_pins;
};

// DecodeHSTSPreloadRaw resolves |hostname| in the preloaded data. It returns
// false on internal error and true otherwise. After a successful return,
// |*out_found| is true iff a relevant entry has been found. If so, |*out|
// contains the details.
//
// Don't call this function, call DecodeHSTSPreload, below.
//
// Although this code should be robust, it never processes attacker-controlled
// data -- it only operates on the preloaded data built into the binary.
//
// The preloaded data is represented as a trie and matches the hostname
// backwards. Each node in the trie starts with a number of characters, which
// must match exactly. After that is a dispatch table which maps the next
// character in the hostname to another node in the trie.
//
// In the dispatch table, the zero character represents the "end of string"
// (which is the *beginning* of a hostname since we process it backwards). The
// value in that case is special -- rather than an offset to another trie node,
// it contains the HSTS information: whether subdomains are included, pinsets
// etc. If an "end of string" matches a period in the hostname then the
// information is remembered because, if no more specific node is found, then
// that information applies to the hostname.
//
// Dispatch tables are always given in order, but the "end of string" (zero)
// value always comes before an entry for '.'.
bool DecodeHSTSPreloadRaw(const std::string& search_hostname,
                          bool* out_found,
                          PreloadResult* out) {
  HuffmanDecoder huffman(kHSTSHuffmanTree, sizeof(kHSTSHuffmanTree));
  BitReader reader(kPreloadedHSTSData, kPreloadedHSTSBits);
  size_t bit_offset = kHSTSRootPosition;
  static const char kEndOfString = 0;
  static const char kEndOfTable = 127;

  *out_found = false;

  // Ensure that |search_hostname| is a valid hostname before
  // processing.
  if (CanonicalizeHost(search_hostname).empty()) {
    return true;
  }

  // Normalize any trailing '.' used for DNS suffix searches.
  std::string hostname = search_hostname;
  size_t found = hostname.find_last_not_of('.');
  if (found != std::string::npos) {
    hostname.erase(found + 1);
  } else {
    hostname.clear();
  }

  // |hostname| has already undergone IDN conversion, so should be
  // entirely A-Labels. The preload data is entirely normalized to
  // lower case.
  base::StringToLowerASCII(&hostname);

  if (hostname.empty()) {
    return true;
  }

  // hostname_offset contains one more than the index of the current character
  // in the hostname that is being considered. It's one greater so that we can
  // represent the position just before the beginning (with zero).
  size_t hostname_offset = hostname.size();

  for (;;) {
    // Seek to the desired location.
    if (!reader.Seek(bit_offset)) {
      return false;
    }

    // Decode the unary length of the common prefix.
    size_t prefix_length;
    if (!reader.Unary(&prefix_length)) {
      return false;
    }

    // Match each character in the prefix.
    for (size_t i = 0; i < prefix_length; ++i) {
      if (hostname_offset == 0) {
        // We can't match the terminator with a prefix string.
        return true;
      }

      char c;
      if (!huffman.Decode(&reader, &c)) {
        return false;
      }
      if (hostname[hostname_offset - 1] != c) {
        return true;
      }
      hostname_offset--;
    }

    bool is_first_offset = true;
    size_t current_offset = 0;

    // Next is the dispatch table.
    for (;;) {
      char c;
      if (!huffman.Decode(&reader, &c)) {
        return false;
      }
      if (c == kEndOfTable) {
        // No exact match.
        return true;
      }

      if (c == kEndOfString) {
        PreloadResult tmp;
        if (!reader.Next(&tmp.sts_include_subdomains) ||
            !reader.Next(&tmp.force_https) || !reader.Next(&tmp.has_pins)) {
          return false;
        }

        tmp.pkp_include_subdomains = tmp.sts_include_subdomains;

        if (tmp.has_pins) {
          if (!reader.Read(4, &tmp.pinset_id) ||
              !reader.Read(9, &tmp.domain_id) ||
              (!tmp.sts_include_subdomains &&
               !reader.Next(&tmp.pkp_include_subdomains))) {
            return false;
          }
        }

        tmp.hostname_offset = hostname_offset;

        if (hostname_offset == 0 || hostname[hostname_offset - 1] == '.') {
          *out_found = tmp.sts_include_subdomains || tmp.pkp_include_subdomains;
          *out = tmp;

          if (hostname_offset > 0) {
            out->force_https &= tmp.sts_include_subdomains;
          } else {
            *out_found = true;
            return true;
          }
        }

        continue;
      }

      // The entries in a dispatch table are in order thus we can tell if there
      // will be no match if the current character past the one that we want.
      if (hostname_offset == 0 || hostname[hostname_offset - 1] < c) {
        return true;
      }

      if (is_first_offset) {
        // The first offset is backwards from the current position.
        uint32 jump_delta_bits;
        uint32 jump_delta;
        if (!reader.Read(5, &jump_delta_bits) ||
            !reader.Read(jump_delta_bits, &jump_delta)) {
          return false;
        }

        if (bit_offset < jump_delta) {
          return false;
        }

        current_offset = bit_offset - jump_delta;
        is_first_offset = false;
      } else {
        // Subsequent offsets are forward from the target of the first offset.
        uint32 is_long_jump;
        if (!reader.Read(1, &is_long_jump)) {
          return false;
        }

        uint32 jump_delta;
        if (!is_long_jump) {
          if (!reader.Read(7, &jump_delta)) {
            return false;
          }
        } else {
          uint32 jump_delta_bits;
          if (!reader.Read(4, &jump_delta_bits) ||
              !reader.Read(jump_delta_bits + 8, &jump_delta)) {
            return false;
          }
        }

        current_offset += jump_delta;
        if (current_offset >= bit_offset) {
          return false;
        }
      }

      DCHECK_LT(0u, hostname_offset);
      if (hostname[hostname_offset - 1] == c) {
        bit_offset = current_offset;
        hostname_offset--;
        break;
      }
    }
  }
}

bool DecodeHSTSPreload(const std::string& hostname, PreloadResult* out) {
  bool found;
  if (!DecodeHSTSPreloadRaw(hostname, &found, out)) {
    DCHECK(false) << "Internal error in DecodeHSTSPreloadRaw for hostname "
                  << hostname;
    return false;
  }

  return found;
}

}  // namespace

TransportSecurityState::TransportSecurityState()
    : delegate_(NULL), enable_static_pins_(true) {
// Static pinning is only enabled for official builds to make sure that
// others don't end up with pins that cannot be easily updated.
#if !defined(OFFICIAL_BUILD) || defined(OS_ANDROID) || defined(OS_IOS)
  enable_static_pins_ = false;
#endif
  DCHECK(CalledOnValidThread());
}

// Both HSTS and HPKP cause fatal SSL errors, so return true if a
// host has either.
bool TransportSecurityState::ShouldSSLErrorsBeFatal(const std::string& host) {
  STSState sts_state;
  PKPState pkp_state;
  if (GetStaticDomainState(host, &sts_state, &pkp_state))
    return true;
  if (GetDynamicSTSState(host, &sts_state))
    return true;
  return GetDynamicPKPState(host, &pkp_state);
}

bool TransportSecurityState::ShouldUpgradeToSSL(const std::string& host) {
  STSState dynamic_sts_state;
  if (GetDynamicSTSState(host, &dynamic_sts_state))
    return dynamic_sts_state.ShouldUpgradeToSSL();

  STSState static_sts_state;
  PKPState unused;
  if (GetStaticDomainState(host, &static_sts_state, &unused) &&
      static_sts_state.ShouldUpgradeToSSL()) {
    return true;
  }

  return false;
}

bool TransportSecurityState::CheckPublicKeyPins(
    const std::string& host,
    bool is_issued_by_known_root,
    const HashValueVector& public_key_hashes,
    std::string* pinning_failure_log) {
  // Perform pin validation if, and only if, all these conditions obtain:
  //
  // * the server's certificate chain chains up to a known root (i.e. not a
  //   user-installed trust anchor); and
  // * the server actually has public key pins.
  if (!is_issued_by_known_root || !HasPublicKeyPins(host)) {
    return true;
  }

  bool pins_are_valid =
      CheckPublicKeyPinsImpl(host, public_key_hashes, pinning_failure_log);
  if (!pins_are_valid) {
    LOG(ERROR) << *pinning_failure_log;
    ReportUMAOnPinFailure(host);
  }

  UMA_HISTOGRAM_BOOLEAN("Net.PublicKeyPinSuccess", pins_are_valid);
  return pins_are_valid;
}

bool TransportSecurityState::HasPublicKeyPins(const std::string& host) {
  PKPState dynamic_state;
  if (GetDynamicPKPState(host, &dynamic_state))
    return dynamic_state.HasPublicKeyPins();

  STSState unused;
  PKPState static_pkp_state;
  if (GetStaticDomainState(host, &unused, &static_pkp_state)) {
    if (static_pkp_state.HasPublicKeyPins())
      return true;
  }

  return false;
}

void TransportSecurityState::SetDelegate(
    TransportSecurityState::Delegate* delegate) {
  DCHECK(CalledOnValidThread());
  delegate_ = delegate;
}

void TransportSecurityState::AddHSTSInternal(
    const std::string& host,
    TransportSecurityState::STSState::UpgradeMode upgrade_mode,
    const base::Time& expiry,
    bool include_subdomains) {
  DCHECK(CalledOnValidThread());

  STSState sts_state;
  sts_state.last_observed = base::Time::Now();
  sts_state.include_subdomains = include_subdomains;
  sts_state.expiry = expiry;
  sts_state.upgrade_mode = upgrade_mode;

  EnableSTSHost(host, sts_state);
}

void TransportSecurityState::AddHPKPInternal(const std::string& host,
                                             const base::Time& last_observed,
                                             const base::Time& expiry,
                                             bool include_subdomains,
                                             const HashValueVector& hashes) {
  DCHECK(CalledOnValidThread());

  PKPState pkp_state;
  pkp_state.last_observed = last_observed;
  pkp_state.expiry = expiry;
  pkp_state.include_subdomains = include_subdomains;
  pkp_state.spki_hashes = hashes;

  EnablePKPHost(host, pkp_state);
}

void TransportSecurityState::EnableSTSHost(const std::string& host,
                                           const STSState& state) {
  DCHECK(CalledOnValidThread());

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return;

  // Only store new state when HSTS is explicitly enabled. If it is
  // disabled, remove the state from the enabled hosts.
  if (state.ShouldUpgradeToSSL()) {
    STSState sts_state(state);
    // No need to store this value since it is redundant. (|canonicalized_host|
    // is the map key.)
    sts_state.domain.clear();

    enabled_sts_hosts_[HashHost(canonicalized_host)] = sts_state;
  } else {
    const std::string hashed_host = HashHost(canonicalized_host);
    enabled_sts_hosts_.erase(hashed_host);
  }

  DirtyNotify();
}

void TransportSecurityState::EnablePKPHost(const std::string& host,
                                           const PKPState& state) {
  DCHECK(CalledOnValidThread());

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return;

  // Only store new state when HPKP is explicitly enabled. If it is
  // disabled, remove the state from the enabled hosts.
  if (state.HasPublicKeyPins()) {
    PKPState pkp_state(state);
    // No need to store this value since it is redundant. (|canonicalized_host|
    // is the map key.)
    pkp_state.domain.clear();

    enabled_pkp_hosts_[HashHost(canonicalized_host)] = pkp_state;
  } else {
    const std::string hashed_host = HashHost(canonicalized_host);
    enabled_pkp_hosts_.erase(hashed_host);
  }

  DirtyNotify();
}

bool TransportSecurityState::DeleteDynamicDataForHost(const std::string& host) {
  DCHECK(CalledOnValidThread());

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  const std::string hashed_host = HashHost(canonicalized_host);
  bool deleted = false;
  STSStateMap::iterator sts_interator = enabled_sts_hosts_.find(hashed_host);
  if (sts_interator != enabled_sts_hosts_.end()) {
    enabled_sts_hosts_.erase(sts_interator);
    deleted = true;
  }

  PKPStateMap::iterator pkp_iterator = enabled_pkp_hosts_.find(hashed_host);
  if (pkp_iterator != enabled_pkp_hosts_.end()) {
    enabled_pkp_hosts_.erase(pkp_iterator);
    deleted = true;
  }

  if (deleted)
    DirtyNotify();
  return deleted;
}

void TransportSecurityState::ClearDynamicData() {
  DCHECK(CalledOnValidThread());
  enabled_sts_hosts_.clear();
  enabled_pkp_hosts_.clear();
}

void TransportSecurityState::DeleteAllDynamicDataSince(const base::Time& time) {
  DCHECK(CalledOnValidThread());

  bool dirtied = false;
  STSStateMap::iterator sts_iterator = enabled_sts_hosts_.begin();
  while (sts_iterator != enabled_sts_hosts_.end()) {
    if (sts_iterator->second.last_observed >= time) {
      dirtied = true;
      enabled_sts_hosts_.erase(sts_iterator++);
      continue;
    }

    ++sts_iterator;
  }

  PKPStateMap::iterator pkp_iterator = enabled_pkp_hosts_.begin();
  while (pkp_iterator != enabled_pkp_hosts_.end()) {
    if (pkp_iterator->second.last_observed >= time) {
      dirtied = true;
      enabled_pkp_hosts_.erase(pkp_iterator++);
      continue;
    }

    ++pkp_iterator;
  }

  if (dirtied)
    DirtyNotify();
}

TransportSecurityState::~TransportSecurityState() {
  DCHECK(CalledOnValidThread());
}

void TransportSecurityState::DirtyNotify() {
  DCHECK(CalledOnValidThread());

  if (delegate_)
    delegate_->StateIsDirty(this);
}

bool TransportSecurityState::AddHSTSHeader(const std::string& host,
                                           const std::string& value) {
  DCHECK(CalledOnValidThread());

  base::Time now = base::Time::Now();
  base::TimeDelta max_age;
  bool include_subdomains;
  if (!ParseHSTSHeader(value, &max_age, &include_subdomains)) {
    return false;
  }

  // Handle max-age == 0.
  STSState::UpgradeMode upgrade_mode;
  if (max_age.InSeconds() == 0) {
    upgrade_mode = STSState::MODE_DEFAULT;
  } else {
    upgrade_mode = STSState::MODE_FORCE_HTTPS;
  }

  AddHSTSInternal(host, upgrade_mode, now + max_age, include_subdomains);
  return true;
}

bool TransportSecurityState::AddHPKPHeader(const std::string& host,
                                           const std::string& value,
                                           const SSLInfo& ssl_info) {
  DCHECK(CalledOnValidThread());

  base::Time now = base::Time::Now();
  base::TimeDelta max_age;
  bool include_subdomains;
  HashValueVector spki_hashes;
  if (!ParseHPKPHeader(value, ssl_info.public_key_hashes, &max_age,
                       &include_subdomains, &spki_hashes)) {
    return false;
  }
  // Handle max-age == 0.
  if (max_age.InSeconds() == 0)
    spki_hashes.clear();
  AddHPKPInternal(host, now, now + max_age, include_subdomains, spki_hashes);
  return true;
}

void TransportSecurityState::AddHSTS(const std::string& host,
                                     const base::Time& expiry,
                                     bool include_subdomains) {
  DCHECK(CalledOnValidThread());
  AddHSTSInternal(host, STSState::MODE_FORCE_HTTPS, expiry, include_subdomains);
}

void TransportSecurityState::AddHPKP(const std::string& host,
                                     const base::Time& expiry,
                                     bool include_subdomains,
                                     const HashValueVector& hashes) {
  DCHECK(CalledOnValidThread());
  AddHPKPInternal(host, base::Time::Now(), expiry, include_subdomains, hashes);
}

// static
bool TransportSecurityState::IsGooglePinnedProperty(const std::string& host) {
  PreloadResult result;
  return DecodeHSTSPreload(host, &result) && result.has_pins &&
         kPinsets[result.pinset_id].accepted_pins == kGoogleAcceptableCerts;
}

// static
void TransportSecurityState::ReportUMAOnPinFailure(const std::string& host) {
  PreloadResult result;
  if (!DecodeHSTSPreload(host, &result) ||
      !result.has_pins) {
    return;
  }

  DCHECK(result.domain_id != DOMAIN_NOT_PINNED);

  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "Net.PublicKeyPinFailureDomain", result.domain_id);
}

// static
bool TransportSecurityState::IsBuildTimely() {
  // If the build metadata aren't embedded in the binary then we can't use the
  // build time to determine if the build is timely, return true by default. If
  // we're building an official build then keep using the build time, even if
  // it's invalid it'd be a date in the past and this function will return
  // false.
#if defined(DONT_EMBED_BUILD_METADATA) && !defined(OFFICIAL_BUILD)
  return true;
#else
  const base::Time build_time = base::GetBuildTime();
  // We consider built-in information to be timely for 10 weeks.
  return (base::Time::Now() - build_time).InDays() < 70 /* 10 weeks */;
#endif
}

bool TransportSecurityState::CheckPublicKeyPinsImpl(
    const std::string& host,
    const HashValueVector& hashes,
    std::string* failure_log) {
  PKPState dynamic_state;
  if (GetDynamicPKPState(host, &dynamic_state))
    return dynamic_state.CheckPublicKeyPins(hashes, failure_log);

  PKPState static_pkp_state;
  STSState unused;
  if (GetStaticDomainState(host, &unused, &static_pkp_state))
    return static_pkp_state.CheckPublicKeyPins(hashes, failure_log);

  // HasPublicKeyPins should have returned true in order for this method
  // to have been called, so if we fall through to here, it's an error.
  return false;
}

bool TransportSecurityState::GetStaticDomainState(const std::string& host,
                                                  STSState* sts_state,
                                                  PKPState* pkp_state) const {
  DCHECK(CalledOnValidThread());

  sts_state->upgrade_mode = STSState::MODE_FORCE_HTTPS;
  sts_state->include_subdomains = false;
  pkp_state->include_subdomains = false;

  if (!IsBuildTimely())
    return false;

  PreloadResult result;
  if (!DecodeHSTSPreload(host, &result))
    return false;

  sts_state->domain = host.substr(result.hostname_offset);
  pkp_state->domain = sts_state->domain;
  sts_state->include_subdomains = result.sts_include_subdomains;
  sts_state->last_observed = base::GetBuildTime();
  sts_state->upgrade_mode = STSState::MODE_DEFAULT;
  if (result.force_https) {
    sts_state->upgrade_mode = STSState::MODE_FORCE_HTTPS;
  }

  if (enable_static_pins_ && result.has_pins) {
    pkp_state->include_subdomains = result.pkp_include_subdomains;
    pkp_state->last_observed = base::GetBuildTime();

    if (result.pinset_id >= arraysize(kPinsets))
      return false;
    const Pinset *pinset = &kPinsets[result.pinset_id];

    if (pinset->accepted_pins) {
      const char* const* sha1_hash = pinset->accepted_pins;
      while (*sha1_hash) {
        AddHash(*sha1_hash, &pkp_state->spki_hashes);
        sha1_hash++;
      }
    }
    if (pinset->rejected_pins) {
      const char* const* sha1_hash = pinset->rejected_pins;
      while (*sha1_hash) {
        AddHash(*sha1_hash, &pkp_state->bad_spki_hashes);
        sha1_hash++;
      }
    }
  }

  return true;
}

bool TransportSecurityState::GetDynamicSTSState(const std::string& host,
                                                STSState* result) {
  DCHECK(CalledOnValidThread());

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  base::Time current_time(base::Time::Now());

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    std::string host_sub_chunk(&canonicalized_host[i],
                               canonicalized_host.size() - i);
    STSStateMap::iterator j = enabled_sts_hosts_.find(HashHost(host_sub_chunk));
    if (j == enabled_sts_hosts_.end())
      continue;

    // If the entry is invalid, drop it.
    if (current_time > j->second.expiry) {
      enabled_sts_hosts_.erase(j);
      DirtyNotify();
      continue;
    }

    // If this is the most specific STS match, add it to the result. Note: a STS
    // entry at a more specific domain overrides a less specific domain whether
    // or not |include_subdomains| is set.
    if (current_time <= j->second.expiry) {
      if (i == 0 || j->second.include_subdomains) {
        *result = j->second;
        result->domain = DNSDomainToString(host_sub_chunk);
        return true;
      }

      break;
    }
  }

  return false;
}

bool TransportSecurityState::GetDynamicPKPState(const std::string& host,
                                                PKPState* result) {
  DCHECK(CalledOnValidThread());

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  base::Time current_time(base::Time::Now());

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    std::string host_sub_chunk(&canonicalized_host[i],
                               canonicalized_host.size() - i);
    PKPStateMap::iterator j = enabled_pkp_hosts_.find(HashHost(host_sub_chunk));
    if (j == enabled_pkp_hosts_.end())
      continue;

    // If the entry is invalid, drop it.
    if (current_time > j->second.expiry) {
      enabled_pkp_hosts_.erase(j);
      DirtyNotify();
      continue;
    }

    // If this is the most specific PKP match, add it to the result. Note: a PKP
    // entry at a more specific domain overrides a less specific domain whether
    // or not |include_subdomains| is set.
    if (current_time <= j->second.expiry) {
      if (i == 0 || j->second.include_subdomains) {
        *result = j->second;
        result->domain = DNSDomainToString(host_sub_chunk);
        return true;
      }

      break;
    }
  }

  return false;
}

void TransportSecurityState::AddOrUpdateEnabledSTSHosts(
    const std::string& hashed_host,
    const STSState& state) {
  DCHECK(CalledOnValidThread());
  DCHECK(state.ShouldUpgradeToSSL());
  enabled_sts_hosts_[hashed_host] = state;
}

void TransportSecurityState::AddOrUpdateEnabledPKPHosts(
    const std::string& hashed_host,
    const PKPState& state) {
  DCHECK(CalledOnValidThread());
  DCHECK(state.HasPublicKeyPins());
  enabled_pkp_hosts_[hashed_host] = state;
}

TransportSecurityState::STSState::STSState()
    : upgrade_mode(MODE_DEFAULT), include_subdomains(false) {
}

TransportSecurityState::STSState::~STSState() {
}

bool TransportSecurityState::STSState::ShouldUpgradeToSSL() const {
  return upgrade_mode == MODE_FORCE_HTTPS;
}

bool TransportSecurityState::STSState::ShouldSSLErrorsBeFatal() const {
  return true;
}

TransportSecurityState::STSStateIterator::STSStateIterator(
    const TransportSecurityState& state)
    : iterator_(state.enabled_sts_hosts_.begin()),
      end_(state.enabled_sts_hosts_.end()) {
}

TransportSecurityState::STSStateIterator::~STSStateIterator() {
}

TransportSecurityState::PKPState::PKPState() : include_subdomains(false) {
}

TransportSecurityState::PKPState::~PKPState() {
}

bool TransportSecurityState::PKPState::CheckPublicKeyPins(
    const HashValueVector& hashes,
    std::string* failure_log) const {
  // Validate that hashes is not empty. By the time this code is called (in
  // production), that should never happen, but it's good to be defensive.
  // And, hashes *can* be empty in some test scenarios.
  if (hashes.empty()) {
    failure_log->append(
        "Rejecting empty public key chain for public-key-pinned domains: " +
        domain);
    return false;
  }

  if (HashesIntersect(bad_spki_hashes, hashes)) {
    failure_log->append("Rejecting public key chain for domain " + domain +
                        ". Validated chain: " + HashesToBase64String(hashes) +
                        ", matches one or more bad hashes: " +
                        HashesToBase64String(bad_spki_hashes));
    return false;
  }

  // If there are no pins, then any valid chain is acceptable.
  if (spki_hashes.empty())
    return true;

  if (HashesIntersect(spki_hashes, hashes)) {
    return true;
  }

  failure_log->append("Rejecting public key chain for domain " + domain +
                      ". Validated chain: " + HashesToBase64String(hashes) +
                      ", expected: " + HashesToBase64String(spki_hashes));
  return false;
}

bool TransportSecurityState::PKPState::HasPublicKeyPins() const {
  return spki_hashes.size() > 0 || bad_spki_hashes.size() > 0;
}

bool TransportSecurityState::PKPState::ShouldSSLErrorsBeFatal() const {
  return true;
}

TransportSecurityState::PKPStateIterator::PKPStateIterator(
    const TransportSecurityState& state)
    : iterator_(state.enabled_pkp_hosts_.begin()),
      end_(state.enabled_pkp_hosts_.end()) {
}

TransportSecurityState::PKPStateIterator::~PKPStateIterator() {
}

}  // namespace
