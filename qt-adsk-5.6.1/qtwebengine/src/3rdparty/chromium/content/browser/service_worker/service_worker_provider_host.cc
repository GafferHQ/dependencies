// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_provider_host.h"

#include "base/guid.h"
#include "base/stl_util.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_request_handler.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_registration_handle.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/resource_request_body.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"

namespace content {

namespace {

ServiceWorkerClientInfo FocusOnUIThread(int render_process_id,
                                        int render_frame_id) {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host));

  if (!render_frame_host || !web_contents)
    return ServiceWorkerClientInfo();

  FrameTreeNode* frame_tree_node = render_frame_host->frame_tree_node();

  // Focus the frame in the frame tree node, in case it has changed.
  frame_tree_node->frame_tree()->SetFocusedFrame(frame_tree_node);

  // Focus the frame's view to make sure the frame is now considered as focused.
  render_frame_host->GetView()->Focus();

  // Move the web contents to the foreground.
  web_contents->Activate();

  return ServiceWorkerProviderHost::GetWindowClientInfoOnUI(render_process_id,
                                                            render_frame_id);
}

}  // anonymous namespace

ServiceWorkerProviderHost::OneShotGetReadyCallback::OneShotGetReadyCallback(
    const GetRegistrationForReadyCallback& callback)
    : callback(callback),
      called(false) {
}

ServiceWorkerProviderHost::OneShotGetReadyCallback::~OneShotGetReadyCallback() {
}

ServiceWorkerProviderHost::ServiceWorkerProviderHost(
    int render_process_id,
    int route_id,
    int provider_id,
    ServiceWorkerProviderType provider_type,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerDispatcherHost* dispatcher_host)
    : client_uuid_(base::GenerateGUID()),
      render_process_id_(render_process_id),
      route_id_(route_id),
      render_thread_id_(kDocumentMainThreadId),
      provider_id_(provider_id),
      provider_type_(provider_type),
      context_(context),
      dispatcher_host_(dispatcher_host),
      allow_association_(true) {
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, render_process_id_);
  DCHECK_NE(SERVICE_WORKER_PROVIDER_UNKNOWN, provider_type_);
  DCHECK_NE(SERVICE_WORKER_PROVIDER_FOR_SANDBOXED_FRAME, provider_type_);
  if (provider_type_ == SERVICE_WORKER_PROVIDER_FOR_CONTROLLER) {
    // Actual thread id is set when the service worker context gets started.
    render_thread_id_ = kInvalidEmbeddedWorkerThreadId;
  }
  context_->RegisterProviderHostByClientID(client_uuid_, this);
}

ServiceWorkerProviderHost::~ServiceWorkerProviderHost() {
  if (context_)
    context_->UnregisterProviderHostByClientID(client_uuid_);

  // Clear docurl so the deferred activation of a waiting worker
  // won't associate the new version with a provider being destroyed.
  document_url_ = GURL();
  if (controlling_version_.get())
    controlling_version_->RemoveControllee(this);

  for (auto& key_registration : matching_registrations_) {
    DecreaseProcessReference(key_registration.second->pattern());
    key_registration.second->RemoveListener(this);
  }

  for (const GURL& pattern : associated_patterns_)
    DecreaseProcessReference(pattern);
}

int ServiceWorkerProviderHost::frame_id() const {
  if (provider_type_ == SERVICE_WORKER_PROVIDER_FOR_WINDOW)
    return route_id_;
  return MSG_ROUTING_NONE;
}

void ServiceWorkerProviderHost::OnVersionAttributesChanged(
    ServiceWorkerRegistration* registration,
    ChangedVersionAttributesMask changed_mask,
    const ServiceWorkerRegistrationInfo& info) {
  if (!get_ready_callback_ || get_ready_callback_->called)
    return;
  if (changed_mask.active_changed() && registration->active_version()) {
    // Wait until the state change so we don't send the get for ready
    // registration complete message before set version attributes message.
    registration->active_version()->RegisterStatusChangeCallback(base::Bind(
          &ServiceWorkerProviderHost::ReturnRegistrationForReadyIfNeeded,
          AsWeakPtr()));
  }
}

void ServiceWorkerProviderHost::OnRegistrationFailed(
    ServiceWorkerRegistration* registration) {
  if (associated_registration_ == registration)
    DisassociateRegistration();
  RemoveMatchingRegistration(registration);
}

void ServiceWorkerProviderHost::OnRegistrationFinishedUninstalling(
    ServiceWorkerRegistration* registration) {
  RemoveMatchingRegistration(registration);
}

void ServiceWorkerProviderHost::OnSkippedWaiting(
    ServiceWorkerRegistration* registration) {
  if (associated_registration_ != registration)
    return;
  // A client is "using" a registration if it is controlled by the active
  // worker of the registration. skipWaiting doesn't cause a client to start
  // using the registration.
  if (!controlling_version_)
    return;
  ServiceWorkerVersion* active_version = registration->active_version();
  DCHECK_EQ(active_version->status(), ServiceWorkerVersion::ACTIVATING);
  SetControllerVersionAttribute(active_version,
                                true /* notify_controllerchange */);
}

void ServiceWorkerProviderHost::SetDocumentUrl(const GURL& url) {
  DCHECK(!url.has_ref());
  document_url_ = url;
}

void ServiceWorkerProviderHost::SetTopmostFrameUrl(const GURL& url) {
  topmost_frame_url_ = url;
}

void ServiceWorkerProviderHost::SetControllerVersionAttribute(
    ServiceWorkerVersion* version,
    bool notify_controllerchange) {
  if (version == controlling_version_.get())
    return;

  scoped_refptr<ServiceWorkerVersion> previous_version = controlling_version_;
  controlling_version_ = version;
  if (version)
    version->AddControllee(this);
  if (previous_version.get())
    previous_version->RemoveControllee(this);

  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  // SetController message should be sent only for controllees.
  DCHECK(IsProviderForClient());
  Send(new ServiceWorkerMsg_SetControllerServiceWorker(
      render_thread_id_, provider_id(), GetOrCreateServiceWorkerHandle(version),
      notify_controllerchange));
}

bool ServiceWorkerProviderHost::SetHostedVersionId(int64 version_id) {
  if (!context_)
    return true;  // System is shutting down.
  if (active_version())
    return false;  // Unexpected bad message.

  ServiceWorkerVersion* live_version = context_->GetLiveVersion(version_id);
  if (!live_version)
    return true;  // Was deleted before it got started.

  ServiceWorkerVersionInfo info = live_version->GetInfo();
  if (info.running_status != ServiceWorkerVersion::STARTING ||
      info.process_id != render_process_id_) {
    // If we aren't trying to start this version in our process
    // something is amiss.
    return false;
  }

  running_hosted_version_ = live_version;
  return true;
}

bool ServiceWorkerProviderHost::IsProviderForClient() const {
  switch (provider_type_) {
    case SERVICE_WORKER_PROVIDER_FOR_WINDOW:
    case SERVICE_WORKER_PROVIDER_FOR_WORKER:
    case SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER:
      return true;
    case SERVICE_WORKER_PROVIDER_FOR_CONTROLLER:
      return false;
    case SERVICE_WORKER_PROVIDER_FOR_SANDBOXED_FRAME:
    case SERVICE_WORKER_PROVIDER_UNKNOWN:
      NOTREACHED() << provider_type_;
  }
  NOTREACHED() << provider_type_;
  return false;
}

blink::WebServiceWorkerClientType ServiceWorkerProviderHost::client_type()
    const {
  switch (provider_type_) {
    case SERVICE_WORKER_PROVIDER_FOR_WINDOW:
      return blink::WebServiceWorkerClientTypeWindow;
    case SERVICE_WORKER_PROVIDER_FOR_WORKER:
      return blink::WebServiceWorkerClientTypeWorker;
    case SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER:
      return blink::WebServiceWorkerClientTypeSharedWorker;
    case SERVICE_WORKER_PROVIDER_FOR_CONTROLLER:
    case SERVICE_WORKER_PROVIDER_FOR_SANDBOXED_FRAME:
    case SERVICE_WORKER_PROVIDER_UNKNOWN:
      NOTREACHED() << provider_type_;
  }
  NOTREACHED() << provider_type_;
  return blink::WebServiceWorkerClientTypeWindow;
}

void ServiceWorkerProviderHost::AssociateRegistration(
    ServiceWorkerRegistration* registration,
    bool notify_controllerchange) {
  DCHECK(CanAssociateRegistration(registration));
  associated_registration_ = registration;
  AddMatchingRegistration(registration);
  SendAssociateRegistrationMessage();
  SetControllerVersionAttribute(registration->active_version(),
                                notify_controllerchange);
}

void ServiceWorkerProviderHost::DisassociateRegistration() {
  queued_events_.clear();
  if (!associated_registration_.get())
    return;
  associated_registration_ = NULL;
  SetControllerVersionAttribute(NULL, false /* notify_controllerchange */);

  if (!dispatcher_host_)
    return;

  // Disassociation message should be sent only for controllees.
  DCHECK(IsProviderForClient());
  Send(new ServiceWorkerMsg_DisassociateRegistration(
      render_thread_id_, provider_id()));
}

void ServiceWorkerProviderHost::AddMatchingRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(ServiceWorkerUtils::ScopeMatches(
        registration->pattern(), document_url_));
  size_t key = registration->pattern().spec().size();
  if (ContainsKey(matching_registrations_, key))
    return;
  IncreaseProcessReference(registration->pattern());
  registration->AddListener(this);
  matching_registrations_[key] = registration;
  ReturnRegistrationForReadyIfNeeded();
}

void ServiceWorkerProviderHost::RemoveMatchingRegistration(
    ServiceWorkerRegistration* registration) {
  size_t key = registration->pattern().spec().size();
  DCHECK(ContainsKey(matching_registrations_, key));
  DecreaseProcessReference(registration->pattern());
  registration->RemoveListener(this);
  matching_registrations_.erase(key);
}

void ServiceWorkerProviderHost::AddAllMatchingRegistrations() {
  DCHECK(context_);
  const std::map<int64, ServiceWorkerRegistration*>& registrations =
      context_->GetLiveRegistrations();
  for (const auto& key_registration : registrations) {
    ServiceWorkerRegistration* registration = key_registration.second;
    if (!registration->is_uninstalled() &&
        ServiceWorkerUtils::ScopeMatches(registration->pattern(),
                                         document_url_))
      AddMatchingRegistration(registration);
  }
}

ServiceWorkerRegistration*
ServiceWorkerProviderHost::MatchRegistration() const {
  ServiceWorkerRegistrationMap::const_reverse_iterator it =
      matching_registrations_.rbegin();
  for (; it != matching_registrations_.rend(); ++it) {
    if (it->second->is_uninstalled())
      continue;
    if (it->second->is_uninstalling())
      return nullptr;
    return it->second.get();
  }
  return nullptr;
}

void ServiceWorkerProviderHost::NotifyControllerLost() {
  SetControllerVersionAttribute(nullptr, true /* notify_controllerchange */);
}

scoped_ptr<ServiceWorkerRequestHandler>
ServiceWorkerProviderHost::CreateRequestHandler(
    FetchRequestMode request_mode,
    FetchCredentialsMode credentials_mode,
    ResourceType resource_type,
    RequestContextType request_context_type,
    RequestContextFrameType frame_type,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    scoped_refptr<ResourceRequestBody> body) {
  if (IsHostToRunningServiceWorker()) {
    return scoped_ptr<ServiceWorkerRequestHandler>(
        new ServiceWorkerContextRequestHandler(
            context_, AsWeakPtr(), blob_storage_context, resource_type));
  }
  if (ServiceWorkerUtils::IsMainResourceType(resource_type) ||
      controlling_version()) {
    return scoped_ptr<ServiceWorkerRequestHandler>(
        new ServiceWorkerControlleeRequestHandler(context_,
                                                  AsWeakPtr(),
                                                  blob_storage_context,
                                                  request_mode,
                                                  credentials_mode,
                                                  resource_type,
                                                  request_context_type,
                                                  frame_type,
                                                  body));
  }
  return scoped_ptr<ServiceWorkerRequestHandler>();
}

ServiceWorkerObjectInfo
ServiceWorkerProviderHost::GetOrCreateServiceWorkerHandle(
    ServiceWorkerVersion* version) {
  DCHECK(dispatcher_host_);
  if (!context_ || !version)
    return ServiceWorkerObjectInfo();
  ServiceWorkerHandle* handle = dispatcher_host_->FindServiceWorkerHandle(
      provider_id(), version->version_id());
  if (handle) {
    handle->IncrementRefCount();
    return handle->GetObjectInfo();
  }

  scoped_ptr<ServiceWorkerHandle> new_handle(
      ServiceWorkerHandle::Create(context_, AsWeakPtr(), version));
  handle = new_handle.get();
  dispatcher_host_->RegisterServiceWorkerHandle(new_handle.Pass());
  return handle->GetObjectInfo();
}

bool ServiceWorkerProviderHost::CanAssociateRegistration(
    ServiceWorkerRegistration* registration) {
  if (!context_)
    return false;
  if (running_hosted_version_.get())
    return false;
  if (!registration || associated_registration_.get() || !allow_association_)
    return false;
  return true;
}

void ServiceWorkerProviderHost::PostMessage(
    ServiceWorkerVersion* version,
    const base::string16& message,
    const std::vector<TransferredMessagePort>& sent_message_ports) {
  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  std::vector<int> new_routing_ids;
  dispatcher_host_->message_port_message_filter()->
      UpdateMessagePortsWithNewRoutes(sent_message_ports,
                                      &new_routing_ids);

  ServiceWorkerMsg_MessageToDocument_Params params;
  params.thread_id = kDocumentMainThreadId;
  params.provider_id = provider_id();
  params.service_worker_info = GetOrCreateServiceWorkerHandle(version);
  params.message = message;
  params.message_ports = sent_message_ports;
  params.new_routing_ids = new_routing_ids;
  Send(new ServiceWorkerMsg_MessageToDocument(params));
}

void ServiceWorkerProviderHost::Focus(const GetClientInfoCallback& callback) {
  if (provider_type_ != SERVICE_WORKER_PROVIDER_FOR_WINDOW) {
    callback.Run(ServiceWorkerClientInfo());
    return;
  }
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&FocusOnUIThread, render_process_id_, route_id_), callback);
}

void ServiceWorkerProviderHost::GetWindowClientInfo(
    const GetClientInfoCallback& callback) const {
  if (provider_type_ != SERVICE_WORKER_PROVIDER_FOR_WINDOW) {
    callback.Run(ServiceWorkerClientInfo());
    return;
  }
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ServiceWorkerProviderHost::GetWindowClientInfoOnUI,
                 render_process_id_, route_id_),
      callback);
}

// static
ServiceWorkerClientInfo ServiceWorkerProviderHost::GetWindowClientInfoOnUI(
    int render_process_id,
    int render_frame_id) {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return ServiceWorkerClientInfo();

  // TODO(mlamouri,michaeln): it is possible to end up collecting information
  // for a frame that is actually being navigated and isn't exactly what we are
  // expecting.
  return ServiceWorkerClientInfo(
      render_frame_host->GetVisibilityState(),
      render_frame_host->IsFocused(),
      render_frame_host->GetLastCommittedURL(),
      render_frame_host->GetParent() ? REQUEST_CONTEXT_FRAME_TYPE_NESTED
                                     : REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL,
      blink::WebServiceWorkerClientTypeWindow);
}

void ServiceWorkerProviderHost::AddScopedProcessReferenceToPattern(
    const GURL& pattern) {
  associated_patterns_.push_back(pattern);
  IncreaseProcessReference(pattern);
}

void ServiceWorkerProviderHost::ClaimedByRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(registration->active_version());
  if (registration == associated_registration_) {
    SetControllerVersionAttribute(registration->active_version(),
                                  true /* notify_controllerchange */);
  } else if (allow_association_) {
    DisassociateRegistration();
    AssociateRegistration(registration, true /* notify_controllerchange */);
  }
}

bool ServiceWorkerProviderHost::GetRegistrationForReady(
    const GetRegistrationForReadyCallback& callback) {
  if (get_ready_callback_)
    return false;
  get_ready_callback_.reset(new OneShotGetReadyCallback(callback));
  ReturnRegistrationForReadyIfNeeded();
  return true;
}

void ServiceWorkerProviderHost::PrepareForCrossSiteTransfer() {
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, render_process_id_);
  DCHECK_NE(MSG_ROUTING_NONE, route_id_);
  DCHECK_EQ(kDocumentMainThreadId, render_thread_id_);
  DCHECK_NE(SERVICE_WORKER_PROVIDER_UNKNOWN, provider_type_);

  for (const GURL& pattern : associated_patterns_)
    DecreaseProcessReference(pattern);

  for (auto& key_registration : matching_registrations_)
    DecreaseProcessReference(key_registration.second->pattern());

  if (associated_registration_.get()) {
    if (dispatcher_host_) {
      Send(new ServiceWorkerMsg_DisassociateRegistration(
          render_thread_id_, provider_id()));
    }
  }

  render_process_id_ = ChildProcessHost::kInvalidUniqueID;
  route_id_ = MSG_ROUTING_NONE;
  render_thread_id_ = kInvalidEmbeddedWorkerThreadId;
  provider_id_ = kInvalidServiceWorkerProviderId;
  provider_type_ = SERVICE_WORKER_PROVIDER_UNKNOWN;
  dispatcher_host_ = nullptr;
}

void ServiceWorkerProviderHost::CompleteCrossSiteTransfer(
    int new_process_id,
    int new_frame_id,
    int new_provider_id,
    ServiceWorkerProviderType new_provider_type,
    ServiceWorkerDispatcherHost* new_dispatcher_host) {
  DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, render_process_id_);
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, new_process_id);
  DCHECK_NE(MSG_ROUTING_NONE, new_frame_id);

  render_process_id_ = new_process_id;
  route_id_ = new_frame_id;
  render_thread_id_ = kDocumentMainThreadId;
  provider_id_ = new_provider_id;
  provider_type_ = new_provider_type;
  dispatcher_host_ = new_dispatcher_host;

  for (const GURL& pattern : associated_patterns_)
    IncreaseProcessReference(pattern);

  for (auto& key_registration : matching_registrations_)
    IncreaseProcessReference(key_registration.second->pattern());

  if (associated_registration_.get()) {
    SendAssociateRegistrationMessage();
    if (dispatcher_host_ && associated_registration_->active_version()) {
      Send(new ServiceWorkerMsg_SetControllerServiceWorker(
          render_thread_id_, provider_id(),
          GetOrCreateServiceWorkerHandle(
              associated_registration_->active_version()),
          false /* shouldNotifyControllerChange */));
    }
  }
}

void ServiceWorkerProviderHost::SendUpdateFoundMessage(
    int registration_handle_id) {
  if (!dispatcher_host_)
    return;  // Could be nullptr in some tests.

  if (!IsReadyToSendMessages()) {
    queued_events_.push_back(
        base::Bind(&ServiceWorkerProviderHost::SendUpdateFoundMessage,
                   AsWeakPtr(), registration_handle_id));
    return;
  }

  Send(new ServiceWorkerMsg_UpdateFound(
      render_thread_id_, registration_handle_id));
}

void ServiceWorkerProviderHost::SendSetVersionAttributesMessage(
    int registration_handle_id,
    ChangedVersionAttributesMask changed_mask,
    ServiceWorkerVersion* installing_version,
    ServiceWorkerVersion* waiting_version,
    ServiceWorkerVersion* active_version) {
  if (!dispatcher_host_)
    return;  // Could be nullptr in some tests.
  if (!changed_mask.changed())
    return;

  if (!IsReadyToSendMessages()) {
    queued_events_.push_back(
        base::Bind(&ServiceWorkerProviderHost::SendSetVersionAttributesMessage,
                   AsWeakPtr(), registration_handle_id, changed_mask,
                   make_scoped_refptr(installing_version),
                   make_scoped_refptr(waiting_version),
                   make_scoped_refptr(active_version)));
    return;
  }

  ServiceWorkerVersionAttributes attrs;
  if (changed_mask.installing_changed())
    attrs.installing = GetOrCreateServiceWorkerHandle(installing_version);
  if (changed_mask.waiting_changed())
    attrs.waiting = GetOrCreateServiceWorkerHandle(waiting_version);
  if (changed_mask.active_changed())
    attrs.active = GetOrCreateServiceWorkerHandle(active_version);

  Send(new ServiceWorkerMsg_SetVersionAttributes(
      render_thread_id_, provider_id_, registration_handle_id,
      changed_mask.changed(), attrs));
}

void ServiceWorkerProviderHost::SendServiceWorkerStateChangedMessage(
    int worker_handle_id,
    blink::WebServiceWorkerState state) {
  if (!dispatcher_host_)
    return;

  if (!IsReadyToSendMessages()) {
    queued_events_.push_back(base::Bind(
        &ServiceWorkerProviderHost::SendServiceWorkerStateChangedMessage,
        AsWeakPtr(), worker_handle_id, state));
    return;
  }

  Send(new ServiceWorkerMsg_ServiceWorkerStateChanged(
      render_thread_id_, worker_handle_id, state));
}

void ServiceWorkerProviderHost::SetReadyToSendMessagesToWorker(
    int render_thread_id) {
  DCHECK(!IsReadyToSendMessages());
  render_thread_id_ = render_thread_id;

  for (const auto& event : queued_events_)
    event.Run();
  queued_events_.clear();
}

void ServiceWorkerProviderHost::SendAssociateRegistrationMessage() {
  if (!dispatcher_host_)
    return;

  ServiceWorkerRegistrationHandle* handle =
      dispatcher_host_->GetOrCreateRegistrationHandle(
          AsWeakPtr(), associated_registration_.get());

  ServiceWorkerVersionAttributes attrs;
  attrs.installing = GetOrCreateServiceWorkerHandle(
      associated_registration_->installing_version());
  attrs.waiting = GetOrCreateServiceWorkerHandle(
      associated_registration_->waiting_version());
  attrs.active = GetOrCreateServiceWorkerHandle(
      associated_registration_->active_version());

  // Association message should be sent only for controllees.
  DCHECK(IsProviderForClient());
  dispatcher_host_->Send(new ServiceWorkerMsg_AssociateRegistration(
      render_thread_id_, provider_id(), handle->GetObjectInfo(), attrs));
}

void ServiceWorkerProviderHost::IncreaseProcessReference(
    const GURL& pattern) {
  if (context_ && context_->process_manager()) {
    context_->process_manager()->AddProcessReferenceToPattern(
        pattern, render_process_id_);
  }
}

void ServiceWorkerProviderHost::DecreaseProcessReference(
    const GURL& pattern) {
  if (context_ && context_->process_manager()) {
    context_->process_manager()->RemoveProcessReferenceFromPattern(
        pattern, render_process_id_);
  }
}

void ServiceWorkerProviderHost::ReturnRegistrationForReadyIfNeeded() {
  if (!get_ready_callback_ || get_ready_callback_->called)
    return;
  ServiceWorkerRegistration* registration = MatchRegistration();
  if (!registration)
    return;
  if (registration->active_version()) {
    get_ready_callback_->callback.Run(registration);
    get_ready_callback_->callback.Reset();
    get_ready_callback_->called = true;
    return;
  }
}

bool ServiceWorkerProviderHost::IsReadyToSendMessages() const {
  return render_thread_id_ != kInvalidEmbeddedWorkerThreadId;
}

bool ServiceWorkerProviderHost::IsContextAlive() {
  return context_ != NULL;
}

void ServiceWorkerProviderHost::Send(IPC::Message* message) const {
  DCHECK(dispatcher_host_);
  DCHECK(IsReadyToSendMessages());
  dispatcher_host_->Send(message);
}

}  // namespace content
