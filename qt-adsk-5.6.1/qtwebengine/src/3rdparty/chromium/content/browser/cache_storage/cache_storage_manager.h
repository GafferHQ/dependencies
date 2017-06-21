// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/common/content_export.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/quota/quota_client.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {
class BlobStorageContext;
class QuotaManagerProxy;
}

namespace content {

class CacheStorageQuotaClient;

// Keeps track of a CacheStorage per origin. There is one
// CacheStorageManager per ServiceWorkerContextCore.
// TODO(jkarlin): Remove CacheStorage from memory once they're no
// longer in active use.
class CONTENT_EXPORT CacheStorageManager {
 public:
  static scoped_ptr<CacheStorageManager> Create(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy);

  static scoped_ptr<CacheStorageManager> Create(
      CacheStorageManager* old_manager);

  virtual ~CacheStorageManager();

  // Methods to support the CacheStorage spec. These methods call the
  // corresponding CacheStorage method on the appropriate thread.
  void OpenCache(const GURL& origin,
                 const std::string& cache_name,
                 const CacheStorage::CacheAndErrorCallback& callback);
  void HasCache(const GURL& origin,
                const std::string& cache_name,
                const CacheStorage::BoolAndErrorCallback& callback);
  void DeleteCache(const GURL& origin,
                   const std::string& cache_name,
                   const CacheStorage::BoolAndErrorCallback& callback);
  void EnumerateCaches(const GURL& origin,
                       const CacheStorage::StringsAndErrorCallback& callback);
  void MatchCache(const GURL& origin,
                  const std::string& cache_name,
                  scoped_ptr<ServiceWorkerFetchRequest> request,
                  const CacheStorageCache::ResponseCallback& callback);
  void MatchAllCaches(const GURL& origin,
                      scoped_ptr<ServiceWorkerFetchRequest> request,
                      const CacheStorageCache::ResponseCallback& callback);

  // This must be called before creating any of the public *Cache functions
  // above.
  void SetBlobParametersForCache(
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context);

  base::WeakPtr<CacheStorageManager> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class CacheStorageQuotaClient;
  friend class CacheStorageManagerTest;
  friend class CacheStorageMigrationTest;

  typedef std::map<GURL, CacheStorage*> CacheStorageMap;

  CacheStorageManager(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy);

  // The returned CacheStorage* is owned by this manager.
  CacheStorage* FindOrCreateCacheStorage(const GURL& origin);

  // QuotaClient support
  void GetOriginUsage(const GURL& origin_url,
                      const storage::QuotaClient::GetUsageCallback& callback);
  void GetOrigins(const storage::QuotaClient::GetOriginsCallback& callback);
  void GetOriginsForHost(
      const std::string& host,
      const storage::QuotaClient::GetOriginsCallback& callback);
  void DeleteOriginData(const GURL& origin,
                        const storage::QuotaClient::DeletionCallback& callback);
  static void DeleteOriginDidClose(
      const GURL& origin,
      const storage::QuotaClient::DeletionCallback& callback,
      scoped_ptr<CacheStorage> cache_storage,
      base::WeakPtr<CacheStorageManager> cache_manager);

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter()
      const {
    return request_context_getter_;
  }
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context() const {
    return blob_context_;
  }
  base::FilePath root_path() const { return root_path_; }
  const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner() const {
    return cache_task_runner_;
  }

  bool IsMemoryBacked() const { return root_path_.empty(); }

  // Map a origin to the path. Exposed for testing.
  static base::FilePath ConstructLegacyOriginPath(
      const base::FilePath& root_path,
      const GURL& origin);
  // Map a database identifier (computed from an origin) to the path. Exposed
  // for testing.
  static base::FilePath ConstructOriginPath(const base::FilePath& root_path,
                                            const GURL& origin);

  // Migrate from old origin-based path to storage identifier-based path.
  // TODO(jsbell); Remove method and all calls after a few releases.
  void MigrateOrigin(const GURL& origin);
  static void MigrateOriginOnTaskRunner(const base::FilePath& old_path,
                                        const base::FilePath& new_path);

  base::FilePath root_path_;
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;

  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  // The map owns the CacheStorages and the CacheStorages are only accessed on
  // |cache_task_runner_|.
  CacheStorageMap cache_storage_map_;

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  base::WeakPtr<storage::BlobStorageContext> blob_context_;

  base::WeakPtrFactory<CacheStorageManager> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(CacheStorageManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_
