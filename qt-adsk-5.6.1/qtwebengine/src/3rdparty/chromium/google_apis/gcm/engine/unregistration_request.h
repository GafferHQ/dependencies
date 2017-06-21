// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_UNREGISTRATION_REQUEST_H_
#define GOOGLE_APIS_GCM_ENGINE_UNREGISTRATION_REQUEST_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace gcm {

class GCMStatsRecorder;

// Encapsulates the common logic applying to both GCM unregistration requests
// and InstanceID delete-token requests. In case an attempt fails, it will retry
// using the backoff policy.
// TODO(fgorski): Consider sharing code with RegistrationRequest if possible.
class GCM_EXPORT UnregistrationRequest : public net::URLFetcherDelegate {
 public:
  // Outcome of the response parsing. Note that these enums are consumed by a
  // histogram, so ordering should not be modified.
  enum Status {
    SUCCESS,                  // Unregistration completed successfully.
    URL_FETCHING_FAILED,      // URL fetching failed.
    NO_RESPONSE_BODY,         // No response body.
    RESPONSE_PARSING_FAILED,  // Failed to parse a meaningful output from
                              // response
                              // body.
    INCORRECT_APP_ID,         // App ID returned by the fetcher does not match
                              // request.
    INVALID_PARAMETERS,       // Request parameters were invalid.
    SERVICE_UNAVAILABLE,      // Unregistration service unavailable.
    INTERNAL_SERVER_ERROR,    // Internal server error happened during request.
    HTTP_NOT_OK,              // HTTP response code was not OK.
    UNKNOWN_ERROR,            // Unknown error.
    REACHED_MAX_RETRIES,      // Reached maximum number of retries.
    // NOTE: Always keep this entry at the end. Add new status types only
    // immediately above this line. Make sure to update the corresponding
    // histogram enum accordingly.
    UNREGISTRATION_STATUS_COUNT,
  };

  // Callback completing the unregistration request.
  typedef base::Callback<void(Status success)> UnregistrationCallback;

  // Defines the common info about an unregistration/token-deletion request.
  // All parameters are mandatory.
  struct GCM_EXPORT RequestInfo {
    RequestInfo(uint64 android_id,
                uint64 security_token,
                const std::string& app_id);
    ~RequestInfo();

    // Android ID of the device.
    uint64 android_id;
    // Security token of the device.
    uint64 security_token;
    // Application ID.
    std::string app_id;
  };

  // Encapsulates the custom logic that is needed to build and process the
  // unregistration request.
  class GCM_EXPORT CustomRequestHandler {
   public:
    CustomRequestHandler();
    virtual ~CustomRequestHandler();

    // Builds the HTTP request body data. It is called after
    // UnregistrationRequest::BuildRequestBody to append more custom info to
    // |body|. Note that the request body is encoded in HTTP form format.
    virtual void BuildRequestBody(std::string* body) = 0;

    // Parses the HTTP response. It is called after
    // UnregistrationRequest::ParseResponse to proceed the parsing.
    virtual Status ParseResponse(const net::URLFetcher* source) = 0;

    // Reports various UMAs, including status, retry count and completion time.
    virtual void ReportUMAs(Status status,
                            int retry_count,
                            base::TimeDelta complete_time) = 0;
  };

  // Creates an instance of UnregistrationRequest. |callback| will be called
  // once registration has been revoked or there has been an error that makes
  // further retries pointless.
  UnregistrationRequest(
      const GURL& registration_url,
      const RequestInfo& request_info,
      scoped_ptr<CustomRequestHandler> custom_request_handler,
      const net::BackoffEntry::Policy& backoff_policy,
      const UnregistrationCallback& callback,
      int max_retry_count,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      GCMStatsRecorder* recorder,
      const std::string& source_to_record);
  ~UnregistrationRequest() override;

  // Starts an unregistration request.
  void Start();

 private:
  // URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void BuildRequestHeaders(std::string* extra_headers);
  void BuildRequestBody(std::string* body);
  Status ParseResponse(const net::URLFetcher* source);

  // Schedules a retry attempt and informs the backoff of previous request's
  // failure, when |update_backoff| is true.
  void RetryWithBackoff(bool update_backoff);

  UnregistrationCallback callback_;
  RequestInfo request_info_;
  scoped_ptr<CustomRequestHandler> custom_request_handler_;
  GURL registration_url_;

  net::BackoffEntry backoff_entry_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
  base::TimeTicks request_start_time_;
  int retries_left_;

  // Recorder that records GCM activities for debugging purpose. Not owned.
  GCMStatsRecorder* recorder_;
  std::string source_to_record_;

  base::WeakPtrFactory<UnregistrationRequest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UnregistrationRequest);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_UNREGISTRATION_REQUEST_H_
