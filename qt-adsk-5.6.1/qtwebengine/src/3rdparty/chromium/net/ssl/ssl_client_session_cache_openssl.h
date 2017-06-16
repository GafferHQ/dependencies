// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_CLIENT_SESSION_CACHE_OPENSSL_H
#define NET_SSL_SSL_CLIENT_SESSION_CACHE_OPENSSL_H

#include <openssl/ssl.h>

#include <string>

#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/ssl/scoped_openssl_types.h"

namespace base {
class Clock;
}

namespace net {

class NET_EXPORT SSLClientSessionCacheOpenSSL {
 public:
  struct Config {
    // The maximum number of entries in the cache.
    size_t max_entries = 1024;
    // The number of calls to Lookup before a new check for expired sessions.
    size_t expiration_check_count = 256;
    // How long each session should last.
    base::TimeDelta timeout = base::TimeDelta::FromHours(1);
  };

  explicit SSLClientSessionCacheOpenSSL(const Config& config);
  ~SSLClientSessionCacheOpenSSL();

  size_t size() const;

  // Returns the session associated with |cache_key| and moves it to the front
  // of the MRU list. Returns null if there is none. The caller is responsible
  // for taking a reference to the pointer if the cache is destroyed or a call
  // to Insert is made.
  SSL_SESSION* Lookup(const std::string& cache_key);

  // Inserts |session| into the cache at |cache_key|. If there is an existing
  // one, it is released. Every |expiration_check_count| calls, the cache is
  // checked for stale entries.
  void Insert(const std::string& cache_key, SSL_SESSION* session);

  // Removes all entries from the cache.
  void Flush();

  void SetClockForTesting(scoped_ptr<base::Clock> clock);

 private:
  struct CacheEntry {
    CacheEntry();
    ~CacheEntry();

    ScopedSSL_SESSION session;
    // The time at which this entry was created.
    base::Time creation_time;
  };

  using CacheEntryMap =
      base::MRUCacheBase<std::string,
                         CacheEntry*,
                         base::MRUCachePointerDeletor<CacheEntry*>,
                         base::MRUCacheHashMap>;

  // Returns true if |entry| is expired as of |now|.
  bool IsExpired(CacheEntry* entry, const base::Time& now);

  // Removes all expired sessions from the cache.
  void FlushExpiredSessions();

  scoped_ptr<base::Clock> clock_;
  Config config_;
  CacheEntryMap cache_;
  size_t lookups_since_flush_;

  // TODO(davidben): After https://crbug.com/458365 is fixed, replace this with
  // a ThreadChecker. The session cache should be single-threaded like other
  // classes in net.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientSessionCacheOpenSSL);
};

}  // namespace net

#endif  // NET_SSL_SSL_CLIENT_SESSION_CACHE_OPENSSL_H
