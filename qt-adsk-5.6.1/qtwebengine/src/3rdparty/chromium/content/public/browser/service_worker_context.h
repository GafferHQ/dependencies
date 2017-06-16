// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "content/public/browser/service_worker_usage_info.h"
#include "net/base/completion_callback.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace content {

// Represents the per-StoragePartition ServiceWorker data.
class ServiceWorkerContext {
 public:
  // https://rawgithub.com/slightlyoff/ServiceWorker/master/spec/service_worker/index.html#url-scope:
  // roughly, must be of the form "<origin>/<path>/*".
  typedef GURL Scope;

  typedef base::Callback<void(bool success)> ResultCallback;

  typedef base::Callback<void(const std::vector<ServiceWorkerUsageInfo>&
                                  usage_info)> GetUsageInfoCallback;

  typedef base::Callback<void(bool has_service_worker)>
      CheckHasServiceWorkerCallback;

  // Registers the header name which should not be passed to the ServiceWorker.
  // Must be called from the IO thread.
  CONTENT_EXPORT static void AddExcludedHeadersForFetchEvent(
      const std::set<std::string>& header_names);

  // Returns true if the header name should not be passed to the ServiceWorker.
  // Must be called from the IO thread.
  static bool IsExcludedHeaderNameForFetchEvent(const std::string& header_name);

  // Retrieves the ServiceWorkerContext, if any, associated with |request|.
  CONTENT_EXPORT static ServiceWorkerContext* GetServiceWorkerContext(
      net::URLRequest* request);

  // Equivalent to calling navigator.serviceWorker.register(script_url, {scope:
  // pattern}) from a renderer, except that |pattern| is an absolute URL instead
  // of relative to some current origin.  |callback| is passed true when the JS
  // promise is fulfilled or false when the JS promise is rejected.
  //
  // The registration can fail if:
  //  * |script_url| is on a different origin from |pattern|
  //  * Fetching |script_url| fails.
  //  * |script_url| fails to parse or its top-level execution fails.
  //    TODO: The error message for this needs to be available to developers.
  //  * Something unexpected goes wrong, like a renderer crash or a full disk.
  virtual void RegisterServiceWorker(const Scope& pattern,
                                     const GURL& script_url,
                                     const ResultCallback& callback) = 0;

  // Equivalent to calling navigator.serviceWorker.unregister(pattern) from a
  // renderer, except that |pattern| is an absolute URL instead of relative to
  // some current origin.  |callback| is passed true when the JS promise is
  // fulfilled or false when the JS promise is rejected.
  //
  // Unregistration can fail if:
  //  * No Service Worker was registered for |pattern|.
  //  * Something unexpected goes wrong, like a renderer crash.
  virtual void UnregisterServiceWorker(const Scope& pattern,
                                       const ResultCallback& callback) = 0;

  // TODO(jyasskin): Provide a way to SendMessage to a Scope.

  // Determines if a request for |url| can be satisfied while offline.
  // This method always completes asynchronously.
  virtual void CanHandleMainResourceOffline(const GURL& url,
                                            const GURL& first_party,
                                            const net::CompletionCallback&
                                            callback) = 0;

  // Methods used in response to browsing data and quota manager requests.
  virtual void GetAllOriginsInfo(const GetUsageInfoCallback& callback) = 0;
  virtual void DeleteForOrigin(const GURL& origin_url) = 0;

  // Returns true if an active Service Worker registration exists that matches
  // |url|, and if |other_url| falls inside the scope of the same registration.
  // Note this still returns true even if there is a Service Worker registration
  // which has a longer match for |other_url|.
  // This function can be called from any thread, but the callback will always
  // be called on the UI thread.
  virtual void CheckHasServiceWorker(
      const GURL& url,
      const GURL& other_url,
      const CheckHasServiceWorkerCallback& callback) = 0;

 protected:
  ServiceWorkerContext() {}
  virtual ~ServiceWorkerContext() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
