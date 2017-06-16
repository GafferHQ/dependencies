// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_client_auth_cache.h"

#include "base/logging.h"
#include "net/cert/x509_certificate.h"

namespace net {

SSLClientAuthCache::SSLClientAuthCache() {
  CertDatabase::GetInstance()->AddObserver(this);
}

SSLClientAuthCache::~SSLClientAuthCache() {
  CertDatabase::GetInstance()->RemoveObserver(this);
}

bool SSLClientAuthCache::Lookup(
    const HostPortPair& server,
    scoped_refptr<X509Certificate>* certificate) {
  DCHECK(certificate);

  AuthCacheMap::iterator iter = cache_.find(server);
  if (iter == cache_.end())
    return false;

  *certificate = iter->second;
  return true;
}

void SSLClientAuthCache::Add(const HostPortPair& server,
                             X509Certificate* value) {
  cache_[server] = value;

  // TODO(wtc): enforce a maximum number of entries.
}

void SSLClientAuthCache::Remove(const HostPortPair& server) {
  cache_.erase(server);
}

void SSLClientAuthCache::OnCertAdded(const X509Certificate* cert) {
  cache_.clear();
}

}  // namespace net
