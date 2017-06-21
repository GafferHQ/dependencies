// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/cache_storage/cache_storage_types.h"
#include "content/common/service_worker/service_worker_types.h"
#include "net/disk_cache/disk_cache.h"

namespace net {
class URLRequestContextGetter;
class IOBufferWithSize;
}

namespace storage {
class BlobDataHandle;
class BlobStorageContext;
class QuotaManagerProxy;
}

namespace content {

class CacheStorageBlobToDiskCache;
class CacheMetadata;
class CacheStorageScheduler;
class TestCacheStorageCache;

// Represents a ServiceWorker Cache as seen in
// https://slightlyoff.github.io/ServiceWorker/spec/service_worker/
// The asynchronous methods are executed serially. Callbacks to the
// public functions will be called so long as the cache object lives.
class CONTENT_EXPORT CacheStorageCache
    : public base::RefCounted<CacheStorageCache> {
 public:
  using ErrorCallback = base::Callback<void(CacheStorageError)>;
  using ResponseCallback =
      base::Callback<void(CacheStorageError,
                          scoped_ptr<ServiceWorkerResponse>,
                          scoped_ptr<storage::BlobDataHandle>)>;
  using Requests = std::vector<ServiceWorkerFetchRequest>;
  using RequestsCallback =
      base::Callback<void(CacheStorageError, scoped_ptr<Requests>)>;

  static scoped_refptr<CacheStorageCache> CreateMemoryCache(
      const GURL& origin,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context);
  static scoped_refptr<CacheStorageCache> CreatePersistentCache(
      const GURL& origin,
      const base::FilePath& path,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context);

  // Returns ERROR_TYPE_NOT_FOUND if not found.
  void Match(scoped_ptr<ServiceWorkerFetchRequest> request,
             const ResponseCallback& callback);

  // Runs given batch operations. This corresponds to the Batch Cache Operations
  // algorithm in the spec.
  //
  // |operations| cannot mix PUT and DELETE operations and cannot contain
  // multiple DELETE operations.
  //
  // In the case of the PUT operation, puts request and response objects in the
  // cache and returns OK when all operations are successfully completed.
  // In the case of the DELETE operation, returns ERROR_NOT_FOUND if a specified
  // entry is not found. Otherwise deletes it and returns OK.
  //
  // TODO(nhiroki): This function should run all operations atomically.
  // http://crbug.com/486637
  void BatchOperation(const std::vector<CacheStorageBatchOperation>& operations,
                      const ErrorCallback& callback);
  void BatchDidOneOperation(const base::Closure& barrier_closure,
                            ErrorCallback* callback,
                            CacheStorageError error);
  void BatchDidAllOperations(scoped_ptr<ErrorCallback> callback);

  // TODO(jkarlin): Have keys take an optional ServiceWorkerFetchRequest.
  // Returns CACHE_STORAGE_OK and a vector of requests if there are no errors.
  void Keys(const RequestsCallback& callback);

  // Closes the backend. Future operations that require the backend
  // will exit early. Close should only be called once per CacheStorageCache.
  void Close(const base::Closure& callback);

  // The size of the cache contents in memory. Returns 0 if the cache backend is
  // not a memory cache backend.
  int64 MemoryBackedSize() const;

  base::WeakPtr<CacheStorageCache> AsWeakPtr();

 private:
  friend class base::RefCounted<CacheStorageCache>;
  friend class TestCacheStorageCache;

  struct KeysContext;
  struct PutContext;

  // The backend progresses from uninitialized, to open, to closed, and cannot
  // reverse direction.  The open step may be skipped.
  enum BackendState {
    BACKEND_UNINITIALIZED,  // No backend, create backend on first operation.
    BACKEND_OPEN,           // Backend can be used.
    BACKEND_CLOSED          // Backend cannot be used.  All ops should fail.
  };

  using Entries = std::vector<disk_cache::Entry*>;
  using ScopedBackendPtr = scoped_ptr<disk_cache::Backend>;
  using BlobToDiskCacheIDMap =
      IDMap<CacheStorageBlobToDiskCache, IDMapOwnPointer>;

  CacheStorageCache(
      const GURL& origin,
      const base::FilePath& path,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context);

  // Async operations in progress will cancel and not run their callbacks.
  virtual ~CacheStorageCache();

  // Match callbacks
  void MatchImpl(scoped_ptr<ServiceWorkerFetchRequest> request,
                 const ResponseCallback& callback);
  void MatchDidOpenEntry(scoped_ptr<ServiceWorkerFetchRequest> request,
                         const ResponseCallback& callback,
                         scoped_ptr<disk_cache::Entry*> entry_ptr,
                         int rv);
  void MatchDidReadMetadata(scoped_ptr<ServiceWorkerFetchRequest> request,
                            const ResponseCallback& callback,
                            disk_cache::ScopedEntryPtr entry,
                            scoped_ptr<CacheMetadata> headers);

  // Puts the request and response object in the cache. The response body (if
  // present) is stored in the cache, but not the request body. Returns OK on
  // success.
  void Put(const CacheStorageBatchOperation& operation,
           const ErrorCallback& callback);
  void PutImpl(scoped_ptr<PutContext> put_context);
  void PutDidDelete(scoped_ptr<PutContext> put_context,
                    CacheStorageError delete_error);
  void PutDidCreateEntry(scoped_ptr<disk_cache::Entry*> entry_ptr,
                         scoped_ptr<PutContext> put_context,
                         int rv);
  void PutDidWriteHeaders(scoped_ptr<PutContext> put_context,
                          int expected_bytes,
                          int rv);
  void PutDidWriteBlobToCache(scoped_ptr<PutContext> put_context,
                              BlobToDiskCacheIDMap::KeyType blob_to_cache_key,
                              disk_cache::ScopedEntryPtr entry,
                              bool success);

  // Returns ERROR_NOT_FOUND if not found. Otherwise deletes and returns OK.
  void Delete(const CacheStorageBatchOperation& operation,
              const ErrorCallback& callback);
  void DeleteImpl(scoped_ptr<ServiceWorkerFetchRequest> request,
                  const ErrorCallback& callback);
  void DeleteDidOpenEntry(
      const GURL& origin,
      scoped_ptr<ServiceWorkerFetchRequest> request,
      const CacheStorageCache::ErrorCallback& callback,
      scoped_ptr<disk_cache::Entry*> entryptr,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      int rv);

  // Keys callbacks.
  void KeysImpl(const RequestsCallback& callback);
  void KeysDidOpenNextEntry(scoped_ptr<KeysContext> keys_context, int rv);
  void KeysProcessNextEntry(scoped_ptr<KeysContext> keys_context,
                            const Entries::iterator& iter);
  void KeysDidReadMetadata(scoped_ptr<KeysContext> keys_context,
                           const Entries::iterator& iter,
                           scoped_ptr<CacheMetadata> metadata);

  void CloseImpl(const base::Closure& callback);

  // Loads the backend and calls the callback with the result (true for
  // success). The callback will always be called. Virtual for tests.
  virtual void CreateBackend(const ErrorCallback& callback);
  void CreateBackendDidCreate(const CacheStorageCache::ErrorCallback& callback,
                              scoped_ptr<ScopedBackendPtr> backend_ptr,
                              int rv);

  void InitBackend();
  void InitDone(CacheStorageError error);

  void PendingClosure(const base::Closure& callback);
  void PendingErrorCallback(const ErrorCallback& callback,
                            CacheStorageError error);
  void PendingResponseCallback(
      const ResponseCallback& callback,
      CacheStorageError error,
      scoped_ptr<ServiceWorkerResponse> response,
      scoped_ptr<storage::BlobDataHandle> blob_data_handle);
  void PendingRequestsCallback(const RequestsCallback& callback,
                               CacheStorageError error,
                               scoped_ptr<Requests> requests);

  // Be sure to check |backend_state_| before use.
  scoped_ptr<disk_cache::Backend> backend_;

  GURL origin_;
  base::FilePath path_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  BackendState backend_state_;
  scoped_ptr<CacheStorageScheduler> scheduler_;
  bool initializing_;

  // Owns the elements of the list
  BlobToDiskCacheIDMap active_blob_to_disk_cache_writers_;

  // Whether or not to store data in disk or memory.
  bool memory_only_;

  base::WeakPtrFactory<CacheStorageCache> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageCache);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_H_
