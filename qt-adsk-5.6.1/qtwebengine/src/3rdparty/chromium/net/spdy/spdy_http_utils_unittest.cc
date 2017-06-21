// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_utils.h"

#include "base/basictypes.h"
#include "net/http/http_request_info.h"
#include "net/spdy/spdy_framer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

bool kDirect = true;

}  // namespace

TEST(SpdyHttpUtilsTest, ConvertRequestPriorityToSpdy3Priority) {
  EXPECT_EQ(0, ConvertRequestPriorityToSpdyPriority(HIGHEST, SPDY3));
  EXPECT_EQ(1, ConvertRequestPriorityToSpdyPriority(MEDIUM, SPDY3));
  EXPECT_EQ(2, ConvertRequestPriorityToSpdyPriority(LOW, SPDY3));
  EXPECT_EQ(3, ConvertRequestPriorityToSpdyPriority(LOWEST, SPDY3));
  EXPECT_EQ(4, ConvertRequestPriorityToSpdyPriority(IDLE, SPDY3));
}

TEST(SpdyHttpUtilsTest, ConvertSpdy3PriorityToRequestPriority) {
  EXPECT_EQ(HIGHEST, ConvertSpdyPriorityToRequestPriority(0, SPDY3));
  EXPECT_EQ(MEDIUM, ConvertSpdyPriorityToRequestPriority(1, SPDY3));
  EXPECT_EQ(LOW, ConvertSpdyPriorityToRequestPriority(2, SPDY3));
  EXPECT_EQ(LOWEST, ConvertSpdyPriorityToRequestPriority(3, SPDY3));
  EXPECT_EQ(IDLE, ConvertSpdyPriorityToRequestPriority(4, SPDY3));
  // These are invalid values, but we should still handle them
  // gracefully.
  for (int i = 5; i < kuint8max; ++i) {
    EXPECT_EQ(IDLE, ConvertSpdyPriorityToRequestPriority(i, SPDY3));
  }
}

TEST(SpdyHttpUtilsTest, CreateSpdyHeadersFromHttpRequestSPDY2) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  SpdyHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, request.extra_headers, SPDY2,
                                   kDirect, &headers);
  EXPECT_EQ("GET", headers["method"]);
  EXPECT_EQ("https", headers["scheme"]);
  EXPECT_EQ("www.google.com", headers["host"]);
  EXPECT_EQ("/index.html", headers["url"]);
  EXPECT_EQ("HTTP/1.1", headers["version"]);
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

TEST(SpdyHttpUtilsTest, CreateSpdyHeadersFromHttpRequestSPDY3) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  SpdyHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, request.extra_headers, SPDY3,
                                   kDirect, &headers);
  EXPECT_EQ("GET", headers[":method"]);
  EXPECT_EQ("https", headers[":scheme"]);
  EXPECT_EQ("www.google.com", headers[":host"]);
  EXPECT_EQ("/index.html", headers[":path"]);
  EXPECT_EQ("HTTP/1.1", headers[":version"]);
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

TEST(SpdyHttpUtilsTest, CreateSpdyHeadersFromHttpRequestHTTP2) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  SpdyHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, request.extra_headers, HTTP2,
                                   kDirect, &headers);
  EXPECT_EQ("GET", headers[":method"]);
  EXPECT_EQ("https", headers[":scheme"]);
  EXPECT_EQ("www.google.com", headers[":authority"]);
  EXPECT_EQ("/index.html", headers[":path"]);
  EXPECT_TRUE(headers.end() == headers.find(":version"));
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

TEST(SpdyHttpUtilsTest, CreateSpdyHeadersFromHttpRequestProxySPDY2) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  SpdyHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, request.extra_headers, SPDY2,
                                   !kDirect, &headers);
  EXPECT_EQ("GET", headers["method"]);
  EXPECT_EQ("https", headers["scheme"]);
  EXPECT_EQ("www.google.com", headers["host"]);
  EXPECT_EQ("https://www.google.com/index.html", headers["url"]);
  EXPECT_EQ("HTTP/1.1", headers["version"]);
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

TEST(SpdyHttpUtilsTest, CreateSpdyHeadersFromHttpRequestProxySPDY3) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  SpdyHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, request.extra_headers, SPDY3,
                                   !kDirect, &headers);
  EXPECT_EQ("GET", headers[":method"]);
  EXPECT_EQ("https", headers[":scheme"]);
  EXPECT_EQ("www.google.com", headers[":host"]);
  EXPECT_EQ("/index.html", headers[":path"]);
  EXPECT_EQ("HTTP/1.1", headers[":version"]);
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

TEST(SpdyHttpUtilsTest, CreateSpdyHeadersFromHttpRequestProxyHTTP2) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  SpdyHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, request.extra_headers, HTTP2,
                                   !kDirect, &headers);
  EXPECT_EQ("GET", headers[":method"]);
  EXPECT_EQ("https", headers[":scheme"]);
  EXPECT_EQ("www.google.com", headers[":authority"]);
  EXPECT_EQ("/index.html", headers[":path"]);
  EXPECT_TRUE(headers.end() == headers.find(":version"));
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

TEST(SpdyHttpUtilsTest, CreateSpdyHeadersFromHttpRequestConnectSPDY2) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "CONNECT";
  request.url = url;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  SpdyHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, request.extra_headers, SPDY2,
                                   kDirect, &headers);
  EXPECT_EQ("CONNECT", headers["method"]);
  EXPECT_TRUE(headers.end() == headers.find("scheme"));
  EXPECT_EQ("www.google.com", headers["host"]);
  EXPECT_EQ("www.google.com:443", headers["url"]);
  EXPECT_EQ("HTTP/1.1", headers["version"]);
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

TEST(SpdyHttpUtilsTest, CreateSpdyHeadersFromHttpRequestConnectSPDY3) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "CONNECT";
  request.url = url;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  SpdyHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, request.extra_headers, SPDY3,
                                   kDirect, &headers);
  EXPECT_EQ("CONNECT", headers[":method"]);
  EXPECT_TRUE(headers.end() == headers.find(":scheme"));
  EXPECT_EQ("www.google.com", headers[":host"]);
  EXPECT_EQ("www.google.com:443", headers[":path"]);
  EXPECT_EQ("HTTP/1.1", headers[":version"]);
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

TEST(SpdyHttpUtilsTest, CreateSpdyHeadersFromHttpRequestConnectHTTP2) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "CONNECT";
  request.url = url;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  SpdyHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, request.extra_headers, HTTP2,
                                   kDirect, &headers);
  EXPECT_EQ("CONNECT", headers[":method"]);
  EXPECT_TRUE(headers.end() == headers.find(":scheme"));
  EXPECT_EQ("www.google.com", headers[":authority"]);
  EXPECT_EQ("www.google.com:443", headers[":path"]);
  EXPECT_TRUE(headers.end() == headers.find(":version"));
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

}  // namespace net
