// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mojo_host_resolver_impl.h"

#include "base/basictypes.h"
#include "base/stl_util.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mojo_host_type_converters.h"

namespace net {

// Handles host resolution for a single request and sends a response when done.
// Also detects connection errors for HostResolverRequestClient and cancels the
// outstanding resolve request. Owned by MojoHostResolverImpl.
class MojoHostResolverImpl::Job {
 public:
  Job(MojoHostResolverImpl* resolver_service,
      net::HostResolver* resolver,
      const net::HostResolver::RequestInfo& request_info,
      const BoundNetLog& net_log,
      interfaces::HostResolverRequestClientPtr client);
  ~Job();

  void Start();

 private:
  // Completion callback for the HostResolver::Resolve request.
  void OnResolveDone(int result);

  // Mojo error handler.
  void OnConnectionError();

  MojoHostResolverImpl* resolver_service_;
  net::HostResolver* resolver_;
  net::HostResolver::RequestInfo request_info_;
  const BoundNetLog net_log_;
  interfaces::HostResolverRequestClientPtr client_;
  net::HostResolver::RequestHandle handle_;
  AddressList result_;
  base::ThreadChecker thread_checker_;
};

MojoHostResolverImpl::MojoHostResolverImpl(net::HostResolver* resolver,
                                           const BoundNetLog& net_log)
    : resolver_(resolver), net_log_(net_log) {
}

MojoHostResolverImpl::~MojoHostResolverImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  STLDeleteElements(&pending_jobs_);
}

void MojoHostResolverImpl::Resolve(
    interfaces::HostResolverRequestInfoPtr request_info,
    interfaces::HostResolverRequestClientPtr client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Job* job = new Job(this, resolver_,
                     request_info->To<net::HostResolver::RequestInfo>(),
                     net_log_, client.Pass());
  pending_jobs_.insert(job);
  job->Start();
}

void MojoHostResolverImpl::DeleteJob(Job* job) {
  DCHECK(thread_checker_.CalledOnValidThread());
  size_t num_erased = pending_jobs_.erase(job);
  DCHECK(num_erased);
  delete job;
}

MojoHostResolverImpl::Job::Job(
    MojoHostResolverImpl* resolver_service,
    net::HostResolver* resolver,
    const net::HostResolver::RequestInfo& request_info,
    const BoundNetLog& net_log,
    interfaces::HostResolverRequestClientPtr client)
    : resolver_service_(resolver_service),
      resolver_(resolver),
      request_info_(request_info),
      net_log_(net_log),
      client_(client.Pass()),
      handle_(nullptr) {
  client_.set_connection_error_handler(base::Bind(
      &MojoHostResolverImpl::Job::OnConnectionError, base::Unretained(this)));
}

void MojoHostResolverImpl::Job::Start() {
  DVLOG(1) << "Resolve " << request_info_.host_port_pair().ToString();
  int result =
      resolver_->Resolve(request_info_, DEFAULT_PRIORITY, &result_,
                         base::Bind(&MojoHostResolverImpl::Job::OnResolveDone,
                                    base::Unretained(this)),
                         &handle_, net_log_);

  if (result != ERR_IO_PENDING)
    OnResolveDone(result);
}

MojoHostResolverImpl::Job::~Job() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (handle_)
    resolver_->CancelRequest(handle_);
}

void MojoHostResolverImpl::Job::OnResolveDone(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  handle_ = nullptr;
  DVLOG(1) << "Resolved " << request_info_.host_port_pair().ToString()
           << " with error " << result << " and " << result_.size()
           << " results!";
  for (const auto& address : result_) {
    DVLOG(1) << address.ToString();
  }
  if (result == OK)
    client_->ReportResult(result, interfaces::AddressList::From(result_));
  else
    client_->ReportResult(result, nullptr);

  resolver_service_->DeleteJob(this);
}

void MojoHostResolverImpl::Job::OnConnectionError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // |resolver_service_| should always outlive us.
  DCHECK(resolver_service_);
  DVLOG(1) << "Connection error on request for "
           << request_info_.host_port_pair().ToString();
  resolver_service_->DeleteJob(this);
}

}  // namespace net
