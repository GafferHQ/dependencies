// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

static void SaveStatusCallback(bool* called,
                               ServiceWorkerStatusCode* out,
                               ServiceWorkerStatusCode status) {
  *called = true;
  *out = status;
}

}

static const int kRenderProcessId = 1;
static const int kRenderFrameId = 1;

class TestingServiceWorkerDispatcherHost : public ServiceWorkerDispatcherHost {
 public:
  TestingServiceWorkerDispatcherHost(
      int process_id,
      ServiceWorkerContextWrapper* context_wrapper,
      ResourceContext* resource_context,
      EmbeddedWorkerTestHelper* helper)
      : ServiceWorkerDispatcherHost(process_id, NULL, resource_context),
        bad_messages_received_count_(0),
        helper_(helper) {
    Init(context_wrapper);
  }

  bool Send(IPC::Message* message) override { return helper_->Send(message); }

  IPC::TestSink* ipc_sink() { return helper_->ipc_sink(); }

  void ShutdownForBadMessage() override { ++bad_messages_received_count_; }

  int bad_messages_received_count_;

 protected:
  EmbeddedWorkerTestHelper* helper_;
  ~TestingServiceWorkerDispatcherHost() override {}
};

class ServiceWorkerDispatcherHostTest : public testing::Test {
 protected:
  ServiceWorkerDispatcherHostTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_.reset(
        new EmbeddedWorkerTestHelper(base::FilePath(), kRenderProcessId));
    dispatcher_host_ = new TestingServiceWorkerDispatcherHost(
        kRenderProcessId, context_wrapper(), &resource_context_, helper_.get());
  }

  void TearDown() override { helper_.reset(); }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() {
    return helper_->context_wrapper();
  }

  void SendRegister(int64 provider_id, GURL pattern, GURL worker_url) {
    dispatcher_host_->OnMessageReceived(
        ServiceWorkerHostMsg_RegisterServiceWorker(
            -1, -1, provider_id, pattern, worker_url));
    base::RunLoop().RunUntilIdle();
  }

  void Register(int64 provider_id,
                GURL pattern,
                GURL worker_url,
                uint32 expected_message) {
    SendRegister(provider_id, pattern, worker_url);
    EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
        expected_message));
    dispatcher_host_->ipc_sink()->ClearMessages();
  }

  void SendUnregister(int64 provider_id, int64 registration_id) {
    dispatcher_host_->OnMessageReceived(
        ServiceWorkerHostMsg_UnregisterServiceWorker(-1, -1, provider_id,
                                                     registration_id));
    base::RunLoop().RunUntilIdle();
  }

  void Unregister(int64 provider_id,
                  int64 registration_id,
                  uint32 expected_message) {
    SendUnregister(provider_id, registration_id);
    EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
        expected_message));
    dispatcher_host_->ipc_sink()->ClearMessages();
  }

  void SendGetRegistration(int64 provider_id, GURL document_url) {
    dispatcher_host_->OnMessageReceived(
        ServiceWorkerHostMsg_GetRegistration(
            -1, -1, provider_id, document_url));
    base::RunLoop().RunUntilIdle();
  }

  void GetRegistration(int64 provider_id,
                       GURL document_url,
                       uint32 expected_message) {
    SendGetRegistration(provider_id, document_url);
    EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
        expected_message));
    dispatcher_host_->ipc_sink()->ClearMessages();
  }

  void SendGetRegistrations(int64 provider_id) {
    dispatcher_host_->OnMessageReceived(
        ServiceWorkerHostMsg_GetRegistrations(-1, -1, provider_id));
    base::RunLoop().RunUntilIdle();
  }

  void GetRegistrations(int64 provider_id, uint32 expected_message) {
    SendGetRegistrations(provider_id);
    EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
        expected_message));
    dispatcher_host_->ipc_sink()->ClearMessages();
  }

  ServiceWorkerProviderHost* CreateServiceWorkerProviderHost(int provider_id) {
    return new ServiceWorkerProviderHost(
        kRenderProcessId, kRenderFrameId, provider_id,
        SERVICE_WORKER_PROVIDER_FOR_WINDOW, context()->AsWeakPtr(),
        dispatcher_host_.get());
  }


  TestBrowserThreadBundle browser_thread_bundle_;
  content::MockResourceContext resource_context_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host_;
};

class ServiceWorkerTestContentBrowserClient : public TestContentBrowserClient {
 public:
  ServiceWorkerTestContentBrowserClient() {}
  bool AllowServiceWorker(const GURL& scope,
                          const GURL& first_party,
                          content::ResourceContext* context,
                          int render_process_id,
                          int render_frame_id) override {
    return false;
  }
};

TEST_F(ServiceWorkerDispatcherHostTest,
       Register_ContentSettingsDisallowsServiceWorker) {
  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);

  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(host.Pass());

  Register(kProviderId,
           GURL("https://www.example.com/"),
           GURL("https://www.example.com/bar"),
           ServiceWorkerMsg_ServiceWorkerRegistrationError::ID);
  GetRegistration(kProviderId,
                  GURL("https://www.example.com/"),
                  ServiceWorkerMsg_ServiceWorkerGetRegistrationError::ID);
  GetRegistrations(kProviderId,
                   ServiceWorkerMsg_ServiceWorkerGetRegistrationsError::ID);

  // Add a registration into a live registration map so that Unregister() can
  // find it.
  const int64 kRegistrationId = 999;  // Dummy value
  scoped_refptr<ServiceWorkerRegistration> registration(
      new ServiceWorkerRegistration(GURL("https://www.example.com/"),
                                    kRegistrationId, context()->AsWeakPtr()));
  Unregister(kProviderId, kRegistrationId,
             ServiceWorkerMsg_ServiceWorkerUnregistrationError::ID);

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ServiceWorkerDispatcherHostTest, Register_HTTPS) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(host.Pass());

  Register(kProviderId,
           GURL("https://www.example.com/"),
           GURL("https://www.example.com/bar"),
           ServiceWorkerMsg_ServiceWorkerRegistered::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, Register_NonSecureTransportLocalhost) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("http://127.0.0.3:81/foo"));
  context()->AddProviderHost(host.Pass());

  Register(kProviderId,
           GURL("http://127.0.0.3:81/bar"),
           GURL("http://127.0.0.3:81/baz"),
           ServiceWorkerMsg_ServiceWorkerRegistered::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, Register_InvalidScopeShouldFail) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(host.Pass());

  SendRegister(kProviderId, GURL(""),
               GURL("https://www.example.com/bar/hoge.js"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, Register_InvalidScriptShouldFail) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(host.Pass());

  SendRegister(kProviderId, GURL("https://www.example.com/bar/"), GURL(""));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, Register_NonSecureOriginShouldFail) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("http://www.example.com/foo"));
  context()->AddProviderHost(host.Pass());

  SendRegister(kProviderId,
               GURL("http://www.example.com/"),
               GURL("http://www.example.com/bar"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, Register_CrossOriginShouldFail) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(host.Pass());

  // Script has a different host
  SendRegister(kProviderId,
               GURL("https://www.example.com/"),
               GURL("https://foo.example.com/bar"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);

  // Scope has a different host
  SendRegister(kProviderId,
               GURL("https://foo.example.com/"),
               GURL("https://www.example.com/bar"));
  EXPECT_EQ(2, dispatcher_host_->bad_messages_received_count_);

  // Script has a different port
  SendRegister(kProviderId,
               GURL("https://www.example.com/"),
               GURL("https://www.example.com:8080/bar"));
  EXPECT_EQ(3, dispatcher_host_->bad_messages_received_count_);

  // Scope has a different transport
  SendRegister(kProviderId,
               GURL("wss://www.example.com/"),
               GURL("https://www.example.com/bar"));
  EXPECT_EQ(4, dispatcher_host_->bad_messages_received_count_);

  // Script and scope have a different host but match each other
  SendRegister(kProviderId,
               GURL("https://foo.example.com/"),
               GURL("https://foo.example.com/bar"));
  EXPECT_EQ(5, dispatcher_host_->bad_messages_received_count_);

  // Script and scope URLs are invalid
  SendRegister(kProviderId,
               GURL(),
               GURL("h@ttps://@"));
  EXPECT_EQ(6, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest,
       Register_FileSystemDocumentShouldFail) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("filesystem:https://www.example.com/temporary/a"));
  context()->AddProviderHost(host.Pass());

  SendRegister(kProviderId,
               GURL("filesystem:https://www.example.com/temporary/"),
               GURL("https://www.example.com/temporary/bar"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);

  SendRegister(kProviderId,
               GURL("https://www.example.com/temporary/"),
               GURL("filesystem:https://www.example.com/temporary/bar"));
  EXPECT_EQ(2, dispatcher_host_->bad_messages_received_count_);

  SendRegister(kProviderId,
               GURL("filesystem:https://www.example.com/temporary/"),
               GURL("filesystem:https://www.example.com/temporary/bar"));
  EXPECT_EQ(3, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest,
       Register_FileSystemScriptOrScopeShouldFail) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/temporary/"));
  context()->AddProviderHost(host.Pass());

  SendRegister(kProviderId,
               GURL("filesystem:https://www.example.com/temporary/"),
               GURL("https://www.example.com/temporary/bar"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);

  SendRegister(kProviderId,
               GURL("https://www.example.com/temporary/"),
               GURL("filesystem:https://www.example.com/temporary/bar"));
  EXPECT_EQ(2, dispatcher_host_->bad_messages_received_count_);

  SendRegister(kProviderId,
               GURL("filesystem:https://www.example.com/temporary/"),
               GURL("filesystem:https://www.example.com/temporary/bar"));
  EXPECT_EQ(3, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, EarlyContextDeletion) {
  helper_->ShutdownContext();

  // Let the shutdown reach the simulated IO thread.
  base::RunLoop().RunUntilIdle();

  Register(-1,
           GURL(),
           GURL(),
           ServiceWorkerMsg_ServiceWorkerRegistrationError::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, ProviderCreatedAndDestroyed) {
  const int kProviderId = 1001;  // Test with a value != kRenderProcessId.

  dispatcher_host_->OnMessageReceived(ServiceWorkerHostMsg_ProviderCreated(
      kProviderId, MSG_ROUTING_NONE, SERVICE_WORKER_PROVIDER_FOR_WINDOW));
  EXPECT_TRUE(context()->GetProviderHost(kRenderProcessId, kProviderId));

  // Two with the same ID should be seen as a bad message.
  dispatcher_host_->OnMessageReceived(ServiceWorkerHostMsg_ProviderCreated(
      kProviderId, MSG_ROUTING_NONE, SERVICE_WORKER_PROVIDER_FOR_WINDOW));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);

  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderDestroyed(kProviderId));
  EXPECT_FALSE(context()->GetProviderHost(kRenderProcessId, kProviderId));

  // Destroying an ID that does not exist warrants a bad message.
  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderDestroyed(kProviderId));
  EXPECT_EQ(2, dispatcher_host_->bad_messages_received_count_);

  // Deletion of the dispatcher_host should cause providers for that
  // process to get deleted as well.
  dispatcher_host_->OnMessageReceived(ServiceWorkerHostMsg_ProviderCreated(
      kProviderId, MSG_ROUTING_NONE, SERVICE_WORKER_PROVIDER_FOR_WINDOW));
  EXPECT_TRUE(context()->GetProviderHost(kRenderProcessId, kProviderId));
  EXPECT_TRUE(dispatcher_host_->HasOneRef());
  dispatcher_host_ = NULL;
  EXPECT_FALSE(context()->GetProviderHost(kRenderProcessId, kProviderId));
}

TEST_F(ServiceWorkerDispatcherHostTest, GetRegistration_SameOrigin) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(host.Pass());

  GetRegistration(kProviderId,
                  GURL("https://www.example.com/"),
                  ServiceWorkerMsg_DidGetRegistration::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, GetRegistration_CrossOriginShouldFail) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(host.Pass());

  SendGetRegistration(kProviderId, GURL("https://foo.example.com/"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest,
       GetRegistration_InvalidScopeShouldFail) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(host.Pass());

  SendGetRegistration(kProviderId, GURL(""));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest,
       GetRegistration_NonSecureOriginShouldFail) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("http://www.example.com/foo"));
  context()->AddProviderHost(host.Pass());

  SendGetRegistration(kProviderId, GURL("http://www.example.com/"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, GetRegistration_EarlyContextDeletion) {
  helper_->ShutdownContext();

  // Let the shutdown reach the simulated IO thread.
  base::RunLoop().RunUntilIdle();

  GetRegistration(-1,
                  GURL(),
                  ServiceWorkerMsg_ServiceWorkerGetRegistrationError::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, GetRegistrations_SecureOrigin) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(host.Pass());

  GetRegistrations(kProviderId, ServiceWorkerMsg_DidGetRegistrations::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest,
       GetRegistrations_NonSecureOriginShouldFail) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("http://www.example.com/foo"));
  context()->AddProviderHost(host.Pass());

  SendGetRegistrations(kProviderId);
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, GetRegistrations_EarlyContextDeletion) {
  helper_->ShutdownContext();

  // Let the shutdown reach the simulated IO thread.
  base::RunLoop().RunUntilIdle();

  GetRegistrations(-1, ServiceWorkerMsg_ServiceWorkerGetRegistrationsError::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, CleanupOnRendererCrash) {
  // Add a provider and worker.
  const int64 kProviderId = 99;  // Dummy value
  dispatcher_host_->OnMessageReceived(ServiceWorkerHostMsg_ProviderCreated(
      kProviderId, MSG_ROUTING_NONE, SERVICE_WORKER_PROVIDER_FOR_WINDOW));

  GURL pattern = GURL("http://www.example.com/");
  scoped_refptr<ServiceWorkerRegistration> registration(
      new ServiceWorkerRegistration(pattern,
                                    1L,
                                    helper_->context()->AsWeakPtr()));
  scoped_refptr<ServiceWorkerVersion> version(
      new ServiceWorkerVersion(registration.get(),
                               GURL("http://www.example.com/service_worker.js"),
                               1L,
                               helper_->context()->AsWeakPtr()));
  std::vector<ServiceWorkerDatabase::ResourceRecord> records;
  records.push_back(
      ServiceWorkerDatabase::ResourceRecord(10, version->script_url(), 100));
  version->script_cache_map()->SetResources(records);

  // Make the registration findable via storage functions.
  helper_->context()->storage()->LazyInitialize(base::Bind(&base::DoNothing));
  base::RunLoop().RunUntilIdle();
  bool called = false;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_ABORT;
  helper_->context()->storage()->StoreRegistration(
      registration.get(),
      version.get(),
      base::Bind(&SaveStatusCallback, &called, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  ASSERT_EQ(SERVICE_WORKER_OK, status);

  helper_->SimulateAddProcessToPattern(pattern, kRenderProcessId);

  // Start up the worker.
  status = SERVICE_WORKER_ERROR_ABORT;
  version->StartWorker(base::Bind(&SaveStatusCallback, &called, &status));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(called);
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  EXPECT_TRUE(context()->GetProviderHost(kRenderProcessId, kProviderId));
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version->running_status());

  // Simulate the render process crashing.
  dispatcher_host_->OnFilterRemoved();

  // The dispatcher host should clean up the state from the process.
  EXPECT_FALSE(context()->GetProviderHost(kRenderProcessId, kProviderId));
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version->running_status());

  // We should be able to hook up a new dispatcher host although the old object
  // is not yet destroyed. This is what the browser does when reusing a crashed
  // render process.
  scoped_refptr<TestingServiceWorkerDispatcherHost> new_dispatcher_host(
      new TestingServiceWorkerDispatcherHost(kRenderProcessId,
                                             context_wrapper(),
                                             &resource_context_,
                                             helper_.get()));

  // To show the new dispatcher can operate, simulate provider creation. Since
  // the old dispatcher cleaned up the old provider host, the new one won't
  // complain.
  new_dispatcher_host->OnMessageReceived(ServiceWorkerHostMsg_ProviderCreated(
      kProviderId, MSG_ROUTING_NONE, SERVICE_WORKER_PROVIDER_FOR_WINDOW));
  EXPECT_EQ(0, new_dispatcher_host->bad_messages_received_count_);
}

}  // namespace content
