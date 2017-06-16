// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/linked_hash_map.h"
#include "net/base/net_export.h"
#include "net/http/http_server_properties.h"

namespace base {
class ListValue;
}

namespace BASE_HASH_NAMESPACE {

template <>
struct hash<net::AlternativeService> {
  size_t operator()(const net::AlternativeService& entry) const {
    return entry.protocol ^ hash<std::string>()(entry.host) ^ entry.port;
  }
};

}  // namespace BASE_HASH_NAMESPACE

namespace net {

// The implementation for setting/retrieving the HTTP server properties.
class NET_EXPORT HttpServerPropertiesImpl
    : public HttpServerProperties,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  HttpServerPropertiesImpl();
  ~HttpServerPropertiesImpl() override;

  // Initializes |spdy_servers_map_| with the servers (host/port) from
  // |spdy_servers| that either support SPDY or not.
  void InitializeSpdyServers(std::vector<std::string>* spdy_servers,
                             bool support_spdy);

  void InitializeAlternativeServiceServers(
      AlternativeServiceMap* alternate_protocol_servers);

  void InitializeSpdySettingsServers(SpdySettingsMap* spdy_settings_map);

  void InitializeSupportsQuic(IPAddressNumber* last_address);

  void InitializeServerNetworkStats(
      ServerNetworkStatsMap* server_network_stats_map);

  // Get the list of servers (host/port) that support SPDY. The max_size is the
  // number of MRU servers that support SPDY that are to be returned.
  void GetSpdyServerList(base::ListValue* spdy_server_list,
                         size_t max_size) const;

  // Returns flattened string representation of the |host_port_pair|. Used by
  // unittests.
  static std::string GetFlattenedSpdyServer(const HostPortPair& host_port_pair);

  // Returns the canonical host suffix for |host|, or std::string() if none
  // exists.
  std::string GetCanonicalSuffix(const std::string& host);

  // -----------------------------
  // HttpServerProperties methods:
  // -----------------------------

  base::WeakPtr<HttpServerProperties> GetWeakPtr() override;
  void Clear() override;
  bool SupportsRequestPriority(const HostPortPair& server) override;
  bool GetSupportsSpdy(const HostPortPair& server) override;
  void SetSupportsSpdy(const HostPortPair& server, bool support_spdy) override;
  bool RequiresHTTP11(const HostPortPair& server) override;
  void SetHTTP11Required(const HostPortPair& server) override;
  void MaybeForceHTTP11(const HostPortPair& server,
                        SSLConfig* ssl_config) override;
  AlternativeServiceVector GetAlternativeServices(
      const HostPortPair& origin) override;
  bool SetAlternativeService(const HostPortPair& origin,
                             const AlternativeService& alternative_service,
                             double alternative_probability) override;
  bool SetAlternativeServices(const HostPortPair& origin,
                              const AlternativeServiceInfoVector&
                                  alternative_service_info_vector) override;
  void MarkAlternativeServiceBroken(
      const AlternativeService& alternative_service) override;
  void MarkAlternativeServiceRecentlyBroken(
      const AlternativeService& alternative_service) override;
  bool IsAlternativeServiceBroken(
      const AlternativeService& alternative_service) const override;
  bool WasAlternativeServiceRecentlyBroken(
      const AlternativeService& alternative_service) override;
  void ConfirmAlternativeService(
      const AlternativeService& alternative_service) override;
  void ClearAlternativeServices(const HostPortPair& origin) override;
  const AlternativeServiceMap& alternative_service_map() const override;
  scoped_ptr<base::Value> GetAlternativeServiceInfoAsValue() const override;
  void SetAlternativeServiceProbabilityThreshold(double threshold) override;
  const SettingsMap& GetSpdySettings(
      const HostPortPair& host_port_pair) override;
  bool SetSpdySetting(const HostPortPair& host_port_pair,
                      SpdySettingsIds id,
                      SpdySettingsFlags flags,
                      uint32 value) override;
  void ClearSpdySettings(const HostPortPair& host_port_pair) override;
  void ClearAllSpdySettings() override;
  const SpdySettingsMap& spdy_settings_map() const override;
  bool GetSupportsQuic(IPAddressNumber* last_address) const override;
  void SetSupportsQuic(bool used_quic, const IPAddressNumber& address) override;
  void SetServerNetworkStats(const HostPortPair& host_port_pair,
                             ServerNetworkStats stats) override;
  const ServerNetworkStats* GetServerNetworkStats(
      const HostPortPair& host_port_pair) override;
  const ServerNetworkStatsMap& server_network_stats_map() const override;

 private:
  friend class HttpServerPropertiesImplPeer;

  // |spdy_servers_map_| has flattened representation of servers (host, port)
  // that either support or not support SPDY protocol.
  typedef base::MRUCache<std::string, bool> SpdyServerHostPortMap;
  typedef std::map<HostPortPair, HostPortPair> CanonicalHostMap;
  typedef std::vector<std::string> CanonicalSufficList;
  typedef std::set<HostPortPair> Http11ServerHostPortSet;

  // Linked hash map from AlternativeService to expiration time.  This container
  // is a queue with O(1) enqueue and dequeue, and a hash_map with O(1) lookup
  // at the same time.
  typedef linked_hash_map<AlternativeService, base::TimeTicks>
      BrokenAlternativeServices;
  // Map to the number of times each alternative service has been marked broken.
  typedef std::map<AlternativeService, int> RecentlyBrokenAlternativeServices;

  // Return the iterator for |server|, or for its canonical host, or end.
  AlternativeServiceMap::const_iterator GetAlternateProtocolIterator(
      const HostPortPair& server);

  // Return the canonical host for |server|, or end if none exists.
  CanonicalHostMap::const_iterator GetCanonicalHost(HostPortPair server) const;

  void RemoveCanonicalHost(const HostPortPair& server);
  void ExpireBrokenAlternateProtocolMappings();
  void ScheduleBrokenAlternateProtocolMappingsExpiration();

  SpdyServerHostPortMap spdy_servers_map_;
  Http11ServerHostPortSet http11_servers_;

  AlternativeServiceMap alternative_service_map_;
  BrokenAlternativeServices broken_alternative_services_;
  // Class invariant:  Every alternative service in broken_alternative_services_
  // must also be in recently_broken_alternative_services_.
  RecentlyBrokenAlternativeServices recently_broken_alternative_services_;

  IPAddressNumber last_quic_address_;
  SpdySettingsMap spdy_settings_map_;
  ServerNetworkStatsMap server_network_stats_map_;
  // Contains a map of servers which could share the same alternate protocol.
  // Map from a Canonical host/port (host is some postfix of host names) to an
  // actual origin, which has a plausible alternate protocol mapping.
  CanonicalHostMap canonical_host_to_origin_map_;
  // Contains list of suffixes (for exmaple ".c.youtube.com",
  // ".googlevideo.com", ".googleusercontent.com") of canonical hostnames.
  CanonicalSufficList canonical_suffixes_;

  double alternative_service_probability_threshold_;

  base::WeakPtrFactory<HttpServerPropertiesImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesImpl);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_
