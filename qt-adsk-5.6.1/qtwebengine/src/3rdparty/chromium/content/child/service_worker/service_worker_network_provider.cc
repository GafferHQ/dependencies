// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_network_provider.h"

#include "base/atomic_sequence_num.h"
#include "content/child/child_thread_impl.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/common/service_worker/service_worker_messages.h"

namespace content {

namespace {

const char kUserDataKey[] = "SWProviderKey";

// Must be unique in the child process.
int GetNextProviderId() {
  static base::StaticAtomicSequenceNumber sequence;
  return sequence.GetNext();  // We start at zero.
}

// When the provider is for a sandboxed iframe we use
// kInvalidServiceWorkerProviderId as the provider type and we don't create
// ServiceWorkerProviderContext and ServiceWorkerProviderHost.
int GenerateProviderIdForType(const ServiceWorkerProviderType provider_type) {
  if (provider_type == SERVICE_WORKER_PROVIDER_FOR_SANDBOXED_FRAME)
    return kInvalidServiceWorkerProviderId;
  return GetNextProviderId();
}

}  // namespace

void ServiceWorkerNetworkProvider::AttachToDocumentState(
    base::SupportsUserData* datasource_userdata,
    scoped_ptr<ServiceWorkerNetworkProvider> network_provider) {
  datasource_userdata->SetUserData(&kUserDataKey, network_provider.release());
}

ServiceWorkerNetworkProvider* ServiceWorkerNetworkProvider::FromDocumentState(
    base::SupportsUserData* datasource_userdata) {
  return static_cast<ServiceWorkerNetworkProvider*>(
      datasource_userdata->GetUserData(&kUserDataKey));
}

ServiceWorkerNetworkProvider::ServiceWorkerNetworkProvider(
    int route_id,
    ServiceWorkerProviderType provider_type)
    : provider_id_(GenerateProviderIdForType(provider_type)) {
  if (provider_id_ == kInvalidServiceWorkerProviderId)
    return;
  context_ = new ServiceWorkerProviderContext(provider_id_);
  if (!ChildThreadImpl::current())
    return;  // May be null in some tests.
  ChildThreadImpl::current()->Send(new ServiceWorkerHostMsg_ProviderCreated(
      provider_id_, route_id, provider_type));
}

ServiceWorkerNetworkProvider::~ServiceWorkerNetworkProvider() {
  if (provider_id_ == kInvalidServiceWorkerProviderId)
    return;
  if (!ChildThreadImpl::current())
    return;  // May be null in some tests.
  ChildThreadImpl::current()->Send(
      new ServiceWorkerHostMsg_ProviderDestroyed(provider_id_));
}

void ServiceWorkerNetworkProvider::SetServiceWorkerVersionId(
    int64 version_id) {
  DCHECK_NE(kInvalidServiceWorkerProviderId, provider_id_);
  if (!ChildThreadImpl::current())
    return;  // May be null in some tests.
  ChildThreadImpl::current()->Send(
      new ServiceWorkerHostMsg_SetVersionId(provider_id_, version_id));
}

}  // namespace content
