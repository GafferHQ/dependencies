// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_request_handler.h"

#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_read_from_cache_job.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/service_worker/service_worker_write_to_cache_job.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/resource_response_info.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"

namespace content {

ServiceWorkerContextRequestHandler::ServiceWorkerContextRequestHandler(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    ResourceType resource_type)
    : ServiceWorkerRequestHandler(context,
                                  provider_host,
                                  blob_storage_context,
                                  resource_type),
      version_(provider_host_->running_hosted_version()) {
  DCHECK(provider_host_->IsHostToRunningServiceWorker());
}

ServiceWorkerContextRequestHandler::~ServiceWorkerContextRequestHandler() {
}

net::URLRequestJob* ServiceWorkerContextRequestHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    ResourceContext* resource_context) {
  if (!provider_host_ || !version_.get() || !context_)
    return NULL;

  // We currently have no use case for hijacking a redirected request.
  if (request->url_chain().size() > 1)
    return NULL;

  // We only use the script cache for main script loading and
  // importScripts(), even if a cached script is xhr'd, we don't
  // retrieve it from the script cache.
  // TODO(michaeln): Get the desired behavior clarified in the spec,
  // and make tweak the behavior here to match.
  if (resource_type_ != RESOURCE_TYPE_SERVICE_WORKER &&
      resource_type_ != RESOURCE_TYPE_SCRIPT) {
    return NULL;
  }

  if (ShouldAddToScriptCache(request->url())) {
    ServiceWorkerRegistration* registration =
        context_->GetLiveRegistration(version_->registration_id());
    DCHECK(registration);  // We're registering or updating so must be there.

    int64 response_id = context_->storage()->NewResourceId();
    if (response_id == kInvalidServiceWorkerResponseId)
      return NULL;

    // Bypass the browser cache for initial installs and update
    // checks after 24 hours have passed.
    int extra_load_flags = 0;
    base::TimeDelta time_since_last_check =
        base::Time::Now() - registration->last_update_check();
    if (time_since_last_check > base::TimeDelta::FromHours(
                                    kServiceWorkerScriptMaxCacheAgeInHours) ||
        version_->force_bypass_cache_for_scripts()) {
      extra_load_flags = net::LOAD_BYPASS_CACHE;
    }

    ServiceWorkerVersion* stored_version = registration->waiting_version()
                                               ? registration->waiting_version()
                                               : registration->active_version();
    int64 incumbent_response_id = kInvalidServiceWorkerResourceId;
    if (stored_version && stored_version->script_url() == request->url()) {
      incumbent_response_id =
          stored_version->script_cache_map()->LookupResourceId(request->url());
    }
    return new ServiceWorkerWriteToCacheJob(
        request, network_delegate, resource_type_, context_, version_.get(),
        extra_load_flags, response_id, incumbent_response_id);
  }

  int64 response_id = kInvalidServiceWorkerResponseId;
  if (ShouldReadFromScriptCache(request->url(), &response_id)) {
    return new ServiceWorkerReadFromCacheJob(
        request, network_delegate, context_, version_, response_id);
  }

  // NULL means use the network.
  return NULL;
}

void ServiceWorkerContextRequestHandler::GetExtraResponseInfo(
    ResourceResponseInfo* response_info) const {
  response_info->was_fetched_via_service_worker = false;
  response_info->was_fallback_required_by_service_worker = false;
  response_info->original_url_via_service_worker = GURL();
  response_info->response_type_via_service_worker =
      blink::WebServiceWorkerResponseTypeDefault;
}

bool ServiceWorkerContextRequestHandler::ShouldAddToScriptCache(
    const GURL& url) {
  // We only write imports that occur during the initial eval.
  if (version_->status() != ServiceWorkerVersion::NEW &&
      version_->status() != ServiceWorkerVersion::INSTALLING) {
    return false;
  }
  return version_->script_cache_map()->LookupResourceId(url) ==
         kInvalidServiceWorkerResponseId;
}

bool ServiceWorkerContextRequestHandler::ShouldReadFromScriptCache(
    const GURL& url, int64* response_id_out) {
  // We don't read from the script cache until the version is INSTALLED.
  if (version_->status() == ServiceWorkerVersion::NEW ||
      version_->status() == ServiceWorkerVersion::INSTALLING)
    return false;
  *response_id_out = version_->script_cache_map()->LookupResourceId(url);
  return *response_id_out != kInvalidServiceWorkerResponseId;
}

}  // namespace content
