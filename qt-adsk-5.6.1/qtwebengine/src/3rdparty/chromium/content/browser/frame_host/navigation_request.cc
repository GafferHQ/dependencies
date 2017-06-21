// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_request.h"

#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/resource_request_body.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/content_client.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"

namespace content {

namespace {

// Returns the net load flags to use based on the navigation type.
// TODO(clamy): unify the code with what is happening on the renderer side.
int LoadFlagFromNavigationType(FrameMsg_Navigate_Type::Value navigation_type) {
  int load_flags = net::LOAD_NORMAL;
  switch (navigation_type) {
    case FrameMsg_Navigate_Type::RELOAD:
    case FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL:
      load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case FrameMsg_Navigate_Type::RELOAD_IGNORING_CACHE:
      load_flags |= net::LOAD_BYPASS_CACHE;
      break;
    case FrameMsg_Navigate_Type::RESTORE:
      load_flags |= net::LOAD_PREFERRING_CACHE;
      break;
    case FrameMsg_Navigate_Type::RESTORE_WITH_POST:
      load_flags |= net::LOAD_ONLY_FROM_CACHE;
      break;
    case FrameMsg_Navigate_Type::NORMAL:
    default:
      break;
  }
  return load_flags;
}

}  // namespace

// static
scoped_ptr<NavigationRequest> NavigationRequest::CreateBrowserInitiated(
    FrameTreeNode* frame_tree_node,
    const GURL& dest_url,
    const Referrer& dest_referrer,
    const FrameNavigationEntry& frame_entry,
    const NavigationEntryImpl& entry,
    FrameMsg_Navigate_Type::Value navigation_type,
    bool is_same_document_history_load,
    base::TimeTicks navigation_start,
    NavigationControllerImpl* controller) {
  std::string method = entry.GetHasPostData() ? "POST" : "GET";

  // Copy existing headers and add necessary headers that may not be present
  // in the RequestNavigationParams.
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(entry.extra_headers());
  headers.SetHeaderIfMissing(net::HttpRequestHeaders::kUserAgent,
                             GetContentClient()->GetUserAgent());
  // TODO(clamy): match what blink is doing with accept headers.
  headers.SetHeaderIfMissing("Accept", "*/*");

  // Fill POST data from the browser in the request body.
  scoped_refptr<ResourceRequestBody> request_body;
  if (entry.GetHasPostData()) {
    request_body = new ResourceRequestBody();
    request_body->AppendBytes(
        reinterpret_cast<const char *>(
            entry.GetBrowserInitiatedPostData()->front()),
        entry.GetBrowserInitiatedPostData()->size());
  }

  scoped_ptr<NavigationRequest> navigation_request(new NavigationRequest(
      frame_tree_node,
      entry.ConstructCommonNavigationParams(dest_url, dest_referrer,
                                            frame_entry, navigation_type),
      BeginNavigationParams(method, headers.ToString(),
                            LoadFlagFromNavigationType(navigation_type), false),
      entry.ConstructRequestNavigationParams(
          frame_entry, navigation_start, is_same_document_history_load,
          controller->HasCommittedRealLoad(frame_tree_node),
          controller->GetPendingEntryIndex() == -1,
          controller->GetIndexOfEntry(&entry),
          controller->GetLastCommittedEntryIndex(),
          controller->GetEntryCount()),
      request_body, true, &frame_entry, &entry));
  return navigation_request.Pass();
}

// static
scoped_ptr<NavigationRequest> NavigationRequest::CreateRendererInitiated(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params,
    scoped_refptr<ResourceRequestBody> body,
    int current_history_list_offset,
    int current_history_list_length) {
  // TODO(clamy): Check if some PageState should be provided here.
  // TODO(clamy): See how we should handle override of the user agent when the
  // navigation may start in a renderer and commit in another one.
  // TODO(clamy): See if the navigation start time should be measured in the
  // renderer and sent to the browser instead of being measured here.
  // TODO(clamy): The pending history list offset should be properly set.
  // TODO(clamy): Set has_committed_real_load.
  RequestNavigationParams request_params;
  request_params.current_history_list_offset = current_history_list_offset;
  request_params.current_history_list_length = current_history_list_length;
  scoped_ptr<NavigationRequest> navigation_request(
      new NavigationRequest(frame_tree_node, common_params, begin_params,
                            request_params, body, false, nullptr, nullptr));
  return navigation_request.Pass();
}

NavigationRequest::NavigationRequest(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params,
    const RequestNavigationParams& request_params,
    scoped_refptr<ResourceRequestBody> body,
    bool browser_initiated,
    const FrameNavigationEntry* frame_entry,
    const NavigationEntryImpl* entry)
    : frame_tree_node_(frame_tree_node),
      common_params_(common_params),
      begin_params_(begin_params),
      request_params_(request_params),
      browser_initiated_(browser_initiated),
      state_(NOT_STARTED),
      restore_type_(NavigationEntryImpl::RESTORE_NONE),
      is_view_source_(false),
      bindings_(NavigationEntryImpl::kInvalidBindings) {
  DCHECK_IMPLIES(browser_initiated, entry != nullptr && frame_entry != nullptr);
  if (browser_initiated) {
    // TODO(clamy): use the FrameNavigationEntry for the source SiteInstance
    // once it has been moved from the NavigationEntry.
    source_site_instance_ = entry->source_site_instance();
    dest_site_instance_ = frame_entry->site_instance();
    restore_type_ = entry->restore_type();
    is_view_source_ = entry->IsViewSourceMode();
    bindings_ = entry->bindings();
  } else {
    // This is needed to have about:blank and data URLs commit in the same
    // SiteInstance as the initiating renderer.
    source_site_instance_ =
        frame_tree_node->current_frame_host()->GetSiteInstance();
  }

  const GURL& first_party_for_cookies =
      frame_tree_node->IsMainFrame()
          ? common_params.url
          : frame_tree_node->frame_tree()->root()->current_url();
  bool parent_is_main_frame = !frame_tree_node->parent() ?
      false : frame_tree_node->parent()->IsMainFrame();
  info_.reset(new NavigationRequestInfo(
      common_params, begin_params, first_party_for_cookies,
      frame_tree_node->IsMainFrame(), parent_is_main_frame,
      frame_tree_node->frame_tree_node_id(), body));
}

NavigationRequest::~NavigationRequest() {
}

bool NavigationRequest::BeginNavigation() {
  DCHECK(!loader_);
  DCHECK(state_ == NOT_STARTED || state_ == WAITING_FOR_RENDERER_RESPONSE);
  state_ = STARTED;

  if (ShouldMakeNetworkRequestForURL(common_params_.url)) {
    loader_ = NavigationURLLoader::Create(
        frame_tree_node_->navigator()->GetController()->GetBrowserContext(),
        frame_tree_node_->frame_tree_node_id(), info_.Pass(), this);
    return true;
  }

  // There is no need to make a network request for this navigation, so commit
  // it immediately.
  state_ = RESPONSE_STARTED;
  frame_tree_node_->navigator()->CommitNavigation(
      frame_tree_node_, nullptr, scoped_ptr<StreamHandle>());
  return false;

  // TODO(davidben): Fire (and add as necessary) observer methods such as
  // DidStartProvisionalLoadForFrame for the navigation.
}

void NavigationRequest::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const scoped_refptr<ResourceResponse>& response) {
  // TODO(davidben): Track other changes from redirects. These are important
  // for, e.g., reloads.
  common_params_.url = redirect_info.new_url;

  // TODO(davidben): This where prerender and navigation_interceptor should be
  // integrated. For now, just always follow all redirects.
  loader_->FollowRedirect();
}

void NavigationRequest::OnResponseStarted(
    const scoped_refptr<ResourceResponse>& response,
    scoped_ptr<StreamHandle> body) {
  DCHECK(state_ == STARTED);
  state_ = RESPONSE_STARTED;
  frame_tree_node_->navigator()->CommitNavigation(frame_tree_node_,
                                                  response.get(), body.Pass());
}

void NavigationRequest::OnRequestFailed(bool has_stale_copy_in_cache,
                                        int net_error) {
  DCHECK(state_ == STARTED);
  state_ = FAILED;
  frame_tree_node_->navigator()->FailedNavigation(
      frame_tree_node_, has_stale_copy_in_cache, net_error);
}

void NavigationRequest::OnRequestStarted(base::TimeTicks timestamp) {
  frame_tree_node_->navigator()->LogResourceRequestTime(timestamp,
                                                        common_params_.url);
}

}  // namespace content
