// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_manager.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_netlog_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"

namespace content {

// static
DevToolsManager* DevToolsManager::GetInstance() {
  return Singleton<DevToolsManager>::get();
}

DevToolsManager::DevToolsManager()
    : delegate_(GetContentClient()->browser()->GetDevToolsManagerDelegate()),
      attached_hosts_count_(0) {
}

DevToolsManager::~DevToolsManager() {
  DCHECK(!attached_hosts_count_);
}

void DevToolsManager::AgentHostStateChanged(
    DevToolsAgentHostImpl* agent_host, bool attached) {
  if (attached) {
    if (!attached_hosts_count_) {
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(&DevToolsNetLogObserver::Attach));
    }
    ++attached_hosts_count_;
  } else {
    --attached_hosts_count_;
    if (!attached_hosts_count_) {
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(&DevToolsNetLogObserver::Detach));
    }
  }
}

}  // namespace content
