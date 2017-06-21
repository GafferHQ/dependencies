// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_DELEGATE_IMPL_H_
#define NET_BASE_NETWORK_DELEGATE_IMPL_H_

#include "base/strings/string16.h"
#include "net/base/completion_callback.h"
#include "net/base/network_delegate.h"
#include "net/cookies/canonical_cookie.h"

class GURL;

namespace base {
class FilePath;
}

namespace net {

class CookieOptions;
class HttpRequestHeaders;
class HttpResponseHeaders;
class ProxyInfo;
class ProxyServer;
class ProxyService;
class URLRequest;

class NET_EXPORT NetworkDelegateImpl : public NetworkDelegate {
 public:
  ~NetworkDelegateImpl() override {}

 private:
  // This is the interface for subclasses of NetworkDelegate to implement. These
  // member functions will be called by the respective public notification
  // member function, which will perform basic sanity checking.

  // Called before a request is sent. Allows the delegate to rewrite the URL
  // being fetched by modifying |new_url|. If set, the URL must be valid. The
  // reference fragment from the original URL is not automatically appended to
  // |new_url|; callers are responsible for copying the reference fragment if
  // desired.
  // |callback| and |new_url| are valid only until OnURLRequestDestroyed is
  // called for this request. Returns a net status code, generally either OK to
  // continue with the request or ERR_IO_PENDING if the result is not ready yet.
  // A status code other than OK and ERR_IO_PENDING will cancel the request and
  // report the status code as the reason.
  //
  // The default implementation returns OK (continue with request).
  int OnBeforeURLRequest(URLRequest* request,
                         const CompletionCallback& callback,
                         GURL* new_url) override;

  // Called as the proxy is being resolved for |url|. Allows the delegate to
  // override the proxy resolution decision made by ProxyService. The delegate
  // may override the decision by modifying the ProxyInfo |result|.
  void OnResolveProxy(const GURL& url,
                      int load_flags,
                      const ProxyService& proxy_service,
                      ProxyInfo* result) override;

  // Called when use of |bad_proxy| fails due to |net_error|. |net_error| is
  // the network error encountered, if any, and OK if the fallback was
  // for a reason other than a network error (e.g. the proxy service was
  // explicitly directed to skip a proxy).
  void OnProxyFallback(const ProxyServer& bad_proxy, int net_error) override;

  // Called right before the HTTP headers are sent. Allows the delegate to
  // read/write |headers| before they get sent out. |callback| and |headers| are
  // valid only until OnCompleted or OnURLRequestDestroyed is called for this
  // request.
  // See OnBeforeURLRequest for return value description. Returns OK by default.
  int OnBeforeSendHeaders(URLRequest* request,
                          const CompletionCallback& callback,
                          HttpRequestHeaders* headers) override;

  // Called after a proxy connection. Allows the delegate to read/write
  // |headers| before they get sent out. |headers| is valid only until
  // OnCompleted or OnURLRequestDestroyed is called for this request.
  void OnBeforeSendProxyHeaders(URLRequest* request,
                                const ProxyInfo& proxy_info,
                                HttpRequestHeaders* headers) override;

  // Called right before the HTTP request(s) are being sent to the network.
  // |headers| is only valid until OnCompleted or OnURLRequestDestroyed is
  // called for this request.
  void OnSendHeaders(URLRequest* request,
                     const HttpRequestHeaders& headers) override;

  // Called for HTTP requests when the headers have been received.
  // |original_response_headers| contains the headers as received over the
  // network, these must not be modified. |override_response_headers| can be set
  // to new values, that should be considered as overriding
  // |original_response_headers|.
  // If the response is a redirect, and the Location response header value is
  // identical to |allowed_unsafe_redirect_url|, then the redirect is never
  // blocked and the reference fragment is not copied from the original URL
  // to the redirection target.
  //
  // |callback|, |original_response_headers|, and |override_response_headers|
  // are only valid until OnURLRequestDestroyed is called for this request.
  // See OnBeforeURLRequest for return value description. Returns OK by default.
  int OnHeadersReceived(
      URLRequest* request,
      const CompletionCallback& callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) override;

  // Called right after a redirect response code was received.
  // |new_location| is only valid until OnURLRequestDestroyed is called for this
  // request.
  void OnBeforeRedirect(URLRequest* request, const GURL& new_location) override;

  // This corresponds to URLRequestDelegate::OnResponseStarted.
  void OnResponseStarted(URLRequest* request) override;

  // Called every time we read raw bytes.
  void OnRawBytesRead(const URLRequest& request, int bytes_read) override;

  // Indicates that the URL request has been completed or failed.
  // |started| indicates whether the request has been started. If false,
  // some information like the socket address is not available.
  void OnCompleted(URLRequest* request, bool started) override;

  // Called when an URLRequest is being destroyed. Note that the request is
  // being deleted, so it's not safe to call any methods that may result in
  // a virtual method call.
  void OnURLRequestDestroyed(URLRequest* request) override;

  // Corresponds to ProxyResolverJSBindings::OnError.
  void OnPACScriptError(int line_number, const base::string16& error) override;

  // Called when a request receives an authentication challenge
  // specified by |auth_info|, and is unable to respond using cached
  // credentials. |callback| and |credentials| must be non-NULL, and must
  // be valid until OnURLRequestDestroyed is called for |request|.
  //
  // The following return values are allowed:
  //  - AUTH_REQUIRED_RESPONSE_NO_ACTION: |auth_info| is observed, but
  //    no action is being taken on it.
  //  - AUTH_REQUIRED_RESPONSE_SET_AUTH: |credentials| is filled in with
  //    a username and password, which should be used in a response to
  //    |auth_info|.
  //  - AUTH_REQUIRED_RESPONSE_CANCEL_AUTH: The authentication challenge
  //    should not be attempted.
  //  - AUTH_REQUIRED_RESPONSE_IO_PENDING: The action will be decided
  //    asynchronously. |callback| will be invoked when the decision is made,
  //    and one of the other AuthRequiredResponse values will be passed in with
  //    the same semantics as described above.
  AuthRequiredResponse OnAuthRequired(URLRequest* request,
                                      const AuthChallengeInfo& auth_info,
                                      const AuthCallback& callback,
                                      AuthCredentials* credentials) override;

  // Called when reading cookies to allow the network delegate to block access
  // to the cookie. This method will never be invoked when
  // LOAD_DO_NOT_SEND_COOKIES is specified.
  bool OnCanGetCookies(const URLRequest& request,
                       const CookieList& cookie_list) override;

  // Called when a cookie is set to allow the network delegate to block access
  // to the cookie. This method will never be invoked when
  // LOAD_DO_NOT_SAVE_COOKIES is specified.
  bool OnCanSetCookie(const URLRequest& request,
                      const std::string& cookie_line,
                      CookieOptions* options) override;

  // Called when a file access is attempted to allow the network delegate to
  // allow or block access to the given file path.  Returns true if access is
  // allowed.
  bool OnCanAccessFile(const URLRequest& request,
                       const base::FilePath& path) const override;

  // Returns true if the given |url| has to be requested over connection that
  // is not tracked by the server. Usually is false, unless user privacy
  // settings block cookies from being get or set.
  bool OnCanEnablePrivacyMode(
      const GURL& url,
      const GURL& first_party_for_cookies) const override;

  // Returns true if the embedder has enabled the "first-party" cookie
  // experiment, and false otherwise.
  //
  // TODO(mkwst): Remove this once we decide whether or not we wish to ship
  // first-party cookies. https://crbug.com/459154
  bool OnFirstPartyOnlyCookieExperimentEnabled() const override;

  // Called when the |referrer_url| for requesting |target_url| during handling
  // of the |request| is does not comply with the referrer policy (e.g. a
  // secure referrer for an insecure initial target).
  // Returns true if the request should be cancelled. Otherwise, the referrer
  // header is stripped from the request.
  bool OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      const URLRequest& request,
      const GURL& target_url,
      const GURL& referrer_url) const override;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_DELEGATE_IMPL_H_
