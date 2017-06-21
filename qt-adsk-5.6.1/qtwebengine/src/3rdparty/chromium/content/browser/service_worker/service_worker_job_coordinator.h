// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_

#include <deque>
#include <map>

#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_unregister_job.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class EmbeddedWorkerRegistry;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerStorage;

// This class manages all in-flight registration or unregistration jobs.
class CONTENT_EXPORT ServiceWorkerJobCoordinator {
 public:
  explicit ServiceWorkerJobCoordinator(
      base::WeakPtr<ServiceWorkerContextCore> context);
  ~ServiceWorkerJobCoordinator();

  void Register(const GURL& pattern,
                const GURL& script_url,
                ServiceWorkerProviderHost* provider_host,
                const ServiceWorkerRegisterJob::RegistrationCallback& callback);

  void Unregister(
      const GURL& pattern,
      const ServiceWorkerUnregisterJob::UnregistrationCallback& callback);

  void Update(ServiceWorkerRegistration* registration, bool force_bypass_cache);

  // Calls ServiceWorkerRegisterJobBase::Abort() on all jobs and removes them.
  void AbortAll();

  // Removes the job. A job that was not aborted must call FinishJob when it is
  // done.
  void FinishJob(const GURL& pattern, ServiceWorkerRegisterJobBase* job);

 private:
  class JobQueue {
   public:
    JobQueue();
    ~JobQueue();

    // Adds a job to the queue. If an identical job is already at the end of the
    // queue, no new job is added. Returns the job in the queue, regardless of
    // whether it was newly added.
    ServiceWorkerRegisterJobBase* Push(
        scoped_ptr<ServiceWorkerRegisterJobBase> job);

    // Dooms the installing worker of the running register/update job if a
    // register/update job is scheduled to run after it. This corresponds to
    // the "Terminate installing worker" steps at the beginning of the spec's
    // [[Update]] and [[Install]] algorithms.
    void DoomInstallingWorkerIfNeeded();

    // Starts the first job in the queue.
    void StartOneJob();

    // Removes a job from the queue.
    void Pop(ServiceWorkerRegisterJobBase* job);

    bool empty() { return jobs_.empty(); }

    // Aborts all jobs in the queue and removes them.
    void AbortAll();

    // Marks that the browser is shutting down, so jobs may be destroyed before
    // finishing.
    void ClearForShutdown();

   private:
    std::deque<ServiceWorkerRegisterJobBase*> jobs_;
  };

  typedef std::map<GURL, JobQueue> RegistrationJobMap;

  // The ServiceWorkerContextCore object should always outlive the
  // job coordinator, the core owns the coordinator.
  base::WeakPtr<ServiceWorkerContextCore> context_;
  RegistrationJobMap job_queues_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerJobCoordinator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_
