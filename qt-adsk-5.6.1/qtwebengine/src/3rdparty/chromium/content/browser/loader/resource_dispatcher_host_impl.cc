// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#include "content/browser/loader/resource_dispatcher_host_impl.h"

#include <algorithm>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/profiler/scoped_tracker.h"
#include "base/stl_util.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/time/time.h"
#include "content/browser/appcache/appcache_interceptor.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/bad_message.h"
#include "content/browser/cert_store_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/download/save_file_manager.h"
#include "content/browser/download/save_file_resource_handler.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/loader/async_resource_handler.h"
#include "content/browser/loader/cross_site_resource_handler.h"
#include "content/browser/loader/detachable_resource_handler.h"
#include "content/browser/loader/mime_type_resource_handler.h"
#include "content/browser/loader/navigation_resource_handler.h"
#include "content/browser/loader/navigation_url_loader_impl_core.h"
#include "content/browser/loader/power_save_block_resource_throttle.h"
#include "content/browser/loader/redirect_to_file_resource_handler.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/stream_resource_handler.h"
#include "content/browser/loader/sync_resource_handler.h"
#include "content/browser/loader/throttling_resource_handler.h"
#include "content/browser/loader/upload_data_stream_builder.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/streams/stream_registry.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/appcache_interfaces.h"
#include "content/common/navigation_params.h"
#include "content/common/resource_messages.h"
#include "content/common/ssl_status_serialization.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/browser/stream_info.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "net/base/auth.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/request_priority.h"
#include "net/base/upload_data_stream.h"
#include "net/cert/cert_status_flags.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/fileapi/file_permission_policy.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "url/url_constants.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using storage::ShareableFileReference;

// ----------------------------------------------------------------------------

namespace content {

namespace {

static ResourceDispatcherHostImpl* g_resource_dispatcher_host;

// The interval for calls to ResourceDispatcherHostImpl::UpdateLoadStates
const int kUpdateLoadStatesIntervalMsec = 250;

// Maximum byte "cost" of all the outstanding requests for a renderer.
// See delcaration of |max_outstanding_requests_cost_per_process_| for details.
// This bound is 25MB, which allows for around 6000 outstanding requests.
const int kMaxOutstandingRequestsCostPerProcess = 26214400;

// The number of milliseconds after noting a user gesture that we will
// tag newly-created URLRequest objects with the
// net::LOAD_MAYBE_USER_GESTURE load flag. This is a fairly arbitrary
// guess at how long to expect direct impact from a user gesture, but
// this should be OK as the load flag is a best-effort thing only,
// rather than being intended as fully accurate.
const int kUserGestureWindowMs = 3500;

// Ratio of |max_num_in_flight_requests_| that any one renderer is allowed to
// use. Arbitrarily chosen.
const double kMaxRequestsPerProcessRatio = 0.45;

// TODO(jkarlin): The value is high to reduce the chance of the detachable
// request timing out, forcing a blocked second request to open a new connection
// and start over. Reduce this value once we have a better idea of what it
// should be and once we stop blocking multiple simultaneous requests for the
// same resource (see bugs 46104 and 31014).
const int kDefaultDetachableCancelDelayMs = 30000;

enum SHA1HistogramTypes {
  // SHA-1 is not present in the certificate chain.
  SHA1_NOT_PRESENT = 0,
  // SHA-1 is present in the certificate chain, and the leaf expires on or
  // after January 1, 2017.
  SHA1_EXPIRES_AFTER_JANUARY_2017 = 1,
  // SHA-1 is present in the certificate chain, and the leaf expires on or
  // after June 1, 2016.
  SHA1_EXPIRES_AFTER_JUNE_2016 = 2,
  // SHA-1 is present in the certificate chain, and the leaf expires on or
  // after January 1, 2016.
  SHA1_EXPIRES_AFTER_JANUARY_2016 = 3,
  // SHA-1 is present in the certificate chain, but the leaf expires before
  // January 1, 2016
  SHA1_PRESENT = 4,
  // Always keep this at the end.
  SHA1_HISTOGRAM_TYPES_MAX,
};

void RecordCertificateHistograms(const net::SSLInfo& ssl_info,
                                 ResourceType resource_type) {
  // The internal representation of the dates for UI treatment of SHA-1.
  // See http://crbug.com/401365 for details
  static const int64_t kJanuary2017 = INT64_C(13127702400000000);
  static const int64_t kJune2016 = INT64_C(13109213000000000);
  static const int64_t kJanuary2016 = INT64_C(13096080000000000);

  SHA1HistogramTypes sha1_histogram = SHA1_NOT_PRESENT;
  if (ssl_info.cert_status & net::CERT_STATUS_SHA1_SIGNATURE_PRESENT) {
    DCHECK(ssl_info.cert.get());
    if (ssl_info.cert->valid_expiry() >=
        base::Time::FromInternalValue(kJanuary2017)) {
      sha1_histogram = SHA1_EXPIRES_AFTER_JANUARY_2017;
    } else if (ssl_info.cert->valid_expiry() >=
               base::Time::FromInternalValue(kJune2016)) {
      sha1_histogram = SHA1_EXPIRES_AFTER_JUNE_2016;
    } else if (ssl_info.cert->valid_expiry() >=
               base::Time::FromInternalValue(kJanuary2016)) {
      sha1_histogram = SHA1_EXPIRES_AFTER_JANUARY_2016;
    } else {
      sha1_histogram = SHA1_PRESENT;
    }
  }
  if (resource_type == RESOURCE_TYPE_MAIN_FRAME) {
    UMA_HISTOGRAM_ENUMERATION("Net.Certificate.SHA1.MainFrame",
                              sha1_histogram,
                              SHA1_HISTOGRAM_TYPES_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Net.Certificate.SHA1.Subresource",
                              sha1_histogram,
                              SHA1_HISTOGRAM_TYPES_MAX);
  }
}

bool IsDetachableResourceType(ResourceType type) {
  switch (type) {
    case RESOURCE_TYPE_PREFETCH:
    case RESOURCE_TYPE_PING:
      return true;
    default:
      return false;
  }
}

// Aborts a request before an URLRequest has actually been created.
void AbortRequestBeforeItStarts(ResourceMessageFilter* filter,
                                IPC::Message* sync_result,
                                int request_id) {
  if (sync_result) {
    SyncLoadResult result;
    result.error_code = net::ERR_ABORTED;
    ResourceHostMsg_SyncLoad::WriteReplyParams(sync_result, result);
    filter->Send(sync_result);
  } else {
    // Tell the renderer that this request was disallowed.
    ResourceMsg_RequestCompleteData request_complete_data;
    request_complete_data.error_code = net::ERR_ABORTED;
    request_complete_data.was_ignored_by_handler = false;
    request_complete_data.exists_in_cache = false;
    // No security info needed, connection not established.
    request_complete_data.completion_time = base::TimeTicks();
    request_complete_data.encoded_data_length = 0;
    filter->Send(new ResourceMsg_RequestComplete(
        request_id, request_complete_data));
  }
}

void SetReferrerForRequest(net::URLRequest* request, const Referrer& referrer) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!referrer.url.is_valid() ||
      command_line->HasSwitch(switches::kNoReferrers)) {
    request->SetReferrer(std::string());
  } else {
    request->SetReferrer(referrer.url.spec());
  }

  net::URLRequest::ReferrerPolicy net_referrer_policy =
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
  switch (referrer.policy) {
    case blink::WebReferrerPolicyAlways:
    case blink::WebReferrerPolicyNever:
    case blink::WebReferrerPolicyOrigin:
      net_referrer_policy = net::URLRequest::NEVER_CLEAR_REFERRER;
      break;
    case blink::WebReferrerPolicyNoReferrerWhenDowngrade:
      net_referrer_policy =
          net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      break;
    case blink::WebReferrerPolicyOriginWhenCrossOrigin:
      net_referrer_policy =
          net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
      break;
    case blink::WebReferrerPolicyDefault:
    default:
      net_referrer_policy =
          command_line->HasSwitch(switches::kReducedReferrerGranularity)
              ? net::URLRequest::
                    REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN
              : net::URLRequest::
                    CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      break;
  }
  request->set_referrer_policy(net_referrer_policy);
}

// Consults the RendererSecurity policy to determine whether the
// ResourceDispatcherHostImpl should service this request.  A request might be
// disallowed if the renderer is not authorized to retrieve the request URL or
// if the renderer is attempting to upload an unauthorized file.
bool ShouldServiceRequest(int process_type,
                          int child_id,
                          const ResourceHostMsg_Request& request_data,
                          storage::FileSystemContext* file_system_context) {
  if (process_type == PROCESS_TYPE_PLUGIN)
    return true;

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Check if the renderer is permitted to request the requested URL.
  if (!policy->CanRequestURL(child_id, request_data.url)) {
    VLOG(1) << "Denied unauthorized request for "
            << request_data.url.possibly_invalid_spec();
    return false;
  }

  // Check if the renderer is permitted to upload the requested files.
  if (request_data.request_body.get()) {
    const std::vector<ResourceRequestBody::Element>* uploads =
        request_data.request_body->elements();
    std::vector<ResourceRequestBody::Element>::const_iterator iter;
    for (iter = uploads->begin(); iter != uploads->end(); ++iter) {
      if (iter->type() == ResourceRequestBody::Element::TYPE_FILE &&
          !policy->CanReadFile(child_id, iter->path())) {
        NOTREACHED() << "Denied unauthorized upload of "
                     << iter->path().value();
        return false;
      }
      if (iter->type() == ResourceRequestBody::Element::TYPE_FILE_FILESYSTEM) {
        storage::FileSystemURL url =
            file_system_context->CrackURL(iter->filesystem_url());
        if (!policy->CanReadFileSystemFile(child_id, url)) {
          NOTREACHED() << "Denied unauthorized upload of "
                       << iter->filesystem_url().spec();
          return false;
        }
      }
    }
  }

  return true;
}

void RemoveDownloadFileFromChildSecurityPolicy(int child_id,
                                               const base::FilePath& path) {
  ChildProcessSecurityPolicyImpl::GetInstance()->RevokeAllPermissionsForFile(
      child_id, path);
}

DownloadInterruptReason CallbackAndReturn(
    const DownloadUrlParameters::OnStartedCallback& started_cb,
    DownloadInterruptReason interrupt_reason) {
  if (started_cb.is_null())
    return interrupt_reason;
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          started_cb, static_cast<DownloadItem*>(NULL), interrupt_reason));

  return interrupt_reason;
}

int GetCertID(net::URLRequest* request, int child_id) {
  if (request->ssl_info().cert.get()) {
    return CertStore::GetInstance()->StoreCert(request->ssl_info().cert.get(),
                                               child_id);
  }
  return 0;
}

void NotifyRedirectOnUI(int render_process_id,
                        int render_frame_host,
                        scoped_ptr<ResourceRedirectDetails> details) {
  RenderFrameHostImpl* host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_host);
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(host));
  if (!web_contents)
    return;
  web_contents->DidGetRedirectForResourceRequest(host, *details.get());
}

void NotifyResponseOnUI(int render_process_id,
                        int render_frame_host,
                        scoped_ptr<ResourceRequestDetails> details) {
  RenderFrameHostImpl* host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_host);
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(host));
  if (!web_contents)
    return;
  web_contents->DidGetResourceResponseStart(*details.get());
}

bool IsValidatedSCT(
    const net::SignedCertificateTimestampAndStatus& sct_status) {
  return sct_status.status == net::ct::SCT_STATUS_OK;
}

storage::BlobStorageContext* GetBlobStorageContext(
    ChromeBlobStorageContext* blob_storage_context) {
  if (!blob_storage_context)
    return NULL;
  return blob_storage_context->context();
}

void AttachRequestBodyBlobDataHandles(
    ResourceRequestBody* body,
    storage::BlobStorageContext* blob_context) {
  DCHECK(blob_context);
  for (size_t i = 0; i < body->elements()->size(); ++i) {
    const ResourceRequestBody::Element& element = (*body->elements())[i];
    if (element.type() != ResourceRequestBody::Element::TYPE_BLOB)
      continue;
    scoped_ptr<storage::BlobDataHandle> handle =
        blob_context->GetBlobDataFromUUID(element.blob_uuid());
    DCHECK(handle);
    if (!handle)
      continue;
    // Ensure the blob and any attached shareable files survive until
    // upload completion. The |body| takes ownership of |handle|.
    const void* key = handle.get();
    body->SetUserData(key, handle.release());
  }
}

// PlzNavigate
// This method is called in the UI thread to send the timestamp of a resource
// request to the respective Navigator (for an UMA histogram).
void LogResourceRequestTimeOnUI(
    base::TimeTicks timestamp,
    int render_process_id,
    int render_frame_id,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (host != NULL) {
    DCHECK(host->frame_tree_node()->IsMainFrame());
    host->frame_tree_node()->navigator()->LogResourceRequestTime(
        timestamp, url);
  }
}

}  // namespace

// static
ResourceDispatcherHost* ResourceDispatcherHost::Get() {
  return g_resource_dispatcher_host;
}

ResourceDispatcherHostImpl::ResourceDispatcherHostImpl()
    : save_file_manager_(new SaveFileManager()),
      request_id_(-1),
      is_shutdown_(false),
      num_in_flight_requests_(0),
      max_num_in_flight_requests_(base::SharedMemory::GetHandleLimit()),
      max_num_in_flight_requests_per_process_(
          static_cast<int>(
              max_num_in_flight_requests_ * kMaxRequestsPerProcessRatio)),
      max_outstanding_requests_cost_per_process_(
          kMaxOutstandingRequestsCostPerProcess),
      filter_(NULL),
      delegate_(NULL),
      allow_cross_origin_auth_prompt_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!g_resource_dispatcher_host);
  g_resource_dispatcher_host = this;

  GetContentClient()->browser()->ResourceDispatcherHostCreated();

  ANNOTATE_BENIGN_RACE(
      &last_user_gesture_time_,
      "We don't care about the precise value, see http://crbug.com/92889");

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&ResourceDispatcherHostImpl::OnInit,
                                     base::Unretained(this)));

  update_load_states_timer_.reset(
      new base::RepeatingTimer<ResourceDispatcherHostImpl>());
}

ResourceDispatcherHostImpl::~ResourceDispatcherHostImpl() {
  DCHECK(outstanding_requests_stats_map_.empty());
  DCHECK(g_resource_dispatcher_host);
  g_resource_dispatcher_host = NULL;
}

// static
ResourceDispatcherHostImpl* ResourceDispatcherHostImpl::Get() {
  return g_resource_dispatcher_host;
}

void ResourceDispatcherHostImpl::SetDelegate(
    ResourceDispatcherHostDelegate* delegate) {
  delegate_ = delegate;
}

void ResourceDispatcherHostImpl::SetAllowCrossOriginAuthPrompt(bool value) {
  allow_cross_origin_auth_prompt_ = value;
}

void ResourceDispatcherHostImpl::AddResourceContext(ResourceContext* context) {
  active_resource_contexts_.insert(context);
}

void ResourceDispatcherHostImpl::RemoveResourceContext(
    ResourceContext* context) {
  CHECK(ContainsKey(active_resource_contexts_, context));
  active_resource_contexts_.erase(context);
}

void ResourceDispatcherHostImpl::CancelRequestsForContext(
    ResourceContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(context);

  CHECK(ContainsKey(active_resource_contexts_, context));

  // Note that request cancellation has side effects. Therefore, we gather all
  // the requests to cancel first, and then we start cancelling. We assert at
  // the end that there are no more to cancel since the context is about to go
  // away.
  typedef std::vector<linked_ptr<ResourceLoader>> LoaderList;
  LoaderList loaders_to_cancel;

  for (LoaderMap::iterator i = pending_loaders_.begin();
       i != pending_loaders_.end();) {
    if (i->second->GetRequestInfo()->GetContext() == context) {
      loaders_to_cancel.push_back(i->second);
      IncrementOutstandingRequestsMemory(-1, *i->second->GetRequestInfo());
      pending_loaders_.erase(i++);
    } else {
      ++i;
    }
  }

  for (BlockedLoadersMap::iterator i = blocked_loaders_map_.begin();
       i != blocked_loaders_map_.end();) {
    BlockedLoadersList* loaders = i->second;
    if (loaders->empty()) {
      // This can happen if BlockRequestsForRoute() has been called for a route,
      // but we haven't blocked any matching requests yet.
      ++i;
      continue;
    }
    ResourceRequestInfoImpl* info = loaders->front()->GetRequestInfo();
    if (info->GetContext() == context) {
      blocked_loaders_map_.erase(i++);
      for (BlockedLoadersList::const_iterator it = loaders->begin();
           it != loaders->end(); ++it) {
        linked_ptr<ResourceLoader> loader = *it;
        info = loader->GetRequestInfo();
        // We make the assumption that all requests on the list have the same
        // ResourceContext.
        DCHECK_EQ(context, info->GetContext());
        IncrementOutstandingRequestsMemory(-1, *info);
        loaders_to_cancel.push_back(loader);
      }
      delete loaders;
    } else {
      ++i;
    }
  }

#ifndef NDEBUG
  for (LoaderList::iterator i = loaders_to_cancel.begin();
       i != loaders_to_cancel.end(); ++i) {
    // There is no strict requirement that this be the case, but currently
    // downloads, streams, detachable requests, transferred requests, and
    // browser-owned requests are the only requests that aren't cancelled when
    // the associated processes go away. It may be OK for this invariant to
    // change in the future, but if this assertion fires without the invariant
    // changing, then it's indicative of a leak.
    DCHECK((*i)->GetRequestInfo()->IsDownload() ||
           (*i)->GetRequestInfo()->is_stream() ||
           ((*i)->GetRequestInfo()->detachable_handler() &&
            (*i)->GetRequestInfo()->detachable_handler()->is_detached()) ||
           (*i)->GetRequestInfo()->GetProcessType() == PROCESS_TYPE_BROWSER ||
           (*i)->is_transferring());
  }
#endif

  loaders_to_cancel.clear();

  // Validate that no more requests for this context were added.
  for (LoaderMap::const_iterator i = pending_loaders_.begin();
       i != pending_loaders_.end(); ++i) {
    // http://crbug.com/90971
    CHECK_NE(i->second->GetRequestInfo()->GetContext(), context);
  }

  for (BlockedLoadersMap::const_iterator i = blocked_loaders_map_.begin();
       i != blocked_loaders_map_.end(); ++i) {
    BlockedLoadersList* loaders = i->second;
    if (!loaders->empty()) {
      ResourceRequestInfoImpl* info = loaders->front()->GetRequestInfo();
      // http://crbug.com/90971
      CHECK_NE(info->GetContext(), context);
    }
  }
}

DownloadInterruptReason ResourceDispatcherHostImpl::BeginDownload(
    scoped_ptr<net::URLRequest> request,
    const Referrer& referrer,
    bool is_content_initiated,
    ResourceContext* context,
    int child_id,
    int route_id,
    bool prefer_cache,
    bool do_not_prompt_for_login,
    scoped_ptr<DownloadSaveInfo> save_info,
    uint32 download_id,
    const DownloadStartedCallback& started_callback) {
  if (is_shutdown_)
    return CallbackAndReturn(started_callback,
                             DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN);

  const GURL& url = request->original_url();

  // http://crbug.com/90971
  char url_buf[128];
  base::strlcpy(url_buf, url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);
  CHECK(ContainsKey(active_resource_contexts_, context));

  SetReferrerForRequest(request.get(), referrer);

  int extra_load_flags = net::LOAD_NORMAL;
  if (prefer_cache) {
    // If there is upload data attached, only retrieve from cache because there
    // is no current mechanism to prompt the user for their consent for a
    // re-post. For GETs, try to retrieve data from the cache and skip
    // validating the entry if present.
    if (request->get_upload() != NULL)
      extra_load_flags |= net::LOAD_ONLY_FROM_CACHE;
    else
      extra_load_flags |= net::LOAD_PREFERRING_CACHE;
  } else {
    extra_load_flags |= net::LOAD_DISABLE_CACHE;
  }
  request->SetLoadFlags(request->load_flags() | extra_load_flags);

  // We treat a download as a main frame load, and thus update the policy URL on
  // redirects.
  //
  // TODO(davidben): Is this correct? If this came from a
  // ViewHostMsg_DownloadUrl in a frame, should it have first-party URL set
  // appropriately?
  request->set_first_party_url_policy(
      net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT);

  // Check if the renderer is permitted to request the requested URL.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->
          CanRequestURL(child_id, url)) {
    VLOG(1) << "Denied unauthorized download request for "
            << url.possibly_invalid_spec();
    return CallbackAndReturn(started_callback,
                             DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST);
  }

  request_id_--;

  const net::URLRequestContext* request_context = context->GetRequestContext();
  if (!request_context->job_factory()->IsHandledURL(url)) {
    VLOG(1) << "Download request for unsupported protocol: "
            << url.possibly_invalid_spec();
    return CallbackAndReturn(started_callback,
                             DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST);
  }

  ResourceRequestInfoImpl* extra_info =
      CreateRequestInfo(child_id, route_id, true, context);
  extra_info->set_do_not_prompt_for_login(do_not_prompt_for_login);
  extra_info->AssociateWithRequest(request.get());  // Request takes ownership.

  if (request->url().SchemeIs(url::kBlobScheme)) {
    ChromeBlobStorageContext* blob_context =
        GetChromeBlobStorageContextForResourceContext(context);
    storage::BlobProtocolHandler::SetRequestedBlobDataHandle(
        request.get(),
        blob_context->context()->GetBlobDataFromPublicURL(request->url()));
  }

  // From this point forward, the |DownloadResourceHandler| is responsible for
  // |started_callback|.
  scoped_ptr<ResourceHandler> handler(
      CreateResourceHandlerForDownload(request.get(), is_content_initiated,
                                       true, download_id, save_info.Pass(),
                                       started_callback));

  BeginRequestInternal(request.Pass(), handler.Pass());

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

void ResourceDispatcherHostImpl::ClearLoginDelegateForRequest(
    net::URLRequest* request) {
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  if (info) {
    ResourceLoader* loader = GetLoader(info->GetGlobalRequestID());
    if (loader)
      loader->ClearLoginDelegate();
  }
}

void ResourceDispatcherHostImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&ResourceDispatcherHostImpl::OnShutdown,
                                     base::Unretained(this)));
}

scoped_ptr<ResourceHandler>
ResourceDispatcherHostImpl::CreateResourceHandlerForDownload(
    net::URLRequest* request,
    bool is_content_initiated,
    bool must_download,
    uint32 id,
    scoped_ptr<DownloadSaveInfo> save_info,
    const DownloadUrlParameters::OnStartedCallback& started_cb) {
  scoped_ptr<ResourceHandler> handler(
      new DownloadResourceHandler(id, request, started_cb, save_info.Pass()));
  if (delegate_) {
    const ResourceRequestInfo* request_info(
        ResourceRequestInfo::ForRequest(request));

    ScopedVector<ResourceThrottle> throttles;
    delegate_->DownloadStarting(
        request, request_info->GetContext(), request_info->GetChildID(),
        request_info->GetRouteID(), request_info->GetRequestID(),
        is_content_initiated, must_download, &throttles);
    if (!throttles.empty()) {
      handler.reset(
          new ThrottlingResourceHandler(
              handler.Pass(), request, throttles.Pass()));
    }
  }
  return handler.Pass();
}

scoped_ptr<ResourceHandler>
ResourceDispatcherHostImpl::MaybeInterceptAsStream(net::URLRequest* request,
                                                   ResourceResponse* response,
                                                   std::string* payload) {
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  const std::string& mime_type = response->head.mime_type;

  GURL origin;
  if (!delegate_ ||
      !delegate_->ShouldInterceptResourceAsStream(request,
                                                  mime_type,
                                                  &origin,
                                                  payload)) {
    return scoped_ptr<ResourceHandler>();
  }

  StreamContext* stream_context =
      GetStreamContextForResourceContext(info->GetContext());

  scoped_ptr<StreamResourceHandler> handler(
      new StreamResourceHandler(request,
                                stream_context->registry(),
                                origin));

  info->set_is_stream(true);
  scoped_ptr<StreamInfo> stream_info(new StreamInfo);
  stream_info->handle = handler->stream()->CreateHandle();
  stream_info->original_url = request->url();
  stream_info->mime_type = mime_type;
  // Make a copy of the response headers so it is safe to pass across threads;
  // the old handler (AsyncResourceHandler) may modify it in parallel via the
  // ResourceDispatcherHostDelegate.
  if (response->head.headers.get()) {
    stream_info->response_headers =
        new net::HttpResponseHeaders(response->head.headers->raw_headers());
  }
  delegate_->OnStreamCreated(request, stream_info.Pass());
  return handler.Pass();
}

ResourceDispatcherHostLoginDelegate*
ResourceDispatcherHostImpl::CreateLoginDelegate(
    ResourceLoader* loader,
    net::AuthChallengeInfo* auth_info) {
  if (!delegate_)
    return NULL;

  return delegate_->CreateLoginDelegate(auth_info, loader->request());
}

bool ResourceDispatcherHostImpl::HandleExternalProtocol(ResourceLoader* loader,
                                                        const GURL& url) {
  if (!delegate_)
    return false;

  ResourceRequestInfoImpl* info = loader->GetRequestInfo();

  if (!IsResourceTypeFrame(info->GetResourceType()))
    return false;

  const net::URLRequestJobFactory* job_factory =
      info->GetContext()->GetRequestContext()->job_factory();
  if (job_factory->IsHandledURL(url))
    return false;

  return delegate_->HandleExternalProtocol(
      url, info->GetChildID(), info->GetRouteID(), info->IsMainFrame(),
      info->GetPageTransition(), info->HasUserGesture());
}

void ResourceDispatcherHostImpl::DidStartRequest(ResourceLoader* loader) {
  // Make sure we have the load state monitor running.
  if (!update_load_states_timer_->IsRunning() &&
      scheduler_->HasLoadingClients()) {
    update_load_states_timer_->Start(
        FROM_HERE, TimeDelta::FromMilliseconds(kUpdateLoadStatesIntervalMsec),
        this, &ResourceDispatcherHostImpl::UpdateLoadInfo);
  }
}

void ResourceDispatcherHostImpl::DidReceiveRedirect(ResourceLoader* loader,
                                                    const GURL& new_url) {
  ResourceRequestInfoImpl* info = loader->GetRequestInfo();

  int render_process_id, render_frame_host;
  if (!info->GetAssociatedRenderFrame(&render_process_id, &render_frame_host))
    return;

  // Notify the observers on the UI thread.
  scoped_ptr<ResourceRedirectDetails> detail(new ResourceRedirectDetails(
      loader->request(),
      GetCertID(loader->request(), info->GetChildID()),
      new_url));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &NotifyRedirectOnUI,
          render_process_id, render_frame_host, base::Passed(&detail)));
}

void ResourceDispatcherHostImpl::DidReceiveResponse(ResourceLoader* loader) {
  ResourceRequestInfoImpl* info = loader->GetRequestInfo();
  net::URLRequest* request = loader->request();
  if (request->was_fetched_via_proxy() &&
      request->was_fetched_via_spdy() &&
      request->url().SchemeIs(url::kHttpScheme)) {
    scheduler_->OnReceivedSpdyProxiedHttpResponse(
        info->GetChildID(), info->GetRouteID());
  }

  int render_process_id, render_frame_host;
  if (!info->GetAssociatedRenderFrame(&render_process_id, &render_frame_host))
    return;

  // Notify the observers on the UI thread.
  scoped_ptr<ResourceRequestDetails> detail(new ResourceRequestDetails(
      request, GetCertID(request, info->GetChildID())));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &NotifyResponseOnUI,
          render_process_id, render_frame_host, base::Passed(&detail)));
}

void ResourceDispatcherHostImpl::DidFinishLoading(ResourceLoader* loader) {
  ResourceRequestInfo* info = loader->GetRequestInfo();

  // Record final result of all resource loads.
  if (info->GetResourceType() == RESOURCE_TYPE_MAIN_FRAME) {
    // This enumeration has "3" appended to its name to distinguish it from
    // older versions.
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "Net.ErrorCodesForMainFrame3",
        -loader->request()->status().error());

    // Record time to success and error for the most common errors, and for
    // the aggregate remainder errors.
    base::TimeDelta request_loading_time(
        base::TimeTicks::Now() - loader->request()->creation_time());
    switch (loader->request()->status().error()) {
      case net::OK:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.Success", request_loading_time);
        break;
      case net::ERR_ABORTED:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.ErrAborted", request_loading_time);
        break;
      case net::ERR_CONNECTION_RESET:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.ErrConnectionReset", request_loading_time);
        break;
      case net::ERR_CONNECTION_TIMED_OUT:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.ErrConnectionTimedOut", request_loading_time);
        break;
      case net::ERR_INTERNET_DISCONNECTED:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.ErrInternetDisconnected", request_loading_time);
        break;
      case net::ERR_NAME_NOT_RESOLVED:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.ErrNameNotResolved", request_loading_time);
        break;
      case net::ERR_TIMED_OUT:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.ErrTimedOut", request_loading_time);
        break;
      default:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.MiscError", request_loading_time);
        break;
    }

    if (loader->request()->url().SchemeIsCryptographic()) {
      if (loader->request()->url().host() == "www.google.com") {
        UMA_HISTOGRAM_SPARSE_SLOWLY("Net.ErrorCodesForHTTPSGoogleMainFrame2",
                                    -loader->request()->status().error());
      }

      int num_valid_scts = std::count_if(
          loader->request()->ssl_info().signed_certificate_timestamps.begin(),
          loader->request()->ssl_info().signed_certificate_timestamps.end(),
          IsValidatedSCT);
      UMA_HISTOGRAM_COUNTS_100(
          "Net.CertificateTransparency.MainFrameValidSCTCount", num_valid_scts);
    }
  } else {
    if (info->GetResourceType() == RESOURCE_TYPE_IMAGE) {
      UMA_HISTOGRAM_SPARSE_SLOWLY(
          "Net.ErrorCodesForImages",
          -loader->request()->status().error());
    }
    // This enumeration has "2" appended to distinguish it from older versions.
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "Net.ErrorCodesForSubresources2",
        -loader->request()->status().error());
  }

  if (loader->request()->url().SchemeIsCryptographic()) {
    RecordCertificateHistograms(loader->request()->ssl_info(),
                                info->GetResourceType());
  }

  if (delegate_)
    delegate_->RequestComplete(loader->request());

  // Destroy the ResourceLoader.
  RemovePendingRequest(info->GetChildID(), info->GetRequestID());
}

void ResourceDispatcherHostImpl::OnInit() {
  scheduler_.reset(new ResourceScheduler);
}

void ResourceDispatcherHostImpl::OnShutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  is_shutdown_ = true;
  pending_loaders_.clear();

  // Make sure we shutdown the timer now, otherwise by the time our destructor
  // runs if the timer is still running the Task is deleted twice (once by
  // the MessageLoop and the second time by RepeatingTimer).
  update_load_states_timer_.reset();

  // Clear blocked requests if any left.
  // Note that we have to do this in 2 passes as we cannot call
  // CancelBlockedRequestsForRoute while iterating over
  // blocked_loaders_map_, as it modifies it.
  std::set<GlobalRoutingID> ids;
  for (BlockedLoadersMap::const_iterator iter = blocked_loaders_map_.begin();
       iter != blocked_loaders_map_.end(); ++iter) {
    std::pair<std::set<GlobalRoutingID>::iterator, bool> result =
        ids.insert(iter->first);
    // We should not have duplicates.
    DCHECK(result.second);
  }
  for (std::set<GlobalRoutingID>::const_iterator iter = ids.begin();
       iter != ids.end(); ++iter) {
    CancelBlockedRequestsForRoute(iter->child_id, iter->route_id);
  }

  scheduler_.reset();
}

bool ResourceDispatcherHostImpl::OnMessageReceived(
    const IPC::Message& message,
    ResourceMessageFilter* filter) {
  filter_ = filter;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ResourceDispatcherHostImpl, message)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_RequestResource, OnRequestResource)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ResourceHostMsg_SyncLoad, OnSyncLoad)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_ReleaseDownloadedFile,
                        OnReleaseDownloadedFile)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_DataDownloaded_ACK, OnDataDownloadedACK)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_UploadProgress_ACK, OnUploadProgressACK)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_CancelRequest, OnCancelRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled && IPC_MESSAGE_ID_CLASS(message.type()) == ResourceMsgStart) {
    base::PickleIterator iter(message);
    int request_id = -1;
    bool ok = iter.ReadInt(&request_id);
    DCHECK(ok);
    GlobalRequestID id(filter_->child_id(), request_id);
    DelegateMap::iterator it = delegate_map_.find(id);
    if (it != delegate_map_.end()) {
      base::ObserverList<ResourceMessageDelegate>::Iterator del_it(it->second);
      ResourceMessageDelegate* delegate;
      while (!handled && (delegate = del_it.GetNext()) != NULL) {
        handled = delegate->OnMessageReceived(message);
      }
    }

    // As the unhandled resource message effectively has no consumer, mark it as
    // handled to prevent needless propagation through the filter pipeline.
    handled = true;
  }

  filter_ = NULL;
  return handled;
}

void ResourceDispatcherHostImpl::OnRequestResource(
    int routing_id,
    int request_id,
    const ResourceHostMsg_Request& request_data) {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/477117 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "477117 ResourceDispatcherHostImpl::OnRequestResource"));
  // When logging time-to-network only care about main frame and non-transfer
  // navigations.
  if (request_data.resource_type == RESOURCE_TYPE_MAIN_FRAME &&
      request_data.transferred_request_request_id == -1 &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&LogResourceRequestTimeOnUI,
                   TimeTicks::Now(),
                   filter_->child_id(),
                   request_data.render_frame_id,
                   request_data.url));
  }
  BeginRequest(request_id, request_data, NULL, routing_id);
}

// Begins a resource request with the given params on behalf of the specified
// child process.  Responses will be dispatched through the given receiver. The
// process ID is used to lookup WebContentsImpl from routing_id's in the case of
// a request from a renderer.  request_context is the cookie/cache context to be
// used for this request.
//
// If sync_result is non-null, then a SyncLoad reply will be generated, else
// a normal asynchronous set of response messages will be generated.
void ResourceDispatcherHostImpl::OnSyncLoad(
    int request_id,
    const ResourceHostMsg_Request& request_data,
    IPC::Message* sync_result) {
  BeginRequest(request_id, request_data, sync_result,
               sync_result->routing_id());
}

void ResourceDispatcherHostImpl::UpdateRequestForTransfer(
    int child_id,
    int route_id,
    int request_id,
    const ResourceHostMsg_Request& request_data,
    const linked_ptr<ResourceLoader>& loader) {
  ResourceRequestInfoImpl* info = loader->GetRequestInfo();
  GlobalRoutingID old_routing_id(
      request_data.transferred_request_child_id, info->GetRouteID());
  GlobalRequestID old_request_id(request_data.transferred_request_child_id,
                                 request_data.transferred_request_request_id);
  GlobalRoutingID new_routing_id(child_id, route_id);
  GlobalRequestID new_request_id(child_id, request_id);

  // Clear out data that depends on |info| before updating it.
  // We always need to move the memory stats to the new process.  In contrast,
  // stats.num_requests is only tracked for some requests (those that require
  // file descriptors for their shared memory buffer).
  IncrementOutstandingRequestsMemory(-1, *info);
  bool should_update_count = info->counted_as_in_flight_request();
  if (should_update_count)
    IncrementOutstandingRequestsCount(-1, info);
  pending_loaders_.erase(old_request_id);

  // ResourceHandlers should always get state related to the request from the
  // ResourceRequestInfo rather than caching it locally.  This lets us update
  // the info object when a transfer occurs.
  info->UpdateForTransfer(child_id, route_id, request_data.origin_pid,
                          request_id, request_data.parent_render_frame_id,
                          filter_->GetWeakPtr());

  // Update maps that used the old IDs, if necessary.  Some transfers in tests
  // do not actually use a different ID, so not all maps need to be updated.
  pending_loaders_[new_request_id] = loader;
  IncrementOutstandingRequestsMemory(1, *info);
  if (should_update_count)
    IncrementOutstandingRequestsCount(1, info);
  if (old_routing_id != new_routing_id) {
    if (blocked_loaders_map_.find(old_routing_id) !=
            blocked_loaders_map_.end()) {
      blocked_loaders_map_[new_routing_id] =
          blocked_loaders_map_[old_routing_id];
      blocked_loaders_map_.erase(old_routing_id);
    }
  }
  if (old_request_id != new_request_id) {
    DelegateMap::iterator it = delegate_map_.find(old_request_id);
    if (it != delegate_map_.end()) {
      // Tell each delegate that the request ID has changed.
      base::ObserverList<ResourceMessageDelegate>::Iterator del_it(it->second);
      ResourceMessageDelegate* delegate;
      while ((delegate = del_it.GetNext()) != NULL) {
        delegate->set_request_id(new_request_id);
      }
      // Now store the observer list under the new request ID.
      delegate_map_[new_request_id] = delegate_map_[old_request_id];
      delegate_map_.erase(old_request_id);
    }
  }

  AppCacheInterceptor::CompleteCrossSiteTransfer(
      loader->request(),
      child_id,
      request_data.appcache_host_id);

  ServiceWorkerRequestHandler* handler =
      ServiceWorkerRequestHandler::GetHandler(loader->request());
  if (handler) {
    handler->CompleteCrossSiteTransfer(
        child_id, request_data.service_worker_provider_id);
  }

  // We should have a CrossSiteResourceHandler to finish the transfer.
  DCHECK(info->cross_site_handler());
}

void ResourceDispatcherHostImpl::BeginRequest(
    int request_id,
    const ResourceHostMsg_Request& request_data,
    IPC::Message* sync_result,  // only valid for sync
    int route_id) {
  int process_type = filter_->process_type();
  int child_id = filter_->child_id();

  // Reject invalid priority.
  if (request_data.priority < net::MINIMUM_PRIORITY ||
      request_data.priority > net::MAXIMUM_PRIORITY) {
    bad_message::ReceivedBadMessage(filter_, bad_message::RDH_INVALID_PRIORITY);
    return;
  }

  // If we crash here, figure out what URL the renderer was requesting.
  // http://crbug.com/91398
  char url_buf[128];
  base::strlcpy(url_buf, request_data.url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);

  // If the request that's coming in is being transferred from another process,
  // we want to reuse and resume the old loader rather than start a new one.
  LoaderMap::iterator it = pending_loaders_.find(
      GlobalRequestID(request_data.transferred_request_child_id,
                      request_data.transferred_request_request_id));
  if (it != pending_loaders_.end()) {
    // If the request is transferring to a new process, we can update our
    // state and let it resume with its existing ResourceHandlers.
    if (it->second->is_transferring()) {
      linked_ptr<ResourceLoader> deferred_loader = it->second;
      UpdateRequestForTransfer(child_id, route_id, request_id,
                                request_data, deferred_loader);

      deferred_loader->CompleteTransfer();
    } else {
      bad_message::ReceivedBadMessage(
          filter_, bad_message::RDH_REQUEST_NOT_TRANSFERRING);
    }
    return;
  }

  ResourceContext* resource_context = NULL;
  net::URLRequestContext* request_context = NULL;
  filter_->GetContexts(request_data, &resource_context, &request_context);
  // http://crbug.com/90971
  CHECK(ContainsKey(active_resource_contexts_, resource_context));

  if (is_shutdown_ ||
      !ShouldServiceRequest(process_type, child_id, request_data,
                            filter_->file_system_context())) {
    AbortRequestBeforeItStarts(filter_, sync_result, request_id);
    return;
  }

  // Allow the observer to block/handle the request.
  if (delegate_ && !delegate_->ShouldBeginRequest(request_data.method,
                                                  request_data.url,
                                                  request_data.resource_type,
                                                  resource_context)) {
    AbortRequestBeforeItStarts(filter_, sync_result, request_id);
    return;
  }

  // Construct the request.
  scoped_ptr<net::URLRequest> new_request = request_context->CreateRequest(
      request_data.url, request_data.priority, NULL);

  new_request->set_method(request_data.method);
  new_request->set_first_party_for_cookies(
      request_data.first_party_for_cookies);

  // If the request is a MAIN_FRAME request, the first-party URL gets updated on
  // redirects.
  if (request_data.resource_type == RESOURCE_TYPE_MAIN_FRAME) {
    new_request->set_first_party_url_policy(
        net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT);
  }

  const Referrer referrer(request_data.referrer, request_data.referrer_policy);
  SetReferrerForRequest(new_request.get(), referrer);

  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(request_data.headers);
  new_request->SetExtraRequestHeaders(headers);

  storage::BlobStorageContext* blob_context =
      GetBlobStorageContext(filter_->blob_storage_context());
  // Resolve elements from request_body and prepare upload data.
  if (request_data.request_body.get()) {
    // |blob_context| could be null when the request is from the plugins because
    // ResourceMessageFilters created in PluginProcessHost don't have the blob
    // context.
    if (blob_context) {
      // Attaches the BlobDataHandles to request_body not to free the blobs and
      // any attached shareable files until upload completion. These data will
      // be used in UploadDataStream and ServiceWorkerURLRequestJob.
      AttachRequestBodyBlobDataHandles(
          request_data.request_body.get(),
          blob_context);
    }
    new_request->set_upload(UploadDataStreamBuilder::Build(
        request_data.request_body.get(),
        blob_context,
        filter_->file_system_context(),
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)
            .get()));
  }

  bool allow_download = request_data.allow_download &&
      IsResourceTypeFrame(request_data.resource_type);
  bool do_not_prompt_for_login = request_data.do_not_prompt_for_login;
  bool is_sync_load = sync_result != NULL;
  int load_flags =
      BuildLoadFlagsForRequest(request_data, child_id, is_sync_load);
  if (request_data.resource_type == RESOURCE_TYPE_PREFETCH ||
      request_data.resource_type == RESOURCE_TYPE_FAVICON) {
    do_not_prompt_for_login = true;
  }
  if (request_data.resource_type == RESOURCE_TYPE_IMAGE &&
      HTTP_AUTH_RELATION_BLOCKED_CROSS ==
          HttpAuthRelationTypeOf(request_data.url,
                                 request_data.first_party_for_cookies)) {
    // Prevent third-party image content from prompting for login, as this
    // is often a scam to extract credentials for another domain from the user.
    // Only block image loads, as the attack applies largely to the "src"
    // property of the <img> tag. It is common for web properties to allow
    // untrusted values for <img src>; this is considered a fair thing for an
    // HTML sanitizer to do. Conversely, any HTML sanitizer that didn't
    // filter sources for <script>, <link>, <embed>, <object>, <iframe> tags
    // would be considered vulnerable in and of itself.
    do_not_prompt_for_login = true;
    load_flags |= net::LOAD_DO_NOT_USE_EMBEDDED_IDENTITY;
  }

  // Sync loads should have maximum priority and should be the only
  // requets that have the ignore limits flag set.
  if (is_sync_load) {
    DCHECK_EQ(request_data.priority, net::MAXIMUM_PRIORITY);
    DCHECK_NE(load_flags & net::LOAD_IGNORE_LIMITS, 0);
  } else {
    DCHECK_EQ(load_flags & net::LOAD_IGNORE_LIMITS, 0);
  }
  new_request->SetLoadFlags(load_flags);

  // Make extra info and read footer (contains request ID).
  ResourceRequestInfoImpl* extra_info = new ResourceRequestInfoImpl(
      process_type, child_id, route_id,
      -1,  // frame_tree_node_id
      request_data.origin_pid,
      request_id,
      request_data.render_frame_id,
      request_data.is_main_frame,
      request_data.parent_is_main_frame,
      request_data.parent_render_frame_id,
      request_data.resource_type,
      request_data.transition_type,
      request_data.should_replace_current_entry,
      false,  // is download
      false,  // is stream
      allow_download,
      request_data.has_user_gesture,
      request_data.enable_load_timing,
      request_data.enable_upload_progress,
      do_not_prompt_for_login,
      request_data.referrer_policy,
      request_data.visiblity_state,
      resource_context, filter_->GetWeakPtr(),
      !is_sync_load);
  // Request takes ownership.
  extra_info->AssociateWithRequest(new_request.get());

  if (new_request->url().SchemeIs(url::kBlobScheme)) {
    // Hang on to a reference to ensure the blob is not released prior
    // to the job being started.
    storage::BlobProtocolHandler::SetRequestedBlobDataHandle(
        new_request.get(),
        filter_->blob_storage_context()->context()->GetBlobDataFromPublicURL(
            new_request->url()));
  }

  // Initialize the service worker handler for the request. We don't use
  // ServiceWorker for synchronous loads to avoid renderer deadlocks.
  ServiceWorkerRequestHandler::InitializeHandler(
      new_request.get(),
      filter_->service_worker_context(),
      blob_context,
      child_id,
      request_data.service_worker_provider_id,
      request_data.skip_service_worker || is_sync_load,
      request_data.fetch_request_mode,
      request_data.fetch_credentials_mode,
      request_data.resource_type,
      request_data.fetch_request_context_type,
      request_data.fetch_frame_type,
      request_data.request_body);

  // Have the appcache associate its extra info with the request.
  AppCacheInterceptor::SetExtraRequestInfo(
      new_request.get(), filter_->appcache_service(), child_id,
      request_data.appcache_host_id, request_data.resource_type,
      request_data.should_reset_appcache);

  scoped_ptr<ResourceHandler> handler(
       CreateResourceHandler(
           new_request.get(),
           request_data, sync_result, route_id, process_type, child_id,
           resource_context));

  if (handler)
    BeginRequestInternal(new_request.Pass(), handler.Pass());
}

scoped_ptr<ResourceHandler> ResourceDispatcherHostImpl::CreateResourceHandler(
    net::URLRequest* request,
    const ResourceHostMsg_Request& request_data,
    IPC::Message* sync_result,
    int route_id,
    int process_type,
    int child_id,
    ResourceContext* resource_context) {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/456331 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456331 ResourceDispatcherHostImpl::CreateResourceHandler"));
  // Construct the IPC resource handler.
  scoped_ptr<ResourceHandler> handler;
  if (sync_result) {
    // download_to_file is not supported for synchronous requests.
    if (request_data.download_to_file) {
      bad_message::ReceivedBadMessage(filter_, bad_message::RDH_BAD_DOWNLOAD);
      return scoped_ptr<ResourceHandler>();
    }

    handler.reset(new SyncResourceHandler(request, sync_result, this));
  } else {
    handler.reset(new AsyncResourceHandler(request, this));

    // The RedirectToFileResourceHandler depends on being next in the chain.
    if (request_data.download_to_file) {
      handler.reset(
          new RedirectToFileResourceHandler(handler.Pass(), request));
    }
  }

  // Prefetches and <a ping> requests outlive their child process.
  if (!sync_result && IsDetachableResourceType(request_data.resource_type)) {
    handler.reset(new DetachableResourceHandler(
        request,
        base::TimeDelta::FromMilliseconds(kDefaultDetachableCancelDelayMs),
        handler.Pass()));
  }

  // PlzNavigate: If using --enable-browser-side-navigation, the
  // CrossSiteResourceHandler is not needed. This codepath is not used for the
  // actual navigation request, but only the subsequent blob URL load. This does
  // not require request transfers.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    // Install a CrossSiteResourceHandler for all main frame requests. This will
    // check whether a transfer is required and, if so, pause for the UI thread
    // to drive the transfer.
    bool is_swappable_navigation =
        request_data.resource_type == RESOURCE_TYPE_MAIN_FRAME;
    // If we are using --site-per-process, install it for subframes as well.
    if (!is_swappable_navigation &&
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSitePerProcess)) {
      is_swappable_navigation =
          request_data.resource_type == RESOURCE_TYPE_SUB_FRAME;
    }
    if (is_swappable_navigation && process_type == PROCESS_TYPE_RENDERER)
      handler.reset(new CrossSiteResourceHandler(handler.Pass(), request));
  }

  return AddStandardHandlers(request, request_data.resource_type,
                             resource_context, filter_->appcache_service(),
                             child_id, route_id, handler.Pass());
}

scoped_ptr<ResourceHandler> ResourceDispatcherHostImpl::AddStandardHandlers(
    net::URLRequest* request,
    ResourceType resource_type,
    ResourceContext* resource_context,
    AppCacheService* appcache_service,
    int child_id,
    int route_id,
    scoped_ptr<ResourceHandler> handler) {

  PluginService* plugin_service = nullptr;
#if defined(ENABLE_PLUGINS)
  plugin_service = PluginService::GetInstance();
#endif
  // Insert a buffered event handler before the actual one.
  handler.reset(new MimeTypeResourceHandler(handler.Pass(), this,
                                            plugin_service, request));

  ScopedVector<ResourceThrottle> throttles;
  if (delegate_) {
    delegate_->RequestBeginning(request,
                                resource_context,
                                appcache_service,
                                resource_type,
                                &throttles);
  }

  if (request->has_upload()) {
    // Block power save while uploading data.
    throttles.push_back(new PowerSaveBlockResourceThrottle());
  }

  throttles.push_back(
      scheduler_->ScheduleRequest(child_id, route_id, request).release());

  handler.reset(
      new ThrottlingResourceHandler(handler.Pass(), request, throttles.Pass()));

  return handler.Pass();
}

void ResourceDispatcherHostImpl::OnReleaseDownloadedFile(int request_id) {
  UnregisterDownloadedTempFile(filter_->child_id(), request_id);
}

void ResourceDispatcherHostImpl::OnDataDownloadedACK(int request_id) {
  // TODO(michaeln): maybe throttle DataDownloaded messages
}

void ResourceDispatcherHostImpl::RegisterDownloadedTempFile(
    int child_id, int request_id, const base::FilePath& file_path) {
  scoped_refptr<ShareableFileReference> reference =
      ShareableFileReference::Get(file_path);
  DCHECK(reference.get());

  registered_temp_files_[child_id][request_id] = reference;
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
      child_id, reference->path());

  // When the temp file is deleted, revoke permissions that the renderer has
  // to that file. This covers an edge case where the file is deleted and then
  // the same name is re-used for some other purpose, we don't want the old
  // renderer to still have access to it.
  //
  // We do this when the file is deleted because the renderer can take a blob
  // reference to the temp file that outlives the url loaded that it was
  // loaded with to keep the file (and permissions) alive.
  reference->AddFinalReleaseCallback(
      base::Bind(&RemoveDownloadFileFromChildSecurityPolicy,
                 child_id));
}

void ResourceDispatcherHostImpl::UnregisterDownloadedTempFile(
    int child_id, int request_id) {
  DeletableFilesMap& map = registered_temp_files_[child_id];
  DeletableFilesMap::iterator found = map.find(request_id);
  if (found == map.end())
    return;

  map.erase(found);

  // Note that we don't remove the security bits here. This will be done
  // when all file refs are deleted (see RegisterDownloadedTempFile).
}

bool ResourceDispatcherHostImpl::Send(IPC::Message* message) {
  delete message;
  return false;
}

void ResourceDispatcherHostImpl::OnUploadProgressACK(int request_id) {
  ResourceLoader* loader = GetLoader(filter_->child_id(), request_id);
  if (loader)
    loader->OnUploadProgressACK();
}

// Note that this cancel is subtly different from the other
// CancelRequest methods in this file, which also tear down the loader.
void ResourceDispatcherHostImpl::OnCancelRequest(int request_id) {
  int child_id = filter_->child_id();

  // When the old renderer dies, it sends a message to us to cancel its
  // requests.
  if (IsTransferredNavigation(GlobalRequestID(child_id, request_id)))
    return;

  ResourceLoader* loader = GetLoader(child_id, request_id);
  if (!loader) {
    // We probably want to remove this warning eventually, but I wanted to be
    // able to notice when this happens during initial development since it
    // should be rare and may indicate a bug.
    DVLOG(1) << "Canceling a request that wasn't found";
    return;
  }

  loader->CancelRequest(true);
}

ResourceRequestInfoImpl* ResourceDispatcherHostImpl::CreateRequestInfo(
    int child_id,
    int route_id,
    bool download,
    ResourceContext* context) {
  return new ResourceRequestInfoImpl(
      PROCESS_TYPE_RENDERER,
      child_id,
      route_id,
      -1,  // frame_tree_node_id
      0,
      request_id_,
      MSG_ROUTING_NONE,  // render_frame_id
      false,             // is_main_frame
      false,             // parent_is_main_frame
      -1,                // parent_render_frame_id
      RESOURCE_TYPE_SUB_RESOURCE,
      ui::PAGE_TRANSITION_LINK,
      false,     // should_replace_current_entry
      download,  // is_download
      false,     // is_stream
      download,  // allow_download
      false,     // has_user_gesture
      false,     // enable_load_timing
      false,     // enable_upload_progress
      false,     // do_not_prompt_for_login
      blink::WebReferrerPolicyDefault,
      blink::WebPageVisibilityStateVisible,
      context,
      base::WeakPtr<ResourceMessageFilter>(),  // filter
      true);                                   // is_async
}

void ResourceDispatcherHostImpl::OnRenderViewHostCreated(int child_id,
                                                         int route_id,
                                                         bool is_visible,
                                                         bool is_audible) {
  scheduler_->OnClientCreated(child_id, route_id, is_visible, is_audible);
}

void ResourceDispatcherHostImpl::OnRenderViewHostDeleted(
    int child_id,
    int route_id) {
  scheduler_->OnClientDeleted(child_id, route_id);
  CancelRequestsForRoute(child_id, route_id);
}

void ResourceDispatcherHostImpl::OnRenderViewHostSetIsLoading(int child_id,
                                                              int route_id,
                                                              bool is_loading) {
  scheduler_->OnLoadingStateChanged(child_id, route_id, !is_loading);
}

void ResourceDispatcherHostImpl::OnRenderViewHostWasHidden(
    int child_id,
    int route_id) {
  scheduler_->OnVisibilityChanged(child_id, route_id, false);
}

void ResourceDispatcherHostImpl::OnRenderViewHostWasShown(
    int child_id,
    int route_id) {
  scheduler_->OnVisibilityChanged(child_id, route_id, true);
}

void ResourceDispatcherHostImpl::OnAudioRenderHostStreamStateChanged(
    int child_id,
    int route_id,
    bool is_playing) {
  // The ResourceDispatcherHost may have already been shut down.
  // See http://crbug.com/455098
  if (!scheduler_)
    return;
  scheduler_->OnAudibilityChanged(child_id, route_id, is_playing);
}

// This function is only used for saving feature.
void ResourceDispatcherHostImpl::BeginSaveFile(
    const GURL& url,
    const Referrer& referrer,
    int child_id,
    int route_id,
    ResourceContext* context) {
  if (is_shutdown_)
    return;

  // http://crbug.com/90971
  char url_buf[128];
  base::strlcpy(url_buf, url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);
  CHECK(ContainsKey(active_resource_contexts_, context));

  request_id_--;

  const net::URLRequestContext* request_context = context->GetRequestContext();
  bool known_proto =
      request_context->job_factory()->IsHandledURL(url);
  if (!known_proto) {
    // Since any URLs which have non-standard scheme have been filtered
    // by save manager(see GURL::SchemeIsStandard). This situation
    // should not happen.
    NOTREACHED();
    return;
  }

  scoped_ptr<net::URLRequest> request(
      request_context->CreateRequest(url, net::DEFAULT_PRIORITY, NULL));
  request->set_method("GET");
  SetReferrerForRequest(request.get(), referrer);

  // So far, for saving page, we need fetch content from cache, in the
  // future, maybe we can use a configuration to configure this behavior.
  request->SetLoadFlags(net::LOAD_PREFERRING_CACHE);

  // Since we're just saving some resources we need, disallow downloading.
  ResourceRequestInfoImpl* extra_info =
      CreateRequestInfo(child_id, route_id, false, context);
  extra_info->AssociateWithRequest(request.get());  // Request takes ownership.

  scoped_ptr<ResourceHandler> handler(
      new SaveFileResourceHandler(request.get(),
                                  child_id,
                                  route_id,
                                  url,
                                  save_file_manager_.get()));

  BeginRequestInternal(request.Pass(), handler.Pass());
}

void ResourceDispatcherHostImpl::MarkAsTransferredNavigation(
    const GlobalRequestID& id) {
  GetLoader(id)->MarkAsTransferring();
}

void ResourceDispatcherHostImpl::CancelTransferringNavigation(
    const GlobalRequestID& id) {
  // Request should still exist and be in the middle of a transfer.
  DCHECK(IsTransferredNavigation(id));
  RemovePendingRequest(id.child_id, id.request_id);
}

void ResourceDispatcherHostImpl::ResumeDeferredNavigation(
    const GlobalRequestID& id) {
  ResourceLoader* loader = GetLoader(id);
  // The response we were meant to resume could have already been canceled.
  if (loader)
    loader->CompleteTransfer();
}

// The object died, so cancel and detach all requests associated with it except
// for downloads and detachable resources, which belong to the browser process
// even if initiated via a renderer.
void ResourceDispatcherHostImpl::CancelRequestsForProcess(int child_id) {
  CancelRequestsForRoute(child_id, -1 /* cancel all */);
  registered_temp_files_.erase(child_id);
}

void ResourceDispatcherHostImpl::CancelRequestsForRoute(int child_id,
                                                        int route_id) {
  // Since pending_requests_ is a map, we first build up a list of all of the
  // matching requests to be cancelled, and then we cancel them.  Since there
  // may be more than one request to cancel, we cannot simply hold onto the map
  // iterators found in the first loop.

  // Find the global ID of all matching elements.
  bool any_requests_transferring = false;
  std::vector<GlobalRequestID> matching_requests;
  for (LoaderMap::const_iterator i = pending_loaders_.begin();
       i != pending_loaders_.end(); ++i) {
    if (i->first.child_id != child_id)
      continue;

    ResourceRequestInfoImpl* info = i->second->GetRequestInfo();

    GlobalRequestID id(child_id, i->first.request_id);
    DCHECK(id == i->first);
    // Don't cancel navigations that are expected to live beyond this process.
    if (IsTransferredNavigation(id))
      any_requests_transferring = true;
    if (info->detachable_handler()) {
      info->detachable_handler()->Detach();
    } else if (!info->IsDownload() && !info->is_stream() &&
               !IsTransferredNavigation(id) &&
               (route_id == -1 || route_id == info->GetRouteID())) {
      matching_requests.push_back(id);
    }
  }

  // Remove matches.
  for (size_t i = 0; i < matching_requests.size(); ++i) {
    LoaderMap::iterator iter = pending_loaders_.find(matching_requests[i]);
    // Although every matching request was in pending_requests_ when we built
    // matching_requests, it is normal for a matching request to be not found
    // in pending_requests_ after we have removed some matching requests from
    // pending_requests_.  For example, deleting a net::URLRequest that has
    // exclusive (write) access to an HTTP cache entry may unblock another
    // net::URLRequest that needs exclusive access to the same cache entry, and
    // that net::URLRequest may complete and remove itself from
    // pending_requests_. So we need to check that iter is not equal to
    // pending_requests_.end().
    if (iter != pending_loaders_.end())
      RemovePendingLoader(iter);
  }

  // Don't clear the blocked loaders or offline policy maps if any of the
  // requests in route_id are being transferred to a new process, since those
  // maps will be updated with the new route_id after the transfer.  Otherwise
  // we will lose track of this info when the old route goes away, before the
  // new one is created.
  if (any_requests_transferring)
    return;

  // Now deal with blocked requests if any.
  if (route_id != -1) {
    if (blocked_loaders_map_.find(GlobalRoutingID(child_id, route_id)) !=
        blocked_loaders_map_.end()) {
      CancelBlockedRequestsForRoute(child_id, route_id);
    }
  } else {
    // We have to do all render views for the process |child_id|.
    // Note that we have to do this in 2 passes as we cannot call
    // CancelBlockedRequestsForRoute while iterating over
    // blocked_loaders_map_, as it modifies it.
    std::set<int> route_ids;
    for (BlockedLoadersMap::const_iterator iter = blocked_loaders_map_.begin();
         iter != blocked_loaders_map_.end(); ++iter) {
      if (iter->first.child_id == child_id)
        route_ids.insert(iter->first.route_id);
    }
    for (std::set<int>::const_iterator iter = route_ids.begin();
        iter != route_ids.end(); ++iter) {
      CancelBlockedRequestsForRoute(child_id, *iter);
    }
  }
}

// Cancels the request and removes it from the list.
void ResourceDispatcherHostImpl::RemovePendingRequest(int child_id,
                                                      int request_id) {
  LoaderMap::iterator i = pending_loaders_.find(
      GlobalRequestID(child_id, request_id));
  if (i == pending_loaders_.end()) {
    NOTREACHED() << "Trying to remove a request that's not here";
    return;
  }
  RemovePendingLoader(i);
}

void ResourceDispatcherHostImpl::RemovePendingLoader(
    const LoaderMap::iterator& iter) {
  ResourceRequestInfoImpl* info = iter->second->GetRequestInfo();

  // Remove the memory credit that we added when pushing the request onto
  // the pending list.
  IncrementOutstandingRequestsMemory(-1, *info);

  pending_loaders_.erase(iter);
}

void ResourceDispatcherHostImpl::CancelRequest(int child_id,
                                               int request_id) {
  ResourceLoader* loader = GetLoader(child_id, request_id);
  if (!loader) {
    // We probably want to remove this warning eventually, but I wanted to be
    // able to notice when this happens during initial development since it
    // should be rare and may indicate a bug.
    DVLOG(1) << "Canceling a request that wasn't found";
    return;
  }

  RemovePendingRequest(child_id, request_id);
}

ResourceDispatcherHostImpl::OustandingRequestsStats
ResourceDispatcherHostImpl::GetOutstandingRequestsStats(
    const ResourceRequestInfoImpl& info) {
  OutstandingRequestsStatsMap::iterator entry =
      outstanding_requests_stats_map_.find(info.GetChildID());
  OustandingRequestsStats stats = { 0, 0 };
  if (entry != outstanding_requests_stats_map_.end())
    stats = entry->second;
  return stats;
}

void ResourceDispatcherHostImpl::UpdateOutstandingRequestsStats(
    const ResourceRequestInfoImpl& info,
    const OustandingRequestsStats& stats) {
  if (stats.memory_cost == 0 && stats.num_requests == 0)
    outstanding_requests_stats_map_.erase(info.GetChildID());
  else
    outstanding_requests_stats_map_[info.GetChildID()] = stats;
}

ResourceDispatcherHostImpl::OustandingRequestsStats
ResourceDispatcherHostImpl::IncrementOutstandingRequestsMemory(
    int count,
    const ResourceRequestInfoImpl& info) {
  DCHECK_EQ(1, abs(count));

  // Retrieve the previous value (defaulting to 0 if not found).
  OustandingRequestsStats stats = GetOutstandingRequestsStats(info);

  // Insert/update the total; delete entries when their count reaches 0.
  stats.memory_cost += count * info.memory_cost();
  DCHECK_GE(stats.memory_cost, 0);
  UpdateOutstandingRequestsStats(info, stats);

  return stats;
}

ResourceDispatcherHostImpl::OustandingRequestsStats
ResourceDispatcherHostImpl::IncrementOutstandingRequestsCount(
    int count,
    ResourceRequestInfoImpl* info) {
  DCHECK_EQ(1, abs(count));
  num_in_flight_requests_ += count;

  // Keep track of whether this request is counting toward the number of
  // in-flight requests for this process, in case we need to transfer it to
  // another process. This should be a toggle.
  DCHECK_NE(info->counted_as_in_flight_request(), count > 0);
  info->set_counted_as_in_flight_request(count > 0);

  OustandingRequestsStats stats = GetOutstandingRequestsStats(*info);
  stats.num_requests += count;
  DCHECK_GE(stats.num_requests, 0);
  UpdateOutstandingRequestsStats(*info, stats);

  return stats;
}

bool ResourceDispatcherHostImpl::HasSufficientResourcesForRequest(
    net::URLRequest* request) {
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  OustandingRequestsStats stats = IncrementOutstandingRequestsCount(1, info);

  if (stats.num_requests > max_num_in_flight_requests_per_process_)
    return false;
  if (num_in_flight_requests_ > max_num_in_flight_requests_)
    return false;

  return true;
}

void ResourceDispatcherHostImpl::FinishedWithResourcesForRequest(
    net::URLRequest* request) {
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  IncrementOutstandingRequestsCount(-1, info);
}

void ResourceDispatcherHostImpl::BeginNavigationRequest(
    ResourceContext* resource_context,
    int frame_tree_node_id,
    const NavigationRequestInfo& info,
    NavigationURLLoaderImplCore* loader) {
  // PlzNavigate: BeginNavigationRequest currently should only be used for the
  // browser-side navigations project.
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));

  ResourceType resource_type = info.is_main_frame ?
      RESOURCE_TYPE_MAIN_FRAME : RESOURCE_TYPE_SUB_FRAME;

  if (is_shutdown_ ||
      // TODO(davidben): Check ShouldServiceRequest here. This is important; it
      // needs to be checked relative to the child that /requested/ the
      // navigation. It's where file upload checks, etc., come in.
      (delegate_ && !delegate_->ShouldBeginRequest(
          info.begin_params.method,
          info.common_params.url,
          resource_type,
          resource_context))) {
    loader->NotifyRequestFailed(false, net::ERR_ABORTED);
    return;
  }

  // Save the URL on the stack to help catch URLRequests which outlive their
  // URLRequestContexts. See https://crbug.com/90971
  char url_buf[128];
  base::strlcpy(
      url_buf, info.common_params.url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);
  CHECK(ContainsKey(active_resource_contexts_, resource_context));

  const net::URLRequestContext* request_context =
      resource_context->GetRequestContext();

  int load_flags = info.begin_params.load_flags;
  load_flags |= net::LOAD_VERIFY_EV_CERT;
  if (info.is_main_frame)
    load_flags |= net::LOAD_MAIN_FRAME;

  // TODO(davidben): BuildLoadFlagsForRequest includes logic for
  // CanSendCookiesForOrigin and CanReadRawCookies. Is this needed here?

  // Sync loads should have maximum priority and should be the only
  // requests that have the ignore limits flag set.
  DCHECK(!(load_flags & net::LOAD_IGNORE_LIMITS));

  scoped_ptr<net::URLRequest> new_request;
  new_request = request_context->CreateRequest(
      info.common_params.url, net::HIGHEST, nullptr);

  new_request->set_method(info.begin_params.method);
  new_request->set_first_party_for_cookies(
      info.first_party_for_cookies);
  if (info.is_main_frame) {
    new_request->set_first_party_url_policy(
        net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT);
  }

  SetReferrerForRequest(new_request.get(), info.common_params.referrer);

  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(info.begin_params.headers);
  new_request->SetExtraRequestHeaders(headers);

  new_request->SetLoadFlags(load_flags);

  // Resolve elements from request_body and prepare upload data.
  if (info.request_body.get()) {
    storage::BlobStorageContext* blob_context = GetBlobStorageContext(
        GetChromeBlobStorageContextForResourceContext(resource_context));
    AttachRequestBodyBlobDataHandles(
        info.request_body.get(),
        blob_context);
    // TODO(davidben): The FileSystemContext is null here. In the case where
    // another renderer requested this navigation, this should be the same
    // FileSystemContext passed into ShouldServiceRequest.
    new_request->set_upload(UploadDataStreamBuilder::Build(
        info.request_body.get(),
        blob_context,
        nullptr,  // file_system_context
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)
            .get()));
  }

  request_id_--;

  // Make extra info and read footer (contains request ID).
  //
  // TODO(davidben): Associate the request with the FrameTreeNode and/or tab so
  // that IO thread -> UI thread hops will work.
  ResourceRequestInfoImpl* extra_info = new ResourceRequestInfoImpl(
      PROCESS_TYPE_BROWSER,
      -1,  // child_id
      -1,  // route_id
      info.frame_tree_node_id,
      -1,  // request_data.origin_pid,
      request_id_,
      -1,  // request_data.render_frame_id,
      info.is_main_frame,
      info.parent_is_main_frame,
      -1,  // request_data.parent_render_frame_id,
      resource_type,
      info.common_params.transition,
      // should_replace_current_entry. This was only maintained at layer for
      // request transfers and isn't needed for browser-side navigations.
      false,
      false,  // is download
      false,  // is stream
      info.common_params.allow_download,
      info.begin_params.has_user_gesture,
      true,   // enable_load_timing
      false,  // enable_upload_progress
      false,  // do_not_prompt_for_login
      info.common_params.referrer.policy,
      // TODO(davidben): This is only used for prerenders. Replace
      // is_showing with something for that. Or maybe it just comes from the
      // same mechanism as the cookie one.
      blink::WebPageVisibilityStateVisible,
      resource_context,
      base::WeakPtr<ResourceMessageFilter>(),  // filter
      true);
  // Request takes ownership.
  extra_info->AssociateWithRequest(new_request.get());

  if (new_request->url().SchemeIs(url::kBlobScheme)) {
    // Hang on to a reference to ensure the blob is not released prior
    // to the job being started.
    ChromeBlobStorageContext* blob_context =
        GetChromeBlobStorageContextForResourceContext(resource_context);
    storage::BlobProtocolHandler::SetRequestedBlobDataHandle(
        new_request.get(),
        blob_context->context()->GetBlobDataFromPublicURL(new_request->url()));
  }

  // TODO(davidben): Attach ServiceWorkerRequestHandler.
  // TODO(michaeln): Help out with this and that.
  // TODO(davidben): Attach AppCacheInterceptor.

  scoped_ptr<ResourceHandler> handler(new NavigationResourceHandler(
      new_request.get(), loader));

  // TODO(davidben): Pass in the appropriate appcache_service. Also fix the
  // dependency on child_id/route_id. Those are used by the ResourceScheduler;
  // currently it's a no-op.
  handler = AddStandardHandlers(new_request.get(), resource_type,
                                resource_context,
                                nullptr,  // appcache_service
                                -1,  // child_id
                                -1,  // route_id
                                handler.Pass());

  BeginRequestInternal(new_request.Pass(), handler.Pass());
}

// static
int ResourceDispatcherHostImpl::CalculateApproximateMemoryCost(
    net::URLRequest* request) {
  // The following fields should be a minor size contribution (experimentally
  // on the order of 100). However since they are variable length, it could
  // in theory be a sizeable contribution.
  int strings_cost = request->extra_request_headers().ToString().size() +
                     request->original_url().spec().size() +
                     request->referrer().size() +
                     request->method().size();

  // Note that this expression will typically be dominated by:
  // |kAvgBytesPerOutstandingRequest|.
  return kAvgBytesPerOutstandingRequest + strings_cost;
}

void ResourceDispatcherHostImpl::BeginRequestInternal(
    scoped_ptr<net::URLRequest> request,
    scoped_ptr<ResourceHandler> handler) {
  DCHECK(!request->is_pending());
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request.get());

  if ((TimeTicks::Now() - last_user_gesture_time_) <
      TimeDelta::FromMilliseconds(kUserGestureWindowMs)) {
    request->SetLoadFlags(
        request->load_flags() | net::LOAD_MAYBE_USER_GESTURE);
  }

  // Add the memory estimate that starting this request will consume.
  info->set_memory_cost(CalculateApproximateMemoryCost(request.get()));

  // If enqueing/starting this request will exceed our per-process memory
  // bound, abort it right away.
  OustandingRequestsStats stats = IncrementOutstandingRequestsMemory(1, *info);
  if (stats.memory_cost > max_outstanding_requests_cost_per_process_) {
    // We call "CancelWithError()" as a way of setting the net::URLRequest's
    // status -- it has no effect beyond this, since the request hasn't started.
    request->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);

    bool defer = false;
    handler->OnResponseCompleted(request->status(), std::string(), &defer);
    if (defer) {
      // TODO(darin): The handler is not ready for us to kill the request. Oops!
      NOTREACHED();
    }

    IncrementOutstandingRequestsMemory(-1, *info);

    // A ResourceHandler must not outlive its associated URLRequest.
    handler.reset();
    return;
  }

  linked_ptr<ResourceLoader> loader(
      new ResourceLoader(request.Pass(), handler.Pass(), this));

  GlobalRoutingID id(info->GetGlobalRoutingID());
  BlockedLoadersMap::const_iterator iter = blocked_loaders_map_.find(id);
  if (iter != blocked_loaders_map_.end()) {
    // The request should be blocked.
    iter->second->push_back(loader);
    return;
  }

  StartLoading(info, loader);
}

void ResourceDispatcherHostImpl::StartLoading(
    ResourceRequestInfoImpl* info,
    const linked_ptr<ResourceLoader>& loader) {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/456331 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456331 ResourceDispatcherHostImpl::StartLoading"));
  pending_loaders_[info->GetGlobalRequestID()] = loader;

  loader->StartRequest();
}

void ResourceDispatcherHostImpl::OnUserGesture(WebContentsImpl* contents) {
  last_user_gesture_time_ = TimeTicks::Now();
}

net::URLRequest* ResourceDispatcherHostImpl::GetURLRequest(
    const GlobalRequestID& id) {
  ResourceLoader* loader = GetLoader(id);
  if (!loader)
    return NULL;

  return loader->request();
}

// static
bool ResourceDispatcherHostImpl::LoadInfoIsMoreInteresting(const LoadInfo& a,
                                                           const LoadInfo& b) {
  // Set |*_uploading_size| to be the size of the corresponding upload body if
  // it's currently being uploaded.

  uint64 a_uploading_size = 0;
  if (a.load_state.state == net::LOAD_STATE_SENDING_REQUEST)
    a_uploading_size = a.upload_size;

  uint64 b_uploading_size = 0;
  if (b.load_state.state == net::LOAD_STATE_SENDING_REQUEST)
    b_uploading_size = b.upload_size;

  if (a_uploading_size != b_uploading_size)
    return a_uploading_size > b_uploading_size;

  return a.load_state.state > b.load_state.state;
}

// static
void ResourceDispatcherHostImpl::UpdateLoadInfoOnUIThread(
    scoped_ptr<LoadInfoMap> info_map) {
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466285
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466285 ResourceDispatcherHostImpl::UpdateLoadInfoOnUIThread"));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& load_info : *info_map) {
    RenderViewHostImpl* view = RenderViewHostImpl::FromID(
        load_info.first.child_id, load_info.first.route_id);
    // The view could be gone at this point.
    if (view) {
      view->LoadStateChanged(load_info.second.url, load_info.second.load_state,
                             load_info.second.upload_position,
                             load_info.second.upload_size);
    }
  }
}

scoped_ptr<ResourceDispatcherHostImpl::LoadInfoMap>
ResourceDispatcherHostImpl::GetLoadInfoForAllRoutes() {
  // Populate this map with load state changes, and then send them on to the UI
  // thread where they can be passed along to the respective RVHs.
  scoped_ptr<LoadInfoMap> info_map(new LoadInfoMap());

  for (const auto& loader : pending_loaders_) {
    net::URLRequest* request = loader.second->request();
    net::UploadProgress upload_progress = request->GetUploadProgress();

    LoadInfo load_info;
    load_info.url = request->url();
    load_info.load_state = request->GetLoadState();
    load_info.upload_size = upload_progress.size();
    load_info.upload_position = upload_progress.position();

    GlobalRoutingID id(loader.second->GetRequestInfo()->GetGlobalRoutingID());
    LoadInfoMap::iterator existing = info_map->find(id);

    if (existing == info_map->end() ||
        LoadInfoIsMoreInteresting(load_info, existing->second)) {
      (*info_map)[id] = load_info;
    }
  }
  return info_map.Pass();
}

void ResourceDispatcherHostImpl::UpdateLoadInfo() {
  scoped_ptr<LoadInfoMap> info_map(GetLoadInfoForAllRoutes());

  // Stop the timer if there are no more pending requests. Future new requests
  // will restart it as necessary.
  // Also stop the timer if there are no loading clients, to avoid waking up
  // unnecessarily when there is a long running (hanging get) request.
  if (info_map->empty() || !scheduler_->HasLoadingClients()) {
    update_load_states_timer_->Stop();
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ResourceDispatcherHostImpl::UpdateLoadInfoOnUIThread,
                 base::Passed(&info_map)));
}

void ResourceDispatcherHostImpl::BlockRequestsForRoute(int child_id,
                                                       int route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GlobalRoutingID key(child_id, route_id);
  DCHECK(blocked_loaders_map_.find(key) == blocked_loaders_map_.end()) <<
      "BlockRequestsForRoute called  multiple time for the same RVH";
  blocked_loaders_map_[key] = new BlockedLoadersList();
}

void ResourceDispatcherHostImpl::ResumeBlockedRequestsForRoute(int child_id,
                                                               int route_id) {
  ProcessBlockedRequestsForRoute(child_id, route_id, false);
}

void ResourceDispatcherHostImpl::CancelBlockedRequestsForRoute(int child_id,
                                                               int route_id) {
  ProcessBlockedRequestsForRoute(child_id, route_id, true);
}

void ResourceDispatcherHostImpl::ProcessBlockedRequestsForRoute(
    int child_id,
    int route_id,
    bool cancel_requests) {
  BlockedLoadersMap::iterator iter = blocked_loaders_map_.find(
      GlobalRoutingID(child_id, route_id));
  if (iter == blocked_loaders_map_.end()) {
    // It's possible to reach here if the renderer crashed while an interstitial
    // page was showing.
    return;
  }

  BlockedLoadersList* loaders = iter->second;

  // Removing the vector from the map unblocks any subsequent requests.
  blocked_loaders_map_.erase(iter);

  for (BlockedLoadersList::iterator loaders_iter = loaders->begin();
       loaders_iter != loaders->end(); ++loaders_iter) {
    linked_ptr<ResourceLoader> loader = *loaders_iter;
    ResourceRequestInfoImpl* info = loader->GetRequestInfo();
    if (cancel_requests) {
      IncrementOutstandingRequestsMemory(-1, *info);
    } else {
      StartLoading(info, loader);
    }
  }

  delete loaders;
}

ResourceDispatcherHostImpl::HttpAuthRelationType
ResourceDispatcherHostImpl::HttpAuthRelationTypeOf(
    const GURL& request_url,
    const GURL& first_party) {
  if (!first_party.is_valid())
    return HTTP_AUTH_RELATION_TOP;

  if (net::registry_controlled_domains::SameDomainOrHost(
          first_party, request_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES))
    return HTTP_AUTH_RELATION_SAME_DOMAIN;

  if (allow_cross_origin_auth_prompt())
    return HTTP_AUTH_RELATION_ALLOWED_CROSS;

  return HTTP_AUTH_RELATION_BLOCKED_CROSS;
}

bool ResourceDispatcherHostImpl::allow_cross_origin_auth_prompt() {
  return allow_cross_origin_auth_prompt_;
}

bool ResourceDispatcherHostImpl::IsTransferredNavigation(
    const GlobalRequestID& id) const {
  ResourceLoader* loader = GetLoader(id);
  return loader ? loader->is_transferring() : false;
}

ResourceLoader* ResourceDispatcherHostImpl::GetLoader(
    const GlobalRequestID& id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  LoaderMap::const_iterator i = pending_loaders_.find(id);
  if (i == pending_loaders_.end())
    return NULL;

  return i->second.get();
}

ResourceLoader* ResourceDispatcherHostImpl::GetLoader(int child_id,
                                                      int request_id) const {
  return GetLoader(GlobalRequestID(child_id, request_id));
}

void ResourceDispatcherHostImpl::RegisterResourceMessageDelegate(
    const GlobalRequestID& id, ResourceMessageDelegate* delegate) {
  DelegateMap::iterator it = delegate_map_.find(id);
  if (it == delegate_map_.end()) {
    it = delegate_map_.insert(
                           std::make_pair(
                               id,
                               new base::ObserverList<ResourceMessageDelegate>))
             .first;
  }
  it->second->AddObserver(delegate);
}

void ResourceDispatcherHostImpl::UnregisterResourceMessageDelegate(
    const GlobalRequestID& id, ResourceMessageDelegate* delegate) {
  DCHECK(ContainsKey(delegate_map_, id));
  DelegateMap::iterator it = delegate_map_.find(id);
  DCHECK(it->second->HasObserver(delegate));
  it->second->RemoveObserver(delegate);
  if (!it->second->might_have_observers()) {
    delete it->second;
    delegate_map_.erase(it);
  }
}

int ResourceDispatcherHostImpl::BuildLoadFlagsForRequest(
    const ResourceHostMsg_Request& request_data,
    int child_id,
    bool is_sync_load) {
  int load_flags = request_data.load_flags;

  // Although EV status is irrelevant to sub-frames and sub-resources, we have
  // to perform EV certificate verification on all resources because an HTTP
  // keep-alive connection created to load a sub-frame or a sub-resource could
  // be reused to load a main frame.
  load_flags |= net::LOAD_VERIFY_EV_CERT;
  if (request_data.resource_type == RESOURCE_TYPE_MAIN_FRAME) {
    load_flags |= net::LOAD_MAIN_FRAME;
  } else if (request_data.resource_type == RESOURCE_TYPE_PREFETCH) {
    load_flags |= net::LOAD_PREFETCH;
  }

  if (is_sync_load)
    load_flags |= net::LOAD_IGNORE_LIMITS;

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Raw headers are sensitive, as they include Cookie/Set-Cookie, so only
  // allow requesting them if requester has ReadRawCookies permission.
  if ((load_flags & net::LOAD_REPORT_RAW_HEADERS)
      && !policy->CanReadRawCookies(child_id)) {
    VLOG(1) << "Denied unauthorized request for raw headers";
    load_flags &= ~net::LOAD_REPORT_RAW_HEADERS;
  }

  return load_flags;
}

}  // namespace content
