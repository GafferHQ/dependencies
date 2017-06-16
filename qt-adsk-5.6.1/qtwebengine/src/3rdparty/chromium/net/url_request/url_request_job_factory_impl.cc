// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_factory_impl.h"

#include "base/stl_util.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_manager.h"
#include "url/gurl.h"

namespace net {

namespace {

URLRequestInterceptor* g_interceptor_for_testing = NULL;

}  // namespace

URLRequestJobFactoryImpl::URLRequestJobFactoryImpl() {}

URLRequestJobFactoryImpl::~URLRequestJobFactoryImpl() {
  STLDeleteValues(&protocol_handler_map_);
}

bool URLRequestJobFactoryImpl::SetProtocolHandler(
    const std::string& scheme,
    ProtocolHandler* protocol_handler) {
  DCHECK(CalledOnValidThread());

  if (!protocol_handler) {
    ProtocolHandlerMap::iterator it = protocol_handler_map_.find(scheme);
    if (it == protocol_handler_map_.end())
      return false;

    delete it->second;
    protocol_handler_map_.erase(it);
    return true;
  }

  if (ContainsKey(protocol_handler_map_, scheme))
    return false;
  protocol_handler_map_[scheme] = protocol_handler;
  return true;
}

URLRequestJob* URLRequestJobFactoryImpl::MaybeCreateJobWithProtocolHandler(
    const std::string& scheme,
    URLRequest* request,
    NetworkDelegate* network_delegate) const {
  DCHECK(CalledOnValidThread());
  if (g_interceptor_for_testing) {
    URLRequestJob* job = g_interceptor_for_testing->MaybeInterceptRequest(
        request, network_delegate);
    if (job)
      return job;
  }

  ProtocolHandlerMap::const_iterator it = protocol_handler_map_.find(scheme);
  if (it == protocol_handler_map_.end())
    return NULL;
  return it->second->MaybeCreateJob(request, network_delegate);
}

URLRequestJob* URLRequestJobFactoryImpl::MaybeInterceptRedirect(
    URLRequest* request,
    NetworkDelegate* network_delegate,
    const GURL& location) const {
  return nullptr;
}

URLRequestJob* URLRequestJobFactoryImpl::MaybeInterceptResponse(
    URLRequest* request,
    NetworkDelegate* network_delegate) const {
  return nullptr;
}

bool URLRequestJobFactoryImpl::IsHandledProtocol(
    const std::string& scheme) const {
  DCHECK(CalledOnValidThread());
  return ContainsKey(protocol_handler_map_, scheme) ||
      URLRequestJobManager::GetInstance()->SupportsScheme(scheme);
}

bool URLRequestJobFactoryImpl::IsHandledURL(const GURL& url) const {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }
  return IsHandledProtocol(url.scheme());
}

bool URLRequestJobFactoryImpl::IsSafeRedirectTarget(
    const GURL& location) const {
  DCHECK(CalledOnValidThread());
  if (!location.is_valid()) {
    // Error cases are safely handled.
    return true;
  }
  ProtocolHandlerMap::const_iterator it = protocol_handler_map_.find(
      location.scheme());
  if (it == protocol_handler_map_.end()) {
    // Unhandled cases are safely handled.
    return true;
  }
  return it->second->IsSafeRedirectTarget(location);
}

// static
void URLRequestJobFactoryImpl::SetInterceptorForTesting(
    URLRequestInterceptor* interceptor) {
  DCHECK(!interceptor || !g_interceptor_for_testing);

  g_interceptor_for_testing = interceptor;
}

}  // namespace net
