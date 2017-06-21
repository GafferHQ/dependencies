// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert_net/cert_net_fetcher_impl.h"

#include "base/callback_helpers.h"
#include "base/containers/linked_list.h"
#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/stl_util.h"
#include "base/timer/timer.h"
#include "net/base/load_flags.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"

// TODO(eroman): Add support for POST parameters.
// TODO(eroman): Add controls for bypassing the cache.
// TODO(eroman): Add a maximum number of in-flight jobs/requests.
// TODO(eroman): Add NetLog integration.

namespace net {

namespace {

// The size of the buffer used for reading the response body of the URLRequest.
const int kReadBufferSizeInBytes = 4096;

// The maximum size in bytes for the response body when fetching a CRL.
const int kMaxResponseSizeInBytesForCrl = 5 * 1024 * 1024;

// The maximum size in bytes for the response body when fetching an AIA URL
// (caIssuers/OCSP).
const int kMaxResponseSizeInBytesForAia = 64 * 1024;

// The default timeout in seconds for fetch requests.
const int kTimeoutSeconds = 15;

// Policy for which URLs are allowed to be fetched. This is called both for the
// initial URL and for each redirect. Returns OK on success or a net error
// code on failure.
Error CanFetchUrl(const GURL& url) {
  if (!url.SchemeIs("http"))
    return ERR_DISALLOWED_URL_SCHEME;
  return OK;
}

base::TimeDelta GetTimeout(int timeout_milliseconds) {
  if (timeout_milliseconds == CertNetFetcher::DEFAULT)
    return base::TimeDelta::FromSeconds(kTimeoutSeconds);
  return base::TimeDelta::FromMilliseconds(timeout_milliseconds);
}

size_t GetMaxResponseBytes(int max_response_bytes,
                           size_t default_max_response_bytes) {
  if (max_response_bytes == CertNetFetcher::DEFAULT)
    return default_max_response_bytes;

  // Ensure that the specified limit is not negative, and cannot result in an
  // overflow while reading.
  base::CheckedNumeric<size_t> check(max_response_bytes);
  check += kReadBufferSizeInBytes;
  DCHECK(check.IsValid());

  return max_response_bytes;
}

enum HttpMethod {
  HTTP_METHOD_GET,
  HTTP_METHOD_POST,
};

}  // namespace

// CertNetFetcherImpl::RequestImpl tracks an outstanding call to Fetch().
class CertNetFetcherImpl::RequestImpl : public CertNetFetcher::Request,
                                        public base::LinkNode<RequestImpl> {
 public:
  RequestImpl(Job* job, const FetchCallback& callback)
      : callback_(callback), job_(job) {
    DCHECK(!callback.is_null());
  }

  // Deletion cancels the outstanding request.
  ~RequestImpl() override;

  void OnJobCancelled(Job* job) {
    DCHECK_EQ(job_, job);
    job_ = nullptr;
    callback_.Reset();
  }

  void OnJobCompleted(Job* job,
                      Error error,
                      const std::vector<uint8_t>& response_body) {
    DCHECK_EQ(job_, job);
    job_ = nullptr;
    base::ResetAndReturn(&callback_).Run(error, response_body);
  }

 private:
  // The callback to invoke when the request has completed.
  FetchCallback callback_;

  // A non-owned pointer to the job that is executing the request.
  Job* job_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RequestImpl);
};

struct CertNetFetcherImpl::RequestParams {
  RequestParams();

  bool operator<(const RequestParams& other) const;

  GURL url;
  HttpMethod http_method;
  size_t max_response_bytes;

  // If set to a value <= 0 then means "no timeout".
  base::TimeDelta timeout;

  // IMPORTANT: When adding fields to this structure, update operator<().

 private:
  DISALLOW_COPY_AND_ASSIGN(RequestParams);
};

CertNetFetcherImpl::RequestParams::RequestParams()
    : http_method(HTTP_METHOD_GET), max_response_bytes(0) {
}

bool CertNetFetcherImpl::RequestParams::operator<(
    const RequestParams& other) const {
  if (url != other.url)
    return url < other.url;
  if (http_method != other.http_method)
    return http_method < other.http_method;
  if (max_response_bytes != other.max_response_bytes)
    return max_response_bytes < other.max_response_bytes;
  return timeout < other.timeout;
}

// CertNetFetcherImpl::Job tracks an outstanding URLRequest as well as all of
// the pending requests for it.
class CertNetFetcherImpl::Job : public URLRequest::Delegate {
 public:
  Job(scoped_ptr<RequestParams> request_params, CertNetFetcherImpl* parent);
  ~Job() override;

  // Cancels the job and all requests attached to it. No callbacks will be
  // invoked following cancellation.
  void Cancel();

  const RequestParams& request_params() const { return *request_params_; }

  // Create a request and attaches it to the job. When the job completes it will
  // notify the request of completion through OnJobCompleted. Note that the Job
  // does NOT own the request.
  scoped_ptr<Request> CreateRequest(const FetchCallback& callback);

  // Removes |request| from the job.
  void DetachRequest(RequestImpl* request);

  // Creates and starts a URLRequest for the job. After the request has
  // completed, OnJobCompleted() will be invoked and all the registered requests
  // notified of completion.
  void StartURLRequest(URLRequestContext* context);

 private:
  // The pointers in RequestList are not owned by the Job.
  using RequestList = base::LinkedList<RequestImpl>;

  // Implementation of URLRequest::Delegate
  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnResponseStarted(URLRequest* request) override;
  void OnReadCompleted(URLRequest* request, int bytes_read) override;

  // Clears the URLRequest and timer. Helper for doing work common to
  // cancellation and job completion.
  void Stop();

  // Reads as much data as available from |request|.
  void ReadBody(URLRequest* request);

  // Helper to copy the partial bytes read from the read IOBuffer to an
  // aggregated buffer.
  bool ConsumeBytesRead(URLRequest* request, int num_bytes);

  // Called once the job has exceeded its deadline.
  void OnTimeout();

  // Called when the URLRequest has completed (either success or failure).
  void OnUrlRequestCompleted(URLRequest* request);

  // Called when the Job has completed. The job may finish in response to a
  // timeout, an invalid URL, or the URLRequest completing. By the time this
  // method is called, the response variables have been assigned
  // (result_net_error_ and response_body_).
  void OnJobCompleted();

  // The requests attached to this job.
  RequestList requests_;

  // The input parameters for starting a URLRequest.
  scoped_ptr<RequestParams> request_params_;

  // The URLRequest response information.
  std::vector<uint8_t> response_body_;
  Error result_net_error_;

  scoped_ptr<URLRequest> url_request_;
  scoped_refptr<IOBuffer> read_buffer_;

  // Used to timeout the job when the URLRequest takes too long. This timer is
  // also used for notifying a failure to start the URLRequest.
  base::OneShotTimer<Job> timer_;

  // Non-owned pointer to the CertNetFetcherImpl that created this job.
  CertNetFetcherImpl* parent_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

CertNetFetcherImpl::RequestImpl::~RequestImpl() {
  if (job_)
    job_->DetachRequest(this);
}

CertNetFetcherImpl::Job::Job(scoped_ptr<RequestParams> request_params,
                             CertNetFetcherImpl* parent)
    : request_params_(request_params.Pass()),
      result_net_error_(ERR_IO_PENDING),
      parent_(parent) {
}

CertNetFetcherImpl::Job::~Job() {
  Cancel();
}

void CertNetFetcherImpl::Job::Cancel() {
  parent_ = nullptr;

  // Notify each request of cancellation and remove it from the list.
  for (base::LinkNode<RequestImpl>* current = requests_.head();
       current != requests_.end();) {
    base::LinkNode<RequestImpl>* next = current->next();
    current->value()->OnJobCancelled(this);
    current->RemoveFromList();
    current = next;
  }

  DCHECK(requests_.empty());

  Stop();
}

scoped_ptr<CertNetFetcher::Request> CertNetFetcherImpl::Job::CreateRequest(
    const FetchCallback& callback) {
  scoped_ptr<RequestImpl> request(new RequestImpl(this, callback));
  requests_.Append(request.get());
  return request.Pass();
}

void CertNetFetcherImpl::Job::DetachRequest(RequestImpl* request) {
  scoped_ptr<Job> delete_this;

  request->RemoveFromList();

  // If there are no longer any requests attached to the job then
  // cancel and delete it.
  if (requests_.empty() && !parent_->IsCurrentlyCompletingJob(this))
    delete_this = parent_->RemoveJob(this);
}

void CertNetFetcherImpl::Job::StartURLRequest(URLRequestContext* context) {
  Error error = CanFetchUrl(request_params_->url);
  if (error != OK) {
    result_net_error_ = error;
    // The CertNetFetcher's API contract is that requests always complete
    // asynchronously. Use the timer class so the task is easily cancelled.
    timer_.Start(FROM_HERE, base::TimeDelta(), this, &Job::OnJobCompleted);
    return;
  }

  // Start the URLRequest.
  read_buffer_ = new IOBuffer(kReadBufferSizeInBytes);
  url_request_ =
      context->CreateRequest(request_params_->url, DEFAULT_PRIORITY, this);
  if (request_params_->http_method == HTTP_METHOD_POST)
    url_request_->set_method("POST");
  url_request_->SetLoadFlags(LOAD_DO_NOT_SAVE_COOKIES |
                             LOAD_DO_NOT_SEND_COOKIES);
  url_request_->Start();

  // Start a timer to limit how long the job runs for.
  if (request_params_->timeout > base::TimeDelta())
    timer_.Start(FROM_HERE, request_params_->timeout, this, &Job::OnTimeout);
}

void CertNetFetcherImpl::Job::OnReceivedRedirect(
    URLRequest* request,
    const RedirectInfo& redirect_info,
    bool* defer_redirect) {
  DCHECK_EQ(url_request_.get(), request);

  // Ensure that the new URL matches the policy.
  Error error = CanFetchUrl(redirect_info.new_url);
  if (error != OK) {
    request->CancelWithError(error);
    OnUrlRequestCompleted(request);
    return;
  }
}

void CertNetFetcherImpl::Job::OnResponseStarted(URLRequest* request) {
  DCHECK_EQ(url_request_.get(), request);

  if (!request->status().is_success()) {
    OnUrlRequestCompleted(request);
    return;
  }

  if (request->GetResponseCode() != 200) {
    // TODO(eroman): Use a more specific error code.
    request->CancelWithError(ERR_FAILED);
    OnUrlRequestCompleted(request);
    return;
  }

  ReadBody(request);
}

void CertNetFetcherImpl::Job::OnReadCompleted(URLRequest* request,
                                              int bytes_read) {
  DCHECK_EQ(url_request_.get(), request);

  // Keep reading the response body.
  if (ConsumeBytesRead(request, bytes_read))
    ReadBody(request);
}

void CertNetFetcherImpl::Job::Stop() {
  timer_.Stop();
  url_request_.reset();
}

void CertNetFetcherImpl::Job::ReadBody(URLRequest* request) {
  // Read as many bytes as are available synchronously.
  int num_bytes;
  while (
      request->Read(read_buffer_.get(), kReadBufferSizeInBytes, &num_bytes)) {
    if (!ConsumeBytesRead(request, num_bytes))
      return;
  }

  // Check whether the read failed synchronously.
  if (!request->status().is_io_pending())
    OnUrlRequestCompleted(request);
  return;
}

bool CertNetFetcherImpl::Job::ConsumeBytesRead(URLRequest* request,
                                               int num_bytes) {
  if (num_bytes <= 0) {
    // Error while reading, or EOF.
    OnUrlRequestCompleted(request);
    return false;
  }

  // Enforce maximum size bound.
  if (num_bytes + response_body_.size() > request_params_->max_response_bytes) {
    request->CancelWithError(ERR_FILE_TOO_BIG);
    OnUrlRequestCompleted(request);
    return false;
  }

  // Append the data to |response_body_|.
  response_body_.reserve(num_bytes);
  response_body_.insert(response_body_.end(), read_buffer_->data(),
                        read_buffer_->data() + num_bytes);
  return true;
}

void CertNetFetcherImpl::Job::OnTimeout() {
  result_net_error_ = ERR_TIMED_OUT;
  url_request_->CancelWithError(result_net_error_);
  OnJobCompleted();
}

void CertNetFetcherImpl::Job::OnUrlRequestCompleted(URLRequest* request) {
  DCHECK_EQ(request, url_request_.get());

  if (request->status().is_success())
    result_net_error_ = OK;
  else
    result_net_error_ = static_cast<Error>(request->status().error());

  OnJobCompleted();
}

void CertNetFetcherImpl::Job::OnJobCompleted() {
  // Stop the timer and clear the URLRequest.
  Stop();

  // Invoking the callbacks is subtle as state may be mutated while iterating
  // through the callbacks:
  //
  //   * The parent CertNetFetcherImpl may be deleted
  //   * Requests in this job may be cancelled

  scoped_ptr<Job> delete_this = parent_->RemoveJob(this);
  parent_->SetCurrentlyCompletingJob(this);

  while (!requests_.empty()) {
    base::LinkNode<RequestImpl>* request = requests_.head();
    request->RemoveFromList();
    request->value()->OnJobCompleted(this, result_net_error_, response_body_);
  }

  if (parent_)
    parent_->ClearCurrentlyCompletingJob(this);
}

CertNetFetcherImpl::CertNetFetcherImpl(URLRequestContext* context)
    : currently_completing_job_(nullptr), context_(context) {
}

CertNetFetcherImpl::~CertNetFetcherImpl() {
  STLDeleteElements(&jobs_);

  // The CertNetFetcherImpl was destroyed in a FetchCallback. Detach all
  // remaining requests from the job so no further callbacks are called.
  if (currently_completing_job_)
    currently_completing_job_->Cancel();
}

scoped_ptr<CertNetFetcher::Request> CertNetFetcherImpl::FetchCaIssuers(
    const GURL& url,
    int timeout_milliseconds,
    int max_response_bytes,
    const FetchCallback& callback) {
  scoped_ptr<RequestParams> request_params(new RequestParams);

  request_params->url = url;
  request_params->http_method = HTTP_METHOD_GET;
  request_params->timeout = GetTimeout(timeout_milliseconds);
  request_params->max_response_bytes =
      GetMaxResponseBytes(max_response_bytes, kMaxResponseSizeInBytesForAia);

  return Fetch(request_params.Pass(), callback);
}

scoped_ptr<CertNetFetcher::Request> CertNetFetcherImpl::FetchCrl(
    const GURL& url,
    int timeout_milliseconds,
    int max_response_bytes,
    const FetchCallback& callback) {
  scoped_ptr<RequestParams> request_params(new RequestParams);

  request_params->url = url;
  request_params->http_method = HTTP_METHOD_GET;
  request_params->timeout = GetTimeout(timeout_milliseconds);
  request_params->max_response_bytes =
      GetMaxResponseBytes(max_response_bytes, kMaxResponseSizeInBytesForCrl);

  return Fetch(request_params.Pass(), callback);
}

scoped_ptr<CertNetFetcher::Request> CertNetFetcherImpl::FetchOcsp(
    const GURL& url,
    int timeout_milliseconds,
    int max_response_bytes,
    const FetchCallback& callback) {
  scoped_ptr<RequestParams> request_params(new RequestParams);

  request_params->url = url;
  request_params->http_method = HTTP_METHOD_GET;
  request_params->timeout = GetTimeout(timeout_milliseconds);
  request_params->max_response_bytes =
      GetMaxResponseBytes(max_response_bytes, kMaxResponseSizeInBytesForAia);

  return Fetch(request_params.Pass(), callback);
}

bool CertNetFetcherImpl::JobComparator::operator()(const Job* job1,
                                                   const Job* job2) const {
  return job1->request_params() < job2->request_params();
}

scoped_ptr<CertNetFetcher::Request> CertNetFetcherImpl::Fetch(
    scoped_ptr<RequestParams> request_params,
    const FetchCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If there is an in-progress job that matches the request parameters use it.
  // Otherwise start a new job.
  Job* job = FindJob(*request_params);

  if (!job) {
    job = new Job(request_params.Pass(), this);
    jobs_.insert(job);
    job->StartURLRequest(context_);
  }

  return job->CreateRequest(callback);
}

struct CertNetFetcherImpl::JobToRequestParamsComparator {
  bool operator()(const Job* job,
                  const CertNetFetcherImpl::RequestParams& value) const {
    return job->request_params() < value;
  }
};

CertNetFetcherImpl::Job* CertNetFetcherImpl::FindJob(
    const RequestParams& params) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The JobSet is kept in sorted order so items can be found using binary
  // search.
  JobSet::iterator it = std::lower_bound(jobs_.begin(), jobs_.end(), params,
                                         JobToRequestParamsComparator());
  if (it != jobs_.end() && !(params < (*it)->request_params()))
    return *it;
  return nullptr;
}

scoped_ptr<CertNetFetcherImpl::Job> CertNetFetcherImpl::RemoveJob(Job* job) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool erased_job = jobs_.erase(job) == 1;
  CHECK(erased_job);
  return make_scoped_ptr(job);
}

void CertNetFetcherImpl::SetCurrentlyCompletingJob(Job* job) {
  DCHECK(!currently_completing_job_);
  DCHECK(job);
  currently_completing_job_ = job;
}

void CertNetFetcherImpl::ClearCurrentlyCompletingJob(Job* job) {
  DCHECK_EQ(currently_completing_job_, job);
  currently_completing_job_ = nullptr;
}

bool CertNetFetcherImpl::IsCurrentlyCompletingJob(Job* job) {
  return job == currently_completing_job_;
}

}  // namespace net
