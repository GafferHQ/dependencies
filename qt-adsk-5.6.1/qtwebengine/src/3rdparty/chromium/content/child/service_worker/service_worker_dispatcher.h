// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_

#include <map>
#include <vector>

#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/child/worker_task_runner.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerProvider.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerState.h"

class GURL;

namespace blink {
class WebURL;
}

namespace IPC {
class Message;
}

struct ServiceWorkerMsg_MessageToDocument_Params;

namespace content {

class ServiceWorkerMessageFilter;
class ServiceWorkerProviderContext;
class ThreadSafeSender;
class WebServiceWorkerImpl;
class WebServiceWorkerRegistrationImpl;
struct ServiceWorkerObjectInfo;
struct ServiceWorkerRegistrationObjectInfo;
struct ServiceWorkerVersionAttributes;

// This class manages communication with the browser process about
// registration of the service worker, exposed to renderer and worker
// scripts through methods like navigator.registerServiceWorker().
class CONTENT_EXPORT ServiceWorkerDispatcher
    : public WorkerTaskRunner::Observer {
 public:
  typedef blink::WebServiceWorkerProvider::WebServiceWorkerRegistrationCallbacks
      WebServiceWorkerRegistrationCallbacks;
  typedef blink::WebCallbacks<bool, blink::WebServiceWorkerError>
      WebServiceWorkerUnregistrationCallbacks;
  typedef
      blink::WebServiceWorkerProvider::WebServiceWorkerGetRegistrationCallbacks
      WebServiceWorkerGetRegistrationCallbacks;
  typedef
      blink::WebServiceWorkerProvider::WebServiceWorkerGetRegistrationsCallbacks
      WebServiceWorkerGetRegistrationsCallbacks;
  typedef blink::WebServiceWorkerProvider::
      WebServiceWorkerGetRegistrationForReadyCallbacks
          WebServiceWorkerGetRegistrationForReadyCallbacks;

  explicit ServiceWorkerDispatcher(ThreadSafeSender* thread_safe_sender);
  ~ServiceWorkerDispatcher() override;

  void OnMessageReceived(const IPC::Message& msg);
  bool Send(IPC::Message* msg);

  // Corresponds to navigator.serviceWorker.register().
  void RegisterServiceWorker(
      int provider_id,
      const GURL& pattern,
      const GURL& script_url,
      WebServiceWorkerRegistrationCallbacks* callbacks);
  // Corresponds to ServiceWorkerRegistration.update().
  void UpdateServiceWorker(int provider_id, int64 registration_id);
  // Corresponds to ServiceWorkerRegistration.unregister().
  void UnregisterServiceWorker(
      int provider_id,
      int64 registration_id,
      WebServiceWorkerUnregistrationCallbacks* callbacks);
  // Corresponds to navigator.serviceWorker.getRegistration().
  void GetRegistration(
      int provider_id,
      const GURL& document_url,
      WebServiceWorkerRegistrationCallbacks* callbacks);
  // Corresponds to navigator.serviceWorker.getRegistrations().
  void GetRegistrations(
      int provider_id,
      WebServiceWorkerGetRegistrationsCallbacks* callbacks);

  void GetRegistrationForReady(
      int provider_id,
      WebServiceWorkerGetRegistrationForReadyCallbacks* callbacks);

  // Called when a new provider context for a document is created. Usually
  // this happens when a new document is being loaded, and is called much
  // earlier than AddScriptClient.
  // (This is attached only to the document thread's ServiceWorkerDispatcher)
  void AddProviderContext(ServiceWorkerProviderContext* provider_context);
  void RemoveProviderContext(ServiceWorkerProviderContext* provider_context);

  // Called when navigator.serviceWorker is instantiated or detached
  // for a document whose provider can be identified by |provider_id|.
  void AddProviderClient(int provider_id,
                         blink::WebServiceWorkerProviderClient* client);
  void RemoveProviderClient(int provider_id);

  // If an existing WebServiceWorkerImpl exists for the Service
  // Worker, it is returned; otherwise a WebServiceWorkerImpl is
  // created and its ownership is transferred to the caller. If
  // |adopt_handle| is true, a ServiceWorkerHandleReference will be
  // adopted for the specified Service Worker.
  //
  // TODO(dominicc): The lifetime of WebServiceWorkerImpl is too tricky; this
  // method can return an existing WebServiceWorkerImpl, in which case
  // it is owned by a WebCore::ServiceWorker and the lifetime is not
  // being transferred to the owner; or it can create a
  // WebServiceWorkerImpl, in which case ownership is transferred to
  // the caller who must bounce it to a method that will associate it
  // with a WebCore::ServiceWorker.
  WebServiceWorkerImpl* GetServiceWorker(
      const ServiceWorkerObjectInfo& info,
      bool adopt_handle);

  // Creates a WebServiceWorkerRegistrationImpl for the specified registration
  // and transfers its ownership to the caller. If |adopt_handle| is true, a
  // ServiceWorkerRegistrationHandleReference will be adopted for the
  // registration.
  WebServiceWorkerRegistrationImpl* CreateServiceWorkerRegistration(
      const ServiceWorkerRegistrationObjectInfo& info,
      bool adopt_handle);

  // |thread_safe_sender| needs to be passed in because if the call leads to
  // construction it will be needed.
  static ServiceWorkerDispatcher* GetOrCreateThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender);

  // Unlike GetOrCreateThreadSpecificInstance() this doesn't create a new
  // instance if thread-local instance doesn't exist.
  static ServiceWorkerDispatcher* GetThreadSpecificInstance();

 private:
  typedef IDMap<WebServiceWorkerRegistrationCallbacks,
      IDMapOwnPointer> RegistrationCallbackMap;
  typedef IDMap<WebServiceWorkerUnregistrationCallbacks,
      IDMapOwnPointer> UnregistrationCallbackMap;
  typedef IDMap<WebServiceWorkerGetRegistrationCallbacks,
      IDMapOwnPointer> GetRegistrationCallbackMap;
  typedef IDMap<WebServiceWorkerGetRegistrationsCallbacks,
      IDMapOwnPointer> GetRegistrationsCallbackMap;
  typedef IDMap<WebServiceWorkerGetRegistrationForReadyCallbacks,
      IDMapOwnPointer> GetRegistrationForReadyCallbackMap;

  typedef std::map<int, blink::WebServiceWorkerProviderClient*>
      ProviderClientMap;
  typedef std::map<int, ServiceWorkerProviderContext*> ProviderContextMap;
  typedef std::map<int, ServiceWorkerProviderContext*> WorkerToProviderMap;
  typedef std::map<int, WebServiceWorkerImpl*> WorkerObjectMap;
  typedef std::map<int, WebServiceWorkerRegistrationImpl*>
      RegistrationObjectMap;

  friend class ServiceWorkerDispatcherTest;
  friend class WebServiceWorkerImpl;
  friend class WebServiceWorkerRegistrationImpl;

  // WorkerTaskRunner::Observer implementation.
  void OnWorkerRunLoopStopped() override;

  void OnAssociateRegistrationWithServiceWorker(
      int thread_id,
      int provider_id,
      const ServiceWorkerRegistrationObjectInfo& info,
      const ServiceWorkerVersionAttributes& attrs);
  void OnAssociateRegistration(int thread_id,
                               int provider_id,
                               const ServiceWorkerRegistrationObjectInfo& info,
                               const ServiceWorkerVersionAttributes& attrs);
  void OnDisassociateRegistration(int thread_id,
                                  int provider_id);
  void OnRegistered(int thread_id,
                    int request_id,
                    const ServiceWorkerRegistrationObjectInfo& info,
                    const ServiceWorkerVersionAttributes& attrs);
  void OnUnregistered(int thread_id,
                      int request_id,
                      bool is_success);
  void OnDidGetRegistration(int thread_id,
                            int request_id,
                            const ServiceWorkerRegistrationObjectInfo& info,
                            const ServiceWorkerVersionAttributes& attrs);
  void OnDidGetRegistrations(
      int thread_id,
      int request_id,
      const std::vector<ServiceWorkerRegistrationObjectInfo>& infos,
      const std::vector<ServiceWorkerVersionAttributes>& attrs);
  void OnDidGetRegistrationForReady(
      int thread_id,
      int request_id,
      const ServiceWorkerRegistrationObjectInfo& info,
      const ServiceWorkerVersionAttributes& attrs);
  void OnRegistrationError(int thread_id,
                           int request_id,
                           blink::WebServiceWorkerError::ErrorType error_type,
                           const base::string16& message);
  void OnUnregistrationError(int thread_id,
                             int request_id,
                             blink::WebServiceWorkerError::ErrorType error_type,
                             const base::string16& message);
  void OnGetRegistrationError(
      int thread_id,
      int request_id,
      blink::WebServiceWorkerError::ErrorType error_type,
      const base::string16& message);
  void OnGetRegistrationsError(
      int thread_id,
      int request_id,
      blink::WebServiceWorkerError::ErrorType error_type,
      const base::string16& message);
  void OnServiceWorkerStateChanged(int thread_id,
                                   int handle_id,
                                   blink::WebServiceWorkerState state);
  void OnSetVersionAttributes(int thread_id,
                              int provider_id,
                              int registration_handle_id,
                              int changed_mask,
                              const ServiceWorkerVersionAttributes& attributes);
  void OnUpdateFound(int thread_id,
                     int registration_handle_id);
  void OnSetControllerServiceWorker(int thread_id,
                                    int provider_id,
                                    const ServiceWorkerObjectInfo& info,
                                    bool should_notify_controllerchange);
  void OnPostMessage(const ServiceWorkerMsg_MessageToDocument_Params& params);

  // Keeps map from handle_id to ServiceWorker object.
  void AddServiceWorker(int handle_id, WebServiceWorkerImpl* worker);
  void RemoveServiceWorker(int handle_id);

  // Keeps map from registration_handle_id to ServiceWorkerRegistration object.
  void AddServiceWorkerRegistration(
      int registration_handle_id,
      WebServiceWorkerRegistrationImpl* registration);
  void RemoveServiceWorkerRegistration(
      int registration_handle_id);

  // Returns an existing registration or new one filled in with version
  // attributes. This function assumes given |info| and |attrs| retain handle
  // references and always adopts them.
  // TODO(nhiroki): This assumption seems to impair readability. We could
  // explictly pass ServiceWorker(Registration)HandleReference instead.
  WebServiceWorkerRegistrationImpl* FindOrCreateRegistration(
      const ServiceWorkerRegistrationObjectInfo& info,
      const ServiceWorkerVersionAttributes& attrs);

  RegistrationCallbackMap pending_registration_callbacks_;
  UnregistrationCallbackMap pending_unregistration_callbacks_;
  GetRegistrationCallbackMap pending_get_registration_callbacks_;
  GetRegistrationsCallbackMap pending_get_registrations_callbacks_;
  GetRegistrationForReadyCallbackMap get_for_ready_callbacks_;

  ProviderClientMap provider_clients_;
  ProviderContextMap provider_contexts_;

  WorkerObjectMap service_workers_;
  RegistrationObjectMap registrations_;

  // A map for ServiceWorkers that are associated to a particular document
  // (e.g. as .current).
  WorkerToProviderMap worker_to_provider_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_
