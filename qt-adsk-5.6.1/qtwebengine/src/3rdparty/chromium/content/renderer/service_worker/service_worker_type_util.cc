// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_type_util.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerResponse.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

namespace {

class HeaderVisitor : public blink::WebHTTPHeaderVisitor {
 public:
  HeaderVisitor(ServiceWorkerHeaderMap* headers) : headers_(headers) {}
  virtual ~HeaderVisitor() {}

  virtual void visitHeader(const blink::WebString& name,
                           const blink::WebString& value) {
    const std::string header_name = base::UTF16ToASCII(name);
    const std::string header_value = base::UTF16ToASCII(value);
    CHECK(header_name.find('\0') == std::string::npos);
    CHECK(header_value.find('\0') == std::string::npos);
    headers_->insert(ServiceWorkerHeaderMap::value_type(
        header_name, header_value));
  }

 private:
  ServiceWorkerHeaderMap* headers_;
};

scoped_ptr<HeaderVisitor> MakeHeaderVisitor(ServiceWorkerHeaderMap* headers) {
  return scoped_ptr<HeaderVisitor>(new HeaderVisitor(headers)).Pass();
}

}  // namespace

void GetServiceWorkerHeaderMapFromWebRequest(
    const blink::WebServiceWorkerRequest& web_request,
    ServiceWorkerHeaderMap* headers) {
  DCHECK(headers);
  DCHECK(!headers->size());
  web_request.visitHTTPHeaderFields(MakeHeaderVisitor(headers).get());
}

void GetServiceWorkerHeaderMapFromWebResponse(
    const blink::WebServiceWorkerResponse& web_response,
    ServiceWorkerHeaderMap* headers) {
  DCHECK(headers);
  DCHECK(!headers->size());
  web_response.visitHTTPHeaderFields(MakeHeaderVisitor(headers).get());
}

}  // namespace content
