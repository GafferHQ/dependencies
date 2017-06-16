// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/push_messaging/push_provider.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "content/child/push_messaging/push_dispatcher.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_task_runner.h"
#include "content/common/push_messaging_messages.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushSubscription.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushSubscriptionOptions.h"

namespace content {
namespace {

int CurrentWorkerId() {
  return WorkerTaskRunner::Instance()->CurrentWorkerId();
}

// Returns the id of the given |service_worker_registration|, which
// is only available on the implementation of the interface.
int64 GetServiceWorkerRegistrationId(
    blink::WebServiceWorkerRegistration* service_worker_registration) {
  return static_cast<WebServiceWorkerRegistrationImpl*>(
      service_worker_registration)->registration_id();
}

}  // namespace

static base::LazyInstance<base::ThreadLocalPointer<PushProvider>>::Leaky
    g_push_provider_tls = LAZY_INSTANCE_INITIALIZER;

PushProvider::PushProvider(ThreadSafeSender* thread_safe_sender,
                           PushDispatcher* push_dispatcher)
    : thread_safe_sender_(thread_safe_sender),
      push_dispatcher_(push_dispatcher) {
  g_push_provider_tls.Pointer()->Set(this);
}

PushProvider::~PushProvider() {
  g_push_provider_tls.Pointer()->Set(nullptr);
}

PushProvider* PushProvider::ThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender,
    PushDispatcher* push_dispatcher) {
  if (g_push_provider_tls.Pointer()->Get())
    return g_push_provider_tls.Pointer()->Get();

  PushProvider* provider =
      new PushProvider(thread_safe_sender, push_dispatcher);
  if (CurrentWorkerId())
    WorkerTaskRunner::Instance()->AddStopObserver(provider);
  return provider;
}

void PushProvider::OnWorkerRunLoopStopped() {
  delete this;
}

void PushProvider::subscribe(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    const blink::WebPushSubscriptionOptions& options,
    blink::WebPushSubscriptionCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int request_id = push_dispatcher_->GenerateRequestId(CurrentWorkerId());
  subscription_callbacks_.AddWithID(callbacks, request_id);
  int64 service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  thread_safe_sender_->Send(new PushMessagingHostMsg_SubscribeFromWorker(
      request_id, service_worker_registration_id, options.userVisibleOnly));
}

void PushProvider::unsubscribe(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebPushUnsubscribeCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);

  int request_id = push_dispatcher_->GenerateRequestId(CurrentWorkerId());
  unsubscribe_callbacks_.AddWithID(callbacks, request_id);

  int64 service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  thread_safe_sender_->Send(new PushMessagingHostMsg_Unsubscribe(
      request_id, service_worker_registration_id));
}

void PushProvider::getSubscription(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebPushSubscriptionCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int request_id = push_dispatcher_->GenerateRequestId(CurrentWorkerId());
  subscription_callbacks_.AddWithID(callbacks, request_id);
  int64 service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  thread_safe_sender_->Send(new PushMessagingHostMsg_GetRegistration(
      request_id, service_worker_registration_id));
}

void PushProvider::getPermissionStatus(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    const blink::WebPushSubscriptionOptions& options,
    blink::WebPushPermissionStatusCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int request_id = push_dispatcher_->GenerateRequestId(CurrentWorkerId());
  permission_status_callbacks_.AddWithID(callbacks, request_id);
  int64 service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  thread_safe_sender_->Send(new PushMessagingHostMsg_GetPermissionStatus(
      request_id, service_worker_registration_id, options.userVisibleOnly));
}

bool PushProvider::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PushProvider, message)
    IPC_MESSAGE_HANDLER(PushMessagingMsg_SubscribeFromWorkerSuccess,
                        OnSubscribeFromWorkerSuccess);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_SubscribeFromWorkerError,
                        OnSubscribeFromWorkerError);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_UnsubscribeSuccess,
                        OnUnsubscribeSuccess);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_UnsubscribeError,
                        OnUnsubscribeError);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_GetRegistrationSuccess,
                        OnGetRegistrationSuccess);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_GetRegistrationError,
                        OnGetRegistrationError);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_GetPermissionStatusSuccess,
                        OnGetPermissionStatusSuccess);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_GetPermissionStatusError,
                        OnGetPermissionStatusError);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PushProvider::OnSubscribeFromWorkerSuccess(
    int request_id,
    const GURL& endpoint) {
  blink::WebPushSubscriptionCallbacks* callbacks =
      subscription_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  scoped_ptr<blink::WebPushSubscription> subscription(
      new blink::WebPushSubscription(endpoint));
  callbacks->onSuccess(subscription.release());

  subscription_callbacks_.Remove(request_id);
}

void PushProvider::OnSubscribeFromWorkerError(int request_id,
                                              PushRegistrationStatus status) {
  blink::WebPushSubscriptionCallbacks* callbacks =
      subscription_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  scoped_ptr<blink::WebPushError> error(new blink::WebPushError(
      blink::WebPushError::ErrorTypeAbort,
      blink::WebString::fromUTF8(PushRegistrationStatusToString(status))));
  callbacks->onError(error.release());

  subscription_callbacks_.Remove(request_id);
}

void PushProvider::OnUnsubscribeSuccess(int request_id, bool did_unsubscribe) {
  blink::WebPushUnsubscribeCallbacks* callbacks =
      unsubscribe_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  callbacks->onSuccess(&did_unsubscribe);

  unsubscribe_callbacks_.Remove(request_id);
}

void PushProvider::OnUnsubscribeError(
    int request_id,
    blink::WebPushError::ErrorType error_type,
    const std::string& error_message) {
  blink::WebPushUnsubscribeCallbacks* callbacks =
      unsubscribe_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  scoped_ptr<blink::WebPushError> error(new blink::WebPushError(
      error_type, blink::WebString::fromUTF8(error_message)));
  callbacks->onError(error.release());

  unsubscribe_callbacks_.Remove(request_id);
}

void PushProvider::OnGetRegistrationSuccess(
    int request_id,
    const GURL& endpoint) {
  blink::WebPushSubscriptionCallbacks* callbacks =
      subscription_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  scoped_ptr<blink::WebPushSubscription> subscription(
      new blink::WebPushSubscription(endpoint));
  callbacks->onSuccess(subscription.release());

  subscription_callbacks_.Remove(request_id);
}

void PushProvider::OnGetRegistrationError(
    int request_id,
    PushGetRegistrationStatus status) {
  blink::WebPushSubscriptionCallbacks* callbacks =
      subscription_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  // We are only expecting an error if we can't find a registration.
  callbacks->onSuccess(nullptr);

  subscription_callbacks_.Remove(request_id);
}

void PushProvider::OnGetPermissionStatusSuccess(
    int request_id,
    blink::WebPushPermissionStatus status) {
  blink::WebPushPermissionStatusCallbacks* callbacks =
      permission_status_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  callbacks->onSuccess(&status);

  permission_status_callbacks_.Remove(request_id);
}

void PushProvider::OnGetPermissionStatusError(
    int request_id,
    blink::WebPushError::ErrorType error) {
  blink::WebPushPermissionStatusCallbacks* callbacks =
      permission_status_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  std::string error_message;
  if (error == blink::WebPushError::ErrorTypeNotSupported) {
    error_message =
        "Push subscriptions that don't enable userVisibleOnly are not "
        "supported.";
  }

  scoped_ptr<blink::WebPushError> web_error(new blink::WebPushError(
      error, blink::WebString::fromUTF8(error_message)));

  callbacks->onError(web_error.release());

  permission_status_callbacks_.Remove(request_id);
}

}  // namespace content
