// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_WEBSOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_WEBSOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_

#include <list>
#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/net_export.h"
#include "net/log/net_log.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/transport_client_socket_pool.h"

namespace net {

class ClientSocketFactory;
class HostResolver;
class NetLog;
class WebSocketEndpointLockManager;
class WebSocketTransportConnectSubJob;

// WebSocketTransportConnectJob handles the host resolution necessary for socket
// creation and the TCP connect. WebSocketTransportConnectJob also has fallback
// logic for IPv6 connect() timeouts (which may happen due to networks / routers
// with broken IPv6 support). Those timeouts take 20s, so rather than make the
// user wait 20s for the timeout to fire, we use a fallback timer
// (kIPv6FallbackTimerInMs) and start a connect() to an IPv4 address if the
// timer fires. Then we race the IPv4 connect(s) against the IPv6 connect(s) and
// use the socket that completes successfully first or fails last.
class NET_EXPORT_PRIVATE WebSocketTransportConnectJob : public ConnectJob {
 public:
  WebSocketTransportConnectJob(
      const std::string& group_name,
      RequestPriority priority,
      const scoped_refptr<TransportSocketParams>& params,
      base::TimeDelta timeout_duration,
      const CompletionCallback& callback,
      ClientSocketFactory* client_socket_factory,
      HostResolver* host_resolver,
      ClientSocketHandle* handle,
      Delegate* delegate,
      NetLog* pool_net_log,
      const BoundNetLog& request_net_log);
  ~WebSocketTransportConnectJob() override;

  // Unlike normal socket pools, the WebSocketTransportClientPool uses
  // early-binding of sockets.
  ClientSocketHandle* handle() const { return handle_; }

  // Stash the callback from RequestSocket() here for convenience.
  const CompletionCallback& callback() const { return callback_; }

  const BoundNetLog& request_net_log() const { return request_net_log_; }

  // ConnectJob methods.
  LoadState GetLoadState() const override;

 private:
  friend class WebSocketTransportConnectSubJob;
  friend class TransportConnectJobHelper;
  friend class WebSocketEndpointLockManager;

  // Although it is not strictly necessary, it makes the code simpler if each
  // subjob knows what type it is.
  enum SubJobType { SUB_JOB_IPV4, SUB_JOB_IPV6 };

  int DoResolveHost();
  int DoResolveHostComplete(int result);
  int DoTransportConnect();
  int DoTransportConnectComplete(int result);

  // Called back from a SubJob when it completes.
  void OnSubJobComplete(int result, WebSocketTransportConnectSubJob* job);

  // Called from |fallback_timer_|.
  void StartIPv4JobAsync();

  // Begins the host resolution and the TCP connect.  Returns OK on success
  // and ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  int ConnectInternal() override;

  TransportConnectJobHelper helper_;

  // The addresses are divided into IPv4 and IPv6, which are performed partially
  // in parallel. If the list of IPv6 addresses is non-empty, then the IPv6 jobs
  // go first, followed after |kIPv6FallbackTimerInMs| by the IPv4
  // addresses. First sub-job to establish a connection wins.
  scoped_ptr<WebSocketTransportConnectSubJob> ipv4_job_;
  scoped_ptr<WebSocketTransportConnectSubJob> ipv6_job_;

  base::OneShotTimer<WebSocketTransportConnectJob> fallback_timer_;
  TransportConnectJobHelper::ConnectionLatencyHistogram race_result_;
  ClientSocketHandle* const handle_;
  CompletionCallback callback_;
  BoundNetLog request_net_log_;

  bool had_ipv4_;
  bool had_ipv6_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketTransportConnectJob);
};

class NET_EXPORT_PRIVATE WebSocketTransportClientSocketPool
    : public TransportClientSocketPool {
 public:
  WebSocketTransportClientSocketPool(int max_sockets,
                                     int max_sockets_per_group,
                                     HostResolver* host_resolver,
                                     ClientSocketFactory* client_socket_factory,
                                     NetLog* net_log);

  ~WebSocketTransportClientSocketPool() override;

  // Allow another connection to be started to the IPEndPoint that this |handle|
  // is connected to. Used when the WebSocket handshake completes successfully.
  // This only works if the socket is connected, however the caller does not
  // need to explicitly check for this. Instead, ensure that dead sockets are
  // returned to ReleaseSocket() in a timely fashion.
  static void UnlockEndpoint(ClientSocketHandle* handle);

  // ClientSocketPool implementation.
  int RequestSocket(const std::string& group_name,
                    const void* resolve_info,
                    RequestPriority priority,
                    ClientSocketHandle* handle,
                    const CompletionCallback& callback,
                    const BoundNetLog& net_log) override;
  void RequestSockets(const std::string& group_name,
                      const void* params,
                      int num_sockets,
                      const BoundNetLog& net_log) override;
  void CancelRequest(const std::string& group_name,
                     ClientSocketHandle* handle) override;
  void ReleaseSocket(const std::string& group_name,
                     scoped_ptr<StreamSocket> socket,
                     int id) override;
  void FlushWithError(int error) override;
  void CloseIdleSockets() override;
  int IdleSocketCount() const override;
  int IdleSocketCountInGroup(const std::string& group_name) const override;
  LoadState GetLoadState(const std::string& group_name,
                         const ClientSocketHandle* handle) const override;
  scoped_ptr<base::DictionaryValue> GetInfoAsValue(
      const std::string& name,
      const std::string& type,
      bool include_nested_pools) const override;
  base::TimeDelta ConnectionTimeout() const override;

  // HigherLayeredPool implementation.
  bool IsStalled() const override;

 private:
  class ConnectJobDelegate : public ConnectJob::Delegate {
   public:
    explicit ConnectJobDelegate(WebSocketTransportClientSocketPool* owner);
    ~ConnectJobDelegate() override;

    void OnConnectJobComplete(int result, ConnectJob* job) override;

   private:
    WebSocketTransportClientSocketPool* owner_;

    DISALLOW_COPY_AND_ASSIGN(ConnectJobDelegate);
  };

  // Store the arguments from a call to RequestSocket() that has stalled so we
  // can replay it when there are available socket slots.
  struct StalledRequest {
    StalledRequest(const scoped_refptr<TransportSocketParams>& params,
                   RequestPriority priority,
                   ClientSocketHandle* handle,
                   const CompletionCallback& callback,
                   const BoundNetLog& net_log);
    ~StalledRequest();
    const scoped_refptr<TransportSocketParams> params;
    const RequestPriority priority;
    ClientSocketHandle* const handle;
    const CompletionCallback callback;
    const BoundNetLog net_log;
  };
  friend class ConnectJobDelegate;
  typedef std::map<const ClientSocketHandle*, WebSocketTransportConnectJob*>
      PendingConnectsMap;
  // This is a list so that we can remove requests from the middle, and also
  // so that iterators are not invalidated unless the corresponding request is
  // removed.
  typedef std::list<StalledRequest> StalledRequestQueue;
  typedef std::map<const ClientSocketHandle*, StalledRequestQueue::iterator>
      StalledRequestMap;

  void OnConnectJobComplete(int result, WebSocketTransportConnectJob* job);
  void InvokeUserCallbackLater(ClientSocketHandle* handle,
                               const CompletionCallback& callback,
                               int rv);
  void InvokeUserCallback(ClientSocketHandle* handle,
                          const CompletionCallback& callback,
                          int rv);
  bool ReachedMaxSocketsLimit() const;
  void HandOutSocket(scoped_ptr<StreamSocket> socket,
                     const LoadTimingInfo::ConnectTiming& connect_timing,
                     ClientSocketHandle* handle,
                     const BoundNetLog& net_log);
  void AddJob(ClientSocketHandle* handle,
              scoped_ptr<WebSocketTransportConnectJob> connect_job);
  bool DeleteJob(ClientSocketHandle* handle);
  const WebSocketTransportConnectJob* LookupConnectJob(
      const ClientSocketHandle* handle) const;
  void ActivateStalledRequest();
  bool DeleteStalledRequest(ClientSocketHandle* handle);

  ConnectJobDelegate connect_job_delegate_;
  std::set<const ClientSocketHandle*> pending_callbacks_;
  PendingConnectsMap pending_connects_;
  StalledRequestQueue stalled_request_queue_;
  StalledRequestMap stalled_request_map_;
  NetLog* const pool_net_log_;
  ClientSocketFactory* const client_socket_factory_;
  HostResolver* const host_resolver_;
  const int max_sockets_;
  int handed_out_socket_count_;
  bool flushing_;

  base::WeakPtrFactory<WebSocketTransportClientSocketPool> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketTransportClientSocketPool);
};

}  // namespace net

#endif  // NET_SOCKET_WEBSOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_
