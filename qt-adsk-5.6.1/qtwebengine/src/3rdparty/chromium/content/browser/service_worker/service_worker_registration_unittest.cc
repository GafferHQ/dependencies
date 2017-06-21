// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration.h"

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerRegistrationTest : public testing::Test {
 public:
  ServiceWorkerRegistrationTest()
      : io_thread_(BrowserThread::IO, &message_loop_) {}

  void SetUp() override {
    scoped_ptr<ServiceWorkerDatabaseTaskManager> database_task_manager(
        new MockServiceWorkerDatabaseTaskManager(
            base::ThreadTaskRunnerHandle::Get()));
    context_.reset(
        new ServiceWorkerContextCore(base::FilePath(),
                                     database_task_manager.Pass(),
                                     base::ThreadTaskRunnerHandle::Get(),
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL));
    context_ptr_ = context_->AsWeakPtr();
  }

  void TearDown() override {
    context_.reset();
    base::RunLoop().RunUntilIdle();
  }

  class RegistrationListener : public ServiceWorkerRegistration::Listener {
   public:
    RegistrationListener() {}
    ~RegistrationListener() {
      if (observed_registration_.get())
        observed_registration_->RemoveListener(this);
    }

    void OnVersionAttributesChanged(
        ServiceWorkerRegistration* registration,
        ChangedVersionAttributesMask changed_mask,
        const ServiceWorkerRegistrationInfo& info) override {
      observed_registration_ = registration;
      observed_changed_mask_ = changed_mask;
      observed_info_ = info;
    }

    void OnRegistrationFailed(
        ServiceWorkerRegistration* registration) override {
      NOTREACHED();
    }

    void OnUpdateFound(ServiceWorkerRegistration* registration) override {
      NOTREACHED();
    }

    void Reset() {
      observed_registration_ = NULL;
      observed_changed_mask_ = ChangedVersionAttributesMask();
      observed_info_ = ServiceWorkerRegistrationInfo();
    }

    scoped_refptr<ServiceWorkerRegistration> observed_registration_;
    ChangedVersionAttributesMask observed_changed_mask_;
    ServiceWorkerRegistrationInfo observed_info_;
  };

 protected:
  scoped_ptr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerContextCore> context_ptr_;
  base::MessageLoopForIO message_loop_;
  BrowserThreadImpl io_thread_;
};

TEST_F(ServiceWorkerRegistrationTest, SetAndUnsetVersions) {
  const GURL kScope("http://www.example.not/");
  const GURL kScript("http://www.example.not/service_worker.js");
  int64 kRegistrationId = 1L;
  scoped_refptr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(
          kScope,
          kRegistrationId,
          context_ptr_);

  const int64 version_1_id = 1L;
  const int64 version_2_id = 2L;
  scoped_refptr<ServiceWorkerVersion> version_1 = new ServiceWorkerVersion(
      registration.get(), kScript, version_1_id, context_ptr_);
  scoped_refptr<ServiceWorkerVersion> version_2 = new ServiceWorkerVersion(
      registration.get(), kScript, version_2_id, context_ptr_);

  RegistrationListener listener;
  registration->AddListener(&listener);
  registration->SetActiveVersion(version_1);

  EXPECT_EQ(version_1.get(), registration->active_version());
  EXPECT_EQ(registration, listener.observed_registration_);
  EXPECT_EQ(ChangedVersionAttributesMask::ACTIVE_VERSION,
            listener.observed_changed_mask_.changed());
  EXPECT_EQ(kScope, listener.observed_info_.pattern);
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(kScript, listener.observed_info_.active_version.script_url);
  EXPECT_EQ(listener.observed_info_.installing_version.version_id,
            kInvalidServiceWorkerVersionId);
  EXPECT_EQ(listener.observed_info_.waiting_version.version_id,
            kInvalidServiceWorkerVersionId);
  listener.Reset();

  registration->SetInstallingVersion(version_2);

  EXPECT_EQ(version_2.get(), registration->installing_version());
  EXPECT_EQ(ChangedVersionAttributesMask::INSTALLING_VERSION,
            listener.observed_changed_mask_.changed());
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(version_2_id,
            listener.observed_info_.installing_version.version_id);
  EXPECT_EQ(listener.observed_info_.waiting_version.version_id,
            kInvalidServiceWorkerVersionId);
  listener.Reset();

  registration->SetWaitingVersion(version_2);

  EXPECT_EQ(version_2.get(), registration->waiting_version());
  EXPECT_FALSE(registration->installing_version());
  EXPECT_TRUE(listener.observed_changed_mask_.waiting_changed());
  EXPECT_TRUE(listener.observed_changed_mask_.installing_changed());
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(version_2_id, listener.observed_info_.waiting_version.version_id);
  EXPECT_EQ(listener.observed_info_.installing_version.version_id,
            kInvalidServiceWorkerVersionId);
  listener.Reset();

  registration->UnsetVersion(version_2.get());

  EXPECT_FALSE(registration->waiting_version());
  EXPECT_EQ(ChangedVersionAttributesMask::WAITING_VERSION,
            listener.observed_changed_mask_.changed());
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(listener.observed_info_.waiting_version.version_id,
            kInvalidServiceWorkerVersionId);
  EXPECT_EQ(listener.observed_info_.installing_version.version_id,
            kInvalidServiceWorkerVersionId);
}

TEST_F(ServiceWorkerRegistrationTest, FailedRegistrationNoCrash) {
  const GURL kScope("http://www.example.not/");
  int64 kRegistrationId = 1L;
  scoped_refptr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(
          kScope,
          kRegistrationId,
          context_ptr_);
  scoped_ptr<ServiceWorkerRegistrationHandle> handle(
      new ServiceWorkerRegistrationHandle(
          context_ptr_,
          base::WeakPtr<ServiceWorkerProviderHost>(),
          registration.get()));
  registration->NotifyRegistrationFailed();
  // Don't crash when handle gets destructed.
}

}  // namespace content
