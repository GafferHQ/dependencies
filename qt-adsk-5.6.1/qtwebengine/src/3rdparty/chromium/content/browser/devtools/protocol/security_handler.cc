// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/security_handler.h"

#include <string>

#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {
namespace devtools {
namespace security {

typedef DevToolsProtocolClient::Response Response;

namespace {

std::string SecurityStyleToProtocolSecurityState(
    SecurityStyle security_style) {
  switch (security_style) {
    case SECURITY_STYLE_UNKNOWN:
      return kSecurityStateUnknown;
    case SECURITY_STYLE_UNAUTHENTICATED:
      return kSecurityStateHttp;
    case SECURITY_STYLE_AUTHENTICATION_BROKEN:
      return kSecurityStateInsecure;
    case SECURITY_STYLE_WARNING:
      return kSecurityStateWarning;
    case SECURITY_STYLE_AUTHENTICATED:
      return kSecurityStateSecure;
    default:
      NOTREACHED();
      return kSecurityStateUnknown;
  }
}

void AddExplanations(
    const std::string& security_style,
    const std::vector<SecurityStyleExplanation>& explanations_to_add,
    std::vector<scoped_refptr<SecurityStateExplanation>>* explanations) {
  for (const auto& it : explanations_to_add) {
    explanations->push_back(SecurityStateExplanation::Create()
                                ->set_security_state(security_style)
                                ->set_summary(it.summary)
                                ->set_description(it.description));
  }
}

}  // namespace

SecurityHandler::SecurityHandler()
    : enabled_(false),
      host_(nullptr) {
}

SecurityHandler::~SecurityHandler() {
}

void SecurityHandler::SetClient(scoped_ptr<Client> client) {
  client_.swap(client);
}

void SecurityHandler::AttachToRenderFrameHost() {
  DCHECK(host_);
  WebContents* web_contents = WebContents::FromRenderFrameHost(host_);
  WebContentsObserver::Observe(web_contents);

  // Send an initial SecurityStyleChanged event.
  DCHECK(enabled_);
  SecurityStyleExplanations security_style_explanations;
  SecurityStyle security_style =
      web_contents->GetDelegate()->GetSecurityStyle(
          web_contents, &security_style_explanations);
  SecurityStyleChanged(security_style, security_style_explanations);
}

void SecurityHandler::SetRenderFrameHost(RenderFrameHost* host) {
  host_ = host;
  if (enabled_ && host_)
    AttachToRenderFrameHost();
}

void SecurityHandler::SecurityStyleChanged(
    SecurityStyle security_style,
    const SecurityStyleExplanations& security_style_explanations) {
  DCHECK(enabled_);

  const std::string security_state =
      SecurityStyleToProtocolSecurityState(security_style);

  std::vector<scoped_refptr<SecurityStateExplanation>> explanations;
  AddExplanations(kSecurityStateInsecure,
                  security_style_explanations.broken_explanations,
                  &explanations);
  AddExplanations(kSecurityStateWarning,
                  security_style_explanations.warning_explanations,
                  &explanations);

  client_->SecurityStateChanged(SecurityStateChangedParams::Create()
                                    ->set_security_state(security_state)
                                    ->set_explanations(explanations));
}

Response SecurityHandler::Enable() {
  enabled_ = true;
  if (host_)
    AttachToRenderFrameHost();

  return Response::OK();
}

Response SecurityHandler::Disable() {
  enabled_ = false;
  WebContentsObserver::Observe(nullptr);
  return Response::OK();
}

}  // namespace security
}  // namespace devtools
}  // namespace content
