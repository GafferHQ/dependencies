// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/layered_network_delegate.h"

namespace net {

LayeredNetworkDelegate::LayeredNetworkDelegate(
    scoped_ptr<NetworkDelegate> nested_network_delegate)
    : nested_network_delegate_(nested_network_delegate.Pass()) {
}

LayeredNetworkDelegate::~LayeredNetworkDelegate() {
}

int LayeredNetworkDelegate::OnBeforeURLRequest(
    URLRequest* request,
    const CompletionCallback& callback,
    GURL* new_url) {
  OnBeforeURLRequestInternal(request, callback, new_url);
  return nested_network_delegate_->NotifyBeforeURLRequest(request, callback,
                                                          new_url);
}

void LayeredNetworkDelegate::OnBeforeURLRequestInternal(
    URLRequest* request,
    const CompletionCallback& callback,
    GURL* new_url) {
}

void LayeredNetworkDelegate::OnResolveProxy(const GURL& url,
                                            int load_flags,
                                            const ProxyService& proxy_service,
                                            ProxyInfo* result) {
  OnResolveProxyInternal(url, load_flags, proxy_service, result);
  nested_network_delegate_->NotifyResolveProxy(url, load_flags, proxy_service,
                                               result);
}

void LayeredNetworkDelegate::OnResolveProxyInternal(
    const GURL& url,
    int load_flags,
    const ProxyService& proxy_service,
    ProxyInfo* result) {
}

void LayeredNetworkDelegate::OnProxyFallback(const ProxyServer& bad_proxy,
                                             int net_error) {
  OnProxyFallbackInternal(bad_proxy, net_error);
  nested_network_delegate_->NotifyProxyFallback(bad_proxy, net_error);
}

void LayeredNetworkDelegate::OnProxyFallbackInternal(
    const ProxyServer& bad_proxy,
    int net_error) {
}

int LayeredNetworkDelegate::OnBeforeSendHeaders(
    URLRequest* request,
    const CompletionCallback& callback,
    HttpRequestHeaders* headers) {
  OnBeforeSendHeadersInternal(request, callback, headers);
  return nested_network_delegate_->NotifyBeforeSendHeaders(request, callback,
                                                           headers);
}

void LayeredNetworkDelegate::OnBeforeSendHeadersInternal(
    URLRequest* request,
    const CompletionCallback& callback,
    HttpRequestHeaders* headers) {
}

void LayeredNetworkDelegate::OnBeforeSendProxyHeaders(
    URLRequest* request,
    const ProxyInfo& proxy_info,
    HttpRequestHeaders* headers) {
  OnBeforeSendProxyHeadersInternal(request, proxy_info, headers);
  nested_network_delegate_->NotifyBeforeSendProxyHeaders(request, proxy_info,
                                                         headers);
}

void LayeredNetworkDelegate::OnBeforeSendProxyHeadersInternal(
    URLRequest* request,
    const ProxyInfo& proxy_info,
    HttpRequestHeaders* headers) {
}

void LayeredNetworkDelegate::OnSendHeaders(URLRequest* request,
                                           const HttpRequestHeaders& headers) {
  OnSendHeadersInternal(request, headers);
  nested_network_delegate_->NotifySendHeaders(request, headers);
}

void LayeredNetworkDelegate::OnSendHeadersInternal(
    URLRequest* request,
    const HttpRequestHeaders& headers) {
}

int LayeredNetworkDelegate::OnHeadersReceived(
    URLRequest* request,
    const CompletionCallback& callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  OnHeadersReceivedInternal(request, callback, original_response_headers,
                            override_response_headers,
                            allowed_unsafe_redirect_url);
  return nested_network_delegate_->NotifyHeadersReceived(
      request, callback, original_response_headers, override_response_headers,
      allowed_unsafe_redirect_url);
}

void LayeredNetworkDelegate::OnHeadersReceivedInternal(
    URLRequest* request,
    const CompletionCallback& callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
}

void LayeredNetworkDelegate::OnBeforeRedirect(URLRequest* request,
                                              const GURL& new_location) {
  OnBeforeRedirectInternal(request, new_location);
  nested_network_delegate_->NotifyBeforeRedirect(request, new_location);
}

void LayeredNetworkDelegate::OnBeforeRedirectInternal(
    URLRequest* request,
    const GURL& new_location) {
}

void LayeredNetworkDelegate::OnResponseStarted(URLRequest* request) {
  OnResponseStartedInternal(request);
  nested_network_delegate_->NotifyResponseStarted(request);
}

void LayeredNetworkDelegate::OnResponseStartedInternal(URLRequest* request) {
}

void LayeredNetworkDelegate::OnRawBytesRead(const URLRequest& request,
                                            int bytes_read) {
  OnRawBytesReadInternal(request, bytes_read);
  nested_network_delegate_->NotifyRawBytesRead(request, bytes_read);
}

void LayeredNetworkDelegate::OnRawBytesReadInternal(const URLRequest& request,
                                                    int bytes_read) {
}

void LayeredNetworkDelegate::OnCompleted(URLRequest* request, bool started) {
  OnCompletedInternal(request, started);
  nested_network_delegate_->NotifyCompleted(request, started);
}

void LayeredNetworkDelegate::OnCompletedInternal(URLRequest* request,
                                                 bool started) {
}

void LayeredNetworkDelegate::OnURLRequestDestroyed(URLRequest* request) {
  OnURLRequestDestroyedInternal(request);
  nested_network_delegate_->NotifyURLRequestDestroyed(request);
}

void LayeredNetworkDelegate::OnURLRequestDestroyedInternal(
    URLRequest* request) {
}

void LayeredNetworkDelegate::OnPACScriptError(int line_number,
                                              const base::string16& error) {
  OnPACScriptErrorInternal(line_number, error);
  nested_network_delegate_->NotifyPACScriptError(line_number, error);
}

void LayeredNetworkDelegate::OnPACScriptErrorInternal(
    int line_number,
    const base::string16& error) {
}

NetworkDelegate::AuthRequiredResponse LayeredNetworkDelegate::OnAuthRequired(
    URLRequest* request,
    const AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    AuthCredentials* credentials) {
  OnAuthRequiredInternal(request, auth_info, callback, credentials);
  return nested_network_delegate_->NotifyAuthRequired(request, auth_info,
                                                      callback, credentials);
}

void LayeredNetworkDelegate::OnAuthRequiredInternal(
    URLRequest* request,
    const AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    AuthCredentials* credentials) {
}

bool LayeredNetworkDelegate::OnCanGetCookies(const URLRequest& request,
                                             const CookieList& cookie_list) {
  OnCanGetCookiesInternal(request, cookie_list);
  return nested_network_delegate_->CanGetCookies(request, cookie_list);
}

void LayeredNetworkDelegate::OnCanGetCookiesInternal(
    const URLRequest& request,
    const CookieList& cookie_list) {
}

bool LayeredNetworkDelegate::OnCanSetCookie(const URLRequest& request,
                                            const std::string& cookie_line,
                                            CookieOptions* options) {
  OnCanSetCookieInternal(request, cookie_line, options);
  return nested_network_delegate_->CanSetCookie(request, cookie_line, options);
}

void LayeredNetworkDelegate::OnCanSetCookieInternal(
    const URLRequest& request,
    const std::string& cookie_line,
    CookieOptions* options) {
}

bool LayeredNetworkDelegate::OnCanAccessFile(const URLRequest& request,
                                             const base::FilePath& path) const {
  OnCanAccessFileInternal(request, path);
  return nested_network_delegate_->CanAccessFile(request, path);
}

void LayeredNetworkDelegate::OnCanAccessFileInternal(
    const URLRequest& request,
    const base::FilePath& path) const {
}

bool LayeredNetworkDelegate::OnCanEnablePrivacyMode(
    const GURL& url,
    const GURL& first_party_for_cookies) const {
  OnCanEnablePrivacyModeInternal(url, first_party_for_cookies);
  return nested_network_delegate_->CanEnablePrivacyMode(
      url, first_party_for_cookies);
}

void LayeredNetworkDelegate::OnCanEnablePrivacyModeInternal(
    const GURL& url,
    const GURL& first_party_for_cookies) const {
}

bool LayeredNetworkDelegate::OnFirstPartyOnlyCookieExperimentEnabled() const {
  OnFirstPartyOnlyCookieExperimentEnabledInternal();
  return nested_network_delegate_->FirstPartyOnlyCookieExperimentEnabled();
}

void LayeredNetworkDelegate::OnFirstPartyOnlyCookieExperimentEnabledInternal()
    const {
}

bool LayeredNetworkDelegate::
    OnCancelURLRequestWithPolicyViolatingReferrerHeader(
        const URLRequest& request,
        const GURL& target_url,
        const GURL& referrer_url) const {
  OnCancelURLRequestWithPolicyViolatingReferrerHeaderInternal(
      request, target_url, referrer_url);
  return nested_network_delegate_
      ->CancelURLRequestWithPolicyViolatingReferrerHeader(request, target_url,
                                                          referrer_url);
}

void LayeredNetworkDelegate::
    OnCancelURLRequestWithPolicyViolatingReferrerHeaderInternal(
        const URLRequest& request,
        const GURL& target_url,
        const GURL& referrer_url) const {
}

}  // namespace net
