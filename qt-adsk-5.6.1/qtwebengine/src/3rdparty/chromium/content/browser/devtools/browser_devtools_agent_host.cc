// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/browser_devtools_agent_host.h"

#include "base/bind.h"
#include "content/browser/devtools/devtools_protocol_handler.h"
#include "content/browser/devtools/protocol/system_info_handler.h"
#include "content/browser/devtools/protocol/tethering_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"

namespace content {

scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::CreateForBrowser(
    scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
    const CreateServerSocketCallback& socket_callback) {
  return new BrowserDevToolsAgentHost(tethering_task_runner, socket_callback);
}

BrowserDevToolsAgentHost::BrowserDevToolsAgentHost(
    scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
    const CreateServerSocketCallback& socket_callback)
    : system_info_handler_(new devtools::system_info::SystemInfoHandler()),
      tethering_handler_(
          new devtools::tethering::TetheringHandler(socket_callback,
                                                    tethering_task_runner)),
      tracing_handler_(new devtools::tracing::TracingHandler(
          devtools::tracing::TracingHandler::Browser)),
      protocol_handler_(new DevToolsProtocolHandler(
          this,
          base::Bind(&BrowserDevToolsAgentHost::SendMessageToClient,
                     base::Unretained(this)))) {
  DevToolsProtocolDispatcher* dispatcher = protocol_handler_->dispatcher();
  dispatcher->SetSystemInfoHandler(system_info_handler_.get());
  dispatcher->SetTetheringHandler(tethering_handler_.get());
  dispatcher->SetTracingHandler(tracing_handler_.get());
}

BrowserDevToolsAgentHost::~BrowserDevToolsAgentHost() {
}

void BrowserDevToolsAgentHost::Attach() {
}

void BrowserDevToolsAgentHost::Detach() {
}

DevToolsAgentHost::Type BrowserDevToolsAgentHost::GetType() {
  return TYPE_BROWSER;
}

std::string BrowserDevToolsAgentHost::GetTitle() {
  return "";
}

GURL BrowserDevToolsAgentHost::GetURL() {
  return GURL();
}

bool BrowserDevToolsAgentHost::Activate() {
  return false;
}

bool BrowserDevToolsAgentHost::Close() {
  return false;
}

bool BrowserDevToolsAgentHost::DispatchProtocolMessage(
    const std::string& message) {
  protocol_handler_->HandleMessage(message);
  return true;
}

}  // content
