// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/channel_id_service.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "crypto/ec_private_key.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "url/gurl.h"

#if !defined(USE_OPENSSL)
#include <private/pprthred.h>  // PR_DetachThread
#endif

namespace net {

namespace {

// Used by the GetDomainBoundCertResult histogram to record the final
// outcome of each GetChannelID or GetOrCreateChannelID call.
// Do not re-use values.
enum GetChannelIDResult {
  // Synchronously found and returned an existing domain bound cert.
  SYNC_SUCCESS = 0,
  // Retrieved or generated and returned a domain bound cert asynchronously.
  ASYNC_SUCCESS = 1,
  // Retrieval/generation request was cancelled before the cert generation
  // completed.
  ASYNC_CANCELLED = 2,
  // Cert generation failed.
  ASYNC_FAILURE_KEYGEN = 3,
  // Result code 4 was removed (ASYNC_FAILURE_CREATE_CERT)
  ASYNC_FAILURE_EXPORT_KEY = 5,
  ASYNC_FAILURE_UNKNOWN = 6,
  // GetChannelID or GetOrCreateChannelID was called with
  // invalid arguments.
  INVALID_ARGUMENT = 7,
  // We don't support any of the cert types the server requested.
  UNSUPPORTED_TYPE = 8,
  // Server asked for a different type of certs while we were generating one.
  TYPE_MISMATCH = 9,
  // Couldn't start a worker to generate a cert.
  WORKER_FAILURE = 10,
  GET_CHANNEL_ID_RESULT_MAX
};

void RecordGetChannelIDResult(GetChannelIDResult result) {
  UMA_HISTOGRAM_ENUMERATION("DomainBoundCerts.GetDomainBoundCertResult", result,
                            GET_CHANNEL_ID_RESULT_MAX);
}

void RecordGetChannelIDTime(base::TimeDelta request_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.GetCertTime",
                             request_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(5),
                             50);
}

// On success, returns a ChannelID object and sets |*error| to OK.
// Otherwise, returns NULL, and |*error| will be set to a net error code.
// |serial_number| is passed in because base::RandInt cannot be called from an
// unjoined thread, due to relying on a non-leaked LazyInstance
scoped_ptr<ChannelIDStore::ChannelID> GenerateChannelID(
    const std::string& server_identifier,
    int* error) {
  scoped_ptr<ChannelIDStore::ChannelID> result;

  base::TimeTicks start = base::TimeTicks::Now();
  base::Time creation_time = base::Time::Now();
  scoped_ptr<crypto::ECPrivateKey> key(crypto::ECPrivateKey::Create());

  if (!key) {
    DLOG(ERROR) << "Unable to create channel ID key pair";
    *error = ERR_KEY_GENERATION_FAILED;
    return result.Pass();
  }

  result.reset(new ChannelIDStore::ChannelID(server_identifier, creation_time,
                                             key.Pass()));
  UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.GenerateCertTime",
                             base::TimeTicks::Now() - start,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(5),
                             50);
  *error = OK;
  return result.Pass();
}

}  // namespace

// ChannelIDServiceWorker runs on a worker thread and takes care of the
// blocking process of performing key generation. Will take care of deleting
// itself once Start() is called.
class ChannelIDServiceWorker {
 public:
  typedef base::Callback<void(
      const std::string&,
      int,
      scoped_ptr<ChannelIDStore::ChannelID>)> WorkerDoneCallback;

  ChannelIDServiceWorker(const std::string& server_identifier,
                         const WorkerDoneCallback& callback)
      : server_identifier_(server_identifier),
        origin_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        callback_(callback) {}

  // Starts the worker on |task_runner|. If the worker fails to start, such as
  // if the task runner is shutting down, then it will take care of deleting
  // itself.
  bool Start(const scoped_refptr<base::TaskRunner>& task_runner) {
    DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());

    return task_runner->PostTask(
        FROM_HERE,
        base::Bind(&ChannelIDServiceWorker::Run, base::Owned(this)));
  }

 private:
  void Run() {
    // Runs on a worker thread.
    int error = ERR_FAILED;
    scoped_ptr<ChannelIDStore::ChannelID> channel_id =
        GenerateChannelID(server_identifier_, &error);
#if !defined(USE_OPENSSL)
    // Detach the thread from NSPR.
    // Calling NSS functions attaches the thread to NSPR, which stores
    // the NSPR thread ID in thread-specific data.
    // The threads in our thread pool terminate after we have called
    // PR_Cleanup. Unless we detach them from NSPR, net_unittests gets
    // segfaults on shutdown when the threads' thread-specific data
    // destructors run.
    PR_DetachThread();
#endif
    origin_task_runner_->PostTask(
        FROM_HERE, base::Bind(callback_, server_identifier_, error,
                              base::Passed(&channel_id)));
  }

  const std::string server_identifier_;
  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  WorkerDoneCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ChannelIDServiceWorker);
};

// A ChannelIDServiceJob is a one-to-one counterpart of an
// ChannelIDServiceWorker. It lives only on the ChannelIDService's
// origin task runner's thread.
class ChannelIDServiceJob {
 public:
  ChannelIDServiceJob(bool create_if_missing)
      : create_if_missing_(create_if_missing) {
  }

  ~ChannelIDServiceJob() { DCHECK(requests_.empty()); }

  void AddRequest(ChannelIDService::Request* request,
                  bool create_if_missing = false) {
    create_if_missing_ |= create_if_missing;
    requests_.push_back(request);
  }

  void HandleResult(int error, scoped_ptr<crypto::ECPrivateKey> key) {
    PostAll(error, key.Pass());
  }

  bool CreateIfMissing() const { return create_if_missing_; }

  void CancelRequest(ChannelIDService::Request* req) {
    auto it = std::find(requests_.begin(), requests_.end(), req);
    if (it != requests_.end())
      requests_.erase(it);
  }

 private:
  void PostAll(int error, scoped_ptr<crypto::ECPrivateKey> key) {
    std::vector<ChannelIDService::Request*> requests;
    requests_.swap(requests);

    for (std::vector<ChannelIDService::Request*>::iterator i = requests.begin();
         i != requests.end(); i++) {
      scoped_ptr<crypto::ECPrivateKey> key_copy;
      if (key)
        key_copy.reset(key->Copy());
      (*i)->Post(error, key_copy.Pass());
    }
  }

  std::vector<ChannelIDService::Request*> requests_;
  bool create_if_missing_;
};

// static
const char ChannelIDService::kEPKIPassword[] = "";

ChannelIDService::Request::Request() : service_(NULL) {
}

ChannelIDService::Request::~Request() {
  Cancel();
}

void ChannelIDService::Request::Cancel() {
  if (service_) {
    RecordGetChannelIDResult(ASYNC_CANCELLED);
    callback_.Reset();
    job_->CancelRequest(this);

    service_ = NULL;
  }
}

void ChannelIDService::Request::RequestStarted(
    ChannelIDService* service,
    base::TimeTicks request_start,
    const CompletionCallback& callback,
    scoped_ptr<crypto::ECPrivateKey>* key,
    ChannelIDServiceJob* job) {
  DCHECK(service_ == NULL);
  service_ = service;
  request_start_ = request_start;
  callback_ = callback;
  key_ = key;
  job_ = job;
}

void ChannelIDService::Request::Post(int error,
                                     scoped_ptr<crypto::ECPrivateKey> key) {
  switch (error) {
    case OK: {
      base::TimeDelta request_time = base::TimeTicks::Now() - request_start_;
      UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.GetCertTimeAsync",
                                 request_time,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(5), 50);
      RecordGetChannelIDTime(request_time);
      RecordGetChannelIDResult(ASYNC_SUCCESS);
      break;
    }
    case ERR_KEY_GENERATION_FAILED:
      RecordGetChannelIDResult(ASYNC_FAILURE_KEYGEN);
      break;
    case ERR_PRIVATE_KEY_EXPORT_FAILED:
      RecordGetChannelIDResult(ASYNC_FAILURE_EXPORT_KEY);
      break;
    case ERR_INSUFFICIENT_RESOURCES:
      RecordGetChannelIDResult(WORKER_FAILURE);
      break;
    default:
      RecordGetChannelIDResult(ASYNC_FAILURE_UNKNOWN);
      break;
  }
  service_ = NULL;
  DCHECK(!callback_.is_null());
  if (key)
    *key_ = key.Pass();
  // Running the callback might delete |this| (e.g. the callback cleans up
  // resources created for the request), so we can't touch any of our
  // members afterwards. Reset callback_ first.
  base::ResetAndReturn(&callback_).Run(error);
}

ChannelIDService::ChannelIDService(
    ChannelIDStore* channel_id_store,
    const scoped_refptr<base::TaskRunner>& task_runner)
    : channel_id_store_(channel_id_store),
      task_runner_(task_runner),
      requests_(0),
      key_store_hits_(0),
      inflight_joins_(0),
      workers_created_(0),
      weak_ptr_factory_(this) {
}

ChannelIDService::~ChannelIDService() {
  STLDeleteValues(&inflight_);
}

//static
std::string ChannelIDService::GetDomainForHost(const std::string& host) {
  std::string domain =
      registry_controlled_domains::GetDomainAndRegistry(
          host, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (domain.empty())
    return host;
  return domain;
}

int ChannelIDService::GetOrCreateChannelID(
    const std::string& host,
    scoped_ptr<crypto::ECPrivateKey>* key,
    const CompletionCallback& callback,
    Request* out_req) {
  DVLOG(1) << __FUNCTION__ << " " << host;
  DCHECK(CalledOnValidThread());
  base::TimeTicks request_start = base::TimeTicks::Now();

  if (callback.is_null() || !key || host.empty()) {
    RecordGetChannelIDResult(INVALID_ARGUMENT);
    return ERR_INVALID_ARGUMENT;
  }

  std::string domain = GetDomainForHost(host);
  if (domain.empty()) {
    RecordGetChannelIDResult(INVALID_ARGUMENT);
    return ERR_INVALID_ARGUMENT;
  }

  requests_++;

  // See if a request for the same domain is currently in flight.
  bool create_if_missing = true;
  if (JoinToInFlightRequest(request_start, domain, key, create_if_missing,
                            callback, out_req)) {
    return ERR_IO_PENDING;
  }

  int err = LookupChannelID(request_start, domain, key, create_if_missing,
                            callback, out_req);
  if (err == ERR_FILE_NOT_FOUND) {
    // Sync lookup did not find a valid channel ID.  Start generating a new one.
    workers_created_++;
    ChannelIDServiceWorker* worker = new ChannelIDServiceWorker(
        domain,
        base::Bind(&ChannelIDService::GeneratedChannelID,
                   weak_ptr_factory_.GetWeakPtr()));
    if (!worker->Start(task_runner_)) {
      // TODO(rkn): Log to the NetLog.
      LOG(ERROR) << "ChannelIDServiceWorker couldn't be started.";
      RecordGetChannelIDResult(WORKER_FAILURE);
      return ERR_INSUFFICIENT_RESOURCES;
    }
    // We are waiting for key generation.  Create a job & request to track it.
    ChannelIDServiceJob* job = new ChannelIDServiceJob(create_if_missing);
    inflight_[domain] = job;

    job->AddRequest(out_req);
    out_req->RequestStarted(this, request_start, callback, key, job);
    return ERR_IO_PENDING;
  }

  return err;
}

int ChannelIDService::GetChannelID(const std::string& host,
                                   scoped_ptr<crypto::ECPrivateKey>* key,
                                   const CompletionCallback& callback,
                                   Request* out_req) {
  DVLOG(1) << __FUNCTION__ << " " << host;
  DCHECK(CalledOnValidThread());
  base::TimeTicks request_start = base::TimeTicks::Now();

  if (callback.is_null() || !key || host.empty()) {
    RecordGetChannelIDResult(INVALID_ARGUMENT);
    return ERR_INVALID_ARGUMENT;
  }

  std::string domain = GetDomainForHost(host);
  if (domain.empty()) {
    RecordGetChannelIDResult(INVALID_ARGUMENT);
    return ERR_INVALID_ARGUMENT;
  }

  requests_++;

  // See if a request for the same domain currently in flight.
  bool create_if_missing = false;
  if (JoinToInFlightRequest(request_start, domain, key, create_if_missing,
                            callback, out_req)) {
    return ERR_IO_PENDING;
  }

  int err = LookupChannelID(request_start, domain, key, create_if_missing,
                            callback, out_req);
  return err;
}

void ChannelIDService::GotChannelID(int err,
                                    const std::string& server_identifier,
                                    scoped_ptr<crypto::ECPrivateKey> key) {
  DCHECK(CalledOnValidThread());

  std::map<std::string, ChannelIDServiceJob*>::iterator j;
  j = inflight_.find(server_identifier);
  if (j == inflight_.end()) {
    NOTREACHED();
    return;
  }

  if (err == OK) {
    // Async DB lookup found a valid channel ID.
    key_store_hits_++;
    // ChannelIDService::Request::Post will do the histograms and stuff.
    HandleResult(OK, server_identifier, key.Pass());
    return;
  }
  // Async lookup failed or the channel ID was missing. Return the error
  // directly, unless the channel ID was missing and a request asked to create
  // one.
  if (err != ERR_FILE_NOT_FOUND || !j->second->CreateIfMissing()) {
    HandleResult(err, server_identifier, key.Pass());
    return;
  }
  // At least one request asked to create a channel ID => start generating a new
  // one.
  workers_created_++;
  ChannelIDServiceWorker* worker = new ChannelIDServiceWorker(
      server_identifier,
      base::Bind(&ChannelIDService::GeneratedChannelID,
                 weak_ptr_factory_.GetWeakPtr()));
  if (!worker->Start(task_runner_)) {
    // TODO(rkn): Log to the NetLog.
    LOG(ERROR) << "ChannelIDServiceWorker couldn't be started.";
    HandleResult(ERR_INSUFFICIENT_RESOURCES, server_identifier, nullptr);
  }
}

ChannelIDStore* ChannelIDService::GetChannelIDStore() {
  return channel_id_store_.get();
}

void ChannelIDService::GeneratedChannelID(
    const std::string& server_identifier,
    int error,
    scoped_ptr<ChannelIDStore::ChannelID> channel_id) {
  DCHECK(CalledOnValidThread());

  scoped_ptr<crypto::ECPrivateKey> key;
  if (error == OK) {
    key.reset(channel_id->key()->Copy());
    channel_id_store_->SetChannelID(channel_id.Pass());
  }
  HandleResult(error, server_identifier, key.Pass());
}

void ChannelIDService::HandleResult(int error,
                                    const std::string& server_identifier,
                                    scoped_ptr<crypto::ECPrivateKey> key) {
  DCHECK(CalledOnValidThread());

  std::map<std::string, ChannelIDServiceJob*>::iterator j;
  j = inflight_.find(server_identifier);
  if (j == inflight_.end()) {
    NOTREACHED();
    return;
  }
  ChannelIDServiceJob* job = j->second;
  inflight_.erase(j);

  job->HandleResult(error, key.Pass());
  delete job;
}

bool ChannelIDService::JoinToInFlightRequest(
    const base::TimeTicks& request_start,
    const std::string& domain,
    scoped_ptr<crypto::ECPrivateKey>* key,
    bool create_if_missing,
    const CompletionCallback& callback,
    Request* out_req) {
  ChannelIDServiceJob* job = NULL;
  std::map<std::string, ChannelIDServiceJob*>::const_iterator j =
      inflight_.find(domain);
  if (j != inflight_.end()) {
    // A request for the same domain is in flight already. We'll attach our
    // callback, but we'll also mark it as requiring a channel ID if one's
    // mising.
    job = j->second;
    inflight_joins_++;

    job->AddRequest(out_req, create_if_missing);
    out_req->RequestStarted(this, request_start, callback, key, job);
    return true;
  }
  return false;
}

int ChannelIDService::LookupChannelID(const base::TimeTicks& request_start,
                                      const std::string& domain,
                                      scoped_ptr<crypto::ECPrivateKey>* key,
                                      bool create_if_missing,
                                      const CompletionCallback& callback,
                                      Request* out_req) {
  // Check if a channel ID key already exists for this domain.
  int err = channel_id_store_->GetChannelID(
      domain, key, base::Bind(&ChannelIDService::GotChannelID,
                              weak_ptr_factory_.GetWeakPtr()));

  if (err == OK) {
    // Sync lookup found a valid channel ID.
    DVLOG(1) << "Channel ID store had valid key for " << domain;
    key_store_hits_++;
    RecordGetChannelIDResult(SYNC_SUCCESS);
    base::TimeDelta request_time = base::TimeTicks::Now() - request_start;
    UMA_HISTOGRAM_TIMES("DomainBoundCerts.GetCertTimeSync", request_time);
    RecordGetChannelIDTime(request_time);
    return OK;
  }

  if (err == ERR_IO_PENDING) {
    // We are waiting for async DB lookup.  Create a job & request to track it.
    ChannelIDServiceJob* job = new ChannelIDServiceJob(create_if_missing);
    inflight_[domain] = job;

    job->AddRequest(out_req);
    out_req->RequestStarted(this, request_start, callback, key, job);
    return ERR_IO_PENDING;
  }

  return err;
}

int ChannelIDService::channel_id_count() {
  return channel_id_store_->GetChannelIDCount();
}

}  // namespace net
