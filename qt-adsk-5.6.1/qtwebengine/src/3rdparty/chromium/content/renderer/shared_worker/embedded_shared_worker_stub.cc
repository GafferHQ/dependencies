// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/shared_worker/embedded_shared_worker_stub.h"

#include "base/thread_task_runner_handle.h"
#include "content/child/appcache/appcache_dispatcher.h"
#include "content/child/appcache/web_application_cache_host_impl.h"
#include "content/child/request_extra_data.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_network_provider.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/shared_worker_devtools_agent.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/worker_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/shared_worker/embedded_shared_worker_content_settings_client_proxy.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebServiceWorkerNetworkProvider.h"
#include "third_party/WebKit/public/web/WebSharedWorker.h"
#include "third_party/WebKit/public/web/WebSharedWorkerClient.h"

namespace content {

namespace {

class SharedWorkerWebApplicationCacheHostImpl
    : public WebApplicationCacheHostImpl {
 public:
  SharedWorkerWebApplicationCacheHostImpl(
      blink::WebApplicationCacheHostClient* client)
      : WebApplicationCacheHostImpl(client,
                                    RenderThreadImpl::current()
                                        ->appcache_dispatcher()
                                        ->backend_proxy()) {}

  // Main resource loading is different for workers. The main resource is
  // loaded by the worker using WorkerScriptLoader.
  // These overrides are stubbed out.
  virtual void willStartMainResourceRequest(
      blink::WebURLRequest&,
      const blink::WebApplicationCacheHost*) {}
  virtual void didReceiveResponseForMainResource(const blink::WebURLResponse&) {
  }
  virtual void didReceiveDataForMainResource(const char* data, unsigned len) {}
  virtual void didFinishLoadingMainResource(bool success) {}

  // Cache selection is also different for workers. We know at construction
  // time what cache to select and do so then.
  // These overrides are stubbed out.
  virtual void selectCacheWithoutManifest() {}
  virtual bool selectCacheWithManifest(const blink::WebURL& manifestURL) {
    return true;
  }
};

// We store an instance of this class in the "extra data" of the WebDataSource
// and attach a ServiceWorkerNetworkProvider to it as base::UserData.
// (see createServiceWorkerNetworkProvider).
class DataSourceExtraData
    : public blink::WebDataSource::ExtraData,
      public base::SupportsUserData {
 public:
  DataSourceExtraData() {}
  virtual ~DataSourceExtraData() {}
};

// Called on the main thread only and blink owns it.
class WebServiceWorkerNetworkProviderImpl
    : public blink::WebServiceWorkerNetworkProvider {
 public:
  // Blink calls this method for each request starting with the main script,
  // we tag them with the provider id.
  virtual void willSendRequest(
      blink::WebDataSource* data_source,
      blink::WebURLRequest& request) {
    ServiceWorkerNetworkProvider* provider =
        GetNetworkProviderFromDataSource(data_source);
    scoped_ptr<RequestExtraData> extra_data(new RequestExtraData);
    extra_data->set_service_worker_provider_id(provider->provider_id());
    request.setExtraData(extra_data.release());
  }

  virtual bool isControlledByServiceWorker(
      blink::WebDataSource& data_source) {
    ServiceWorkerNetworkProvider* provider =
        GetNetworkProviderFromDataSource(&data_source);
    return provider->context()->controller_handle_id() !=
        kInvalidServiceWorkerHandleId;
  }

  virtual int64_t serviceWorkerID(
      blink::WebDataSource& data_source) {
    ServiceWorkerNetworkProvider* provider =
        GetNetworkProviderFromDataSource(&data_source);
    if (provider->context()->controller())
      return provider->context()->controller()->version_id();
    return kInvalidServiceWorkerVersionId;
  }

 private:
  ServiceWorkerNetworkProvider* GetNetworkProviderFromDataSource(
      const blink::WebDataSource* data_source) {
    return ServiceWorkerNetworkProvider::FromDocumentState(
        static_cast<DataSourceExtraData*>(data_source->extraData()));
  }
};

}  // namespace

EmbeddedSharedWorkerStub::EmbeddedSharedWorkerStub(
    const GURL& url,
    const base::string16& name,
    const base::string16& content_security_policy,
    blink::WebContentSecurityPolicyType security_policy_type,
    bool pause_on_start,
    int route_id)
    : route_id_(route_id),
      name_(name),
      url_(url) {
  RenderThreadImpl::current()->AddEmbeddedWorkerRoute(route_id_, this);
  impl_ = blink::WebSharedWorker::create(this);
  if (pause_on_start) {
    // Pause worker context when it starts and wait until either DevTools client
    // is attached or explicit resume notification is received.
    impl_->pauseWorkerContextOnStart();
  }
  worker_devtools_agent_.reset(
      new SharedWorkerDevToolsAgent(route_id, impl_));
  impl_->startWorkerContext(url, name_,
                            content_security_policy, security_policy_type);
}

EmbeddedSharedWorkerStub::~EmbeddedSharedWorkerStub() {
  RenderThreadImpl::current()->RemoveEmbeddedWorkerRoute(route_id_);
  DCHECK(!impl_);
}

bool EmbeddedSharedWorkerStub::OnMessageReceived(
    const IPC::Message& message) {
  if (worker_devtools_agent_->OnMessageReceived(message))
    return true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedSharedWorkerStub, message)
    IPC_MESSAGE_HANDLER(WorkerMsg_TerminateWorkerContext,
                        OnTerminateWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_Connect, OnConnect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void EmbeddedSharedWorkerStub::OnChannelError() {
  OnTerminateWorkerContext();
}

void EmbeddedSharedWorkerStub::workerReadyForInspection() {
  Send(new WorkerHostMsg_WorkerReadyForInspection(route_id_));
}

void EmbeddedSharedWorkerStub::workerScriptLoaded() {
  Send(new WorkerHostMsg_WorkerScriptLoaded(route_id_));
  running_ = true;
  // Process any pending connections.
  for (PendingChannelList::const_iterator iter = pending_channels_.begin();
       iter != pending_channels_.end();
       ++iter) {
    ConnectToChannel(*iter);
  }
  pending_channels_.clear();
}

void EmbeddedSharedWorkerStub::workerScriptLoadFailed() {
  Send(new WorkerHostMsg_WorkerScriptLoadFailed(route_id_));
  for (PendingChannelList::const_iterator iter = pending_channels_.begin();
       iter != pending_channels_.end();
       ++iter) {
    blink::WebMessagePortChannel* channel = *iter;
    channel->destroy();
  }
  pending_channels_.clear();
  Shutdown();
}

void EmbeddedSharedWorkerStub::workerContextClosed() {
  Send(new WorkerHostMsg_WorkerContextClosed(route_id_));
}

void EmbeddedSharedWorkerStub::workerContextDestroyed() {
  Send(new WorkerHostMsg_WorkerContextDestroyed(route_id_));
  Shutdown();
}

void EmbeddedSharedWorkerStub::selectAppCacheID(long long app_cache_id) {
  if (app_cache_host_) {
    // app_cache_host_ could become stale as it's owned by blink's
    // DocumentLoader. This method is assumed to be called while it's valid.
    app_cache_host_->backend()->SelectCacheForSharedWorker(
        app_cache_host_->host_id(), app_cache_id);
  }
}

blink::WebNotificationPresenter*
EmbeddedSharedWorkerStub::notificationPresenter() {
  // TODO(horo): delete this method if we have no plan to implement this.
  NOTREACHED();
  return NULL;
}

blink::WebApplicationCacheHost*
EmbeddedSharedWorkerStub::createApplicationCacheHost(
    blink::WebApplicationCacheHostClient* client) {
  app_cache_host_ = new SharedWorkerWebApplicationCacheHostImpl(client);
  return app_cache_host_;
}

blink::WebWorkerContentSettingsClientProxy*
    EmbeddedSharedWorkerStub::createWorkerContentSettingsClientProxy(
    const blink::WebSecurityOrigin& origin) {
  return new EmbeddedSharedWorkerContentSettingsClientProxy(
      GURL(origin.toString()),
      origin.isUnique(),
      route_id_,
      ChildThreadImpl::current()->thread_safe_sender());
}

blink::WebServiceWorkerNetworkProvider*
EmbeddedSharedWorkerStub::createServiceWorkerNetworkProvider(
    blink::WebDataSource* data_source) {
  // Create a content::ServiceWorkerNetworkProvider for this data source so
  // we can observe its requests.
  scoped_ptr<ServiceWorkerNetworkProvider> provider(
      new ServiceWorkerNetworkProvider(
          route_id_, SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER));

  // The provider is kept around for the lifetime of the DataSource
  // and ownership is transferred to the DataSource.
  DataSourceExtraData* extra_data = new DataSourceExtraData();
  data_source->setExtraData(extra_data);
  ServiceWorkerNetworkProvider::AttachToDocumentState(
      extra_data, provider.Pass());

  // Blink is responsible for deleting the returned object.
  return new WebServiceWorkerNetworkProviderImpl();
}

void EmbeddedSharedWorkerStub::sendDevToolsMessage(
    int call_id,
    const blink::WebString& message,
    const blink::WebString& state) {
  worker_devtools_agent_->SendDevToolsMessage(call_id, message, state);
}

void EmbeddedSharedWorkerStub::Shutdown() {
  // WebSharedWorker must be already deleted in the blink side
  // when this is called.
  impl_ = nullptr;
  delete this;
}

bool EmbeddedSharedWorkerStub::Send(IPC::Message* message) {
  return RenderThreadImpl::current()->Send(message);
}

void EmbeddedSharedWorkerStub::ConnectToChannel(
    WebMessagePortChannelImpl* channel) {
  impl_->connect(channel);
  Send(
      new WorkerHostMsg_WorkerConnected(channel->message_port_id(), route_id_));
}

void EmbeddedSharedWorkerStub::OnConnect(int sent_message_port_id,
                                         int routing_id) {
  TransferredMessagePort port;
  port.id = sent_message_port_id;
  WebMessagePortChannelImpl* channel = new WebMessagePortChannelImpl(
      routing_id, port, base::ThreadTaskRunnerHandle::Get().get());
  if (running_) {
    ConnectToChannel(channel);
  } else {
    // If two documents try to load a SharedWorker at the same time, the
    // WorkerMsg_Connect for one of the documents can come in before the
    // worker is started. Just queue up the connect and deliver it once the
    // worker starts.
    pending_channels_.push_back(channel);
  }
}

void EmbeddedSharedWorkerStub::OnTerminateWorkerContext() {
  // After this we wouldn't get any IPC for this stub.
  running_ = false;
  impl_->terminateWorkerContext();
}

}  // namespace content
