// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

struct EmbeddedWorkerMsg_StartWorker_Params;
class GURL;

namespace content {

class EmbeddedWorkerRegistry;
class EmbeddedWorkerTestHelper;
class MessagePortMessageFilter;
class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
struct ServiceWorkerFetchRequest;

// In-Process EmbeddedWorker test helper.
//
// Usage: create an instance of this class to test browser-side embedded worker
// code without creating a child process.  This class will create a
// ServiceWorkerContextWrapper and ServiceWorkerContextCore for you.
//
// By default this class just notifies back WorkerStarted and WorkerStopped
// for StartWorker and StopWorker requests. The default implementation
// also returns success for event messages (e.g. InstallEvent, FetchEvent).
//
// Alternatively consumers can subclass this helper and override On*()
// methods to add their own logic/verification code.
//
// See embedded_worker_instance_unittest.cc for example usages.
//
class EmbeddedWorkerTestHelper : public IPC::Sender,
                                 public IPC::Listener {
 public:
  // Initialize this helper for |context|, and enable this as an IPC
  // sender for |mock_render_process_id|. If |user_data_directory| is empty,
  // the context makes storage stuff in memory.
  EmbeddedWorkerTestHelper(const base::FilePath& user_data_directory,
                           int mock_render_process_id);
  ~EmbeddedWorkerTestHelper() override;

  // Call this to simulate add/associate a process to a pattern.
  // This also registers this sender for the process.
  void SimulateAddProcessToPattern(const GURL& pattern, int process_id);

  // IPC::Sender implementation.
  bool Send(IPC::Message* message) override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  // IPC sink for EmbeddedWorker messages.
  IPC::TestSink* ipc_sink() { return &sink_; }
  // Inner IPC sink for script context messages sent via EmbeddedWorker.
  IPC::TestSink* inner_ipc_sink() { return &inner_sink_; }

  ServiceWorkerContextCore* context();
  ServiceWorkerContextWrapper* context_wrapper() { return wrapper_.get(); }
  void ShutdownContext();

  int mock_render_process_id() const { return mock_render_process_id_;}

 protected:
  // Called when StartWorker, StopWorker and SendMessageToWorker message
  // is sent to the embedded worker. Override if necessary. By default
  // they verify given parameters and:
  // - OnStartWorker calls SimulateWorkerStarted
  // - OnStopWorker calls SimulateWorkerStoped
  // - OnSendMessageToWorker calls the message's respective On*Event handler
  virtual void OnStartWorker(int embedded_worker_id,
                             int64 service_worker_version_id,
                             const GURL& scope,
                             const GURL& script_url);
  virtual void OnStopWorker(int embedded_worker_id);
  virtual bool OnMessageToWorker(int thread_id,
                                 int embedded_worker_id,
                                 const IPC::Message& message);

  // On*Event handlers. Called by the default implementation of
  // OnMessageToWorker when events are sent to the embedded
  // worker. By default they just return success via
  // SimulateSendReplyToBrowser.
  virtual void OnActivateEvent(int embedded_worker_id, int request_id);
  virtual void OnInstallEvent(int embedded_worker_id, int request_id);
  virtual void OnFetchEvent(int embedded_worker_id,
                            int request_id,
                            const ServiceWorkerFetchRequest& request);
  virtual void OnPushEvent(int embedded_worker_id,
                           int request_id,
                           const std::string& data);
  virtual void OnSyncEvent(int embedded_worker_id, int request_id);

  // These functions simulate sending an EmbeddedHostMsg message to the
  // browser.
  void SimulateWorkerReadyForInspection(int embedded_worker_id);
  void SimulateWorkerScriptCached(int embedded_worker_id);
  void SimulateWorkerScriptLoaded(int thread_id, int embedded_worker_id);
  void SimulateWorkerScriptEvaluated(int embedded_worker_id);
  void SimulateWorkerStarted(int embedded_worker_id);
  void SimulateWorkerStopped(int embedded_worker_id);
  void SimulateSend(IPC::Message* message);

  EmbeddedWorkerRegistry* registry();

 private:
  void OnStartWorkerStub(const EmbeddedWorkerMsg_StartWorker_Params& params);
  void OnStopWorkerStub(int embedded_worker_id);
  void OnMessageToWorkerStub(int thread_id,
                             int embedded_worker_id,
                             const IPC::Message& message);
  void OnActivateEventStub(int request_id);
  void OnInstallEventStub(int request_id);
  void OnFetchEventStub(int request_id,
                        const ServiceWorkerFetchRequest& request);
  void OnPushEventStub(int request_id, const std::string& data);
  void OnSyncEventStub(int request_id);

  MessagePortMessageFilter* NewMessagePortMessageFilter();

  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;

  IPC::TestSink sink_;
  IPC::TestSink inner_sink_;

  int next_thread_id_;
  int mock_render_process_id_;

  std::map<int, int64> embedded_worker_id_service_worker_version_id_map_;

  // Updated each time MessageToWorker message is received.
  int current_embedded_worker_id_;

  std::vector<scoped_refptr<MessagePortMessageFilter>>
      message_port_message_filters_;

  base::WeakPtrFactory<EmbeddedWorkerTestHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerTestHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_
