// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_url_request_util.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"

using blink::WebHTTPBody;
using blink::WebString;
using blink::WebURLRequest;

namespace content {

namespace {

const char kThrottledErrorDescription[] =
    "Request throttled. Visit http://dev.chromium.org/throttling for more "
    "information.";

class HeaderFlattener : public blink::WebHTTPHeaderVisitor {
 public:
  HeaderFlattener() : has_accept_header_(false) {}

  virtual void visitHeader(const WebString& name, const WebString& value) {
    // Headers are latin1.
    const std::string& name_latin1 = name.latin1();
    const std::string& value_latin1 = value.latin1();

    // Skip over referrer headers found in the header map because we already
    // pulled it out as a separate parameter.
    if (base::LowerCaseEqualsASCII(name_latin1, "referer"))
      return;

    if (base::LowerCaseEqualsASCII(name_latin1, "accept"))
      has_accept_header_ = true;

    if (!buffer_.empty())
      buffer_.append("\r\n");
    buffer_.append(name_latin1 + ": " + value_latin1);
  }

  const std::string& GetBuffer() {
    // In some cases, WebKit doesn't add an Accept header, but not having the
    // header confuses some web servers.  See bug 808613.
    if (!has_accept_header_) {
      if (!buffer_.empty())
        buffer_.append("\r\n");
      buffer_.append("Accept: */*");
      has_accept_header_ = true;
    }
    return buffer_;
  }

 private:
  std::string buffer_;
  bool has_accept_header_;
};

}  // namespace

ResourceType WebURLRequestToResourceType(const WebURLRequest& request) {
  WebURLRequest::RequestContext requestContext = request.requestContext();
  if (request.frameType() != WebURLRequest::FrameTypeNone) {
    DCHECK(requestContext == WebURLRequest::RequestContextForm ||
           requestContext == WebURLRequest::RequestContextFrame ||
           requestContext == WebURLRequest::RequestContextHyperlink ||
           requestContext == WebURLRequest::RequestContextIframe ||
           requestContext == WebURLRequest::RequestContextInternal ||
           requestContext == WebURLRequest::RequestContextLocation);
    if (request.frameType() == WebURLRequest::FrameTypeTopLevel ||
        request.frameType() == WebURLRequest::FrameTypeAuxiliary) {
      return RESOURCE_TYPE_MAIN_FRAME;
    }
    if (request.frameType() == WebURLRequest::FrameTypeNested)
      return RESOURCE_TYPE_SUB_FRAME;
    NOTREACHED();
    return RESOURCE_TYPE_SUB_RESOURCE;
  }

  switch (requestContext) {
    // Favicon
    case WebURLRequest::RequestContextFavicon:
      return RESOURCE_TYPE_FAVICON;

    // Font
    case WebURLRequest::RequestContextFont:
      return RESOURCE_TYPE_FONT_RESOURCE;

    // Image
    case WebURLRequest::RequestContextImage:
    case WebURLRequest::RequestContextImageSet:
      return RESOURCE_TYPE_IMAGE;

    // Media
    case WebURLRequest::RequestContextAudio:
    case WebURLRequest::RequestContextVideo:
      return RESOURCE_TYPE_MEDIA;

    // Object
    case WebURLRequest::RequestContextEmbed:
    case WebURLRequest::RequestContextObject:
      return RESOURCE_TYPE_OBJECT;

    // Ping
    case WebURLRequest::RequestContextBeacon:
    case WebURLRequest::RequestContextCSPReport:
    case WebURLRequest::RequestContextPing:
      return RESOURCE_TYPE_PING;

    // Prefetch
    case WebURLRequest::RequestContextPrefetch:
      return RESOURCE_TYPE_PREFETCH;

    // Script
    case WebURLRequest::RequestContextImport:
    case WebURLRequest::RequestContextScript:
      return RESOURCE_TYPE_SCRIPT;

    // Style
    case WebURLRequest::RequestContextXSLT:
    case WebURLRequest::RequestContextStyle:
      return RESOURCE_TYPE_STYLESHEET;

    // Subresource
    case WebURLRequest::RequestContextDownload:
    case WebURLRequest::RequestContextManifest:
    case WebURLRequest::RequestContextSubresource:
    case WebURLRequest::RequestContextPlugin:
      return RESOURCE_TYPE_SUB_RESOURCE;

    // TextTrack
    case WebURLRequest::RequestContextTrack:
      return RESOURCE_TYPE_MEDIA;

    // Workers
    case WebURLRequest::RequestContextServiceWorker:
      return RESOURCE_TYPE_SERVICE_WORKER;
    case WebURLRequest::RequestContextSharedWorker:
      return RESOURCE_TYPE_SHARED_WORKER;
    case WebURLRequest::RequestContextWorker:
      return RESOURCE_TYPE_WORKER;

    // Unspecified
    case WebURLRequest::RequestContextInternal:
    case WebURLRequest::RequestContextUnspecified:
      return RESOURCE_TYPE_SUB_RESOURCE;

    // XHR
    case WebURLRequest::RequestContextEventSource:
    case WebURLRequest::RequestContextFetch:
    case WebURLRequest::RequestContextXMLHttpRequest:
      return RESOURCE_TYPE_XHR;

    // These should be handled by the FrameType checks at the top of the
    // function.
    case WebURLRequest::RequestContextForm:
    case WebURLRequest::RequestContextHyperlink:
    case WebURLRequest::RequestContextLocation:
    case WebURLRequest::RequestContextFrame:
    case WebURLRequest::RequestContextIframe:
      NOTREACHED();
      return RESOURCE_TYPE_SUB_RESOURCE;

    default:
      NOTREACHED();
      return RESOURCE_TYPE_SUB_RESOURCE;
  }
}

std::string GetWebURLRequestHeaders(const blink::WebURLRequest& request) {
  HeaderFlattener flattener;
  request.visitHTTPHeaderFields(&flattener);
  return flattener.GetBuffer();
}

int GetLoadFlagsForWebURLRequest(const blink::WebURLRequest& request) {
  int load_flags = net::LOAD_NORMAL;
  GURL url = request.url();
  switch (request.cachePolicy()) {
    case WebURLRequest::ReloadIgnoringCacheData:
      // Required by LayoutTests/http/tests/misc/refresh-headers.php
      load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case WebURLRequest::ReloadBypassingCache:
      load_flags |= net::LOAD_BYPASS_CACHE;
      break;
    case WebURLRequest::ReturnCacheDataElseLoad:
      load_flags |= net::LOAD_PREFERRING_CACHE;
      break;
    case WebURLRequest::ReturnCacheDataDontLoad:
      load_flags |= net::LOAD_ONLY_FROM_CACHE;
      break;
    case WebURLRequest::UseProtocolCachePolicy:
      break;
    default:
      NOTREACHED();
  }

  if (request.reportRawHeaders())
    load_flags |= net::LOAD_REPORT_RAW_HEADERS;

  if (!request.allowStoredCredentials()) {
    load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
    load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
  }

  if (!request.allowStoredCredentials())
    load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;

  return load_flags;
}

scoped_refptr<ResourceRequestBody> GetRequestBodyForWebURLRequest(
    const blink::WebURLRequest& request) {
  scoped_refptr<ResourceRequestBody> request_body;

  if (request.httpBody().isNull()) {
    return request_body;
  }

  const std::string& method = request.httpMethod().latin1();
  // GET and HEAD requests shouldn't have http bodies.
  DCHECK(method != "GET" && method != "HEAD");

  const WebHTTPBody& httpBody = request.httpBody();
  request_body = new ResourceRequestBody();
  size_t i = 0;
  WebHTTPBody::Element element;
  while (httpBody.elementAt(i++, element)) {
    switch (element.type) {
      case WebHTTPBody::Element::TypeData:
        if (!element.data.isEmpty()) {
          // Blink sometimes gives empty data to append. These aren't
          // necessary so they are just optimized out here.
          request_body->AppendBytes(
              element.data.data(), static_cast<int>(element.data.size()));
        }
        break;
      case WebHTTPBody::Element::TypeFile:
        if (element.fileLength == -1) {
          request_body->AppendFileRange(
              base::FilePath::FromUTF16Unsafe(element.filePath),
              0, kuint64max, base::Time());
        } else {
          request_body->AppendFileRange(
              base::FilePath::FromUTF16Unsafe(element.filePath),
              static_cast<uint64>(element.fileStart),
              static_cast<uint64>(element.fileLength),
              base::Time::FromDoubleT(element.modificationTime));
        }
        break;
      case WebHTTPBody::Element::TypeFileSystemURL: {
        GURL file_system_url = element.fileSystemURL;
        DCHECK(file_system_url.SchemeIsFileSystem());
        request_body->AppendFileSystemFileRange(
            file_system_url,
            static_cast<uint64>(element.fileStart),
            static_cast<uint64>(element.fileLength),
            base::Time::FromDoubleT(element.modificationTime));
        break;
      }
      case WebHTTPBody::Element::TypeBlob:
        request_body->AppendBlob(element.blobUUID.utf8());
        break;
      default:
        NOTREACHED();
    }
  }
  request_body->set_identifier(request.httpBody().identifier());
  return request_body;
}

blink::WebURLError CreateWebURLError(const blink::WebURL& unreachable_url,
                                     bool stale_copy_in_cache,
                                     int reason) {
  blink::WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = reason;
  error.unreachableURL = unreachable_url;
  error.staleCopyInCache = stale_copy_in_cache;
  if (reason == net::ERR_ABORTED) {
    error.isCancellation = true;
  } else if (reason == net::ERR_TEMPORARILY_THROTTLED) {
    error.localizedDescription =
        WebString::fromUTF8(kThrottledErrorDescription);
  } else {
    error.localizedDescription =
        WebString::fromUTF8(net::ErrorToString(reason));
  }
  return error;
}

blink::WebURLError CreateWebURLError(const blink::WebURL& unreachable_url,
                                     bool stale_copy_in_cache,
                                     int reason,
                                     bool was_ignored_by_handler) {
  blink::WebURLError error =
      CreateWebURLError(unreachable_url, stale_copy_in_cache, reason);
  error.wasIgnoredByHandler = was_ignored_by_handler;
  return error;
}

}  // namespace content
