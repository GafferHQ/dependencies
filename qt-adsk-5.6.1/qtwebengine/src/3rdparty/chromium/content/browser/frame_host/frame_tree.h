// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_H_
#define CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/common/content_export.h"

namespace content {

class FrameTreeNode;
class Navigator;
class RenderFrameHostDelegate;
class RenderProcessHost;
class RenderViewHostDelegate;
class RenderViewHostImpl;
class RenderFrameHostManager;
class RenderWidgetHostDelegate;

// Represents the frame tree for a page. With the exception of the main frame,
// all FrameTreeNodes will be created/deleted in response to frame attach and
// detach events in the DOM.
//
// The main frame's FrameTreeNode is special in that it is reused. This allows
// it to serve as an anchor for state that needs to persist across top-level
// page navigations.
//
// TODO(ajwong): Move NavigationController ownership to the main frame
// FrameTreeNode. Possibly expose access to it from here.
//
// This object is only used on the UI thread.
class CONTENT_EXPORT FrameTree {
 public:
  // Each FrameTreeNode will default to using the given |navigator| for
  // navigation tasks in the frame.
  // A set of delegates are remembered here so that we can create
  // RenderFrameHostManagers.
  // TODO(creis): This set of delegates will change as we move things to
  // Navigator.
  FrameTree(Navigator* navigator,
            RenderFrameHostDelegate* render_frame_delegate,
            RenderViewHostDelegate* render_view_delegate,
            RenderWidgetHostDelegate* render_widget_delegate,
            RenderFrameHostManager::Delegate* manager_delegate);
  ~FrameTree();

  FrameTreeNode* root() const { return root_.get(); }

  // Returns the FrameTreeNode with the given |frame_tree_node_id| if it is part
  // of this FrameTree.
  FrameTreeNode* FindByID(int frame_tree_node_id);

  // Returns the FrameTreeNode with the given renderer-specific |routing_id|.
  FrameTreeNode* FindByRoutingID(int process_id, int routing_id);

  // Returns the first frame in this tree with the given |name|, or the main
  // frame if |name| is empty.
  // Note that this does NOT support pseudo-names like _self, _top, and _blank,
  // nor searching other FrameTrees (unlike blink::WebView::findFrameByName).
  FrameTreeNode* FindByName(const std::string& name);

  // Executes |on_node| on each node in the frame tree.  If |on_node| returns
  // false, terminates the iteration immediately. Returning false is useful
  // if |on_node| is just doing a search over the tree.  The iteration proceeds
  // top-down and visits a node before adding its children to the queue, making
  // it safe to remove children during the callback.
  void ForEach(const base::Callback<bool(FrameTreeNode*)>& on_node) const;

  // Frame tree manipulation routines.
  // |process_id| is required to disambiguate |new_routing_id|, and it must
  // match the process of the |parent| node.  Otherwise this method returns
  // nullptr.  Passing MSG_ROUTING_NONE for |new_routing_id| will allocate a new
  // routing ID for the new frame.
  RenderFrameHostImpl* AddFrame(FrameTreeNode* parent,
                                int process_id,
                                int new_routing_id,
                                blink::WebTreeScopeType scope,
                                const std::string& frame_name,
                                blink::WebSandboxFlags sandbox_flags);
  void RemoveFrame(FrameTreeNode* child);

  // This method walks the entire frame tree and creates a RenderFrameProxyHost
  // for the given |site_instance| in each node except the |source| one --
  // the source will have a RenderFrameHost.  |source| may be null if there is
  // no node navigating in this frame tree (such as when this is called
  // for an opener's frame tree), in which case no nodes are skipped for
  // RenderFrameProxyHost creation.
  void CreateProxiesForSiteInstance(FrameTreeNode* source,
                                    SiteInstance* site_instance);

  // Convenience accessor for the main frame's RenderFrameHostImpl.
  RenderFrameHostImpl* GetMainFrame() const;

  // Returns the focused frame.
  FrameTreeNode* GetFocusedFrame();

  // Sets the focused frame.
  void SetFocusedFrame(FrameTreeNode* node);

  // Allows a client to listen for frame removal.  The listener should expect
  // to receive the RenderViewHostImpl containing the frame and the renderer-
  // specific frame routing ID of the removed frame.
  void SetFrameRemoveListener(
      const base::Callback<void(RenderFrameHost*)>& on_frame_removed);

  // Creates a RenderViewHost for a new RenderFrameHost in the given
  // |site_instance|.  The RenderViewHost will have its Shutdown method called
  // when all of the RenderFrameHosts using it are deleted.
  RenderViewHostImpl* CreateRenderViewHost(SiteInstance* site_instance,
                                           int routing_id,
                                           int main_frame_routing_id,
                                           bool swapped_out,
                                           bool hidden);

  // Returns the existing RenderViewHost for a new RenderFrameHost.
  // There should always be such a RenderViewHost, because the main frame
  // RenderFrameHost for each SiteInstance should be created before subframes.
  RenderViewHostImpl* GetRenderViewHost(SiteInstance* site_instance);

  // Keeps track of which RenderFrameHosts and RenderFrameProxyHosts are using
  // each RenderViewHost.  When the number drops to zero, we call Shutdown on
  // the RenderViewHost.
  void AddRenderViewHostRef(RenderViewHostImpl* render_view_host);
  void ReleaseRenderViewHostRef(RenderViewHostImpl* render_view_host);

  // This is only meant to be called by FrameTreeNode. Triggers calling
  // the listener installed by SetFrameRemoveListener.
  void FrameRemoved(FrameTreeNode* frame);

  // Updates the overall load progress and notifies the WebContents.
  void UpdateLoadProgress();

  // Returns this FrameTree's total load progress.
  double load_progress() { return load_progress_; }

  // Resets the load progress on all nodes in this FrameTree.
  void ResetLoadProgress();

  // Returns true if at least one of the nodes in this FrameTree is loading.
  bool IsLoading();

 private:
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest, RemoveFocusedFrame);
  typedef base::hash_map<int, RenderViewHostImpl*> RenderViewHostMap;
  typedef std::multimap<int, RenderViewHostImpl*> RenderViewHostMultiMap;

  // A variation to the public ForEach method with a difference that the subtree
  // starting at |skip_this_subtree| will not be recursed into.
  void ForEach(const base::Callback<bool(FrameTreeNode*)>& on_node,
               FrameTreeNode* skip_this_subtree) const;

  // These delegates are installed into all the RenderViewHosts and
  // RenderFrameHosts that we create.
  RenderFrameHostDelegate* render_frame_delegate_;
  RenderViewHostDelegate* render_view_delegate_;
  RenderWidgetHostDelegate* render_widget_delegate_;
  RenderFrameHostManager::Delegate* manager_delegate_;

  // Map of SiteInstance ID to a RenderViewHost.  This allows us to look up the
  // RenderViewHost for a given SiteInstance when creating RenderFrameHosts.
  // Combined with the refcount on RenderViewHost, this allows us to call
  // Shutdown on the RenderViewHost and remove it from the map when no more
  // RenderFrameHosts are using it.
  //
  // Must be declared before |root_| so that it is deleted afterward.  Otherwise
  // the map will be cleared before we delete the RenderFrameHosts in the tree.
  RenderViewHostMap render_view_host_map_;

  // Map of SiteInstance ID to RenderViewHosts that are pending shutdown. The
  // renderers of these RVH are currently executing the unload event in
  // background. When the SwapOutACK is received, they will be deleted. In the
  // meantime, they are kept in this map, as they should not be reused (part of
  // their state is already gone away).
  RenderViewHostMultiMap render_view_host_pending_shutdown_map_;

  scoped_ptr<FrameTreeNode> root_;

  int focused_frame_tree_node_id_;

  base::Callback<void(RenderFrameHost*)> on_frame_removed_;

  // Overall load progress.
  double load_progress_;

  DISALLOW_COPY_AND_ASSIGN(FrameTree);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_H_
