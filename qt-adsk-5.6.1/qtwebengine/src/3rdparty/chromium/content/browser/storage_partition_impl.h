// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
#define CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/host_zoom_level_context.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/media/webrtc_identity_store.h"
#include "content/browser/navigator_connect/navigator_connect_context_impl.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/stashed_port_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace content {

class StoragePartitionImpl : public StoragePartition {
 public:
  CONTENT_EXPORT ~StoragePartitionImpl() override;

  // Quota managed data uses a different bitmask for types than
  // StoragePartition uses. This method generates that mask.
  CONTENT_EXPORT static int GenerateQuotaClientMask(uint32 remove_mask);

  CONTENT_EXPORT void OverrideQuotaManagerForTesting(
      storage::QuotaManager* quota_manager);
  CONTENT_EXPORT void OverrideSpecialStoragePolicyForTesting(
      storage::SpecialStoragePolicy* special_storage_policy);

  // StoragePartition interface.
  base::FilePath GetPath() override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  net::URLRequestContextGetter* GetMediaURLRequestContext() override;
  storage::QuotaManager* GetQuotaManager() override;
  ChromeAppCacheService* GetAppCacheService() override;
  storage::FileSystemContext* GetFileSystemContext() override;
  storage::DatabaseTracker* GetDatabaseTracker() override;
  DOMStorageContextWrapper* GetDOMStorageContext() override;
  IndexedDBContextImpl* GetIndexedDBContext() override;
  // TODO(jsbell): Expose this on the public API as well. crbug.com/466371
  CacheStorageContextImpl* GetCacheStorageContext();
  ServiceWorkerContextWrapper* GetServiceWorkerContext() override;
  GeofencingManager* GetGeofencingManager() override;
  HostZoomMap* GetHostZoomMap() override;
  HostZoomLevelContext* GetHostZoomLevelContext() override;
  ZoomLevelDelegate* GetZoomLevelDelegate() override;
  NavigatorConnectContextImpl* GetNavigatorConnectContext() override;
  PlatformNotificationContextImpl* GetPlatformNotificationContext() override;
  BackgroundSyncContextImpl* GetBackgroundSyncContext();
  StashedPortManager* GetStashedPortManager();

  void ClearDataForOrigin(uint32 remove_mask,
                          uint32 quota_storage_remove_mask,
                          const GURL& storage_origin,
                          net::URLRequestContextGetter* request_context_getter,
                          const base::Closure& callback) override;
  void ClearData(uint32 remove_mask,
                 uint32 quota_storage_remove_mask,
                 const GURL& storage_origin,
                 const OriginMatcherFunction& origin_matcher,
                 const base::Time begin,
                 const base::Time end,
                 const base::Closure& callback) override;

  void Flush() override;

  WebRTCIdentityStore* GetWebRTCIdentityStore();

  // Can return nullptr while |this| is being destroyed.
  BrowserContext* browser_context() const;

  struct DataDeletionHelper;
  struct QuotaManagedDataDeletionHelper;

 private:
  friend class StoragePartitionImplMap;
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionShaderClearTest, ClearShaderCache);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverBoth);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverOnlyTemporary);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverOnlyPersistent);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverNeither);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverSpecificOrigin);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForLastHour);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForLastWeek);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedUnprotectedOrigins);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedProtectedSpecificOrigin);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedProtectedOrigins);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedIgnoreDevTools);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest, RemoveCookieForever);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest, RemoveCookieLastHour);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveUnprotectedLocalStorageForever);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveProtectedLocalStorageForever);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveLocalStorageForLastWeek);

  // The |partition_path| is the absolute path to the root of this
  // StoragePartition's on-disk storage.
  //
  // If |in_memory| is true, the |partition_path| is (ab)used as a way of
  // distinguishing different in-memory partitions, but nothing is persisted
  // on to disk.
  static StoragePartitionImpl* Create(BrowserContext* context,
                                      bool in_memory,
                                      const base::FilePath& profile_path);

  CONTENT_EXPORT StoragePartitionImpl(
      BrowserContext* browser_context,
      const base::FilePath& partition_path,
      storage::QuotaManager* quota_manager,
      ChromeAppCacheService* appcache_service,
      storage::FileSystemContext* filesystem_context,
      storage::DatabaseTracker* database_tracker,
      DOMStorageContextWrapper* dom_storage_context,
      IndexedDBContextImpl* indexed_db_context,
      CacheStorageContextImpl* cache_storage_context,
      ServiceWorkerContextWrapper* service_worker_context,
      WebRTCIdentityStore* webrtc_identity_store,
      storage::SpecialStoragePolicy* special_storage_policy,
      GeofencingManager* geofencing_manager,
      HostZoomLevelContext* host_zoom_level_context,
      NavigatorConnectContextImpl* navigator_connect_context,
      PlatformNotificationContextImpl* platform_notification_context,
      BackgroundSyncContextImpl* background_sync_context,
      StashedPortManager* stashed_port_manager);

  void ClearDataImpl(uint32 remove_mask,
                     uint32 quota_storage_remove_mask,
                     const GURL& remove_origin,
                     const OriginMatcherFunction& origin_matcher,
                     net::URLRequestContextGetter* rq_context,
                     const base::Time begin,
                     const base::Time end,
                     const base::Closure& callback);

  // Used by StoragePartitionImplMap.
  //
  // TODO(ajwong): These should be taken in the constructor and in Create() but
  // because the URLRequestContextGetter still lives in Profile with a tangled
  // initialization, if we try to retrieve the URLRequestContextGetter()
  // before the default StoragePartition is created, we end up reentering the
  // construction and double-initializing.  For now, we retain the legacy
  // behavior while allowing StoragePartitionImpl to expose these accessors by
  // letting StoragePartitionImplMap call these two private settings at the
  // appropriate time.  These should move back into the constructor once
  // URLRequestContextGetter's lifetime is sorted out. We should also move the
  // PostCreateInitialization() out of StoragePartitionImplMap.
  CONTENT_EXPORT void SetURLRequestContext(
      net::URLRequestContextGetter* url_request_context);
  void SetMediaURLRequestContext(
      net::URLRequestContextGetter* media_url_request_context);

  base::FilePath partition_path_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  scoped_refptr<net::URLRequestContextGetter> media_url_request_context_;
  scoped_refptr<storage::QuotaManager> quota_manager_;
  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<storage::FileSystemContext> filesystem_context_;
  scoped_refptr<storage::DatabaseTracker> database_tracker_;
  scoped_refptr<DOMStorageContextWrapper> dom_storage_context_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  scoped_refptr<CacheStorageContextImpl> cache_storage_context_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  scoped_refptr<WebRTCIdentityStore> webrtc_identity_store_;
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<GeofencingManager> geofencing_manager_;
  scoped_refptr<HostZoomLevelContext> host_zoom_level_context_;
  scoped_refptr<NavigatorConnectContextImpl> navigator_connect_context_;
  scoped_refptr<PlatformNotificationContextImpl> platform_notification_context_;
  scoped_refptr<BackgroundSyncContextImpl> background_sync_context_;
  scoped_refptr<StashedPortManager> stashed_port_manager_;

  // Raw pointer that should always be valid. The BrowserContext owns the
  // StoragePartitionImplMap which then owns StoragePartitionImpl. When the
  // BrowserContext is destroyed, |this| will be destroyed too.
  BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
