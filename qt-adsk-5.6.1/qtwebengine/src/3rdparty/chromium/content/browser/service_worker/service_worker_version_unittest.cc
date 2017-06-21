// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/run_loop.h"
#include "content/browser/message_port_service.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

// IPC messages for testing ---------------------------------------------------

#define IPC_MESSAGE_IMPL
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START TestMsgStart

IPC_MESSAGE_CONTROL0(TestMsg_Message);
IPC_MESSAGE_ROUTED1(TestMsg_MessageFromWorker, int);

// ---------------------------------------------------------------------------

namespace content {

namespace {

static const int kRenderProcessId = 1;

class MessageReceiver : public EmbeddedWorkerTestHelper {
 public:
  MessageReceiver()
      : EmbeddedWorkerTestHelper(base::FilePath(), kRenderProcessId),
        current_embedded_worker_id_(0) {}
  ~MessageReceiver() override {}

  bool OnMessageToWorker(int thread_id,
                         int embedded_worker_id,
                         const IPC::Message& message) override {
    if (EmbeddedWorkerTestHelper::OnMessageToWorker(
            thread_id, embedded_worker_id, message)) {
      return true;
    }
    current_embedded_worker_id_ = embedded_worker_id;
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MessageReceiver, message)
      IPC_MESSAGE_HANDLER(TestMsg_Message, OnMessage)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void SimulateSendValueToBrowser(int embedded_worker_id, int value) {
    SimulateSend(new TestMsg_MessageFromWorker(embedded_worker_id, value));
  }

 private:
  void OnMessage() {
    // Do nothing.
  }

  int current_embedded_worker_id_;
  DISALLOW_COPY_AND_ASSIGN(MessageReceiver);
};

void VerifyCalled(bool* called) {
  *called = true;
}

void ObserveStatusChanges(ServiceWorkerVersion* version,
                          std::vector<ServiceWorkerVersion::Status>* statuses) {
  statuses->push_back(version->status());
  version->RegisterStatusChangeCallback(
      base::Bind(&ObserveStatusChanges, base::Unretained(version), statuses));
}

void ReceiveFetchResult(ServiceWorkerStatusCode* status,
                        ServiceWorkerStatusCode actual_status,
                        ServiceWorkerFetchEventResult actual_result,
                        const ServiceWorkerResponse& response) {
  *status = actual_status;
}

// A specialized listener class to receive test messages from a worker.
class MessageReceiverFromWorker : public EmbeddedWorkerInstance::Listener {
 public:
  explicit MessageReceiverFromWorker(EmbeddedWorkerInstance* instance)
      : instance_(instance) {
    instance_->AddListener(this);
  }
  ~MessageReceiverFromWorker() override { instance_->RemoveListener(this); }

  void OnStarted() override { NOTREACHED(); }
  void OnStopped(EmbeddedWorkerInstance::Status old_status) override {
    NOTREACHED();
  }
  bool OnMessageReceived(const IPC::Message& message) override {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MessageReceiverFromWorker, message)
      IPC_MESSAGE_HANDLER(TestMsg_MessageFromWorker, OnMessageFromWorker)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnMessageFromWorker(int value) { received_values_.push_back(value); }
  const std::vector<int>& received_values() const { return received_values_; }

 private:
  EmbeddedWorkerInstance* instance_;
  std::vector<int> received_values_;
  DISALLOW_COPY_AND_ASSIGN(MessageReceiverFromWorker);
};

void SetUpDummyMessagePort(std::vector<TransferredMessagePort>* ports) {
  int port_id = -1;
  MessagePortService::GetInstance()->Create(MSG_ROUTING_NONE, nullptr,
                                            &port_id);
  TransferredMessagePort dummy_port;
  dummy_port.id = port_id;
  ports->push_back(dummy_port);
}

base::Time GetYesterday() {
  return base::Time::Now() - base::TimeDelta::FromDays(1) -
         base::TimeDelta::FromSeconds(1);
}

}  // namespace

class ServiceWorkerVersionTest : public testing::Test {
 protected:
  struct RunningStateListener : public ServiceWorkerVersion::Listener {
    RunningStateListener() : last_status(ServiceWorkerVersion::STOPPED) {}
    ~RunningStateListener() override {}
    void OnRunningStateChanged(ServiceWorkerVersion* version) override {
      last_status = version->running_status();
    }
    ServiceWorkerVersion::RunningStatus last_status;
  };

  ServiceWorkerVersionTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_ = GetMessageReceiver();

    pattern_ = GURL("http://www.example.com/");
    registration_ = new ServiceWorkerRegistration(
        pattern_,
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

    // Simulate adding one process to the pattern.
    helper_->SimulateAddProcessToPattern(pattern_, kRenderProcessId);
    ASSERT_TRUE(helper_->context()->process_manager()
        ->PatternHasProcessToRun(pattern_));
  }

  virtual scoped_ptr<MessageReceiver> GetMessageReceiver() {
    return make_scoped_ptr(new MessageReceiver());
  }

  void TearDown() override {
    version_ = 0;
    registration_ = 0;
    helper_.reset();
  }

  TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<MessageReceiver> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  GURL pattern_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersionTest);
};

class MessageReceiverDisallowStart : public MessageReceiver {
 public:
  MessageReceiverDisallowStart()
      : MessageReceiver() {}
  ~MessageReceiverDisallowStart() override {}

  void OnStartWorker(int embedded_worker_id,
                     int64 service_worker_version_id,
                     const GURL& scope,
                     const GURL& script_url) override {
    // Do nothing.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageReceiverDisallowStart);
};

class ServiceWorkerFailToStartTest : public ServiceWorkerVersionTest {
 protected:
  ServiceWorkerFailToStartTest()
      : ServiceWorkerVersionTest() {}

  scoped_ptr<MessageReceiver> GetMessageReceiver() override {
    return make_scoped_ptr(new MessageReceiverDisallowStart());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFailToStartTest);
};

class MessageReceiverDisallowFetch : public MessageReceiver {
 public:
  MessageReceiverDisallowFetch() : MessageReceiver() {}
  ~MessageReceiverDisallowFetch() override {}

  void OnFetchEvent(int embedded_worker_id,
                    int request_id,
                    const ServiceWorkerFetchRequest& request) override {
    // Do nothing.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageReceiverDisallowFetch);
};

class ServiceWorkerWaitForeverInFetchTest : public ServiceWorkerVersionTest {
 protected:
  ServiceWorkerWaitForeverInFetchTest() : ServiceWorkerVersionTest() {}

  scoped_ptr<MessageReceiver> GetMessageReceiver() override {
    return make_scoped_ptr(new MessageReceiverDisallowFetch());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerWaitForeverInFetchTest);
};

TEST_F(ServiceWorkerVersionTest, ConcurrentStartAndStop) {
  // Call StartWorker() multiple times.
  ServiceWorkerStatusCode status1 = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode status2 = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode status3 = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(CreateReceiverOnCurrentThread(&status1));
  version_->StartWorker(CreateReceiverOnCurrentThread(&status2));

  EXPECT_EQ(ServiceWorkerVersion::STARTING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());

  // Call StartWorker() after it's started.
  version_->StartWorker(CreateReceiverOnCurrentThread(&status3));
  base::RunLoop().RunUntilIdle();

  // All should just succeed.
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);
  EXPECT_EQ(SERVICE_WORKER_OK, status3);

  // Call StopWorker() multiple times.
  status1 = SERVICE_WORKER_ERROR_FAILED;
  status2 = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&status1));
  version_->StopWorker(CreateReceiverOnCurrentThread(&status2));

  EXPECT_EQ(ServiceWorkerVersion::STOPPING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version_->running_status());

  // All StopWorker should just succeed.
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);

  // Start worker again.
  status1 = SERVICE_WORKER_ERROR_FAILED;
  status2 = SERVICE_WORKER_ERROR_FAILED;
  status3 = SERVICE_WORKER_ERROR_FAILED;

  version_->StartWorker(CreateReceiverOnCurrentThread(&status1));

  EXPECT_EQ(ServiceWorkerVersion::STARTING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());

  // Call StopWorker()
  status2 = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&status2));

  // And try calling StartWorker while StopWorker is in queue.
  version_->StartWorker(CreateReceiverOnCurrentThread(&status3));

  EXPECT_EQ(ServiceWorkerVersion::STOPPING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());

  // All should just succeed.
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);
  EXPECT_EQ(SERVICE_WORKER_OK, status3);
}

TEST_F(ServiceWorkerVersionTest, DispatchEventToStoppedWorker) {
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version_->running_status());

  // Dispatch an event without starting the worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  version_->DispatchInstallEvent(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  // The worker should be now started.
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());

  // Stop the worker, and then dispatch an event immediately after that.
  status = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode stop_status = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&stop_status));
  version_->DispatchInstallEvent(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, stop_status);

  // Dispatch an event should return SERVICE_WORKER_OK since the worker
  // should have been restarted to dispatch the event.
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  // The worker should be now started again.
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, ReceiveMessageFromWorker) {
  // Start worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(CreateReceiverOnCurrentThread(&status));
  EXPECT_EQ(ServiceWorkerVersion::STARTING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());

  MessageReceiverFromWorker receiver(version_->embedded_worker());

  // Simulate sending some dummy values from the worker.
  helper_->SimulateSendValueToBrowser(
      version_->embedded_worker()->embedded_worker_id(), 555);
  helper_->SimulateSendValueToBrowser(
      version_->embedded_worker()->embedded_worker_id(), 777);

  // Verify the receiver received the values.
  ASSERT_EQ(2U, receiver.received_values().size());
  EXPECT_EQ(555, receiver.received_values()[0]);
  EXPECT_EQ(777, receiver.received_values()[1]);
}

TEST_F(ServiceWorkerVersionTest, InstallAndWaitCompletion) {
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);

  // Dispatch an install event.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->DispatchInstallEvent(CreateReceiverOnCurrentThread(&status));

  // Wait for the completion.
  bool status_change_called = false;
  version_->RegisterStatusChangeCallback(
      base::Bind(&VerifyCalled, &status_change_called));

  base::RunLoop().RunUntilIdle();

  // Version's status must not have changed during installation.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_FALSE(status_change_called);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLING, version_->status());
}

TEST_F(ServiceWorkerVersionTest, ActivateAndWaitCompletion) {
  version_->SetStatus(ServiceWorkerVersion::ACTIVATING);

  // Dispatch an activate event.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->DispatchActivateEvent(CreateReceiverOnCurrentThread(&status));

  // Wait for the completion.
  bool status_change_called = false;
  version_->RegisterStatusChangeCallback(
      base::Bind(&VerifyCalled, &status_change_called));

  base::RunLoop().RunUntilIdle();

  // Version's status must not have changed during activation.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_FALSE(status_change_called);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATING, version_->status());
}

TEST_F(ServiceWorkerVersionTest, RepeatedlyObserveStatusChanges) {
  EXPECT_EQ(ServiceWorkerVersion::NEW, version_->status());

  // Repeatedly observe status changes (the callback re-registers itself).
  std::vector<ServiceWorkerVersion::Status> statuses;
  version_->RegisterStatusChangeCallback(
      base::Bind(&ObserveStatusChanges, version_, &statuses));

  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATING);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->SetStatus(ServiceWorkerVersion::REDUNDANT);

  // Verify that we could successfully observe repeated status changes.
  ASSERT_EQ(5U, statuses.size());
  ASSERT_EQ(ServiceWorkerVersion::INSTALLING, statuses[0]);
  ASSERT_EQ(ServiceWorkerVersion::INSTALLED, statuses[1]);
  ASSERT_EQ(ServiceWorkerVersion::ACTIVATING, statuses[2]);
  ASSERT_EQ(ServiceWorkerVersion::ACTIVATED, statuses[3]);
  ASSERT_EQ(ServiceWorkerVersion::REDUNDANT, statuses[4]);
}

TEST_F(ServiceWorkerVersionTest, IdleTimeout) {
  // Used to reliably test when the idle time gets reset regardless of clock
  // granularity.
  const base::TimeDelta kOneSecond = base::TimeDelta::FromSeconds(1);

  // Verify the timer is not running when version initializes its status.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  EXPECT_FALSE(version_->timeout_timer_.IsRunning());

  // Verify the timer is running after the worker is started.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_FALSE(version_->idle_time_.is_null());

  // The idle time should be reset if the worker is restarted without
  // controllee.
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->idle_time_ -= kOneSecond;
  base::TimeTicks idle_time = version_->idle_time_;
  version_->StopWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_LT(idle_time, version_->idle_time_);

  // Adding a controllee resets the idle time.
  version_->idle_time_ -= kOneSecond;
  idle_time = version_->idle_time_;
  scoped_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
      33 /* dummy render process id */, MSG_ROUTING_NONE /* render_frame_id */,
      1 /* dummy provider_id */, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
      helper_->context()->AsWeakPtr(), NULL));
  version_->AddControllee(host.get());
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_LT(idle_time, version_->idle_time_);

  // Completing an event resets the idle time.
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->idle_time_ -= kOneSecond;
  idle_time = version_->idle_time_;
  version_->DispatchFetchEvent(ServiceWorkerFetchRequest(),
                               base::Bind(&base::DoNothing),
                               base::Bind(&ReceiveFetchResult, &status));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_LT(idle_time, version_->idle_time_);

  // Dispatching a message event resets the idle time.
  std::vector<TransferredMessagePort> ports;
  SetUpDummyMessagePort(&ports);
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->idle_time_ -= kOneSecond;
  idle_time = version_->idle_time_;
  version_->DispatchMessageEvent(base::string16(), ports,
                                 CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  MessagePortService::GetInstance()->Destroy(ports[0].id);

  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_LT(idle_time, version_->idle_time_);
}

TEST_F(ServiceWorkerVersionTest, SetDevToolsAttached) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(CreateReceiverOnCurrentThread(&status));

  ASSERT_EQ(ServiceWorkerVersion::STARTING, version_->running_status());

  ASSERT_TRUE(version_->timeout_timer_.IsRunning());
  ASSERT_FALSE(version_->start_time_.is_null());
  ASSERT_FALSE(version_->skip_recording_startup_time_);

  // Simulate DevTools is attached. This should deactivate the timer for start
  // timeout, but not stop the timer itself.
  version_->SetDevToolsAttached(true);
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_TRUE(version_->start_time_.is_null());
  EXPECT_TRUE(version_->skip_recording_startup_time_);

  // Simulate DevTools is detached. This should reactivate the timer for start
  // timeout.
  version_->SetDevToolsAttached(false);
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_FALSE(version_->start_time_.is_null());
  EXPECT_TRUE(version_->skip_recording_startup_time_);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, StoppingBeforeDestruct) {
  RunningStateListener listener;
  version_->AddListener(&listener);

  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, listener.last_status);

  version_ = nullptr;
  EXPECT_EQ(ServiceWorkerVersion::STOPPING, listener.last_status);
}

// Test that update isn't triggered for a non-stale worker.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_FreshWorker) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(base::Time::Now());
  version_->DispatchPushEvent(CreateReceiverOnCurrentThread(&status),
                              std::string());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());
}

// Test that update isn't triggered for a non-active worker.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_NonActiveWorker) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;

  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  registration_->SetInstallingVersion(version_);
  registration_->set_last_update_check(GetYesterday());
  version_->DispatchInstallEvent(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());
}

// Test that staleness is detected when starting a worker.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_StartWorker) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;

  // Starting the worker marks it as stale.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(GetYesterday());
  version_->DispatchPushEvent(CreateReceiverOnCurrentThread(&status),
                              std::string());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_FALSE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());

  // Update is actually scheduled after the worker stops.
  version_->StopWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_TRUE(version_->update_timer_.IsRunning());
}

// Test that staleness is detected on a running worker.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_RunningWorker) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;

  // Start a fresh worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(base::Time::Now());
  version_->DispatchPushEvent(CreateReceiverOnCurrentThread(&status),
                              std::string());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->stale_time_.is_null());

  // Simulate it running for a day. It will be marked stale.
  registration_->set_last_update_check(GetYesterday());
  version_->OnTimeoutTimer();
  EXPECT_FALSE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());

  // Simulate it running for past the wait threshold. The update will be
  // scheduled.
  version_->stale_time_ =
      base::TimeTicks::Now() -
      base::TimeDelta::FromMinutes(
          ServiceWorkerVersion::kStartWorkerTimeoutMinutes + 1);
  version_->OnTimeoutTimer();
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_TRUE(version_->update_timer_.IsRunning());
}

// Test that a stream of events doesn't restart the timer.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_DoNotDeferTimer) {
  // Make a stale worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(GetYesterday());
  base::TimeTicks stale_time =
      base::TimeTicks::Now() -
      base::TimeDelta::FromMinutes(
          ServiceWorkerVersion::kStartWorkerTimeoutMinutes + 1);
  version_->stale_time_ = stale_time;

  // Stale time is not deferred.
  version_->DispatchPushEvent(
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback), std::string());
  version_->DispatchPushEvent(
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback), std::string());
  EXPECT_EQ(stale_time, version_->stale_time_);

  // Timeout triggers the update.
  version_->OnTimeoutTimer();
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_TRUE(version_->update_timer_.IsRunning());

  // Update timer is not deferred.
  base::TimeTicks run_time = version_->update_timer_.desired_run_time();
  version_->DispatchPushEvent(
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback), std::string());
  version_->DispatchPushEvent(
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback), std::string());
  version_->DispatchPushEvent(
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback), std::string());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_EQ(run_time, version_->update_timer_.desired_run_time());
}

TEST_F(ServiceWorkerWaitForeverInFetchTest, RequestTimeout) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->DispatchFetchEvent(ServiceWorkerFetchRequest(),
                               base::Bind(&base::DoNothing),
                               base::Bind(&ReceiveFetchResult, &status));
  base::RunLoop().RunUntilIdle();

  // Callback has not completed yet.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NETWORK, status);
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version_->running_status());

  // Simulate timeout.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->SetAllRequestTimes(
      base::TimeTicks::Now() -
      base::TimeDelta::FromMinutes(
          ServiceWorkerVersion::kRequestTimeoutMinutes + 1));
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, status);
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version_->running_status());
}

TEST_F(ServiceWorkerFailToStartTest, RendererCrash) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value
  version_->StartWorker(
      CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // Callback has not completed yet.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NETWORK, status);
  EXPECT_EQ(ServiceWorkerVersion::STARTING, version_->running_status());

  // Simulate renderer crash: do what
  // ServiceWorkerDispatcherHost::OnFilterRemoved does.
  int process_id = helper_->mock_render_process_id();
  helper_->context()->RemoveAllProviderHostsForProcess(process_id);
  helper_->context()->embedded_worker_registry()->RemoveChildProcessSender(
      process_id);
  base::RunLoop().RunUntilIdle();

  // Callback completed.
  EXPECT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, status);
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version_->running_status());
}

TEST_F(ServiceWorkerFailToStartTest, Timeout) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value

  // We could just call StartWorker but make it interesting and test
  // starting the worker as part of dispatching an event.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATING);
  version_->DispatchActivateEvent(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // Callback has not completed yet.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NETWORK, status);
  EXPECT_EQ(ServiceWorkerVersion::STARTING, version_->running_status());

  // Simulate timeout.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->start_time_ =
      base::TimeTicks::Now() -
      base::TimeDelta::FromMinutes(
          ServiceWorkerVersion::kStartWorkerTimeoutMinutes + 1);
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, status);
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version_->running_status());
}

}  // namespace content
