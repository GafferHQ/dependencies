// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_test_util.h"

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socket_test_util.h"

namespace net {

namespace {
const uint64 kA =
    (static_cast<uint64>(0x5851f42d) << 32) + static_cast<uint64>(0x4c957f2d);
const uint64 kC = 12345;
const uint64 kM = static_cast<uint64>(1) << 48;

}  // namespace

LinearCongruentialGenerator::LinearCongruentialGenerator(uint32 seed)
    : current_(seed) {}

uint32 LinearCongruentialGenerator::Generate() {
  uint64 result = current_;
  current_ = (current_ * kA + kC) % kM;
  return static_cast<uint32>(result >> 16);
}

std::string WebSocketStandardRequest(const std::string& path,
                                     const std::string& host,
                                     const std::string& origin,
                                     const std::string& extra_headers) {
  return WebSocketStandardRequestWithCookies(path, host, origin, std::string(),
                                             extra_headers);
}

std::string WebSocketStandardRequestWithCookies(
    const std::string& path,
    const std::string& host,
    const std::string& origin,
    const std::string& cookies,
    const std::string& extra_headers) {
  // Unrelated changes in net/http may change the order and default-values of
  // HTTP headers, causing WebSocket tests to fail. It is safe to update this
  // string in that case.
  return base::StringPrintf(
      "GET %s HTTP/1.1\r\n"
      "Host: %s\r\n"
      "Connection: Upgrade\r\n"
      "Pragma: no-cache\r\n"
      "Cache-Control: no-cache\r\n"
      "Upgrade: websocket\r\n"
      "Origin: %s\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "User-Agent:\r\n"
      "Accept-Encoding: gzip, deflate\r\n"
      "Accept-Language: en-us,fr\r\n"
      "%s"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n"
      "%s\r\n",
      path.c_str(), host.c_str(), origin.c_str(), cookies.c_str(),
      extra_headers.c_str());
}

std::string WebSocketStandardResponse(const std::string& extra_headers) {
  return base::StringPrintf(
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "%s\r\n",
      extra_headers.c_str());
}

struct WebSocketMockClientSocketFactoryMaker::Detail {
  std::string expect_written;
  std::string return_to_read;
  std::vector<MockRead> reads;
  MockWrite write;
  ScopedVector<SequencedSocketData> socket_data_vector;
  ScopedVector<SSLSocketDataProvider> ssl_socket_data_vector;
  MockClientSocketFactory factory;
};

WebSocketMockClientSocketFactoryMaker::WebSocketMockClientSocketFactoryMaker()
    : detail_(new Detail) {
}

WebSocketMockClientSocketFactoryMaker::
    ~WebSocketMockClientSocketFactoryMaker() {
}

MockClientSocketFactory* WebSocketMockClientSocketFactoryMaker::factory() {
  return &detail_->factory;
}

void WebSocketMockClientSocketFactoryMaker::SetExpectations(
    const std::string& expect_written,
    const std::string& return_to_read) {
  const size_t kHttpStreamParserBufferSize = 4096;
  // We need to extend the lifetime of these strings.
  detail_->expect_written = expect_written;
  detail_->return_to_read = return_to_read;
  int sequence = 0;
  detail_->write = MockWrite(SYNCHRONOUS,
                             detail_->expect_written.data(),
                             detail_->expect_written.size(),
                             sequence++);
  // HttpStreamParser reads 4KB at a time. We need to take this implementation
  // detail into account if |return_to_read| is big enough.
  for (size_t place = 0; place < detail_->return_to_read.size();
       place += kHttpStreamParserBufferSize) {
    detail_->reads.push_back(
        MockRead(SYNCHRONOUS, detail_->return_to_read.data() + place,
                 std::min(detail_->return_to_read.size() - place,
                          kHttpStreamParserBufferSize),
                 sequence++));
  }
  scoped_ptr<SequencedSocketData> socket_data(
      new SequencedSocketData(vector_as_array(&detail_->reads),
                              detail_->reads.size(), &detail_->write, 1));
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  AddRawExpectations(socket_data.Pass());
}

void WebSocketMockClientSocketFactoryMaker::AddRawExpectations(
    scoped_ptr<SequencedSocketData> socket_data) {
  detail_->factory.AddSocketDataProvider(socket_data.get());
  detail_->socket_data_vector.push_back(socket_data.Pass());
}

void WebSocketMockClientSocketFactoryMaker::AddSSLSocketDataProvider(
    scoped_ptr<SSLSocketDataProvider> ssl_socket_data) {
  detail_->factory.AddSSLSocketDataProvider(ssl_socket_data.get());
  detail_->ssl_socket_data_vector.push_back(ssl_socket_data.Pass());
}

WebSocketTestURLRequestContextHost::WebSocketTestURLRequestContextHost()
    : url_request_context_(true), url_request_context_initialized_(false) {
  url_request_context_.set_client_socket_factory(maker_.factory());
}

WebSocketTestURLRequestContextHost::~WebSocketTestURLRequestContextHost() {}

void WebSocketTestURLRequestContextHost::AddRawExpectations(
    scoped_ptr<SequencedSocketData> socket_data) {
  maker_.AddRawExpectations(socket_data.Pass());
}

void WebSocketTestURLRequestContextHost::AddSSLSocketDataProvider(
    scoped_ptr<SSLSocketDataProvider> ssl_socket_data) {
  maker_.AddSSLSocketDataProvider(ssl_socket_data.Pass());
}

void WebSocketTestURLRequestContextHost::SetProxyConfig(
    const std::string& proxy_rules) {
  DCHECK(!url_request_context_initialized_);
  proxy_service_.reset(ProxyService::CreateFixed(proxy_rules));
  url_request_context_.set_proxy_service(proxy_service_.get());
}

TestURLRequestContext*
WebSocketTestURLRequestContextHost::GetURLRequestContext() {
  if (!url_request_context_initialized_) {
    url_request_context_.Init();
    // A Network Delegate is required to make the URLRequest::Delegate work.
    url_request_context_.set_network_delegate(&network_delegate_);
    url_request_context_initialized_ = true;
  }
  return &url_request_context_;
}

}  // namespace net
