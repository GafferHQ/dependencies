// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "content/common/content_export.h"
#include "content/public/common/referrer.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerClientType.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerResponseError.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerResponseType.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerState.h"
#include "url/gurl.h"

// This file is to have common definitions that are to be shared by
// browser and child process.

namespace content {

// Indicates the document main thread ID in the child process. This is used for
// messaging between the browser process and the child process.
static const int kDocumentMainThreadId = 0;

// Indicates invalid request ID (i.e. the sender does not expect it gets
// response for the message) for messaging between browser process
// and embedded worker.
static const int kInvalidServiceWorkerRequestId = -1;

// Constants for error messages.
extern const char kServiceWorkerRegisterErrorPrefix[];
extern const char kServiceWorkerUnregisterErrorPrefix[];
extern const char kServiceWorkerGetRegistrationErrorPrefix[];
extern const char kServiceWorkerGetRegistrationsErrorPrefix[];
extern const char kFetchScriptError[];

// Constants for invalid identifiers.
static const int kInvalidServiceWorkerHandleId = -1;
static const int kInvalidServiceWorkerRegistrationHandleId = -1;
static const int kInvalidServiceWorkerProviderId = -1;
static const int64 kInvalidServiceWorkerRegistrationId = -1;
static const int64 kInvalidServiceWorkerVersionId = -1;
static const int64 kInvalidServiceWorkerResourceId = -1;
static const int64 kInvalidServiceWorkerResponseId = -1;
static const int kInvalidEmbeddedWorkerThreadId = -1;
static const int kInvalidServiceWorkerClientId = 0;

// The HTTP cache is bypassed for Service Worker scripts if the last network
// fetch occurred over 24 hours ago.
static const int kServiceWorkerScriptMaxCacheAgeInHours = 24;

// ServiceWorker provider type.
enum ServiceWorkerProviderType {
  SERVICE_WORKER_PROVIDER_UNKNOWN,

  // For ServiceWorker clients.
  SERVICE_WORKER_PROVIDER_FOR_WINDOW,
  SERVICE_WORKER_PROVIDER_FOR_WORKER,
  SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER,

  // For ServiceWorkers.
  SERVICE_WORKER_PROVIDER_FOR_CONTROLLER,

  // For sandboxed frames.
  SERVICE_WORKER_PROVIDER_FOR_SANDBOXED_FRAME,

  SERVICE_WORKER_PROVIDER_TYPE_LAST =
      SERVICE_WORKER_PROVIDER_FOR_SANDBOXED_FRAME
};

enum FetchRequestMode {
  FETCH_REQUEST_MODE_SAME_ORIGIN,
  FETCH_REQUEST_MODE_NO_CORS,
  FETCH_REQUEST_MODE_CORS,
  FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT,
  FETCH_REQUEST_MODE_LAST = FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT
};

enum FetchCredentialsMode {
  FETCH_CREDENTIALS_MODE_OMIT,
  FETCH_CREDENTIALS_MODE_SAME_ORIGIN,
  FETCH_CREDENTIALS_MODE_INCLUDE,
  FETCH_CREDENTIALS_MODE_LAST = FETCH_CREDENTIALS_MODE_INCLUDE
};

// Indicates how the service worker handled a fetch event.
enum ServiceWorkerFetchEventResult {
  // Browser should fallback to native fetch.
  SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
  // Service worker provided a ServiceWorkerResponse.
  SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
  SERVICE_WORKER_FETCH_EVENT_LAST = SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE
};

struct ServiceWorkerCaseInsensitiveCompare {
  bool operator()(const std::string& lhs, const std::string& rhs) const {
    return base::strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
  }
};

typedef std::map<std::string, std::string, ServiceWorkerCaseInsensitiveCompare>
    ServiceWorkerHeaderMap;

// To dispatch fetch request from browser to child process.
struct CONTENT_EXPORT ServiceWorkerFetchRequest {
  ServiceWorkerFetchRequest();
  ServiceWorkerFetchRequest(const GURL& url,
                            const std::string& method,
                            const ServiceWorkerHeaderMap& headers,
                            const Referrer& referrer,
                            bool is_reload);
  ~ServiceWorkerFetchRequest();

  FetchRequestMode mode;
  RequestContextType request_context_type;
  RequestContextFrameType frame_type;
  GURL url;
  std::string method;
  ServiceWorkerHeaderMap headers;
  std::string blob_uuid;
  uint64 blob_size;
  Referrer referrer;
  FetchCredentialsMode credentials_mode;
  bool is_reload;
};

// Represents a response to a fetch.
struct CONTENT_EXPORT ServiceWorkerResponse {
  ServiceWorkerResponse();
  ServiceWorkerResponse(const GURL& url,
                        int status_code,
                        const std::string& status_text,
                        blink::WebServiceWorkerResponseType response_type,
                        const ServiceWorkerHeaderMap& headers,
                        const std::string& blob_uuid,
                        uint64 blob_size,
                        const GURL& stream_url,
                        blink::WebServiceWorkerResponseError error);
  ~ServiceWorkerResponse();

  GURL url;
  int status_code;
  std::string status_text;
  blink::WebServiceWorkerResponseType response_type;
  ServiceWorkerHeaderMap headers;
  std::string blob_uuid;
  uint64 blob_size;
  GURL stream_url;
  blink::WebServiceWorkerResponseError error;
};

// Represents initialization info for a WebServiceWorker object.
struct CONTENT_EXPORT ServiceWorkerObjectInfo {
  ServiceWorkerObjectInfo();
  int handle_id;
  GURL url;
  blink::WebServiceWorkerState state;
  int64 version_id;
};

struct CONTENT_EXPORT ServiceWorkerRegistrationObjectInfo {
  ServiceWorkerRegistrationObjectInfo();
  int handle_id;
  GURL scope;
  int64 registration_id;
};

struct ServiceWorkerVersionAttributes {
  ServiceWorkerObjectInfo installing;
  ServiceWorkerObjectInfo waiting;
  ServiceWorkerObjectInfo active;
};

class ChangedVersionAttributesMask {
 public:
  enum {
    INSTALLING_VERSION = 1 << 0,
    WAITING_VERSION = 1 << 1,
    ACTIVE_VERSION = 1 << 2,
    CONTROLLING_VERSION = 1 << 3,
  };

  ChangedVersionAttributesMask() : changed_(0) {}
  explicit ChangedVersionAttributesMask(int changed) : changed_(changed) {}

  int changed() const { return changed_; }

  void add(int changed_versions) { changed_ |= changed_versions; }
  bool installing_changed() const { return !!(changed_ & INSTALLING_VERSION); }
  bool waiting_changed() const { return !!(changed_ & WAITING_VERSION); }
  bool active_changed() const { return !!(changed_ & ACTIVE_VERSION); }
  bool controller_changed() const { return !!(changed_ & CONTROLLING_VERSION); }

 private:
  int changed_;
};

struct ServiceWorkerClientQueryOptions {
  ServiceWorkerClientQueryOptions();
  blink::WebServiceWorkerClientType client_type;
  bool include_uncontrolled;
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
