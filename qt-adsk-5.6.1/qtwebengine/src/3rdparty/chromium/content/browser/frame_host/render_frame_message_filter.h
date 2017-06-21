// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_MESSAGE_FILTER_H_

#include "content/common/frame_replication_state.h"
#include "content/public/browser/browser_message_filter.h"
#include "third_party/WebKit/public/web/WebTreeScopeType.h"

namespace content {
class RenderWidgetHelper;

// RenderFrameMessageFilter intercepts FrameHost messages on the IO thread
// that require low-latency processing. The canonical example of this is
// child-frame creation which is a sync IPC that provides the renderer
// with the routing id for a newly created RenderFrame.
//
// This object is created on the UI thread and used on the IO thread.
class RenderFrameMessageFilter : public BrowserMessageFilter {
 public:
  RenderFrameMessageFilter(int render_process_id,
                           RenderWidgetHelper* render_widget_helper);

  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ~RenderFrameMessageFilter() override;

  void OnCreateChildFrame(int parent_routing_id,
                          blink::WebTreeScopeType scope,
                          const std::string& frame_name,
                          blink::WebSandboxFlags sandbox_flags,
                          int* new_render_frame_id);

  const int render_process_id_;

  // Needed for issuing routing ids and surface ids.
  scoped_refptr<RenderWidgetHelper> render_widget_helper_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_MESSAGE_FILTER_H_
