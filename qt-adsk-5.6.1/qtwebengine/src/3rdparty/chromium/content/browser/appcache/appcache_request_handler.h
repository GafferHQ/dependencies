// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_REQUEST_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/supports_user_data.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/common/content_export.h"
#include "content/public/common/resource_type.h"

namespace net {
class NetworkDelegate;
class URLRequest;
class URLRequestJob;
}  // namespace net

namespace content {
class AppCacheRequestHandlerTest;
class AppCacheURLRequestJob;

// An instance is created for each net::URLRequest. The instance survives all
// http transactions involved in the processing of its net::URLRequest, and is
// given the opportunity to hijack the request along the way. Callers
// should use AppCacheHost::CreateRequestHandler to manufacture instances
// that can retrieve resources for a particular host.
class CONTENT_EXPORT AppCacheRequestHandler
    : public base::SupportsUserData::Data,
      public AppCacheHost::Observer,
      public AppCacheStorage::Delegate  {
 public:
  ~AppCacheRequestHandler() override;

  // These are called on each request intercept opportunity.
  AppCacheURLRequestJob* MaybeLoadResource(
      net::URLRequest* request, net::NetworkDelegate* network_delegate);
  AppCacheURLRequestJob* MaybeLoadFallbackForRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location);
  AppCacheURLRequestJob* MaybeLoadFallbackForResponse(
      net::URLRequest* request, net::NetworkDelegate* network_delegate);

  void GetExtraResponseInfo(int64* cache_id, GURL* manifest_url);

  // Methods to support cross site navigations.
  void PrepareForCrossSiteTransfer(int old_process_id);
  void CompleteCrossSiteTransfer(int new_process_id, int new_host_id);
  void MaybeCompleteCrossSiteTransferInOldProcess(int old_process_id);

  static bool IsMainResourceType(ResourceType type) {
    return IsResourceTypeFrame(type) ||
           type == RESOURCE_TYPE_SHARED_WORKER;
  }

 private:
  friend class AppCacheHost;

  // Callers should use AppCacheHost::CreateRequestHandler.
  AppCacheRequestHandler(AppCacheHost* host, ResourceType resource_type,
                         bool should_reset_appcache);

  // AppCacheHost::Observer override
  void OnDestructionImminent(AppCacheHost* host) override;

  // Helpers to instruct a waiting job with what response to
  // deliver for the request we're handling.
  void DeliverAppCachedResponse(const AppCacheEntry& entry, int64 cache_id,
                                int64 group_id, const GURL& manifest_url,
                                bool is_fallback,
                                const GURL& namespace_entry_url);
  void DeliverNetworkResponse();
  void DeliverErrorResponse();

  // Helper to retrieve a pointer to the storage object.
  AppCacheStorage* storage() const;

  bool is_main_resource() const {
    return IsMainResourceType(resource_type_);
  }

  // Main-resource loading -------------------------------------
  // Frame and SharedWorker main resources are handled here.

  void MaybeLoadMainResource(net::URLRequest* request,
                             net::NetworkDelegate* network_delegate);

  // AppCacheStorage::Delegate methods
  void OnMainResponseFound(const GURL& url,
                           const AppCacheEntry& entry,
                           const GURL& fallback_url,
                           const AppCacheEntry& fallback_entry,
                           int64 cache_id,
                           int64 group_id,
                           const GURL& mainfest_url) override;

  // Sub-resource loading -------------------------------------
  // Dedicated worker and all manner of sub-resources are handled here.

  void MaybeLoadSubResource(net::URLRequest* request,
                            net::NetworkDelegate* network_delegate);
  void ContinueMaybeLoadSubResource();

  // AppCacheHost::Observer override
  void OnCacheSelectionComplete(AppCacheHost* host) override;

  // Data members -----------------------------------------------

  // What host we're servicing a request for.
  AppCacheHost* host_;

  // Frame vs subresource vs sharedworker loads are somewhat different.
  ResourceType resource_type_;

  // True if corresponding AppCache group should be resetted before load.
  bool should_reset_appcache_;

  // Subresource requests wait until after cache selection completes.
  bool is_waiting_for_cache_selection_;

  // Info about the type of response we found for delivery.
  // These are relevant for both main and subresource requests.
  int64 found_group_id_;
  int64 found_cache_id_;
  AppCacheEntry found_entry_;
  AppCacheEntry found_fallback_entry_;
  GURL found_namespace_entry_url_;
  GURL found_manifest_url_;
  bool found_network_namespace_;

  // True if a cache entry this handler attempted to return was
  // not found in the disk cache. Once set, the handler will take
  // no action on all subsequent intercept opportunities, so the
  // request and any redirects will be handled by the network library.
  bool cache_entry_not_found_;

  // True if this->MaybeLoadResource(...) has been called in the past.
  bool maybe_load_resource_executed_;

  // The job we use to deliver a response.
  scoped_refptr<AppCacheURLRequestJob> job_;

  // During a cross site navigation, we transfer ownership the AppcacheHost
  // from the old processes structures over to the new structures.
  scoped_ptr<AppCacheHost> host_for_cross_site_transfer_;
  int old_process_id_;
  int old_host_id_;

  friend class content::AppCacheRequestHandlerTest;
  DISALLOW_COPY_AND_ASSIGN(AppCacheRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_REQUEST_HANDLER_H_
