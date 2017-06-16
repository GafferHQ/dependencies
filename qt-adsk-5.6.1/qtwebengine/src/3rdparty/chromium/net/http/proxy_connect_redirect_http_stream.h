// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_PROXY_CONNECT_REDIRECT_HTTP_STREAM_H_
#define NET_HTTP_PROXY_CONNECT_REDIRECT_HTTP_STREAM_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_stream.h"

namespace net {

// A dummy HttpStream with no body used when a redirect is returned
// from a proxy.
class ProxyConnectRedirectHttpStream : public HttpStream {
 public:
  // |load_timing_info| is the info that should be returned by
  // GetLoadTimingInfo(), or NULL if there is none. Does not take
  // ownership of |load_timing_info|.
  explicit ProxyConnectRedirectHttpStream(LoadTimingInfo* load_timing_info);
  ~ProxyConnectRedirectHttpStream() override;

  // All functions below are expected not to be called except for the
  // marked one.

  int InitializeStream(const HttpRequestInfo* request_info,
                       RequestPriority priority,
                       const BoundNetLog& net_log,
                       const CompletionCallback& callback) override;
  int SendRequest(const HttpRequestHeaders& request_headers,
                  HttpResponseInfo* response,
                  const CompletionCallback& callback) override;
  int ReadResponseHeaders(const CompletionCallback& callback) override;
  int ReadResponseBody(IOBuffer* buf,
                       int buf_len,
                       const CompletionCallback& callback) override;

  // This function may be called.
  void Close(bool not_reusable) override;

  bool IsResponseBodyComplete() const override;

  // This function may be called.
  bool CanFindEndOfResponse() const override;

  bool IsConnectionReused() const override;
  void SetConnectionReused() override;
  bool IsConnectionReusable() const override;

  int64 GetTotalReceivedBytes() const override;

  // This function may be called.
  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override;

  void GetSSLInfo(SSLInfo* ssl_info) override;
  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) override;
  bool IsSpdyHttpStream() const override;
  void Drain(HttpNetworkSession* session) override;

  // This function may be called.
  void SetPriority(RequestPriority priority) override;

  UploadProgress GetUploadProgress() const override;
  HttpStream* RenewStreamForAuth() override;

 private:
  bool has_load_timing_info_;
  LoadTimingInfo load_timing_info_;
};

}  // namespace net

#endif  // NET_HTTP_PROXY_CONNECT_REDIRECT_HTTP_STREAM_H_
