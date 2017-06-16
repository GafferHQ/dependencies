// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NET_BROWSER_ONLINE_STATE_OBSERVER_H_
#define CONTENT_BROWSER_NET_BROWSER_ONLINE_STATE_OBSERVER_H_

#include "base/basictypes.h"
#include "net/base/network_change_notifier.h"

namespace content {

// Listens for changes to the online state and manages sending
// updates to each RenderProcess via RenderProcessHost IPC.
class BrowserOnlineStateObserver
    : public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  BrowserOnlineStateObserver();
  ~BrowserOnlineStateObserver() override;

  // ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserOnlineStateObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NET_BROWSER_ONLINE_STATE_OBSERVER_H_
