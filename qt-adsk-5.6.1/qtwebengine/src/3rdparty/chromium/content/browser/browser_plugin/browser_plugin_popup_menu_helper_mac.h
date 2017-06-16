// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_POPUP_MENU_HELPER_MAC_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_POPUP_MENU_HELPER_MAC_H_

#include "content/browser/frame_host/popup_menu_helper_mac.h"

namespace content {

class RenderViewHost;
class RenderViewHostImpl;
class RenderFrameHost;
class RenderFrameHostImpl;

// This class is similiar to PopupMenuHelperMac but positions the popup relative
// to the embedder, and issues a reply to the guest.
class BrowserPluginPopupMenuHelper : public PopupMenuHelper {
 public:
  // Creates a BrowserPluginPopupMenuHelper that positions popups relative to
  // |embedder_rvh| and will notify |guest_rfh| when a user selects or cancels
  // the popup.
  BrowserPluginPopupMenuHelper(RenderViewHost* embedder_rvh,
                               RenderFrameHost* guest_rfh);

 private:
  RenderWidgetHostViewMac* GetRenderWidgetHostView() const override;

  RenderViewHostImpl* embedder_rvh_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPopupMenuHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_POPUP_MENU_HELPER_MAC_H_
