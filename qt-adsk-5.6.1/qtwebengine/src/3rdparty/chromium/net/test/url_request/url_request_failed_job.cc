// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/url_request/url_request_failed_job.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"

namespace net {

namespace {

const char kMockHostname[] = "mock.failed.request";

// String names of failure phases matching FailurePhase enum.
const char* kFailurePhase[]{
    "start",      // START
    "readsync",   // READ_SYNC
    "readasync",  // READ_ASYNC
};

static_assert(arraysize(kFailurePhase) ==
                  URLRequestFailedJob::FailurePhase::MAX_FAILURE_PHASE,
              "kFailurePhase must match FailurePhase enum");

class MockJobInterceptor : public URLRequestInterceptor {
 public:
  MockJobInterceptor() {}
  ~MockJobInterceptor() override {}

  // URLRequestJobFactory::ProtocolHandler implementation:
  URLRequestJob* MaybeInterceptRequest(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    int net_error = OK;
    URLRequestFailedJob::FailurePhase phase =
        URLRequestFailedJob::FailurePhase::MAX_FAILURE_PHASE;
    for (size_t i = 0; i < arraysize(kFailurePhase); i++) {
      std::string phase_error_string;
      if (GetValueForKeyInQuery(request->url(), kFailurePhase[i],
                                &phase_error_string)) {
        if (base::StringToInt(phase_error_string, &net_error)) {
          phase = static_cast<URLRequestFailedJob::FailurePhase>(i);
          break;
        }
      }
    }
    return new URLRequestFailedJob(request, network_delegate, phase, net_error);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockJobInterceptor);
};

GURL GetMockUrl(const std::string& scheme,
                const std::string& hostname,
                URLRequestFailedJob::FailurePhase phase,
                int net_error) {
  CHECK_GE(phase, URLRequestFailedJob::FailurePhase::START);
  CHECK_LE(phase, URLRequestFailedJob::FailurePhase::READ_ASYNC);
  CHECK_LT(net_error, OK);
  return GURL(scheme + "://" + hostname + "/error?" + kFailurePhase[phase] +
              "=" + base::IntToString(net_error));
}

}  // namespace

URLRequestFailedJob::URLRequestFailedJob(URLRequest* request,
                                         NetworkDelegate* network_delegate,
                                         FailurePhase phase,
                                         int net_error)
    : URLRequestJob(request, network_delegate),
      phase_(phase),
      net_error_(net_error),
      weak_factory_(this) {
  CHECK_GE(phase, URLRequestFailedJob::FailurePhase::START);
  CHECK_LE(phase, URLRequestFailedJob::FailurePhase::READ_ASYNC);
  CHECK_LT(net_error, OK);
}

URLRequestFailedJob::URLRequestFailedJob(URLRequest* request,
                                         NetworkDelegate* network_delegate,
                                         int net_error)
    : URLRequestFailedJob(request, network_delegate, START, net_error) {
}

void URLRequestFailedJob::Start() {
  if (phase_ == START) {
    if (net_error_ != ERR_IO_PENDING) {
      NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, net_error_));
      return;
    }
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
    return;
  }
  response_info_.headers = new net::HttpResponseHeaders("HTTP/1.1 200 OK");
  NotifyHeadersComplete();
}

bool URLRequestFailedJob::ReadRawData(IOBuffer* buf,
                                      int buf_size,
                                      int* bytes_read) {
  CHECK(phase_ == READ_SYNC || phase_ == READ_ASYNC);
  if (net_error_ != ERR_IO_PENDING && phase_ == READ_SYNC) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, net_error_));
    return false;
  }

  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  if (net_error_ == ERR_IO_PENDING)
    return false;

  DCHECK_EQ(READ_ASYNC, phase_);
  DCHECK_NE(ERR_IO_PENDING, net_error_);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestFailedJob::NotifyDone, weak_factory_.GetWeakPtr(),
                 URLRequestStatus(URLRequestStatus::FAILED, net_error_)));
  return false;
}

int URLRequestFailedJob::GetResponseCode() const {
  // If we have headers, get the response code from them.
  if (response_info_.headers)
    return response_info_.headers->response_code();
  return URLRequestJob::GetResponseCode();
}

void URLRequestFailedJob::GetResponseInfo(HttpResponseInfo* info) {
  *info = response_info_;
}

// static
void URLRequestFailedJob::AddUrlHandler() {
  return AddUrlHandlerForHostname(kMockHostname);
}

// static
void URLRequestFailedJob::AddUrlHandlerForHostname(
    const std::string& hostname) {
  URLRequestFilter* filter = URLRequestFilter::GetInstance();
  // Add |hostname| to URLRequestFilter for HTTP and HTTPS.
  filter->AddHostnameInterceptor(
      "http", hostname,
      scoped_ptr<URLRequestInterceptor>(new MockJobInterceptor()));
  filter->AddHostnameInterceptor(
      "https", hostname,
      scoped_ptr<URLRequestInterceptor>(new MockJobInterceptor()));
}

// static
GURL URLRequestFailedJob::GetMockHttpUrl(int net_error) {
  return GetMockHttpUrlForHostname(net_error, kMockHostname);
}

// static
GURL URLRequestFailedJob::GetMockHttpsUrl(int net_error) {
  return GetMockHttpsUrlForHostname(net_error, kMockHostname);
}

// static
GURL URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(FailurePhase phase,
                                                         int net_error) {
  return GetMockUrl("http", kMockHostname, phase, net_error);
}

// static
GURL URLRequestFailedJob::GetMockHttpUrlForHostname(
    int net_error,
    const std::string& hostname) {
  return GetMockUrl("http", hostname, START, net_error);
}

// static
GURL URLRequestFailedJob::GetMockHttpsUrlForHostname(
    int net_error,
    const std::string& hostname) {
  return GetMockUrl("https", hostname, START, net_error);
}

URLRequestFailedJob::~URLRequestFailedJob() {
}

}  // namespace net
