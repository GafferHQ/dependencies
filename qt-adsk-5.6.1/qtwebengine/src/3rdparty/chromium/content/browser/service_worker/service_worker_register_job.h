// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_register_job_base.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerJobCoordinator;
class ServiceWorkerStorage;

// Handles the initial registration of a Service Worker and the
// subsequent update of existing registrations.
//
// The control flow includes most or all of the following,
// depending on what is already registered:
//  - creating a ServiceWorkerRegistration instance if there isn't
//    already something registered
//  - creating a ServiceWorkerVersion for the new version.
//  - starting a worker for the ServiceWorkerVersion
//  - firing the 'install' event at the ServiceWorkerVersion
//  - firing the 'activate' event at the ServiceWorkerVersion
//  - waiting for older ServiceWorkerVersions to deactivate
//  - designating the new version to be the 'active' version
//  - updating storage
class ServiceWorkerRegisterJob : public ServiceWorkerRegisterJobBase {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode status,
                              const std::string& status_message,
                              ServiceWorkerRegistration* registration)>
      RegistrationCallback;

  // For registration jobs.
  CONTENT_EXPORT ServiceWorkerRegisterJob(
      base::WeakPtr<ServiceWorkerContextCore> context,
      const GURL& pattern,
      const GURL& script_url);

  // For update jobs.
  CONTENT_EXPORT ServiceWorkerRegisterJob(
      base::WeakPtr<ServiceWorkerContextCore> context,
      ServiceWorkerRegistration* registration,
      bool force_bypass_cache);
  ~ServiceWorkerRegisterJob() override;

  // Registers a callback to be called when the promise would resolve (whether
  // successfully or not). Multiple callbacks may be registered.
  // If |provider_host| is not NULL, its process will be regarded as a candidate
  // process to run the worker.
  void AddCallback(const RegistrationCallback& callback,
                   ServiceWorkerProviderHost* provider_host);

  // ServiceWorkerRegisterJobBase implementation:
  void Start() override;
  void Abort() override;
  bool Equals(ServiceWorkerRegisterJobBase* job) const override;
  RegistrationJobType GetType() const override;

  void DoomInstallingWorker();

 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerProviderHostWaitingVersionTest,
                           AssociateInstallingVersionToDocuments);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerProviderHostWaitingVersionTest,
                           DisassociateVersionFromDocuments);

  enum Phase {
    INITIAL,
    START,
    REGISTER,
    UPDATE,
    INSTALL,
    STORE,
    COMPLETE,
    ABORT,
  };

  // Holds internal state of ServiceWorkerRegistrationJob, to compel use of the
  // getter/setter functions.
  struct Internal {
    Internal();
    ~Internal();
    scoped_refptr<ServiceWorkerRegistration> registration;

    // Holds the version created by this job. It can be the 'installing',
    // 'waiting', or 'active' version depending on the phase.
    scoped_refptr<ServiceWorkerVersion> new_version;
  };

  void set_registration(
      const scoped_refptr<ServiceWorkerRegistration>& registration);
  ServiceWorkerRegistration* registration();
  void set_new_version(ServiceWorkerVersion* version);
  ServiceWorkerVersion* new_version();

  void SetPhase(Phase phase);

  void ContinueWithRegistration(
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);
  void ContinueWithUpdate(
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);
  void RegisterAndContinue();
  void ContinueWithUninstallingRegistration(
      const scoped_refptr<ServiceWorkerRegistration>& existing_registration,
      ServiceWorkerStatusCode status);
  void ContinueWithRegistrationForSameScriptUrl(
      const scoped_refptr<ServiceWorkerRegistration>& existing_registration,
      ServiceWorkerStatusCode status);
  void UpdateAndContinue();
  void OnStartWorkerFinished(ServiceWorkerStatusCode status);
  void OnStoreRegistrationComplete(ServiceWorkerStatusCode status);
  void InstallAndContinue();
  void OnInstallFinished(ServiceWorkerStatusCode status);
  void ActivateAndContinue();
  void OnActivateFinished(ServiceWorkerStatusCode status);
  void Complete(ServiceWorkerStatusCode status);
  void Complete(ServiceWorkerStatusCode status,
                const std::string& status_message);
  void CompleteInternal(ServiceWorkerStatusCode status,
                        const std::string& status_message);
  void ResolvePromise(ServiceWorkerStatusCode status,
                      const std::string& status_message,
                      ServiceWorkerRegistration* registration);

  void AddRegistrationToMatchingProviderHosts(
      ServiceWorkerRegistration* registration);

  // The ServiceWorkerContextCore object should always outlive this.
  base::WeakPtr<ServiceWorkerContextCore> context_;

  RegistrationJobType job_type_;
  const GURL pattern_;
  const GURL script_url_;
  std::vector<RegistrationCallback> callbacks_;
  Phase phase_;
  Internal internal_;
  bool doom_installing_worker_;
  bool is_promise_resolved_;
  bool should_uninstall_on_failure_;
  bool force_bypass_cache_;
  ServiceWorkerStatusCode promise_resolved_status_;
  std::string promise_resolved_status_message_;
  scoped_refptr<ServiceWorkerRegistration> promise_resolved_registration_;
  base::WeakPtrFactory<ServiceWorkerRegisterJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegisterJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_
