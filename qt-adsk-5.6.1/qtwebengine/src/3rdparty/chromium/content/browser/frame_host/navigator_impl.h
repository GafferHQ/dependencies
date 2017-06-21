// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

class GURL;

namespace content {

class NavigationControllerImpl;
class NavigatorDelegate;
class NavigatorTest;
struct LoadCommittedDetails;
struct CommitNavigationParams;
struct CommonNavigationParams;
struct RequestNavigationParams;

// This class is an implementation of Navigator, responsible for managing
// navigations in regular browser tabs.
class CONTENT_EXPORT NavigatorImpl : public Navigator {
 public:
  NavigatorImpl(NavigationControllerImpl* navigation_controller,
                NavigatorDelegate* delegate);

  // Navigator implementation.
  NavigatorDelegate* GetDelegate() override;
  NavigationController* GetController() override;
  void DidStartProvisionalLoad(RenderFrameHostImpl* render_frame_host,
                               const GURL& url) override;
  void DidFailProvisionalLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params)
      override;
  void DidFailLoadWithError(RenderFrameHostImpl* render_frame_host,
                            const GURL& url,
                            int error_code,
                            const base::string16& error_description,
                            bool was_ignored_by_handler) override;
  void DidNavigate(RenderFrameHostImpl* render_frame_host,
                   const FrameHostMsg_DidCommitProvisionalLoad_Params&
                       input_params) override;
  bool NavigateToPendingEntry(FrameTreeNode* frame_tree_node,
                              const FrameNavigationEntry& frame_entry,
                              NavigationController::ReloadType reload_type,
                              bool is_same_document_history_load) override;
  void RequestOpenURL(RenderFrameHostImpl* render_frame_host,
                      const GURL& url,
                      SiteInstance* source_site_instance,
                      const Referrer& referrer,
                      WindowOpenDisposition disposition,
                      bool should_replace_current_entry,
                      bool user_gesture) override;
  void RequestTransferURL(RenderFrameHostImpl* render_frame_host,
                          const GURL& url,
                          SiteInstance* source_site_instance,
                          const std::vector<GURL>& redirect_chain,
                          const Referrer& referrer,
                          ui::PageTransition page_transition,
                          WindowOpenDisposition disposition,
                          const GlobalRequestID& transferred_global_request_id,
                          bool should_replace_current_entry,
                          bool user_gesture) override;
  void OnBeforeUnloadACK(FrameTreeNode* frame_tree_node, bool proceed) override;
  void OnBeginNavigation(FrameTreeNode* frame_tree_node,
                         const CommonNavigationParams& common_params,
                         const BeginNavigationParams& begin_params,
                         scoped_refptr<ResourceRequestBody> body) override;
  void CommitNavigation(FrameTreeNode* frame_tree_node,
                        ResourceResponse* response,
                        scoped_ptr<StreamHandle> body) override;
  void FailedNavigation(FrameTreeNode* frame_tree_node,
                        bool has_stale_copy_in_cache,
                        int error_code) override;
  void LogResourceRequestTime(base::TimeTicks timestamp,
                              const GURL& url) override;
  void LogBeforeUnloadTime(
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time) override;
  void CancelNavigation(FrameTreeNode* frame_tree_node) override;

 private:
  // Holds data used to track browser side navigation metrics.
  struct NavigationMetricsData;

  friend class NavigatorTestWithBrowserSideNavigation;
  ~NavigatorImpl() override;

  // Navigates to the given entry, which must be the pending entry.  Private
  // because all callers should use NavigateToPendingEntry.
  bool NavigateToEntry(FrameTreeNode* frame_tree_node,
                       const FrameNavigationEntry& frame_entry,
                       const NavigationEntryImpl& entry,
                       NavigationController::ReloadType reload_type,
                       bool is_same_document_history_load);

  bool ShouldAssignSiteForURL(const GURL& url);

  void CheckWebUIRendererDoesNotDisplayNormalURL(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url);

  // PlzNavigate: if needed, sends a BeforeUnload IPC to the renderer to ask it
  // to execute the beforeUnload event. Otherwise, the navigation request will
  // be started.
  void RequestNavigation(FrameTreeNode* frame_tree_node,
                         const GURL& dest_url,
                         const Referrer& dest_referrer,
                         const FrameNavigationEntry& frame_entry,
                         const NavigationEntryImpl& entry,
                         NavigationController::ReloadType reload_type,
                         bool is_same_document_history_load,
                         base::TimeTicks navigation_start);

  void RecordNavigationMetrics(
      const LoadCommittedDetails& details,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      SiteInstance* site_instance);

  // Called when a navigation has started in a main frame, to update the pending
  // NavigationEntry if the controller does not currently have a
  // browser-initiated one.
  void DidStartMainFrameNavigation(const GURL& url,
                                   SiteInstanceImpl* site_instance);

  // The NavigationController that will keep track of session history for all
  // RenderFrameHost objects using this NavigatorImpl.
  // TODO(nasko): Move ownership of the NavigationController from
  // WebContentsImpl to this class.
  NavigationControllerImpl* controller_;

  // Used to notify the object embedding this Navigator about navigation
  // events. Can be NULL in tests.
  NavigatorDelegate* delegate_;

  scoped_ptr<NavigatorImpl::NavigationMetricsData> navigation_data_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_
