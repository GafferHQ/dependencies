// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_delegate_impl.h"

#include "net/base/net_errors.h"

namespace net {

int NetworkDelegateImpl::OnBeforeURLRequest(URLRequest* request,
                                            const CompletionCallback& callback,
                                            GURL* new_url) {
  return OK;
}

void NetworkDelegateImpl::OnResolveProxy(const GURL& url,
                                         int load_flags,
                                         const ProxyService& proxy_service,
                                         ProxyInfo* result) {
}

void NetworkDelegateImpl::OnProxyFallback(const ProxyServer& bad_proxy,
                                          int net_error) {
}

int NetworkDelegateImpl::OnBeforeSendHeaders(URLRequest* request,
                                             const CompletionCallback& callback,
                                             HttpRequestHeaders* headers) {
  return OK;
}

void NetworkDelegateImpl::OnBeforeSendProxyHeaders(
    URLRequest* request,
    const ProxyInfo& proxy_info,
    HttpRequestHeaders* headers) {
}

void NetworkDelegateImpl::OnSendHeaders(URLRequest* request,
                                        const HttpRequestHeaders& headers) {
}

int NetworkDelegateImpl::OnHeadersReceived(
    URLRequest* request,
    const CompletionCallback& callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  return OK;
}

void NetworkDelegateImpl::OnBeforeRedirect(URLRequest* request,
                                           const GURL& new_location) {
}

void NetworkDelegateImpl::OnResponseStarted(URLRequest* request) {
}

void NetworkDelegateImpl::OnRawBytesRead(const URLRequest& request,
                                         int bytes_read) {
}

void NetworkDelegateImpl::OnCompleted(URLRequest* request, bool started) {
}

void NetworkDelegateImpl::OnURLRequestDestroyed(URLRequest* request) {
}

void NetworkDelegateImpl::OnPACScriptError(int line_number,
                                           const base::string16& error) {
}

NetworkDelegate::AuthRequiredResponse NetworkDelegateImpl::OnAuthRequired(
    URLRequest* request,
    const AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    AuthCredentials* credentials) {
  return AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool NetworkDelegateImpl::OnCanGetCookies(const URLRequest& request,
                                          const CookieList& cookie_list) {
  return true;
}

bool NetworkDelegateImpl::OnCanSetCookie(const URLRequest& request,
                                         const std::string& cookie_line,
                                         CookieOptions* options) {
  return true;
}

bool NetworkDelegateImpl::OnCanAccessFile(const URLRequest& request,
                                          const base::FilePath& path) const {
  return false;
}

bool NetworkDelegateImpl::OnCanEnablePrivacyMode(
    const GURL& url,
    const GURL& first_party_for_cookies) const {
  return false;
}

bool NetworkDelegateImpl::OnFirstPartyOnlyCookieExperimentEnabled() const {
  return false;
}

bool NetworkDelegateImpl::OnCancelURLRequestWithPolicyViolatingReferrerHeader(
    const URLRequest& request,
    const GURL& target_url,
    const GURL& referrer_url) const {
  return false;
}

}  // namespace net
