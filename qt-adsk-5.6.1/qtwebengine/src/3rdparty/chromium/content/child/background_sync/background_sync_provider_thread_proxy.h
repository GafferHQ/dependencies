// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_PROVIDER_THREAD_PROXY_H_
#define CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_PROVIDER_THREAD_PROXY_H_

#include "base/macros.h"
#include "content/child/worker_task_runner.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncProvider.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class BackgroundSyncProvider;

// BackgroundSyncProviderThreadProxy is a proxy to the BackgroundSyncProvider
// for callers running on a different thread than the main thread. There is one
// instance per worker thread.
//
// This class handles all of the thread switching, jumping to the main thread to
// call the WebSyncProvider methods, and wrapping the callbacks passed in with
// code to switch back to the original calling thread.
class BackgroundSyncProviderThreadProxy : public blink::WebSyncProvider,
                                          public WorkerTaskRunner::Observer {
 public:
  static BackgroundSyncProviderThreadProxy* GetThreadInstance(
      base::SingleThreadTaskRunner* main_thread_task_runner,
      BackgroundSyncProvider* permissions_dispatcher);

  // blink::WebSyncProvider implementation
  void registerBackgroundSync(
      const blink::WebSyncRegistration* options,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncRegistrationCallbacks* callbacks);
  void unregisterBackgroundSync(
      blink::WebSyncRegistration::Periodicity periodicity,
      int64_t id,
      const blink::WebString& tag,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncUnregistrationCallbacks* callbacks);
  void getRegistration(
      blink::WebSyncRegistration::Periodicity,
      const blink::WebString& tag,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncRegistrationCallbacks* callbacks);
  void getRegistrations(
      blink::WebSyncRegistration::Periodicity periodicity,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncGetRegistrationsCallbacks* callbacks);
  void getPermissionStatus(
      blink::WebSyncRegistration::Periodicity periodicity,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncGetPermissionStatusCallbacks* callbacks);

  // WorkerTaskRunner::Observer implementation.
  void OnWorkerRunLoopStopped() override;

 private:
  BackgroundSyncProviderThreadProxy(
      base::SingleThreadTaskRunner* main_thread_task_runner,
      BackgroundSyncProvider* sync_provider);

  virtual ~BackgroundSyncProviderThreadProxy();

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // This belongs to the renderer main thread, (created by BlinkPlatformImpl)
  // and so should outlive the BackgroundSyncProviderThreadProxy, which is
  // created for a worker thread.
  BackgroundSyncProvider* sync_provider_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncProviderThreadProxy);
};

}  // namespace content

#endif  // CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_PROVIDER_THREAD_PROXY_H_
