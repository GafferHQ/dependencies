// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/resource_dispatcher_host_delegate.h"

#include "content/public/browser/stream_info.h"

namespace content {

bool ResourceDispatcherHostDelegate::ShouldBeginRequest(
    const std::string& method,
    const GURL& url,
    ResourceType resource_type,
    ResourceContext* resource_context) {
  return true;
}

void ResourceDispatcherHostDelegate::RequestBeginning(
    net::URLRequest* request,
    ResourceContext* resource_context,
    AppCacheService* appcache_service,
    ResourceType resource_type,
    ScopedVector<ResourceThrottle>* throttles) {
}

void ResourceDispatcherHostDelegate::DownloadStarting(
    net::URLRequest* request,
    ResourceContext* resource_context,
    int child_id,
    int route_id,
    int request_id,
    bool is_content_initiated,
    bool must_download,
    ScopedVector<ResourceThrottle>* throttles) {
}

ResourceDispatcherHostLoginDelegate*
    ResourceDispatcherHostDelegate::CreateLoginDelegate(
        net::AuthChallengeInfo* auth_info,
        net::URLRequest* request) {
  return nullptr;
}

bool ResourceDispatcherHostDelegate::HandleExternalProtocol(
    const GURL& url,
    int child_id,
    int route_id,
    bool is_main_frame,
    ui::PageTransition page_transition,
    bool has_user_gesture) {
  return true;
}

bool ResourceDispatcherHostDelegate::ShouldForceDownloadResource(
    const GURL& url,
    const std::string& mime_type) {
  return false;
}

bool ResourceDispatcherHostDelegate::ShouldInterceptResourceAsStream(
    net::URLRequest* request,
    const std::string& mime_type,
    GURL* origin,
    std::string* payload) {
  return false;
}

void ResourceDispatcherHostDelegate::OnStreamCreated(
    net::URLRequest* request,
    scoped_ptr<content::StreamInfo> stream) {
}

void ResourceDispatcherHostDelegate::OnResponseStarted(
    net::URLRequest* request,
    ResourceContext* resource_context,
    ResourceResponse* response,
    IPC::Sender* sender) {
}

void ResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    ResourceContext* resource_context,
    ResourceResponse* response) {
}

void ResourceDispatcherHostDelegate::RequestComplete(
    net::URLRequest* url_request) {
}

ResourceDispatcherHostDelegate::ResourceDispatcherHostDelegate() {
}

ResourceDispatcherHostDelegate::~ResourceDispatcherHostDelegate() {
}

}  // namespace content
