// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_factory.h"

#include "base/logging.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"

namespace content {

// static
RenderFrameHostFactory* RenderFrameHostFactory::factory_ = NULL;

// static
scoped_ptr<RenderFrameHostImpl> RenderFrameHostFactory::Create(
    SiteInstance* site_instance,
    RenderViewHostImpl* render_view_host,
    RenderFrameHostDelegate* delegate,
    RenderWidgetHostDelegate* rwh_delegate,
    FrameTree* frame_tree,
    FrameTreeNode* frame_tree_node,
    int routing_id,
    int flags) {
  if (factory_) {
    return factory_->CreateRenderFrameHost(site_instance, render_view_host,
                                           delegate, rwh_delegate, frame_tree,
                                           frame_tree_node, routing_id, flags);
  }
  return make_scoped_ptr(new RenderFrameHostImpl(
      site_instance, render_view_host, delegate, rwh_delegate, frame_tree,
      frame_tree_node, routing_id, flags));
}

// static
void RenderFrameHostFactory::RegisterFactory(RenderFrameHostFactory* factory) {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = factory;
}

// static
void RenderFrameHostFactory::UnregisterFactory() {
  DCHECK(factory_) << "No factory to unregister.";
  factory_ = NULL;
}

}  // namespace content
