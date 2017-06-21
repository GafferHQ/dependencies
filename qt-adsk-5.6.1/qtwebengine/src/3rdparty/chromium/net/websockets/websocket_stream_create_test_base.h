// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_STREAM_CREATE_TEST_BASE_H_
#define NET_WEBSOCKETS_WEBSOCKET_STREAM_CREATE_TEST_BASE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "net/base/net_export.h"
#include "net/socket/socket_test_util.h"
#include "net/ssl/ssl_info.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_test_util.h"

namespace net {

class HttpRequestHeaders;
class HttpResponseHeaders;
class WebSocketStream;
class WebSocketStreamRequest;
struct WebSocketHandshakeRequestInfo;
struct WebSocketHandshakeResponseInfo;

class WebSocketStreamCreateTestBase {
 public:
  using HeaderKeyValuePair = std::pair<std::string, std::string>;

  WebSocketStreamCreateTestBase();
  virtual ~WebSocketStreamCreateTestBase();

  // A wrapper for CreateAndConnectStreamForTesting that knows about our default
  // parameters.
  void CreateAndConnectStream(const std::string& socket_url,
                              const std::vector<std::string>& sub_protocols,
                              const std::string& origin,
                              scoped_ptr<base::Timer> timer);

  static std::vector<HeaderKeyValuePair> RequestHeadersToVector(
      const HttpRequestHeaders& headers);
  static std::vector<HeaderKeyValuePair> ResponseHeadersToVector(
      const HttpResponseHeaders& headers);

  const std::string& failure_message() const { return failure_message_; }
  bool has_failed() const { return has_failed_; }

  // Runs |connect_run_loop_|. It will stop when the connection establishes or
  // fails.
  void WaitUntilConnectDone();

  // A simple function to make the tests more readable.
  std::vector<std::string> NoSubProtocols();

 protected:
  WebSocketTestURLRequestContextHost url_request_context_host_;
  scoped_ptr<WebSocketStreamRequest> stream_request_;
  // Only set if the connection succeeded.
  scoped_ptr<WebSocketStream> stream_;
  // Only set if the connection failed.
  std::string failure_message_;
  bool has_failed_;
  scoped_ptr<WebSocketHandshakeRequestInfo> request_info_;
  scoped_ptr<WebSocketHandshakeResponseInfo> response_info_;
  scoped_ptr<WebSocketEventInterface::SSLErrorCallbacks> ssl_error_callbacks_;
  SSLInfo ssl_info_;
  bool ssl_fatal_;
  ScopedVector<SSLSocketDataProvider> ssl_data_;

  // This temporarily sets WebSocketEndpointLockManager unlock delay to zero
  // during tests.
  ScopedWebSocketEndpointZeroUnlockDelay zero_unlock_delay_;
  base::RunLoop connect_run_loop_;

 private:
  class TestConnectDelegate;
  DISALLOW_COPY_AND_ASSIGN(WebSocketStreamCreateTestBase);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_STREAM_CREATE_TEST_BASE_H_
