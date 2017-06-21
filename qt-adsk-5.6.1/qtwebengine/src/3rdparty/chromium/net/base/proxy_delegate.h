// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PROXY_DELEGATE_H_
#define NET_BASE_PROXY_DELEGATE_H_

#include "base/macros.h"
#include "net/base/net_export.h"

class GURL;

namespace net {

class HttpRequestHeaders;
class HttpResponseHeaders;
class HostPortPair;
class ProxyInfo;
class ProxyServer;
class ProxyService;
class URLRequest;

// Delegate for setting up a connection.
class NET_EXPORT ProxyDelegate {
 public:
  ProxyDelegate() {
  }

  virtual ~ProxyDelegate() {
  }

  // Called as the proxy is being resolved for |url|. Allows the delegate to
  // override the proxy resolution decision made by ProxyService. The delegate
  // may override the decision by modifying the ProxyInfo |result|.
  virtual void OnResolveProxy(const GURL& url,
                              int load_flags,
                              const ProxyService& proxy_service,
                              ProxyInfo* result) = 0;

  // Called when use of |bad_proxy| fails due to |net_error|. |net_error| is
  // the network error encountered, if any, and OK if the fallback was
  // for a reason other than a network error (e.g. the proxy service was
  // explicitly directed to skip a proxy).
  virtual void OnFallback(const ProxyServer& bad_proxy,
                          int net_error) = 0;

  // Called after a proxy connection. Allows the delegate to read/write
  // |headers| before they get sent out. |headers| is valid only until
  // OnCompleted or OnURLRequestDestroyed is called for this request.
  virtual void OnBeforeSendHeaders(URLRequest* request,
                                   const ProxyInfo& proxy_info,
                                   HttpRequestHeaders* headers) = 0;

  // Called immediately before a proxy tunnel request is sent.
  // Provides the embedder an opportunity to add extra request headers.
  virtual void OnBeforeTunnelRequest(const HostPortPair& proxy_server,
                                     HttpRequestHeaders* extra_headers) = 0;

  // Called when the connect attempt to a CONNECT proxy has completed.
  virtual void OnTunnelConnectCompleted(const HostPortPair& endpoint,
                                        const HostPortPair& proxy_server,
                                        int net_error) = 0;

  // Called after the response headers for the tunnel request are received.
  virtual void OnTunnelHeadersReceived(
      const HostPortPair& origin,
      const HostPortPair& proxy_server,
      const HttpResponseHeaders& response_headers) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyDelegate);
};

}

#endif  // NET_BASE_PROXY_DELEGATE_H_
