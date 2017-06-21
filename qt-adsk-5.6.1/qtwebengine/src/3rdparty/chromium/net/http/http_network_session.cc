// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_session.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/profiler/scoped_tracker.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_stream_factory_impl.h"
#include "net/http/url_security_manager.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_stream_factory.h"
#include "net/quic/quic_utils.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_pool_manager_impl.h"
#include "net/socket/next_proto.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_session_pool.h"

namespace net {

namespace {

ClientSocketPoolManager* CreateSocketPoolManager(
    HttpNetworkSession::SocketPoolType pool_type,
    const HttpNetworkSession::Params& params) {
  // TODO(yutak): Differentiate WebSocket pool manager and allow more
  // simultaneous connections for WebSockets.
  return new ClientSocketPoolManagerImpl(
      params.net_log,
      params.client_socket_factory ? params.client_socket_factory
                                   : ClientSocketFactory::GetDefaultFactory(),
      params.host_resolver, params.cert_verifier, params.channel_id_service,
      params.transport_security_state, params.cert_transparency_verifier,
      params.cert_policy_enforcer, params.ssl_session_cache_shard,
      params.ssl_config_service, pool_type);
}

}  // unnamed namespace

// The maximum receive window sizes for HTTP/2 sessions and streams.
const int32 kSpdySessionMaxRecvWindowSize = 15 * 1024 * 1024;  // 15 MB
const int32 kSpdyStreamMaxRecvWindowSize = 6 * 1024 * 1024;    //  6 MB
// QUIC's socket receive buffer size.
// We should adaptively set this buffer size, but for now, we'll use a size
// that seems large enough to receive data at line rate for most connections,
// and does not consume "too much" memory.
const int32 kQuicSocketReceiveBufferSize = 1024 * 1024;  // 1MB

// Number of recent connections to consider for certain thresholds
// that trigger disabling QUIC.  E.g. disable QUIC if PUBLIC_RESET was
// received post handshake for at least 2 of 20 recent connections.
const int32 kQuicMaxRecentDisabledReasons = 20;

HttpNetworkSession::Params::Params()
    : client_socket_factory(NULL),
      host_resolver(NULL),
      cert_verifier(NULL),
      cert_policy_enforcer(NULL),
      channel_id_service(NULL),
      transport_security_state(NULL),
      cert_transparency_verifier(NULL),
      proxy_service(NULL),
      ssl_config_service(NULL),
      http_auth_handler_factory(NULL),
      network_delegate(NULL),
      net_log(NULL),
      host_mapping_rules(NULL),
      ignore_certificate_errors(false),
      testing_fixed_http_port(0),
      testing_fixed_https_port(0),
      enable_tcp_fast_open_for_ssl(false),
      enable_spdy_compression(true),
      enable_spdy_ping_based_connection_checking(true),
      spdy_default_protocol(kProtoUnknown),
      spdy_session_max_recv_window_size(kSpdySessionMaxRecvWindowSize),
      spdy_stream_max_recv_window_size(kSpdyStreamMaxRecvWindowSize),
      spdy_initial_max_concurrent_streams(0),
      time_func(&base::TimeTicks::Now),
      use_alternate_protocols(false),
      alternative_service_probability_threshold(1),
      enable_quic(false),
      disable_insecure_quic(false),
      enable_quic_for_proxies(false),
      enable_quic_port_selection(true),
      quic_always_require_handshake_confirmation(false),
      quic_disable_connection_pooling(false),
      quic_load_server_info_timeout_srtt_multiplier(0.25f),
      quic_enable_connection_racing(false),
      quic_enable_non_blocking_io(false),
      quic_disable_disk_cache(false),
      quic_prefer_aes(false),
      quic_max_number_of_lossy_connections(0),
      quic_packet_loss_threshold(1.0f),
      quic_socket_receive_buffer_size(kQuicSocketReceiveBufferSize),
      quic_clock(NULL),
      quic_random(NULL),
      quic_max_packet_length(kDefaultMaxPacketSize),
      enable_user_alternate_protocol_ports(false),
      quic_crypto_client_stream_factory(NULL),
      quic_max_recent_disabled_reasons(kQuicMaxRecentDisabledReasons),
      quic_threshold_public_resets_post_handshake(0),
      quic_threshold_timeouts_streams_open(0),
      proxy_delegate(NULL) {
  quic_supported_versions.push_back(QUIC_VERSION_25);
}

HttpNetworkSession::Params::~Params() {}

// TODO(mbelshe): Move the socket factories into HttpStreamFactory.
HttpNetworkSession::HttpNetworkSession(const Params& params)
    : net_log_(params.net_log),
      network_delegate_(params.network_delegate),
      http_server_properties_(params.http_server_properties),
      cert_verifier_(params.cert_verifier),
      http_auth_handler_factory_(params.http_auth_handler_factory),
      proxy_service_(params.proxy_service),
      ssl_config_service_(params.ssl_config_service),
      normal_socket_pool_manager_(
          CreateSocketPoolManager(NORMAL_SOCKET_POOL, params)),
      websocket_socket_pool_manager_(
          CreateSocketPoolManager(WEBSOCKET_SOCKET_POOL, params)),
      quic_stream_factory_(
          params.host_resolver,
          params.client_socket_factory
              ? params.client_socket_factory
              : ClientSocketFactory::GetDefaultFactory(),
          params.http_server_properties,
          params.cert_verifier,
          params.channel_id_service,
          params.transport_security_state,
          params.quic_crypto_client_stream_factory,
          params.quic_random ? params.quic_random : QuicRandom::GetInstance(),
          params.quic_clock ? params.quic_clock : new QuicClock(),
          params.quic_max_packet_length,
          params.quic_user_agent_id,
          params.quic_supported_versions,
          params.enable_quic_port_selection,
          params.quic_always_require_handshake_confirmation,
          params.quic_disable_connection_pooling,
          params.quic_load_server_info_timeout_srtt_multiplier,
          params.quic_enable_connection_racing,
          params.quic_enable_non_blocking_io,
          params.quic_disable_disk_cache,
          params.quic_prefer_aes,
          params.quic_max_number_of_lossy_connections,
          params.quic_packet_loss_threshold,
          params.quic_max_recent_disabled_reasons,
          params.quic_threshold_public_resets_post_handshake,
          params.quic_threshold_timeouts_streams_open,
          params.quic_socket_receive_buffer_size,
          params.quic_connection_options),
      spdy_session_pool_(params.host_resolver,
                         params.ssl_config_service,
                         params.http_server_properties,
                         params.transport_security_state,
                         params.enable_spdy_compression,
                         params.enable_spdy_ping_based_connection_checking,
                         params.spdy_default_protocol,
                         params.spdy_session_max_recv_window_size,
                         params.spdy_stream_max_recv_window_size,
                         params.spdy_initial_max_concurrent_streams,
                         params.time_func,
                         params.trusted_spdy_proxy),
      http_stream_factory_(new HttpStreamFactoryImpl(this, false)),
      http_stream_factory_for_websocket_(new HttpStreamFactoryImpl(this, true)),
      params_(params) {
  DCHECK(proxy_service_);
  DCHECK(ssl_config_service_.get());
  CHECK(http_server_properties_);

  for (int i = ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION;
       i <= ALTERNATE_PROTOCOL_MAXIMUM_VALID_VERSION; ++i) {
    enabled_protocols_[i - ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION] = false;
  }

  // TODO(rtenneti): bug 116575 - consider combining the NextProto and
  // AlternateProtocol.
  for (std::vector<NextProto>::const_iterator it = params_.next_protos.begin();
       it != params_.next_protos.end(); ++it) {
    NextProto proto = *it;

    // Add the protocol to the TLS next protocol list, except for QUIC
    // since it uses UDP.
    if (proto != kProtoQUIC1SPDY3) {
      next_protos_.push_back(proto);
    }

    // Enable the corresponding alternate protocol, except for HTTP
    // which has not corresponding alternative.
    if (proto != kProtoHTTP11) {
      AlternateProtocol alternate = AlternateProtocolFromNextProto(proto);
      if (!IsAlternateProtocolValid(alternate)) {
        NOTREACHED() << "Invalid next proto: " << proto;
        continue;
      }
      enabled_protocols_[alternate - ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION] =
          true;
    }
  }

  http_server_properties_->SetAlternativeServiceProbabilityThreshold(
      params.alternative_service_probability_threshold);
}

HttpNetworkSession::~HttpNetworkSession() {
  STLDeleteElements(&response_drainers_);
  spdy_session_pool_.CloseAllSessions();
}

void HttpNetworkSession::AddResponseDrainer(HttpResponseBodyDrainer* drainer) {
  DCHECK(!ContainsKey(response_drainers_, drainer));
  response_drainers_.insert(drainer);
}

void HttpNetworkSession::RemoveResponseDrainer(
    HttpResponseBodyDrainer* drainer) {
  DCHECK(ContainsKey(response_drainers_, drainer));
  response_drainers_.erase(drainer);
}

TransportClientSocketPool* HttpNetworkSession::GetTransportSocketPool(
    SocketPoolType pool_type) {
  return GetSocketPoolManager(pool_type)->GetTransportSocketPool();
}

SSLClientSocketPool* HttpNetworkSession::GetSSLSocketPool(
    SocketPoolType pool_type) {
  return GetSocketPoolManager(pool_type)->GetSSLSocketPool();
}

SOCKSClientSocketPool* HttpNetworkSession::GetSocketPoolForSOCKSProxy(
    SocketPoolType pool_type,
    const HostPortPair& socks_proxy) {
  return GetSocketPoolManager(pool_type)->GetSocketPoolForSOCKSProxy(
      socks_proxy);
}

HttpProxyClientSocketPool* HttpNetworkSession::GetSocketPoolForHTTPProxy(
    SocketPoolType pool_type,
    const HostPortPair& http_proxy) {
  return GetSocketPoolManager(pool_type)->GetSocketPoolForHTTPProxy(http_proxy);
}

SSLClientSocketPool* HttpNetworkSession::GetSocketPoolForSSLWithProxy(
    SocketPoolType pool_type,
    const HostPortPair& proxy_server) {
  return GetSocketPoolManager(pool_type)->GetSocketPoolForSSLWithProxy(
      proxy_server);
}

scoped_ptr<base::Value> HttpNetworkSession::SocketPoolInfoToValue() const {
  // TODO(yutak): Should merge values from normal pools and WebSocket pools.
  return normal_socket_pool_manager_->SocketPoolInfoToValue();
}

scoped_ptr<base::Value> HttpNetworkSession::SpdySessionPoolInfoToValue() const {
  return spdy_session_pool_.SpdySessionPoolInfoToValue();
}

scoped_ptr<base::Value> HttpNetworkSession::QuicInfoToValue() const {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->Set("sessions", quic_stream_factory_.QuicStreamFactoryInfoToValue());
  dict->SetBoolean("quic_enabled", params_.enable_quic);
  dict->SetBoolean("quic_enabled_for_proxies", params_.enable_quic_for_proxies);
  dict->SetBoolean("enable_quic_port_selection",
                   params_.enable_quic_port_selection);
  scoped_ptr<base::ListValue> connection_options(new base::ListValue);
  for (QuicTagVector::const_iterator it =
           params_.quic_connection_options.begin();
       it != params_.quic_connection_options.end(); ++it) {
    connection_options->AppendString("'" + QuicUtils::TagToString(*it) + "'");
  }
  dict->Set("connection_options", connection_options.Pass());
  dict->SetString("origin_to_force_quic_on",
                  params_.origin_to_force_quic_on.ToString());
  dict->SetDouble("alternative_service_probability_threshold",
                  params_.alternative_service_probability_threshold);
  dict->SetString("disabled_reason",
                  quic_stream_factory_.QuicDisabledReasonString());
  return dict.Pass();
}

void HttpNetworkSession::CloseAllConnections() {
  normal_socket_pool_manager_->FlushSocketPoolsWithError(ERR_ABORTED);
  websocket_socket_pool_manager_->FlushSocketPoolsWithError(ERR_ABORTED);
  spdy_session_pool_.CloseCurrentSessions(ERR_ABORTED);
  quic_stream_factory_.CloseAllSessions(ERR_ABORTED);
}

void HttpNetworkSession::CloseIdleConnections() {
  normal_socket_pool_manager_->CloseIdleSockets();
  websocket_socket_pool_manager_->CloseIdleSockets();
  spdy_session_pool_.CloseCurrentIdleSessions();
}

bool HttpNetworkSession::IsProtocolEnabled(AlternateProtocol protocol) const {
  DCHECK(IsAlternateProtocolValid(protocol));
  return enabled_protocols_[
      protocol - ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION];
}

void HttpNetworkSession::GetNextProtos(NextProtoVector* next_protos) const {
  if (HttpStreamFactory::spdy_enabled()) {
    *next_protos = next_protos_;
  } else {
    next_protos->clear();
  }
}

bool HttpNetworkSession::HasSpdyExclusion(
    HostPortPair host_port_pair) const {
  return params_.forced_spdy_exclusions.find(host_port_pair) !=
      params_.forced_spdy_exclusions.end();
}

ClientSocketPoolManager* HttpNetworkSession::GetSocketPoolManager(
    SocketPoolType pool_type) {
  switch (pool_type) {
    case NORMAL_SOCKET_POOL:
      return normal_socket_pool_manager_.get();
    case WEBSOCKET_SOCKET_POOL:
      return websocket_socket_pool_manager_.get();
    default:
      NOTREACHED();
      break;
  }
  return NULL;
}

}  // namespace net
