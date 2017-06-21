// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/network_handler.h"

#include "base/containers/hash_tables.h"
#include "base/strings/stringprintf.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {
namespace devtools {
namespace network {

using CookieListCallback = net::CookieStore::GetCookieListCallback;

namespace {

net::URLRequestContext* GetRequestContextOnIO(
    ResourceContext* resource_context,
    net::URLRequestContextGetter* context_getter,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestContext* context =
      GetContentClient()->browser()->OverrideRequestContextForURL(
          url, resource_context);
  if (!context)
    context = context_getter->GetURLRequestContext();
  return context;
}

void GotCookiesForURLOnIO(
    const CookieListCallback& callback,
    const net::CookieList& cookie_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, cookie_list));
}

void GetCookiesForURLOnIO(
    ResourceContext* resource_context,
    net::URLRequestContextGetter* context_getter,
    const GURL& url,
    const CookieListCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestContext* request_context =
      GetRequestContextOnIO(resource_context, context_getter, url);
  request_context->cookie_store()->GetAllCookiesForURLAsync(
      url, base::Bind(&GotCookiesForURLOnIO, callback));
}

void GetCookiesForURLOnUI(
    ResourceContext* resource_context,
    net::URLRequestContextGetter* context_getter,
    const GURL& url,
    const CookieListCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GetCookiesForURLOnIO,
                 base::Unretained(resource_context),
                 base::Unretained(context_getter),
                 url,
                 callback));
}

void DeletedCookieOnIO(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      callback);
}

void DeleteCookieOnIO(
    ResourceContext* resource_context,
    net::URLRequestContextGetter* context_getter,
    const GURL& url,
    const std::string& cookie_name,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestContext* request_context =
      GetRequestContextOnIO(resource_context, context_getter, url);
  request_context->cookie_store()->DeleteCookieAsync(
      url, cookie_name, base::Bind(&DeletedCookieOnIO, callback));
}

void DeleteCookieOnUI(
    ResourceContext* resource_context,
    net::URLRequestContextGetter* context_getter,
    const GURL& url,
    const std::string& cookie_name,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&DeleteCookieOnIO,
                 base::Unretained(resource_context),
                 base::Unretained(context_getter),
                 url,
                 cookie_name,
                 callback));
}

class GetCookiesCommand {
 public:
  explicit GetCookiesCommand(
      RenderFrameHostImpl* frame_host,
      const CookieListCallback& callback)
      : callback_(callback),
        request_count_(0) {
    CookieListCallback got_cookies_callback = base::Bind(
        &GetCookiesCommand::GotCookiesForURL, base::Unretained(this));
    BrowserContext* browser_context =
        frame_host->GetSiteInstance()->GetBrowserContext();

    std::queue<FrameTreeNode*> queue;
    queue.push(frame_host->frame_tree_node());
    while (!queue.empty()) {
      FrameTreeNode* node = queue.front();
      queue.pop();

      // Only traverse nodes with the same local root.
      if (node->current_frame_host()->IsCrossProcessSubframe())
        continue;
      int process_id = node->current_frame_host()->GetProcess()->GetID();
      ++request_count_;
      GetCookiesForURLOnUI(
          browser_context->GetResourceContext(),
          browser_context->GetRequestContextForRenderProcess(process_id),
          node->current_url(),
          got_cookies_callback);

      for (size_t i = 0; i < node->child_count(); ++i)
        queue.push(node->child_at(i));
    }
  }

 private:
  void GotCookiesForURL(const net::CookieList& cookie_list) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    for (const net::CanonicalCookie& cookie : cookie_list) {
      std::string key = base::StringPrintf(
          "%s::%s::%s::%d",
          cookie.Name().c_str(),
          cookie.Domain().c_str(),
          cookie.Path().c_str(),
          cookie.IsSecure());
      cookies_[key] = cookie;
    }
    --request_count_;
    if (!request_count_) {
      net::CookieList list;
      list.reserve(cookies_.size());
      for (const auto& pair : cookies_)
        list.push_back(pair.second);
      callback_.Run(list);
      delete this;
    }
  }

  CookieListCallback callback_;
  int request_count_;
  base::hash_map<std::string, net::CanonicalCookie> cookies_;
};

}  // namespace

typedef DevToolsProtocolClient::Response Response;

NetworkHandler::NetworkHandler() : host_(nullptr), weak_factory_(this) {
}

NetworkHandler::~NetworkHandler() {
}

void NetworkHandler::SetRenderFrameHost(RenderFrameHostImpl* host) {
  host_ = host;
}

void NetworkHandler::SetClient(scoped_ptr<Client> client) {
  client_.swap(client);
}

Response NetworkHandler::ClearBrowserCache() {
  if (host_)
    GetContentClient()->browser()->ClearCache(host_);
  return Response::OK();
}

Response NetworkHandler::ClearBrowserCookies() {
  if (host_)
    GetContentClient()->browser()->ClearCookies(host_);
  return Response::OK();
}

Response NetworkHandler::GetCookies(DevToolsCommandId command_id) {
  if (!host_)
    return Response::InternalError("Could not connect to view");
  new GetCookiesCommand(
      host_,
      base::Bind(&NetworkHandler::SendGetCookiesResponse,
                 weak_factory_.GetWeakPtr(),
                 command_id));
  return Response::OK();
}

void NetworkHandler::SendGetCookiesResponse(
    DevToolsCommandId command_id,
    const net::CookieList& cookie_list) {
  std::vector<scoped_refptr<Cookie>> cookies;
  for (size_t i = 0; i < cookie_list.size(); ++i) {
    const net::CanonicalCookie& cookie = cookie_list[i];
    cookies.push_back(Cookie::Create()
         ->set_name(cookie.Name())
         ->set_value(cookie.Value())
         ->set_domain(cookie.Domain())
         ->set_path(cookie.Path())
         ->set_expires(cookie.ExpiryDate().ToDoubleT() * 1000)
         ->set_size(cookie.Name().length() + cookie.Value().length())
         ->set_http_only(cookie.IsHttpOnly())
         ->set_secure(cookie.IsSecure())
         ->set_session(!cookie.IsPersistent()));
  }
  client_->SendGetCookiesResponse(command_id,
      GetCookiesResponse::Create()->set_cookies(cookies));
}

Response NetworkHandler::DeleteCookie(
    DevToolsCommandId command_id,
    const std::string& cookie_name,
    const std::string& url) {
  if (!host_)
    return Response::InternalError("Could not connect to view");
  BrowserContext* browser_context =
      host_->GetSiteInstance()->GetBrowserContext();
  int process_id = host_->GetProcess()->GetID();
  DeleteCookieOnUI(
      browser_context->GetResourceContext(),
      browser_context->GetRequestContextForRenderProcess(process_id),
      GURL(url),
      cookie_name,
      base::Bind(&NetworkHandler::SendDeleteCookieResponse,
                 weak_factory_.GetWeakPtr(),
                 command_id));
  return Response::OK();
}

void NetworkHandler::SendDeleteCookieResponse(DevToolsCommandId command_id) {
  client_->SendDeleteCookieResponse(command_id,
      DeleteCookieResponse::Create());
}


Response NetworkHandler::CanEmulateNetworkConditions(bool* result) {
  *result = false;
  return Response::OK();
}

Response NetworkHandler::EmulateNetworkConditions(bool offline,
                                                  double latency,
                                                  double download_throughput,
                                                  double upload_throughput) {
  return Response::FallThrough();
}

}  // namespace dom
}  // namespace devtools
}  // namespace content
