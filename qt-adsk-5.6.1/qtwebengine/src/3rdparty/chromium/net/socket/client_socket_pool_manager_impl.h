// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_IMPL_H_
#define NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_IMPL_H_

#include <map>
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/template_util.h"
#include "base/threading/non_thread_safe.h"
#include "net/cert/cert_database.h"
#include "net/http/http_network_session.h"
#include "net/socket/client_socket_pool_manager.h"

namespace net {

class CertVerifier;
class ChannelIDService;
class ClientSocketFactory;
class CTVerifier;
class HttpProxyClientSocketPool;
class HostResolver;
class NetLog;
class SOCKSClientSocketPool;
class SSLClientSocketPool;
class SSLConfigService;
class TransportClientSocketPool;
class TransportSecurityState;

namespace internal {

// A helper class for auto-deleting Values in the destructor.
template <typename Key, typename Value>
class OwnedPoolMap : public std::map<Key, Value> {
 public:
  OwnedPoolMap() {
    static_assert(base::is_pointer<Value>::value, "value must be a pointer");
  }

  ~OwnedPoolMap() {
    STLDeleteValues(this);
  }
};

}  // namespace internal

class ClientSocketPoolManagerImpl : public base::NonThreadSafe,
                                    public ClientSocketPoolManager,
                                    public CertDatabase::Observer {
 public:
  ClientSocketPoolManagerImpl(NetLog* net_log,
                              ClientSocketFactory* socket_factory,
                              HostResolver* host_resolver,
                              CertVerifier* cert_verifier,
                              ChannelIDService* channel_id_service,
                              TransportSecurityState* transport_security_state,
                              CTVerifier* cert_transparency_verifier,
                              CertPolicyEnforcer* cert_policy_enforcer,
                              const std::string& ssl_session_cache_shard,
                              SSLConfigService* ssl_config_service,
                              HttpNetworkSession::SocketPoolType pool_type);
  ~ClientSocketPoolManagerImpl() override;

  void FlushSocketPoolsWithError(int error) override;
  void CloseIdleSockets() override;

  TransportClientSocketPool* GetTransportSocketPool() override;

  SSLClientSocketPool* GetSSLSocketPool() override;

  SOCKSClientSocketPool* GetSocketPoolForSOCKSProxy(
      const HostPortPair& socks_proxy) override;

  HttpProxyClientSocketPool* GetSocketPoolForHTTPProxy(
      const HostPortPair& http_proxy) override;

  SSLClientSocketPool* GetSocketPoolForSSLWithProxy(
      const HostPortPair& proxy_server) override;

  // Creates a Value summary of the state of the socket pools.
  scoped_ptr<base::Value> SocketPoolInfoToValue() const override;

  // CertDatabase::Observer methods:
  void OnCertAdded(const X509Certificate* cert) override;
  void OnCACertChanged(const X509Certificate* cert) override;

 private:
  typedef internal::OwnedPoolMap<HostPortPair, TransportClientSocketPool*>
      TransportSocketPoolMap;
  typedef internal::OwnedPoolMap<HostPortPair, SOCKSClientSocketPool*>
      SOCKSSocketPoolMap;
  typedef internal::OwnedPoolMap<HostPortPair, HttpProxyClientSocketPool*>
      HTTPProxySocketPoolMap;
  typedef internal::OwnedPoolMap<HostPortPair, SSLClientSocketPool*>
      SSLSocketPoolMap;

  NetLog* const net_log_;
  ClientSocketFactory* const socket_factory_;
  HostResolver* const host_resolver_;
  CertVerifier* const cert_verifier_;
  ChannelIDService* const channel_id_service_;
  TransportSecurityState* const transport_security_state_;
  CTVerifier* const cert_transparency_verifier_;
  CertPolicyEnforcer* const cert_policy_enforcer_;
  const std::string ssl_session_cache_shard_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;
  const HttpNetworkSession::SocketPoolType pool_type_;

  // Note: this ordering is important.

  scoped_ptr<TransportClientSocketPool> transport_socket_pool_;
  scoped_ptr<SSLClientSocketPool> ssl_socket_pool_;
  TransportSocketPoolMap transport_socket_pools_for_socks_proxies_;
  SOCKSSocketPoolMap socks_socket_pools_;
  TransportSocketPoolMap transport_socket_pools_for_http_proxies_;
  TransportSocketPoolMap transport_socket_pools_for_https_proxies_;
  SSLSocketPoolMap ssl_socket_pools_for_https_proxies_;
  HTTPProxySocketPoolMap http_proxy_socket_pools_;
  SSLSocketPoolMap ssl_socket_pools_for_proxies_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPoolManagerImpl);
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_IMPL_H_
