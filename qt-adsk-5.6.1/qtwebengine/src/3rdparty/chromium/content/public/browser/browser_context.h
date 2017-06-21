// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_

#include "base/callback_forward.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/public/browser/zoom_level_delegate.h"
#include "content/public/common/push_messaging_status.h"

class GURL;

namespace base {
class FilePath;
class Time;
}

namespace storage {
class ExternalMountPoints;
}

namespace net {
class URLRequestContextGetter;
}

namespace storage {
class SpecialStoragePolicy;
}

namespace content {

class BlobHandle;
class BrowserPluginGuestManager;
class DownloadManager;
class DownloadManagerDelegate;
class IndexedDBContext;
class PermissionManager;
class PushMessagingService;
class ResourceContext;
class SiteInstance;
class StoragePartition;
class SSLHostStateDelegate;

// This class holds the context needed for a browsing session.
// It lives on the UI thread. All these methods must only be called on the UI
// thread.
class CONTENT_EXPORT BrowserContext : public base::SupportsUserData {
 public:
  static DownloadManager* GetDownloadManager(BrowserContext* browser_context);

  // Returns BrowserContext specific external mount points. It may return
  // nullptr if the context doesn't have any BrowserContext specific external
  // mount points. Currenty, non-nullptr value is returned only on ChromeOS.
  static storage::ExternalMountPoints* GetMountPoints(BrowserContext* context);

  static content::StoragePartition* GetStoragePartition(
      BrowserContext* browser_context, SiteInstance* site_instance);
  static content::StoragePartition* GetStoragePartitionForSite(
      BrowserContext* browser_context, const GURL& site);
  typedef base::Callback<void(StoragePartition*)> StoragePartitionCallback;
  static void ForEachStoragePartition(
      BrowserContext* browser_context,
      const StoragePartitionCallback& callback);
  static void AsyncObliterateStoragePartition(
      BrowserContext* browser_context,
      const GURL& site,
      const base::Closure& on_gc_required);

  // This function clears the contents of |active_paths| but does not take
  // ownership of the pointer.
  static void GarbageCollectStoragePartitions(
      BrowserContext* browser_context,
      scoped_ptr<base::hash_set<base::FilePath> > active_paths,
      const base::Closure& done);

  // DON'T USE THIS. GetDefaultStoragePartition() is going away.
  // Use GetStoragePartition() instead. Ask ajwong@ if you have problems.
  static content::StoragePartition* GetDefaultStoragePartition(
      BrowserContext* browser_context);

  typedef base::Callback<void(scoped_ptr<BlobHandle>)> BlobCallback;

  // |callback| returns a nullptr scoped_ptr on failure.
  static void CreateMemoryBackedBlob(BrowserContext* browser_context,
                                     const char* data, size_t length,
                                     const BlobCallback& callback);

  // |callback| returns a nullptr scoped_ptr on failure.
  static void CreateFileBackedBlob(BrowserContext* browser_context,
                                   const base::FilePath& path,
                                   int64_t offset,
                                   int64_t size,
                                   const base::Time& expected_modification_time,
                                   const BlobCallback& callback);

  // Delivers a push message with |data| to the Service Worker identified by
  // |origin| and |service_worker_registration_id|.
  static void DeliverPushMessage(
      BrowserContext* browser_context,
      const GURL& origin,
      int64 service_worker_registration_id,
      const std::string& data,
      const base::Callback<void(PushDeliveryStatus)>& callback);

  static void NotifyWillBeDestroyed(BrowserContext* browser_context);

  // Ensures that the corresponding ResourceContext is initialized. Normally the
  // BrowserContext initializs the corresponding getters when its objects are
  // created, but if the embedder wants to pass the ResourceContext to another
  // thread before they use BrowserContext, they should call this to make sure
  // that the ResourceContext is ready.
  static void EnsureResourceContextInitialized(BrowserContext* browser_context);

  // Tells the HTML5 objects on this context to persist their session state
  // across the next restart.
  static void SaveSessionState(BrowserContext* browser_context);

  ~BrowserContext() override;

  // Creates a delegate to initialize a HostZoomMap and persist its information.
  // This is called during creation of each StoragePartition.
  virtual scoped_ptr<ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) = 0;

  // Returns the path of the directory where this context's data is stored.
  virtual base::FilePath GetPath() const = 0;

  // Return whether this context is incognito. Default is false.
  virtual bool IsOffTheRecord() const = 0;

  // Returns the request context information associated with this context.  Call
  // this only on the UI thread, since it can send notifications that should
  // happen on the UI thread.
  // TODO(creis): Remove this version in favor of the one below.
  virtual net::URLRequestContextGetter* GetRequestContext() = 0;

  // Returns the request context appropriate for the given renderer. If the
  // renderer process doesn't have an associated installed app, or if the
  // installed app doesn't have isolated storage, this is equivalent to calling
  // GetRequestContext().
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) = 0;

  // Returns the default request context for media resources associated with
  // this context.
  // TODO(creis): Remove this version in favor of the one below.
  virtual net::URLRequestContextGetter* GetMediaRequestContext() = 0;

  // Returns the request context for media resources associated with this
  // context and renderer process.
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) = 0;
  virtual net::URLRequestContextGetter*
      GetMediaRequestContextForStoragePartition(
          const base::FilePath& partition_path,
          bool in_memory) = 0;

  // Returns the resource context.
  virtual ResourceContext* GetResourceContext() = 0;

  // Returns the DownloadManagerDelegate for this context. This will be called
  // once per context. The embedder owns the delegate and is responsible for
  // ensuring that it outlives DownloadManager. It's valid to return nullptr.
  virtual DownloadManagerDelegate* GetDownloadManagerDelegate() = 0;

  // Returns the guest manager for this context.
  virtual BrowserPluginGuestManager* GetGuestManager() = 0;

  // Returns a special storage policy implementation, or nullptr.
  virtual storage::SpecialStoragePolicy* GetSpecialStoragePolicy() = 0;

  // Returns a push messaging service. The embedder owns the service, and is
  // responsible for ensuring that it outlives RenderProcessHost. It's valid to
  // return nullptr.
  virtual PushMessagingService* GetPushMessagingService() = 0;

  // Returns the SSL host state decisions for this context. The context may
  // return nullptr, implementing the default exception storage strategy.
  virtual SSLHostStateDelegate* GetSSLHostStateDelegate() = 0;

  // Returns the PermissionManager associated with that context if any, nullptr
  // otherwise.
  virtual PermissionManager* GetPermissionManager() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_
