// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_CACHE_STORAGE_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_CACHE_STORAGE_IMPL_H_

#include "content/child/thread_safe_sender.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerCache.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerCacheError.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerCacheStorage.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

class CacheStorageDispatcher;
class ThreadSafeSender;

// This corresponds to an instance of the script-facing CacheStorage object.
// One exists per script execution context (window, worker) and has an origin
// which provides a namespace for the caches. Script calls to the CacheStorage
// object are routed through here to the (per-thread) dispatcher then on to
// the browser, with the origin added to the call parameters. Cache instances
// returned by open() calls are minted by and implemented within the code of
// the CacheStorageDispatcher, and include routing information.
class WebServiceWorkerCacheStorageImpl
    : public blink::WebServiceWorkerCacheStorage {
 public:
  WebServiceWorkerCacheStorageImpl(ThreadSafeSender* thread_safe_sender,
                                   const GURL& origin);
  ~WebServiceWorkerCacheStorageImpl();

  // From WebServiceWorkerCacheStorage:
  virtual void dispatchHas(CacheStorageCallbacks* callbacks,
                           const blink::WebString& cacheName);
  virtual void dispatchOpen(CacheStorageWithCacheCallbacks* callbacks,
                            const blink::WebString& cacheName);
  virtual void dispatchDelete(CacheStorageCallbacks* callbacks,
                              const blink::WebString& cacheName);
  virtual void dispatchKeys(CacheStorageKeysCallbacks* callbacks);
  virtual void dispatchMatch(
      CacheStorageMatchCallbacks* callbacks,
      const blink::WebServiceWorkerRequest& request,
      const blink::WebServiceWorkerCache::QueryParams& query_params);

 private:
  // Helper to return the thread-specific dispatcher.
  CacheStorageDispatcher* GetDispatcher() const;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  const GURL origin_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerCacheStorageImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_CACHE_STORAGE_IMPL_H_
