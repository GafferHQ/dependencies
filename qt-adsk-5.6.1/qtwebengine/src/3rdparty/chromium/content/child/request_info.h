// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_REQUEST_INFO_H_
#define CONTENT_CHILD_REQUEST_INFO_H_

#include <stdint.h>

#include <string>

#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/referrer.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "net/base/request_priority.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "url/gurl.h"

namespace content {

// Structure used when calling BlinkPlatformImpl::CreateResourceLoader().
struct CONTENT_EXPORT RequestInfo {
  RequestInfo();
  ~RequestInfo();

  // HTTP-style method name (e.g., "GET" or "POST").
  std::string method;

  // Absolute URL encoded in ASCII per the rules of RFC-2396.
  GURL url;

  // URL of the document in the top-level window, which may be checked by the
  // third-party cookie blocking policy.
  GURL first_party_for_cookies;

  // Optional parameter, the referrer to use for the request for the url member.
  Referrer referrer;

  // For HTTP(S) requests, the headers parameter can be a \r\n-delimited and
  // \r\n-terminated list of MIME headers.  They should be ASCII-encoded using
  // the standard MIME header encoding rules.  The headers parameter can also
  // be null if no extra request headers need to be set.
  std::string headers;

  // Composed of the values defined in url_request_load_flags.h.
  int load_flags;

  // Process id of the process making the request.
  int requestor_pid;

  // Indicates if the current request is the main frame load, a sub-frame
  // load, or a sub objects load.
  ResourceType request_type;
  RequestContextType fetch_request_context_type;
  RequestContextFrameType fetch_frame_type;

  // Indicates the priority of this request, as determined by WebKit.
  net::RequestPriority priority;

  // Used for plugin to browser requests.
  uint32_t request_context;

  // Identifies what appcache host this request is associated with.
  int appcache_host_id;

  // Used to associated the bridge with a frame's network context.
  int routing_id;

  // If true, then the response body will be downloaded to a file and the
  // path to that file will be provided in ResponseInfo::download_file_path.
  bool download_to_file;

  // True if the request was user initiated.
  bool has_user_gesture;

  // True if the request should not be handled by the ServiceWorker.
  bool skip_service_worker;

  // True if corresponding AppCache group should be resetted.
  bool should_reset_appcache;

  // The request mode passed to the ServiceWorker.
  FetchRequestMode fetch_request_mode;

  // The credentials mode passed to the ServiceWorker.
  FetchCredentialsMode fetch_credentials_mode;

  // TODO(mmenke): Investigate if enable_load_timing is safe to remove.
  // True if load timing data should be collected for the request.
  bool enable_load_timing;

  // True if upload progress should be available.
  bool enable_upload_progress;

  // True if login prompts for this request should be supressed. Cached
  // credentials or default credentials may still be used for authentication.
  bool do_not_prompt_for_login;

  // Extra data associated with this request.  We do not own this pointer.
  blink::WebURLRequest::ExtraData* extra_data;

 private:
  DISALLOW_COPY_AND_ASSIGN(RequestInfo);
};

}  // namespace content

#endif  // CONTENT_CHILD_REQUEST_INFO_H_
