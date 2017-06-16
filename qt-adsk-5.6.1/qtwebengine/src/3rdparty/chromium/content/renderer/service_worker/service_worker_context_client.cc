// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_context_client.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/trace_event.h"
#include "content/child/notifications/notification_data_conversions.h"
#include "content/child/request_extra_data.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_network_provider.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_provider_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/child/worker_task_runner.h"
#include "content/common/devtools_messages.h"
#include "content/common/message_port_messages.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/document_state.h"
#include "content/renderer/devtools/devtools_agent.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/embedded_worker_dispatcher.h"
#include "content/renderer/service_worker/service_worker_type_util.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebCrossOriginServiceWorkerClient.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerClientQueryOptions.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerResponse.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationData.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebServiceWorkerContextClient.h"
#include "third_party/WebKit/public/web/WebServiceWorkerContextProxy.h"
#include "third_party/WebKit/public/web/WebServiceWorkerNetworkProvider.h"

namespace content {

namespace {

// For now client must be a per-thread instance.
base::LazyInstance<base::ThreadLocalPointer<ServiceWorkerContextClient>>::
    Leaky g_worker_client_tls = LAZY_INSTANCE_INITIALIZER;

void CallWorkerContextDestroyedOnMainThread(int embedded_worker_id) {
  if (!RenderThreadImpl::current() ||
      !RenderThreadImpl::current()->embedded_worker_dispatcher())
    return;
  RenderThreadImpl::current()->embedded_worker_dispatcher()->
      WorkerContextDestroyed(embedded_worker_id);
}

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
        ServiceWorkerNetworkProvider::FromDocumentState(
            static_cast<DataSourceExtraData*>(data_source->extraData()));
    scoped_ptr<RequestExtraData> extra_data(new RequestExtraData);
    extra_data->set_service_worker_provider_id(provider->provider_id());
    request.setExtraData(extra_data.release());
  }
};

void SendPostMessageToClientOnMainThread(
    ThreadSafeSender* sender,
    int routing_id,
    const std::string& uuid,
    const base::string16& message,
    scoped_ptr<blink::WebMessagePortChannelArray> channels) {
  sender->Send(new ServiceWorkerHostMsg_PostMessageToClient(
      routing_id, uuid, message,
      WebMessagePortChannelImpl::ExtractMessagePortIDs(channels.release())));
}

void SendCrossOriginMessageToClientOnMainThread(
    ThreadSafeSender* sender,
    int message_port_id,
    const base::string16& message,
    scoped_ptr<blink::WebMessagePortChannelArray> channels) {
  sender->Send(new MessagePortHostMsg_PostMessage(
      message_port_id,
      MessagePortMessage(message),
                         WebMessagePortChannelImpl::ExtractMessagePortIDs(
                             channels.release())));
}

void StashMessagePortOnMainThread(ThreadSafeSender* sender,
                                  int routing_id,
                                  WebMessagePortChannelImpl* channel,
                                  const base::string16& name) {
  DCHECK_GE(channel->message_port_id(), 0);
  channel->set_is_stashed();
  sender->Send(new ServiceWorkerHostMsg_StashMessagePort(
      routing_id, channel->message_port_id(), name));
}

blink::WebURLRequest::FetchRequestMode GetBlinkFetchRequestMode(
    FetchRequestMode mode) {
  return static_cast<blink::WebURLRequest::FetchRequestMode>(mode);
}

blink::WebURLRequest::FetchCredentialsMode GetBlinkFetchCredentialsMode(
    FetchCredentialsMode credentials_mode) {
  return static_cast<blink::WebURLRequest::FetchCredentialsMode>(
      credentials_mode);
}

blink::WebURLRequest::RequestContext GetBlinkRequestContext(
    RequestContextType request_context_type) {
  return static_cast<blink::WebURLRequest::RequestContext>(
      request_context_type);
}

blink::WebURLRequest::FrameType GetBlinkFrameType(
    RequestContextFrameType frame_type) {
  return static_cast<blink::WebURLRequest::FrameType>(frame_type);
}

blink::WebServiceWorkerClientInfo
ToWebServiceWorkerClientInfo(const ServiceWorkerClientInfo& client_info) {
  DCHECK(client_info.IsValid());

  blink::WebServiceWorkerClientInfo web_client_info;

  web_client_info.uuid = base::UTF8ToUTF16(client_info.client_uuid);
  web_client_info.pageVisibilityState = client_info.page_visibility_state;
  web_client_info.isFocused = client_info.is_focused;
  web_client_info.url = client_info.url;
  web_client_info.frameType = GetBlinkFrameType(client_info.frame_type);
  web_client_info.clientType = client_info.client_type;

  return web_client_info;
}

}  // namespace

// Holding data that needs to be bound to the worker context on the
// worker thread.
struct ServiceWorkerContextClient::WorkerContextData {
  using ClientsCallbacksMap =
      IDMap<blink::WebServiceWorkerClientsCallbacks, IDMapOwnPointer>;
  using ClaimClientsCallbacksMap =
      IDMap<blink::WebServiceWorkerClientsClaimCallbacks, IDMapOwnPointer>;
  using ClientCallbacksMap =
      IDMap<blink::WebServiceWorkerClientCallbacks, IDMapOwnPointer>;
  using SkipWaitingCallbacksMap =
      IDMap<blink::WebServiceWorkerSkipWaitingCallbacks, IDMapOwnPointer>;

  explicit WorkerContextData(ServiceWorkerContextClient* owner)
      : weak_factory(owner) {}

  ~WorkerContextData() {
    DCHECK(thread_checker.CalledOnValidThread());
  }

  // Pending callbacks for GetClientDocuments().
  ClientsCallbacksMap clients_callbacks;

  // Pending callbacks for OpenWindow() and FocusClient().
  ClientCallbacksMap client_callbacks;

  // Pending callbacks for SkipWaiting().
  SkipWaitingCallbacksMap skip_waiting_callbacks;

  // Pending callbacks for ClaimClients().
  ClaimClientsCallbacksMap claim_clients_callbacks;

  base::ThreadChecker thread_checker;
  base::WeakPtrFactory<ServiceWorkerContextClient> weak_factory;
};

ServiceWorkerContextClient*
ServiceWorkerContextClient::ThreadSpecificInstance() {
  return g_worker_client_tls.Pointer()->Get();
}

ServiceWorkerContextClient::ServiceWorkerContextClient(
    int embedded_worker_id,
    int64 service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url,
    int worker_devtools_agent_route_id)
    : embedded_worker_id_(embedded_worker_id),
      service_worker_version_id_(service_worker_version_id),
      service_worker_scope_(service_worker_scope),
      script_url_(script_url),
      worker_devtools_agent_route_id_(worker_devtools_agent_route_id),
      sender_(ChildThreadImpl::current()->thread_safe_sender()),
      main_thread_task_runner_(RenderThreadImpl::current()->GetTaskRunner()),
      proxy_(nullptr) {
  TRACE_EVENT_ASYNC_BEGIN0("ServiceWorker",
                           "ServiceWorkerContextClient::StartingWorkerContext",
                           this);
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerContextClient::StartingWorkerContext",
      this,
      "PrepareWorker");
}

ServiceWorkerContextClient::~ServiceWorkerContextClient() {}

void ServiceWorkerContextClient::OnMessageReceived(
    int thread_id,
    int embedded_worker_id,
    const IPC::Message& message) {
  CHECK_EQ(embedded_worker_id_, embedded_worker_id);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerContextClient, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ActivateEvent, OnActivateEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_FetchEvent, OnFetchEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_InstallEvent, OnInstallEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SyncEvent, OnSyncEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_NotificationClickEvent,
                        OnNotificationClickEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_PushEvent, OnPushEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_GeofencingEvent, OnGeofencingEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CrossOriginConnectEvent,
                        OnCrossOriginConnectEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_MessageToWorker, OnPostMessage)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CrossOriginMessageToWorker,
                        OnCrossOriginMessageToWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SendStashedMessagePorts,
                        OnSendStashedMessagePorts)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetClients, OnDidGetClients)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_OpenWindowResponse,
                        OnOpenWindowResponse)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_OpenWindowError,
                        OnOpenWindowError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_FocusClientResponse,
                        OnFocusClientResponse)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidSkipWaiting, OnDidSkipWaiting)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidClaimClients, OnDidClaimClients)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ClaimClientsError, OnClaimClientsError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_Ping, OnPing);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
}

blink::WebURL ServiceWorkerContextClient::scope() const {
  return service_worker_scope_;
}

void ServiceWorkerContextClient::getClients(
    const blink::WebServiceWorkerClientQueryOptions& weboptions,
    blink::WebServiceWorkerClientsCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = context_->clients_callbacks.Add(callbacks);
  ServiceWorkerClientQueryOptions options;
  options.client_type = weboptions.clientType;
  options.include_uncontrolled = weboptions.includeUncontrolled;
  Send(new ServiceWorkerHostMsg_GetClients(
      GetRoutingID(), request_id, options));
}

void ServiceWorkerContextClient::openWindow(
    const blink::WebURL& url,
    blink::WebServiceWorkerClientCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = context_->client_callbacks.Add(callbacks);
  Send(new ServiceWorkerHostMsg_OpenWindow(
      GetRoutingID(), request_id, url));
}

void ServiceWorkerContextClient::setCachedMetadata(const blink::WebURL& url,
                                                    const char* data,
                                                    size_t size) {
  std::vector<char> copy(data, data + size);
  Send(new ServiceWorkerHostMsg_SetCachedMetadata(GetRoutingID(), url, copy));
}

void ServiceWorkerContextClient::clearCachedMetadata(
    const blink::WebURL& url) {
  Send(new ServiceWorkerHostMsg_ClearCachedMetadata(GetRoutingID(), url));
}

void ServiceWorkerContextClient::workerReadyForInspection() {
  Send(new EmbeddedWorkerHostMsg_WorkerReadyForInspection(embedded_worker_id_));
}

void ServiceWorkerContextClient::workerContextFailedToStart() {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!proxy_);

  Send(new EmbeddedWorkerHostMsg_WorkerScriptLoadFailed(embedded_worker_id_));

  RenderThreadImpl::current()->embedded_worker_dispatcher()->
      WorkerContextDestroyed(embedded_worker_id_);
}

void ServiceWorkerContextClient::workerContextStarted(
    blink::WebServiceWorkerContextProxy* proxy) {
  DCHECK(!worker_task_runner_.get());
  DCHECK_NE(0, WorkerTaskRunner::Instance()->CurrentWorkerId());
  worker_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  // g_worker_client_tls.Pointer()->Get() could return NULL if this context
  // gets deleted before workerContextStarted() is called.
  DCHECK(g_worker_client_tls.Pointer()->Get() == NULL);
  DCHECK(!proxy_);
  g_worker_client_tls.Pointer()->Set(this);
  proxy_ = proxy;

  // Initialize pending callback maps. This needs to be freed on the
  // same thread before the worker context goes away in
  // willDestroyWorkerContext.
  context_.reset(new WorkerContextData(this));

  SetRegistrationInServiceWorkerGlobalScope();

  Send(new EmbeddedWorkerHostMsg_WorkerScriptLoaded(
      embedded_worker_id_,
      WorkerTaskRunner::Instance()->CurrentWorkerId(),
      provider_context_->provider_id()));

  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerContextClient::StartingWorkerContext",
      this,
      "ExecuteScript");
}

void ServiceWorkerContextClient::didEvaluateWorkerScript(bool success) {
  Send(new EmbeddedWorkerHostMsg_WorkerScriptEvaluated(
      embedded_worker_id_, success));

  // Schedule a task to send back WorkerStarted asynchronously,
  // so that at the time we send it we can be sure that the
  // worker run loop has been started.
  worker_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ServiceWorkerContextClient::SendWorkerStarted,
                            GetWeakPtr()));
}

void ServiceWorkerContextClient::willDestroyWorkerContext() {
  // At this point OnWorkerRunLoopStopped is already called, so
  // worker_task_runner_->RunsTasksOnCurrentThread() returns false
  // (while we're still on the worker thread).
  proxy_ = NULL;

  // We have to clear callbacks now, as they need to be freed on the
  // same thread.
  context_.reset();

  // This also lets the message filter stop dispatching messages to
  // this client.
  g_worker_client_tls.Pointer()->Set(NULL);
}

void ServiceWorkerContextClient::workerContextDestroyed() {
  DCHECK(g_worker_client_tls.Pointer()->Get() == NULL);

  // Now we should be able to free the WebEmbeddedWorker container on the
  // main thread.
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CallWorkerContextDestroyedOnMainThread,
                 embedded_worker_id_));
}

void ServiceWorkerContextClient::reportException(
    const blink::WebString& error_message,
    int line_number,
    int column_number,
    const blink::WebString& source_url) {
  Send(new EmbeddedWorkerHostMsg_ReportException(
      embedded_worker_id_,
      error_message,
      line_number,
      column_number, GURL(source_url)));
}

void ServiceWorkerContextClient::reportConsoleMessage(
    int source,
    int level,
    const blink::WebString& message,
    int line_number,
    const blink::WebString& source_url) {
  EmbeddedWorkerHostMsg_ReportConsoleMessage_Params params;
  params.source_identifier = source;
  params.message_level = level;
  params.message = message;
  params.line_number = line_number;
  params.source_url = GURL(source_url);

  Send(new EmbeddedWorkerHostMsg_ReportConsoleMessage(
      embedded_worker_id_, params));
}

void ServiceWorkerContextClient::sendDevToolsMessage(
    int call_id,
    const blink::WebString& message,
    const blink::WebString& state_cookie) {
  DevToolsAgent::SendChunkedProtocolMessage(
      sender_.get(), worker_devtools_agent_route_id_,
      call_id, message.utf8(), state_cookie.utf8());
}

void ServiceWorkerContextClient::didHandleActivateEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.ActivateEvent.Time",
      base::TimeTicks::Now() - activate_start_timings_[request_id]);
  activate_start_timings_.erase(request_id);

  Send(new ServiceWorkerHostMsg_ActivateEventFinished(
      GetRoutingID(), request_id, result));
}

void ServiceWorkerContextClient::didHandleInstallEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.InstallEvent.Time",
      base::TimeTicks::Now() - install_start_timings_[request_id]);
  install_start_timings_.erase(request_id);

  Send(new ServiceWorkerHostMsg_InstallEventFinished(
      GetRoutingID(), request_id, result));
}

void ServiceWorkerContextClient::didHandleFetchEvent(int request_id) {
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.FetchEventExecutionTime",
      base::TimeTicks::Now() - fetch_start_timings_[request_id]);
  fetch_start_timings_.erase(request_id);
  Send(new ServiceWorkerHostMsg_FetchEventFinished(
      GetRoutingID(), request_id,
      SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
      ServiceWorkerResponse()));
}

void ServiceWorkerContextClient::didHandleFetchEvent(
    int request_id,
    const blink::WebServiceWorkerResponse& web_response) {
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.FetchEvent.Time",
      base::TimeTicks::Now() - fetch_start_timings_[request_id]);
  fetch_start_timings_.erase(request_id);

  ServiceWorkerHeaderMap headers;
  GetServiceWorkerHeaderMapFromWebResponse(web_response, &headers);
  ServiceWorkerResponse response(
      web_response.url(), web_response.status(),
      web_response.statusText().utf8(), web_response.responseType(), headers,
      web_response.blobUUID().utf8(), web_response.blobSize(),
      web_response.streamURL(), web_response.error());
  Send(new ServiceWorkerHostMsg_FetchEventFinished(
      GetRoutingID(), request_id,
      SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
      response));
}

void ServiceWorkerContextClient::didHandleNotificationClickEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.NotificationClickEvent.Time",
      base::TimeTicks::Now() - notification_click_start_timings_[request_id]);
  notification_click_start_timings_.erase(request_id);

  Send(new ServiceWorkerHostMsg_NotificationClickEventFinished(
      GetRoutingID(), request_id));
}

void ServiceWorkerContextClient::didHandlePushEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  if (result == blink::WebServiceWorkerEventResultCompleted) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "ServiceWorker.PushEvent.Time",
        base::TimeTicks::Now() - push_start_timings_[request_id]);
  }
  push_start_timings_.erase(request_id);

  Send(new ServiceWorkerHostMsg_PushEventFinished(
      GetRoutingID(), request_id, result));
}

void ServiceWorkerContextClient::didHandleSyncEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  Send(new ServiceWorkerHostMsg_SyncEventFinished(GetRoutingID(), request_id,
                                                  result));
}

void ServiceWorkerContextClient::didHandleCrossOriginConnectEvent(
    int request_id,
    bool accept_connection) {
  Send(new ServiceWorkerHostMsg_CrossOriginConnectEventFinished(
      GetRoutingID(), request_id, accept_connection));
}

blink::WebServiceWorkerNetworkProvider*
ServiceWorkerContextClient::createServiceWorkerNetworkProvider(
    blink::WebDataSource* data_source) {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());

  // Create a content::ServiceWorkerNetworkProvider for this data source so
  // we can observe its requests.
  scoped_ptr<ServiceWorkerNetworkProvider> provider(
      new ServiceWorkerNetworkProvider(
          MSG_ROUTING_NONE, SERVICE_WORKER_PROVIDER_FOR_CONTROLLER));
  provider_context_ = provider->context();

  // Tell the network provider about which version to load.
  provider->SetServiceWorkerVersionId(service_worker_version_id_);

  // The provider is kept around for the lifetime of the DataSource
  // and ownership is transferred to the DataSource.
  DataSourceExtraData* extra_data = new DataSourceExtraData();
  data_source->setExtraData(extra_data);
  ServiceWorkerNetworkProvider::AttachToDocumentState(
      extra_data, provider.Pass());

  // Blink is responsible for deleting the returned object.
  return new WebServiceWorkerNetworkProviderImpl();
}

blink::WebServiceWorkerProvider*
ServiceWorkerContextClient::createServiceWorkerProvider() {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(provider_context_);

  // Blink is responsible for deleting the returned object.
  return new WebServiceWorkerProviderImpl(
      sender_.get(), provider_context_.get());
}

void ServiceWorkerContextClient::postMessageToClient(
    const blink::WebString& uuid,
    const blink::WebString& message,
    blink::WebMessagePortChannelArray* channels) {
  // This may send channels for MessagePorts, and all internal book-keeping
  // messages for MessagePort (e.g. QueueMessages) are sent from main thread
  // (with thread hopping), so we need to do the same thread hopping here not
  // to overtake those messages.
  scoped_ptr<blink::WebMessagePortChannelArray> channel_array(channels);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SendPostMessageToClientOnMainThread,
                 sender_,
                 GetRoutingID(),
                 base::UTF16ToUTF8(uuid),
                 static_cast<base::string16>(message),
                 base::Passed(&channel_array)));
}

void ServiceWorkerContextClient::postMessageToCrossOriginClient(
    const blink::WebCrossOriginServiceWorkerClient& client,
    const blink::WebString& message,
    blink::WebMessagePortChannelArray* channels) {
  // This may send channels for MessagePorts, and all internal book-keeping
  // messages for MessagePort (e.g. QueueMessages) are sent from main thread
  // (with thread hopping), so we need to do the same thread hopping here not
  // to overtake those messages.
  scoped_ptr<blink::WebMessagePortChannelArray> channel_array(channels);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SendCrossOriginMessageToClientOnMainThread,
                 sender_, client.clientID,
                 static_cast<base::string16>(message),
                 base::Passed(&channel_array)));
}

void ServiceWorkerContextClient::focus(
    const blink::WebString& uuid,
    blink::WebServiceWorkerClientCallbacks* callback) {
  DCHECK(callback);
  int request_id = context_->client_callbacks.Add(callback);
  Send(new ServiceWorkerHostMsg_FocusClient(GetRoutingID(), request_id,
                                            base::UTF16ToUTF8(uuid)));
}

void ServiceWorkerContextClient::skipWaiting(
    blink::WebServiceWorkerSkipWaitingCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = context_->skip_waiting_callbacks.Add(callbacks);
  Send(new ServiceWorkerHostMsg_SkipWaiting(GetRoutingID(), request_id));
}

void ServiceWorkerContextClient::claim(
    blink::WebServiceWorkerClientsClaimCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = context_->claim_clients_callbacks.Add(callbacks);
  Send(new ServiceWorkerHostMsg_ClaimClients(GetRoutingID(), request_id));
}

void ServiceWorkerContextClient::stashMessagePort(
    blink::WebMessagePortChannel* channel,
    const blink::WebString& name) {
  // All internal book-keeping messages for MessagePort are sent from main
  // thread (with thread hopping), so we need to do the same thread hopping here
  // not to overtake those messages.
  WebMessagePortChannelImpl* channel_impl =
      static_cast<WebMessagePortChannelImpl*>(channel);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&StashMessagePortOnMainThread,
                 sender_, GetRoutingID(),
                 base::Unretained(channel_impl),
                 static_cast<base::string16>(name)));
}

void ServiceWorkerContextClient::Send(IPC::Message* message) {
  sender_->Send(message);
}

void ServiceWorkerContextClient::SendWorkerStarted() {
  DCHECK(worker_task_runner_->RunsTasksOnCurrentThread());
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerContextClient::StartingWorkerContext",
                         this);
  Send(new EmbeddedWorkerHostMsg_WorkerStarted(embedded_worker_id_));
}

void ServiceWorkerContextClient::SetRegistrationInServiceWorkerGlobalScope() {
  DCHECK(worker_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(provider_context_);

  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  bool found =
      provider_context_->GetRegistrationInfoAndVersionAttributes(&info, &attrs);
  if (!found)
    return;  // Cannot be associated with a registration in some tests.

  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(sender_.get());

  // Register a registration and its version attributes with the dispatcher
  // living on the worker thread.
  scoped_ptr<WebServiceWorkerRegistrationImpl> registration(
      dispatcher->CreateServiceWorkerRegistration(info, false));
  registration->SetInstalling(
      dispatcher->GetServiceWorker(attrs.installing, false));
  registration->SetWaiting(
      dispatcher->GetServiceWorker(attrs.waiting, false));
  registration->SetActive(
      dispatcher->GetServiceWorker(attrs.active, false));

  proxy_->setRegistration(registration.release());
}

void ServiceWorkerContextClient::OnActivateEvent(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnActivateEvent");
  activate_start_timings_[request_id] = base::TimeTicks::Now();
  proxy_->dispatchActivateEvent(request_id);
}

void ServiceWorkerContextClient::OnInstallEvent(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnInstallEvent");
  install_start_timings_[request_id] = base::TimeTicks::Now();
  proxy_->dispatchInstallEvent(request_id);
}

void ServiceWorkerContextClient::OnFetchEvent(
    int request_id,
    const ServiceWorkerFetchRequest& request) {
  blink::WebServiceWorkerRequest webRequest;
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnFetchEvent");
  webRequest.setURL(blink::WebURL(request.url));
  webRequest.setMethod(blink::WebString::fromUTF8(request.method));
  for (ServiceWorkerHeaderMap::const_iterator it = request.headers.begin();
       it != request.headers.end();
       ++it) {
    webRequest.setHeader(blink::WebString::fromUTF8(it->first),
                         blink::WebString::fromUTF8(it->second));
  }
  if (!request.blob_uuid.empty()) {
    webRequest.setBlob(blink::WebString::fromUTF8(request.blob_uuid),
                       request.blob_size);
  }
  webRequest.setReferrer(
      blink::WebString::fromUTF8(request.referrer.url.spec()),
      request.referrer.policy);
  webRequest.setMode(GetBlinkFetchRequestMode(request.mode));
  webRequest.setCredentialsMode(
      GetBlinkFetchCredentialsMode(request.credentials_mode));
  webRequest.setRequestContext(
      GetBlinkRequestContext(request.request_context_type));
  webRequest.setFrameType(GetBlinkFrameType(request.frame_type));
  webRequest.setIsReload(request.is_reload);
  fetch_start_timings_[request_id] = base::TimeTicks::Now();
  proxy_->dispatchFetchEvent(request_id, webRequest);
}

void ServiceWorkerContextClient::OnSyncEvent(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnSyncEvent");
  proxy_->dispatchSyncEvent(request_id);
}

void ServiceWorkerContextClient::OnNotificationClickEvent(
    int request_id,
    int64_t persistent_notification_id,
    const PlatformNotificationData& notification_data) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnNotificationClickEvent");
  notification_click_start_timings_[request_id] = base::TimeTicks::Now();
  proxy_->dispatchNotificationClickEvent(
      request_id,
      persistent_notification_id,
      ToWebNotificationData(notification_data));
}

void ServiceWorkerContextClient::OnPushEvent(int request_id,
                                             const std::string& data) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnPushEvent");
  push_start_timings_[request_id] = base::TimeTicks::Now();
  proxy_->dispatchPushEvent(request_id, blink::WebString::fromUTF8(data));
}

void ServiceWorkerContextClient::OnGeofencingEvent(
    int request_id,
    blink::WebGeofencingEventType event_type,
    const std::string& region_id,
    const blink::WebCircularGeofencingRegion& region) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnGeofencingEvent");
  proxy_->dispatchGeofencingEvent(
      request_id, event_type, blink::WebString::fromUTF8(region_id), region);
  Send(new ServiceWorkerHostMsg_GeofencingEventFinished(GetRoutingID(),
                                                        request_id));
}

void ServiceWorkerContextClient::OnCrossOriginConnectEvent(
    int request_id,
    const NavigatorConnectClient& client) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnCrossOriginConnectEvent");
  blink::WebCrossOriginServiceWorkerClient web_client;
  web_client.origin = client.origin;
  web_client.targetURL = client.target_url;
  web_client.clientID = client.message_port_id;
  proxy_->dispatchCrossOriginConnectEvent(request_id, web_client);
}

void ServiceWorkerContextClient::OnPostMessage(
    const base::string16& message,
    const std::vector<TransferredMessagePort>& sent_message_ports,
    const std::vector<int>& new_routing_ids) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnPostEvent");
  blink::WebMessagePortChannelArray ports =
      WebMessagePortChannelImpl::CreatePorts(
          sent_message_ports, new_routing_ids,
          main_thread_task_runner_);

  // dispatchMessageEvent is expected to execute onmessage function
  // synchronously.
  base::TimeTicks before = base::TimeTicks::Now();
  proxy_->dispatchMessageEvent(message, ports);
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.MessageEvent.Time",
      base::TimeTicks::Now() - before);
}

void ServiceWorkerContextClient::OnCrossOriginMessageToWorker(
    const NavigatorConnectClient& client,
    const base::string16& message,
    const std::vector<TransferredMessagePort>& sent_message_ports,
    const std::vector<int>& new_routing_ids) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnCrossOriginMessageToWorker");
  blink::WebMessagePortChannelArray ports =
      WebMessagePortChannelImpl::CreatePorts(
          sent_message_ports, new_routing_ids,
          main_thread_task_runner_);

  blink::WebCrossOriginServiceWorkerClient web_client;
  web_client.origin = client.origin;
  web_client.targetURL = client.target_url;
  web_client.clientID = client.message_port_id;
  proxy_->dispatchCrossOriginMessageEvent(web_client, message, ports);
}

void ServiceWorkerContextClient::OnSendStashedMessagePorts(
    const std::vector<TransferredMessagePort>& stashed_message_ports,
    const std::vector<int>& new_routing_ids,
    const std::vector<base::string16>& port_names) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnSendStashedMessagePorts");
  blink::WebMessagePortChannelArray ports =
      WebMessagePortChannelImpl::CreatePorts(
          stashed_message_ports, new_routing_ids, main_thread_task_runner_);
  for (blink::WebMessagePortChannel* port : ports)
    static_cast<WebMessagePortChannelImpl*>(port)->set_is_stashed();
  proxy_->addStashedMessagePorts(ports, port_names);
}

void ServiceWorkerContextClient::OnDidGetClients(
    int request_id, const std::vector<ServiceWorkerClientInfo>& clients) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnDidGetClients");
  blink::WebServiceWorkerClientsCallbacks* callbacks =
      context_->clients_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  scoped_ptr<blink::WebServiceWorkerClientsInfo> info(
      new blink::WebServiceWorkerClientsInfo);
  blink::WebVector<blink::WebServiceWorkerClientInfo> convertedClients(
      clients.size());
  for (size_t i = 0; i < clients.size(); ++i)
    convertedClients[i] = ToWebServiceWorkerClientInfo(clients[i]);
  info->clients.swap(convertedClients);
  callbacks->onSuccess(info.release());
  context_->clients_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnOpenWindowResponse(
    int request_id,
    const ServiceWorkerClientInfo& client) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnOpenWindowResponse");
  blink::WebServiceWorkerClientCallbacks* callbacks =
      context_->client_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  scoped_ptr<blink::WebServiceWorkerClientInfo> web_client;
  if (!client.IsEmpty()) {
    DCHECK(client.IsValid());
    web_client.reset(new blink::WebServiceWorkerClientInfo(
        ToWebServiceWorkerClientInfo(client)));
  }
  callbacks->onSuccess(web_client.release());
  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnOpenWindowError(
    int request_id,
    const std::string& message) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnOpenWindowError");
  blink::WebServiceWorkerClientCallbacks* callbacks =
      context_->client_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  scoped_ptr<blink::WebServiceWorkerError> error(
      new blink::WebServiceWorkerError(
          blink::WebServiceWorkerError::ErrorTypeUnknown,
          blink::WebString::fromUTF8(message)));
  callbacks->onError(error.release());
  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnFocusClientResponse(
    int request_id, const ServiceWorkerClientInfo& client) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnFocusClientResponse");
  blink::WebServiceWorkerClientCallbacks* callback =
      context_->client_callbacks.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  if (!client.IsEmpty()) {
    DCHECK(client.IsValid());
    scoped_ptr<blink::WebServiceWorkerClientInfo> web_client (
        new blink::WebServiceWorkerClientInfo(
            ToWebServiceWorkerClientInfo(client)));
    callback->onSuccess(web_client.release());
  } else {
    scoped_ptr<blink::WebServiceWorkerError> error(
        new blink::WebServiceWorkerError(
            blink::WebServiceWorkerError::ErrorTypeNotFound,
            "The WindowClient was not found."));
    callback->onError(error.release());
  }

  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnDidSkipWaiting(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnDidSkipWaiting");
  blink::WebServiceWorkerSkipWaitingCallbacks* callbacks =
      context_->skip_waiting_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  callbacks->onSuccess();
  context_->skip_waiting_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnDidClaimClients(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnDidClaimClients");
  blink::WebServiceWorkerClientsClaimCallbacks* callbacks =
      context_->claim_clients_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  callbacks->onSuccess();
  context_->claim_clients_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnClaimClientsError(
    int request_id,
    blink::WebServiceWorkerError::ErrorType error_type,
    const base::string16& message) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnClaimClientsError");
  blink::WebServiceWorkerClientsClaimCallbacks* callbacks =
      context_->claim_clients_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  scoped_ptr<blink::WebServiceWorkerError> error(
      new blink::WebServiceWorkerError(error_type, message));
  callbacks->onError(error.release());
  context_->claim_clients_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnPing() {
  Send(new ServiceWorkerHostMsg_Pong(GetRoutingID()));
}

base::WeakPtr<ServiceWorkerContextClient>
ServiceWorkerContextClient::GetWeakPtr() {
  DCHECK(worker_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(context_);
  return context_->weak_factory.GetWeakPtr();
}

}  // namespace content
