// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerState.h"

namespace content {

namespace {

const int kRenderProcessId = 88;  // A dummy ID for testing.
const int kRenderFrameId = 44;  // A dummy ID for testing.

void VerifyStateChangedMessage(int expected_handle_id,
                              blink::WebServiceWorkerState expected_state,
                              const IPC::Message* message) {
  ASSERT_TRUE(message != NULL);
  ServiceWorkerMsg_ServiceWorkerStateChanged::Param param;
  ASSERT_TRUE(ServiceWorkerMsg_ServiceWorkerStateChanged::Read(
      message, &param));
  EXPECT_EQ(expected_handle_id, base::get<1>(param));
  EXPECT_EQ(expected_state, base::get<2>(param));
}

}  // namespace

class TestingServiceWorkerDispatcherHost : public ServiceWorkerDispatcherHost {
 public:
  TestingServiceWorkerDispatcherHost(
      int process_id,
      ServiceWorkerContextWrapper* context_wrapper,
      ResourceContext* resource_context,
      EmbeddedWorkerTestHelper* helper)
      : ServiceWorkerDispatcherHost(process_id, nullptr, resource_context),
        bad_message_received_count_(0),
        helper_(helper) {
    Init(context_wrapper);
  }

  bool Send(IPC::Message* message) override { return helper_->Send(message); }

  void ShutdownForBadMessage() override { ++bad_message_received_count_; }

  int bad_message_received_count_;

 protected:
  EmbeddedWorkerTestHelper* helper_;
  ~TestingServiceWorkerDispatcherHost() override {}
};

class ServiceWorkerHandleTest : public testing::Test {
 public:
  ServiceWorkerHandleTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_.reset(
        new EmbeddedWorkerTestHelper(base::FilePath(), kRenderProcessId));

    dispatcher_host_ = new TestingServiceWorkerDispatcherHost(
        kRenderProcessId, helper_->context_wrapper(),
        &resource_context_, helper_.get());

    const GURL pattern("http://www.example.com/");
    registration_ = new ServiceWorkerRegistration(
        pattern,
        1L,
        helper_->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(),
        GURL("http://www.example.com/service_worker.js"),
        1L,
        helper_->context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(
        ServiceWorkerDatabase::ResourceRecord(10, version_->script_url(), 100));
    version_->script_cache_map()->SetResources(records);

    // Make the registration findable via storage functions.
    helper_->context()->storage()->LazyInitialize(base::Bind(&base::DoNothing));
    base::RunLoop().RunUntilIdle();
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    helper_->context()->storage()->StoreRegistration(
        registration_.get(),
        version_.get(),
        CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(SERVICE_WORKER_OK, status);

    provider_host_.reset(new ServiceWorkerProviderHost(
        kRenderProcessId, kRenderFrameId, 1, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
        helper_->context()->AsWeakPtr(), dispatcher_host_.get()));

    helper_->SimulateAddProcessToPattern(pattern, kRenderProcessId);
  }

  void TearDown() override {
    dispatcher_host_ = NULL;
    registration_ = NULL;
    version_ = NULL;
    provider_host_.reset();
    helper_.reset();
  }

  IPC::TestSink* ipc_sink() { return helper_->ipc_sink(); }

  TestBrowserThreadBundle browser_thread_bundle_;
  MockResourceContext resource_context_;

  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_ptr<ServiceWorkerProviderHost> provider_host_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHandleTest);
};

TEST_F(ServiceWorkerHandleTest, OnVersionStateChanged) {
  scoped_ptr<ServiceWorkerHandle> handle =
      ServiceWorkerHandle::Create(helper_->context()->AsWeakPtr(),
                                  provider_host_->AsWeakPtr(),
                                  version_.get());

  // Start the worker, and then...
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  // ...dispatch install event.
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  version_->DispatchInstallEvent(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  version_->SetStatus(ServiceWorkerVersion::INSTALLED);

  ASSERT_EQ(4UL, ipc_sink()->message_count());
  ASSERT_EQ(0L, dispatcher_host_->bad_message_received_count_);

  // We should be sending 1. StartWorker,
  EXPECT_EQ(EmbeddedWorkerMsg_StartWorker::ID,
            ipc_sink()->GetMessageAt(0)->type());
  // 2. StateChanged (state == Installing),
  VerifyStateChangedMessage(handle->handle_id(),
                            blink::WebServiceWorkerStateInstalling,
                            ipc_sink()->GetMessageAt(1));
  // 3. SendMessageToWorker (to send InstallEvent), and
  EXPECT_EQ(EmbeddedWorkerContextMsg_MessageToWorker::ID,
            ipc_sink()->GetMessageAt(2)->type());
  // 4. StateChanged (state == Installed).
  VerifyStateChangedMessage(handle->handle_id(),
                            blink::WebServiceWorkerStateInstalled,
                            ipc_sink()->GetMessageAt(3));
}

}  // namespace content
