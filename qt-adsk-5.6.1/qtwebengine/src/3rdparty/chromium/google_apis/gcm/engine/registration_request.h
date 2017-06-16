// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_REGISTRATION_REQUEST_H_
#define GOOGLE_APIS_GCM_ENGINE_REGISTRATION_REQUEST_H_

#include <map>
#include <string>
#include <vector>

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

// Registration request is used to obtain registration IDs for applications that
// want to use GCM. It requires a set of parameters to be specified to identify
// the Chrome instance, the user, the application and a set of senders that will
// be authorized to address the application using it's assigned registration ID.
class GCM_EXPORT RegistrationRequest : public net::URLFetcherDelegate {
 public:
  // This enum is also used in an UMA histogram (GCMRegistrationRequestStatus
  // enum defined in tools/metrics/histograms/histogram.xml). Hence the entries
  // here shouldn't be deleted or re-ordered and new ones should be added to
  // the end.
  enum Status {
    SUCCESS,                    // Registration completed successfully.
    INVALID_PARAMETERS,         // One of request paramteres was invalid.
    INVALID_SENDER,             // One of the provided senders was invalid.
    AUTHENTICATION_FAILED,      // Authentication failed.
    DEVICE_REGISTRATION_ERROR,  // Chrome is not properly registered.
    UNKNOWN_ERROR,              // Unknown error.
    URL_FETCHING_FAILED,        // URL fetching failed.
    HTTP_NOT_OK,                // HTTP status was not OK.
    RESPONSE_PARSING_FAILED,    // Registration response parsing failed.
    REACHED_MAX_RETRIES,        // Reached maximum number of retries.
    // NOTE: always keep this entry at the end. Add new status types only
    // immediately above this line. Make sure to update the corresponding
    // histogram enum accordingly.
    STATUS_COUNT
  };

  // Callback completing the registration request.
  typedef base::Callback<void(Status status,
                              const std::string& registration_id)>
      RegistrationCallback;

  // Defines the common info about a registration/token request. All parameters
  // are mandatory.
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
  // registration request.
  class GCM_EXPORT CustomRequestHandler {
   public:
    CustomRequestHandler();
    virtual ~CustomRequestHandler();

    // Builds the HTTP request body data. It is called after
    // RegistrationRequest::BuildRequestBody to append more custom info to
    // |body|. Note that the request body is encoded in HTTP form format.
    virtual void BuildRequestBody(std::string* body) = 0;

    // Reports various UMAs, including status, retry count and completion time.
    virtual void ReportUMAs(Status status,
                            int retry_count,
                            base::TimeDelta complete_time) = 0;
  };

  RegistrationRequest(
      const GURL& registration_url,
      const RequestInfo& request_info,
      scoped_ptr<CustomRequestHandler> custom_request_handler,
      const net::BackoffEntry::Policy& backoff_policy,
      const RegistrationCallback& callback,
      int max_retry_count,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      GCMStatsRecorder* recorder,
      const std::string& source_to_record);
  ~RegistrationRequest() override;

  void Start();

  // URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

 private:
  // Schedules a retry attempt, informs the backoff of a previous request's
  // failure, when |update_backoff| is true.
  void RetryWithBackoff(bool update_backoff);

  void BuildRequestHeaders(std::string* extra_headers);
  void BuildRequestBody(std::string* body);

  // Parse the response returned by the URL fetcher into token, and returns the
  // status.
  Status ParseResponse(const net::URLFetcher* source, std::string* token);

  RegistrationCallback callback_;
  RequestInfo request_info_;
  scoped_ptr<CustomRequestHandler> custom_request_handler_;
  GURL registration_url_;

  net::BackoffEntry backoff_entry_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
  int retries_left_;
  base::TimeTicks request_start_time_;

  // Recorder that records GCM activities for debugging purpose. Not owned.
  GCMStatsRecorder* recorder_;
  std::string source_to_record_;

  base::WeakPtrFactory<RegistrationRequest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationRequest);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_REGISTRATION_REQUEST_H_
