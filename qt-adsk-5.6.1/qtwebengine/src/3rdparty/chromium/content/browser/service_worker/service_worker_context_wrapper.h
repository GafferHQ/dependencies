// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/common/content_export.h"
#include "content/public/browser/service_worker_context.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace content {

class BrowserContext;
class ServiceWorkerContextCore;
class ServiceWorkerContextObserver;
class StoragePartitionImpl;

// A refcounted wrapper class for our core object. Higher level content lib
// classes keep references to this class on mutliple threads. The inner core
// instance is strictly single threaded and is not refcounted, the core object
// is what is used internally in the service worker lib.
class CONTENT_EXPORT ServiceWorkerContextWrapper
    : NON_EXPORTED_BASE(public ServiceWorkerContext),
      public base::RefCountedThreadSafe<ServiceWorkerContextWrapper> {
 public:
  using StatusCallback = base::Callback<void(ServiceWorkerStatusCode)>;
  using FindRegistrationCallback =
      ServiceWorkerStorage::FindRegistrationCallback;
  using GetRegistrationsInfosCallback =
      ServiceWorkerStorage::GetRegistrationsInfosCallback;
  using GetUserDataCallback = ServiceWorkerStorage::GetUserDataCallback;
  using GetUserDataForAllRegistrationsCallback =
      ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback;

  ServiceWorkerContextWrapper(BrowserContext* browser_context);

  // Init and Shutdown are for use on the UI thread when the profile,
  // storagepartition is being setup and torn down.
  void Init(const base::FilePath& user_data_directory,
            storage::QuotaManagerProxy* quota_manager_proxy,
            storage::SpecialStoragePolicy* special_storage_policy);
  void Shutdown();

  // Deletes all files on disk and restarts the system asynchronously. This
  // leaves the system in a disabled state until it's done. This should be
  // called on the IO thread.
  void DeleteAndStartOver();

  // The StoragePartition should only be used on the UI thread.
  // Can be null before/during init and during/after shutdown.
  StoragePartitionImpl* storage_partition() const;

  void set_storage_partition(StoragePartitionImpl* storage_partition);

  // The process manager can be used on either UI or IO.
  ServiceWorkerProcessManager* process_manager() {
    return process_manager_.get();
  }

  // ServiceWorkerContext implementation:
  void RegisterServiceWorker(const GURL& pattern,
                             const GURL& script_url,
                             const ResultCallback& continuation) override;
  void UnregisterServiceWorker(const GURL& pattern,
                               const ResultCallback& continuation) override;
  void CanHandleMainResourceOffline(
      const GURL& url,
      const GURL& first_party,
      const net::CompletionCallback& callback) override;
  void GetAllOriginsInfo(const GetUsageInfoCallback& callback) override;
  void DeleteForOrigin(const GURL& origin) override;
  void CheckHasServiceWorker(
      const GURL& url,
      const GURL& other_url,
      const CheckHasServiceWorkerCallback& callback) override;

  ServiceWorkerRegistration* GetLiveRegistration(int64_t registration_id);
  ServiceWorkerVersion* GetLiveVersion(int64_t version_id);
  std::vector<ServiceWorkerRegistrationInfo> GetAllLiveRegistrationInfo();
  std::vector<ServiceWorkerVersionInfo> GetAllLiveVersionInfo();

  void FindRegistrationForDocument(const GURL& document_url,
                                   const FindRegistrationCallback& callback);
  void FindRegistrationForId(int64_t registration_id,
                             const GURL& origin,
                             const FindRegistrationCallback& callback);
  void GetAllRegistrations(const GetRegistrationsInfosCallback& callback);
  void GetRegistrationUserData(int64_t registration_id,
                               const std::string& key,
                               const GetUserDataCallback& callback);
  void StoreRegistrationUserData(int64_t registration_id,
                                 const GURL& origin,
                                 const std::string& key,
                                 const std::string& data,
                                 const StatusCallback& callback);
  void ClearRegistrationUserData(int64_t registration_id,
                                 const std::string& key,
                                 const StatusCallback& callback);
  void GetUserDataForAllRegistrations(
      const std::string& key,
      const GetUserDataForAllRegistrationsCallback& callback);

  // DeleteForOrigin with completion callback.  Does not exit early, and returns
  // false if one or more of the deletions fail.
  virtual void DeleteForOrigin(const GURL& origin, const ResultCallback& done);

  void StartServiceWorker(const GURL& pattern, const StatusCallback& callback);
  void UpdateRegistration(const GURL& pattern);
  void SimulateSkipWaiting(int64_t version_id);
  void AddObserver(ServiceWorkerContextObserver* observer);
  void RemoveObserver(ServiceWorkerContextObserver* observer);

  bool is_incognito() const { return is_incognito_; }

 private:
  friend class BackgroundSyncManagerTest;
  friend class base::RefCountedThreadSafe<ServiceWorkerContextWrapper>;
  friend class EmbeddedWorkerTestHelper;
  friend class EmbeddedWorkerBrowserTest;
  friend class ServiceWorkerDispatcherHost;
  friend class ServiceWorkerInternalsUI;
  friend class ServiceWorkerProcessManager;
  friend class ServiceWorkerRequestHandler;
  friend class ServiceWorkerVersionBrowserTest;
  friend class MockServiceWorkerContextWrapper;

  ~ServiceWorkerContextWrapper() override;

  void InitInternal(
      const base::FilePath& user_data_directory,
      scoped_ptr<ServiceWorkerDatabaseTaskManager> database_task_manager,
      const scoped_refptr<base::SingleThreadTaskRunner>& disk_cache_thread,
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy);
  void ShutdownOnIO();

  void DidDeleteAndStartOver(ServiceWorkerStatusCode status);

  void DidGetAllRegistrationsForGetAllOrigins(
      const GetUsageInfoCallback& callback,
      const std::vector<ServiceWorkerRegistrationInfo>& registrations);

  void DidFindRegistrationForCheckHasServiceWorker(
      const GURL& other_url,
      const CheckHasServiceWorkerCallback& callback,
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);

  void DidFindRegistrationForUpdate(
      ServiceWorkerStatusCode status,
      const scoped_refptr<content::ServiceWorkerRegistration>& registration);

  // The core context is only for use on the IO thread.
  // Can be null before/during init, during/after shutdown, and after
  // DeleteAndStartOver fails.
  ServiceWorkerContextCore* context();

  const scoped_refptr<base::ObserverListThreadSafe<
      ServiceWorkerContextObserver>> observer_list_;
  const scoped_ptr<ServiceWorkerProcessManager> process_manager_;
  // Cleared in Shutdown():
  scoped_ptr<ServiceWorkerContextCore> context_core_;

  // Initialized in Init(); true if the user data directory is empty.
  bool is_incognito_;

  // Raw pointer to the StoragePartitionImpl owning |this|.
  StoragePartitionImpl* storage_partition_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_
