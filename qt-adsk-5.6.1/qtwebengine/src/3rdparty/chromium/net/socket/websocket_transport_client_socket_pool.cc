// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_transport_client_socket_pool.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/socket/websocket_transport_connect_sub_job.h"

namespace net {

namespace {

using base::TimeDelta;

// TODO(ricea): For now, we implement a global timeout for compatability with
// TransportConnectJob. Since WebSocketTransportConnectJob controls the address
// selection process more tightly, it could do something smarter here.
const int kTransportConnectJobTimeoutInSeconds = 240;  // 4 minutes.

}  // namespace

WebSocketTransportConnectJob::WebSocketTransportConnectJob(
    const std::string& group_name,
    RequestPriority priority,
    const scoped_refptr<TransportSocketParams>& params,
    TimeDelta timeout_duration,
    const CompletionCallback& callback,
    ClientSocketFactory* client_socket_factory,
    HostResolver* host_resolver,
    ClientSocketHandle* handle,
    Delegate* delegate,
    NetLog* pool_net_log,
    const BoundNetLog& request_net_log)
    : ConnectJob(group_name,
                 timeout_duration,
                 priority,
                 delegate,
                 BoundNetLog::Make(pool_net_log, NetLog::SOURCE_CONNECT_JOB)),
      helper_(params, client_socket_factory, host_resolver, &connect_timing_),
      race_result_(TransportConnectJobHelper::CONNECTION_LATENCY_UNKNOWN),
      handle_(handle),
      callback_(callback),
      request_net_log_(request_net_log),
      had_ipv4_(false),
      had_ipv6_(false) {
  helper_.SetOnIOComplete(this);
}

WebSocketTransportConnectJob::~WebSocketTransportConnectJob() {}

LoadState WebSocketTransportConnectJob::GetLoadState() const {
  LoadState load_state = LOAD_STATE_RESOLVING_HOST;
  if (ipv6_job_)
    load_state = ipv6_job_->GetLoadState();
  // This method should return LOAD_STATE_CONNECTING in preference to
  // LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET when possible because "waiting for
  // available socket" implies that nothing is happening.
  if (ipv4_job_ && load_state != LOAD_STATE_CONNECTING)
    load_state = ipv4_job_->GetLoadState();
  return load_state;
}

int WebSocketTransportConnectJob::DoResolveHost() {
  return helper_.DoResolveHost(priority(), net_log());
}

int WebSocketTransportConnectJob::DoResolveHostComplete(int result) {
  return helper_.DoResolveHostComplete(result, net_log());
}

int WebSocketTransportConnectJob::DoTransportConnect() {
  AddressList ipv4_addresses;
  AddressList ipv6_addresses;
  int result = ERR_UNEXPECTED;
  helper_.set_next_state(
      TransportConnectJobHelper::STATE_TRANSPORT_CONNECT_COMPLETE);

  for (AddressList::const_iterator it = helper_.addresses().begin();
       it != helper_.addresses().end();
       ++it) {
    switch (it->GetFamily()) {
      case ADDRESS_FAMILY_IPV4:
        ipv4_addresses.push_back(*it);
        break;

      case ADDRESS_FAMILY_IPV6:
        ipv6_addresses.push_back(*it);
        break;

      default:
        DVLOG(1) << "Unexpected ADDRESS_FAMILY: " << it->GetFamily();
        break;
    }
  }

  if (!ipv4_addresses.empty()) {
    had_ipv4_ = true;
    ipv4_job_.reset(new WebSocketTransportConnectSubJob(
        ipv4_addresses, this, SUB_JOB_IPV4));
  }

  if (!ipv6_addresses.empty()) {
    had_ipv6_ = true;
    ipv6_job_.reset(new WebSocketTransportConnectSubJob(
        ipv6_addresses, this, SUB_JOB_IPV6));
    result = ipv6_job_->Start();
    switch (result) {
      case OK:
        SetSocket(ipv6_job_->PassSocket());
        race_result_ =
            had_ipv4_
                ? TransportConnectJobHelper::CONNECTION_LATENCY_IPV6_RACEABLE
                : TransportConnectJobHelper::CONNECTION_LATENCY_IPV6_SOLO;
        return result;

      case ERR_IO_PENDING:
        if (ipv4_job_) {
          // This use of base::Unretained is safe because |fallback_timer_| is
          // owned by this object.
          fallback_timer_.Start(
              FROM_HERE,
              TimeDelta::FromMilliseconds(
                  TransportConnectJobHelper::kIPv6FallbackTimerInMs),
              base::Bind(&WebSocketTransportConnectJob::StartIPv4JobAsync,
                         base::Unretained(this)));
        }
        return result;

      default:
        ipv6_job_.reset();
    }
  }

  DCHECK(!ipv6_job_);
  if (ipv4_job_) {
    result = ipv4_job_->Start();
    if (result == OK) {
      SetSocket(ipv4_job_->PassSocket());
      race_result_ =
          had_ipv6_
              ? TransportConnectJobHelper::CONNECTION_LATENCY_IPV4_WINS_RACE
              : TransportConnectJobHelper::CONNECTION_LATENCY_IPV4_NO_RACE;
    }
  }

  return result;
}

int WebSocketTransportConnectJob::DoTransportConnectComplete(int result) {
  if (result == OK)
    helper_.HistogramDuration(race_result_);
  return result;
}

void WebSocketTransportConnectJob::OnSubJobComplete(
    int result,
    WebSocketTransportConnectSubJob* job) {
  if (result == OK) {
    switch (job->type()) {
      case SUB_JOB_IPV4:
        race_result_ =
            had_ipv6_
                ? TransportConnectJobHelper::CONNECTION_LATENCY_IPV4_WINS_RACE
                : TransportConnectJobHelper::CONNECTION_LATENCY_IPV4_NO_RACE;
        break;

      case SUB_JOB_IPV6:
        race_result_ =
            had_ipv4_
                ? TransportConnectJobHelper::CONNECTION_LATENCY_IPV6_RACEABLE
                : TransportConnectJobHelper::CONNECTION_LATENCY_IPV6_SOLO;
        break;
    }
    SetSocket(job->PassSocket());

    // Make sure all connections are cancelled even if this object fails to be
    // deleted.
    ipv4_job_.reset();
    ipv6_job_.reset();
  } else {
    switch (job->type()) {
      case SUB_JOB_IPV4:
        ipv4_job_.reset();
        break;

      case SUB_JOB_IPV6:
        ipv6_job_.reset();
        if (ipv4_job_ && !ipv4_job_->started()) {
          fallback_timer_.Stop();
          result = ipv4_job_->Start();
          if (result != ERR_IO_PENDING) {
            OnSubJobComplete(result, ipv4_job_.get());
            return;
          }
        }
        break;
    }
    if (ipv4_job_ || ipv6_job_)
      return;
  }
  helper_.OnIOComplete(this, result);
}

void WebSocketTransportConnectJob::StartIPv4JobAsync() {
  DCHECK(ipv4_job_);
  int result = ipv4_job_->Start();
  if (result != ERR_IO_PENDING)
    OnSubJobComplete(result, ipv4_job_.get());
}

int WebSocketTransportConnectJob::ConnectInternal() {
  return helper_.DoConnectInternal(this);
}

WebSocketTransportClientSocketPool::WebSocketTransportClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    HostResolver* host_resolver,
    ClientSocketFactory* client_socket_factory,
    NetLog* net_log)
    : TransportClientSocketPool(max_sockets,
                                max_sockets_per_group,
                                host_resolver,
                                client_socket_factory,
                                net_log),
      connect_job_delegate_(this),
      pool_net_log_(net_log),
      client_socket_factory_(client_socket_factory),
      host_resolver_(host_resolver),
      max_sockets_(max_sockets),
      handed_out_socket_count_(0),
      flushing_(false),
      weak_factory_(this) {}

WebSocketTransportClientSocketPool::~WebSocketTransportClientSocketPool() {
  // Clean up any pending connect jobs.
  FlushWithError(ERR_ABORTED);
  DCHECK(pending_connects_.empty());
  DCHECK_EQ(0, handed_out_socket_count_);
  DCHECK(stalled_request_queue_.empty());
  DCHECK(stalled_request_map_.empty());
}

// static
void WebSocketTransportClientSocketPool::UnlockEndpoint(
    ClientSocketHandle* handle) {
  DCHECK(handle->is_initialized());
  DCHECK(handle->socket());
  IPEndPoint address;
  if (handle->socket()->GetPeerAddress(&address) == OK)
    WebSocketEndpointLockManager::GetInstance()->UnlockEndpoint(address);
}

int WebSocketTransportClientSocketPool::RequestSocket(
    const std::string& group_name,
    const void* params,
    RequestPriority priority,
    ClientSocketHandle* handle,
    const CompletionCallback& callback,
    const BoundNetLog& request_net_log) {
  DCHECK(params);
  const scoped_refptr<TransportSocketParams>& casted_params =
      *static_cast<const scoped_refptr<TransportSocketParams>*>(params);

  NetLogTcpClientSocketPoolRequestedSocket(request_net_log, &casted_params);

  CHECK(!callback.is_null());
  CHECK(handle);

  request_net_log.BeginEvent(NetLog::TYPE_SOCKET_POOL);

  if (ReachedMaxSocketsLimit() && !casted_params->ignore_limits()) {
    request_net_log.AddEvent(NetLog::TYPE_SOCKET_POOL_STALLED_MAX_SOCKETS);
    // TODO(ricea): Use emplace_back when C++11 becomes allowed.
    StalledRequest request(
        casted_params, priority, handle, callback, request_net_log);
    stalled_request_queue_.push_back(request);
    StalledRequestQueue::iterator iterator = stalled_request_queue_.end();
    --iterator;
    DCHECK_EQ(handle, iterator->handle);
    // Because StalledRequestQueue is a std::list, its iterators are guaranteed
    // to remain valid as long as the elements are not removed. As long as
    // stalled_request_queue_ and stalled_request_map_ are updated in sync, it
    // is safe to dereference an iterator in stalled_request_map_ to find the
    // corresponding list element.
    stalled_request_map_.insert(
        StalledRequestMap::value_type(handle, iterator));
    return ERR_IO_PENDING;
  }

  scoped_ptr<WebSocketTransportConnectJob> connect_job(
      new WebSocketTransportConnectJob(group_name,
                                       priority,
                                       casted_params,
                                       ConnectionTimeout(),
                                       callback,
                                       client_socket_factory_,
                                       host_resolver_,
                                       handle,
                                       &connect_job_delegate_,
                                       pool_net_log_,
                                       request_net_log));

  int rv = connect_job->Connect();
  // Regardless of the outcome of |connect_job|, it will always be bound to
  // |handle|, since this pool uses early-binding. So the binding is logged
  // here, without waiting for the result.
  request_net_log.AddEvent(
      NetLog::TYPE_SOCKET_POOL_BOUND_TO_CONNECT_JOB,
      connect_job->net_log().source().ToEventParametersCallback());
  if (rv == OK) {
    HandOutSocket(connect_job->PassSocket(),
                  connect_job->connect_timing(),
                  handle,
                  request_net_log);
    request_net_log.EndEvent(NetLog::TYPE_SOCKET_POOL);
  } else if (rv == ERR_IO_PENDING) {
    // TODO(ricea): Implement backup job timer?
    AddJob(handle, connect_job.Pass());
  } else {
    scoped_ptr<StreamSocket> error_socket;
    connect_job->GetAdditionalErrorState(handle);
    error_socket = connect_job->PassSocket();
    if (error_socket) {
      HandOutSocket(error_socket.Pass(),
                    connect_job->connect_timing(),
                    handle,
                    request_net_log);
    }
  }

  if (rv != ERR_IO_PENDING) {
    request_net_log.EndEventWithNetErrorCode(NetLog::TYPE_SOCKET_POOL, rv);
  }

  return rv;
}

void WebSocketTransportClientSocketPool::RequestSockets(
    const std::string& group_name,
    const void* params,
    int num_sockets,
    const BoundNetLog& net_log) {
  NOTIMPLEMENTED();
}

void WebSocketTransportClientSocketPool::CancelRequest(
    const std::string& group_name,
    ClientSocketHandle* handle) {
  DCHECK(!handle->is_initialized());
  if (DeleteStalledRequest(handle))
    return;
  scoped_ptr<StreamSocket> socket = handle->PassSocket();
  if (socket)
    ReleaseSocket(handle->group_name(), socket.Pass(), handle->id());
  if (!DeleteJob(handle))
    pending_callbacks_.erase(handle);
  if (!ReachedMaxSocketsLimit() && !stalled_request_queue_.empty())
    ActivateStalledRequest();
}

void WebSocketTransportClientSocketPool::ReleaseSocket(
    const std::string& group_name,
    scoped_ptr<StreamSocket> socket,
    int id) {
  WebSocketEndpointLockManager::GetInstance()->UnlockSocket(socket.get());
  CHECK_GT(handed_out_socket_count_, 0);
  --handed_out_socket_count_;
  if (!ReachedMaxSocketsLimit() && !stalled_request_queue_.empty())
    ActivateStalledRequest();
}

void WebSocketTransportClientSocketPool::FlushWithError(int error) {
  // Sockets which are in LOAD_STATE_CONNECTING are in danger of unlocking
  // sockets waiting for the endpoint lock. If they connected synchronously,
  // then OnConnectJobComplete(). The |flushing_| flag tells this object to
  // ignore spurious calls to OnConnectJobComplete(). It is safe to ignore those
  // calls because this method will delete the jobs and call their callbacks
  // anyway.
  flushing_ = true;
  for (PendingConnectsMap::iterator it = pending_connects_.begin();
       it != pending_connects_.end();
       ++it) {
    InvokeUserCallbackLater(
        it->second->handle(), it->second->callback(), error);
    delete it->second, it->second = NULL;
  }
  pending_connects_.clear();
  for (StalledRequestQueue::iterator it = stalled_request_queue_.begin();
       it != stalled_request_queue_.end();
       ++it) {
    InvokeUserCallbackLater(it->handle, it->callback, error);
  }
  stalled_request_map_.clear();
  stalled_request_queue_.clear();
  flushing_ = false;
}

void WebSocketTransportClientSocketPool::CloseIdleSockets() {
  // We have no idle sockets.
}

int WebSocketTransportClientSocketPool::IdleSocketCount() const {
  return 0;
}

int WebSocketTransportClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  return 0;
}

LoadState WebSocketTransportClientSocketPool::GetLoadState(
    const std::string& group_name,
    const ClientSocketHandle* handle) const {
  if (stalled_request_map_.find(handle) != stalled_request_map_.end())
    return LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET;
  if (pending_callbacks_.count(handle))
    return LOAD_STATE_CONNECTING;
  return LookupConnectJob(handle)->GetLoadState();
}

scoped_ptr<base::DictionaryValue>
    WebSocketTransportClientSocketPool::GetInfoAsValue(
        const std::string& name,
        const std::string& type,
        bool include_nested_pools) const {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("name", name);
  dict->SetString("type", type);
  dict->SetInteger("handed_out_socket_count", handed_out_socket_count_);
  dict->SetInteger("connecting_socket_count", pending_connects_.size());
  dict->SetInteger("idle_socket_count", 0);
  dict->SetInteger("max_socket_count", max_sockets_);
  dict->SetInteger("max_sockets_per_group", max_sockets_);
  dict->SetInteger("pool_generation_number", 0);
  return dict.Pass();
}

TimeDelta WebSocketTransportClientSocketPool::ConnectionTimeout() const {
  return TimeDelta::FromSeconds(kTransportConnectJobTimeoutInSeconds);
}

bool WebSocketTransportClientSocketPool::IsStalled() const {
  return !stalled_request_queue_.empty();
}

void WebSocketTransportClientSocketPool::OnConnectJobComplete(
    int result,
    WebSocketTransportConnectJob* job) {
  DCHECK_NE(ERR_IO_PENDING, result);

  scoped_ptr<StreamSocket> socket = job->PassSocket();

  // See comment in FlushWithError.
  if (flushing_) {
    WebSocketEndpointLockManager::GetInstance()->UnlockSocket(socket.get());
    return;
  }

  BoundNetLog request_net_log = job->request_net_log();
  CompletionCallback callback = job->callback();
  LoadTimingInfo::ConnectTiming connect_timing = job->connect_timing();

  ClientSocketHandle* const handle = job->handle();
  bool handed_out_socket = false;

  if (result == OK) {
    DCHECK(socket.get());
    handed_out_socket = true;
    HandOutSocket(socket.Pass(), connect_timing, handle, request_net_log);
    request_net_log.EndEvent(NetLog::TYPE_SOCKET_POOL);
  } else {
    // If we got a socket, it must contain error information so pass that
    // up so that the caller can retrieve it.
    job->GetAdditionalErrorState(handle);
    if (socket.get()) {
      handed_out_socket = true;
      HandOutSocket(socket.Pass(), connect_timing, handle, request_net_log);
    }
    request_net_log.EndEventWithNetErrorCode(NetLog::TYPE_SOCKET_POOL, result);
  }
  bool delete_succeeded = DeleteJob(handle);
  DCHECK(delete_succeeded);
  if (!handed_out_socket && !stalled_request_queue_.empty() &&
      !ReachedMaxSocketsLimit())
    ActivateStalledRequest();
  InvokeUserCallbackLater(handle, callback, result);
}

void WebSocketTransportClientSocketPool::InvokeUserCallbackLater(
    ClientSocketHandle* handle,
    const CompletionCallback& callback,
    int rv) {
  DCHECK(!pending_callbacks_.count(handle));
  pending_callbacks_.insert(handle);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&WebSocketTransportClientSocketPool::InvokeUserCallback,
                 weak_factory_.GetWeakPtr(), handle, callback, rv));
}

void WebSocketTransportClientSocketPool::InvokeUserCallback(
    ClientSocketHandle* handle,
    const CompletionCallback& callback,
    int rv) {
  if (pending_callbacks_.erase(handle))
    callback.Run(rv);
}

bool WebSocketTransportClientSocketPool::ReachedMaxSocketsLimit() const {
  return handed_out_socket_count_ >= max_sockets_ ||
         base::checked_cast<int>(pending_connects_.size()) >=
             max_sockets_ - handed_out_socket_count_;
}

void WebSocketTransportClientSocketPool::HandOutSocket(
    scoped_ptr<StreamSocket> socket,
    const LoadTimingInfo::ConnectTiming& connect_timing,
    ClientSocketHandle* handle,
    const BoundNetLog& net_log) {
  DCHECK(socket);
  handle->SetSocket(socket.Pass());
  DCHECK_EQ(ClientSocketHandle::UNUSED, handle->reuse_type());
  DCHECK_EQ(0, handle->idle_time().InMicroseconds());
  handle->set_pool_id(0);
  handle->set_connect_timing(connect_timing);

  net_log.AddEvent(
      NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET,
      handle->socket()->NetLog().source().ToEventParametersCallback());

  ++handed_out_socket_count_;
}

void WebSocketTransportClientSocketPool::AddJob(
    ClientSocketHandle* handle,
    scoped_ptr<WebSocketTransportConnectJob> connect_job) {
  bool inserted =
      pending_connects_.insert(PendingConnectsMap::value_type(
                                   handle, connect_job.release())).second;
  DCHECK(inserted);
}

bool WebSocketTransportClientSocketPool::DeleteJob(ClientSocketHandle* handle) {
  PendingConnectsMap::iterator it = pending_connects_.find(handle);
  if (it == pending_connects_.end())
    return false;
  // Deleting a ConnectJob which holds an endpoint lock can lead to a different
  // ConnectJob proceeding to connect. If the connect proceeds synchronously
  // (usually because of a failure) then it can trigger that job to be
  // deleted. |it| remains valid because std::map guarantees that erase() does
  // not invalid iterators to other entries.
  delete it->second, it->second = NULL;
  DCHECK(pending_connects_.find(handle) == it);
  pending_connects_.erase(it);
  return true;
}

const WebSocketTransportConnectJob*
WebSocketTransportClientSocketPool::LookupConnectJob(
    const ClientSocketHandle* handle) const {
  PendingConnectsMap::const_iterator it = pending_connects_.find(handle);
  CHECK(it != pending_connects_.end());
  return it->second;
}

void WebSocketTransportClientSocketPool::ActivateStalledRequest() {
  DCHECK(!stalled_request_queue_.empty());
  DCHECK(!ReachedMaxSocketsLimit());
  // Usually we will only be able to activate one stalled request at a time,
  // however if all the connects fail synchronously for some reason, we may be
  // able to clear the whole queue at once.
  while (!stalled_request_queue_.empty() && !ReachedMaxSocketsLimit()) {
    StalledRequest request(stalled_request_queue_.front());
    stalled_request_queue_.pop_front();
    stalled_request_map_.erase(request.handle);
    int rv = RequestSocket("ignored",
                           &request.params,
                           request.priority,
                           request.handle,
                           request.callback,
                           request.net_log);
    // ActivateStalledRequest() never returns synchronously, so it is never
    // called re-entrantly.
    if (rv != ERR_IO_PENDING)
      InvokeUserCallbackLater(request.handle, request.callback, rv);
  }
}

bool WebSocketTransportClientSocketPool::DeleteStalledRequest(
    ClientSocketHandle* handle) {
  StalledRequestMap::iterator it = stalled_request_map_.find(handle);
  if (it == stalled_request_map_.end())
    return false;
  stalled_request_queue_.erase(it->second);
  stalled_request_map_.erase(it);
  return true;
}

WebSocketTransportClientSocketPool::ConnectJobDelegate::ConnectJobDelegate(
    WebSocketTransportClientSocketPool* owner)
    : owner_(owner) {}

WebSocketTransportClientSocketPool::ConnectJobDelegate::~ConnectJobDelegate() {}

void
WebSocketTransportClientSocketPool::ConnectJobDelegate::OnConnectJobComplete(
    int result,
    ConnectJob* job) {
  owner_->OnConnectJobComplete(result,
                               static_cast<WebSocketTransportConnectJob*>(job));
}

WebSocketTransportClientSocketPool::StalledRequest::StalledRequest(
    const scoped_refptr<TransportSocketParams>& params,
    RequestPriority priority,
    ClientSocketHandle* handle,
    const CompletionCallback& callback,
    const BoundNetLog& net_log)
    : params(params),
      priority(priority),
      handle(handle),
      callback(callback),
      net_log(net_log) {}

WebSocketTransportClientSocketPool::StalledRequest::~StalledRequest() {}

}  // namespace net
