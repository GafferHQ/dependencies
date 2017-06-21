// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/url_security_manager.h"

#include "net/http/http_auth_filter.h"

namespace net {

URLSecurityManagerWhitelist::URLSecurityManagerWhitelist(
    const HttpAuthFilter* whitelist_default,
    const HttpAuthFilter* whitelist_delegate)
    : whitelist_default_(whitelist_default),
      whitelist_delegate_(whitelist_delegate) {
}

URLSecurityManagerWhitelist::~URLSecurityManagerWhitelist() {}

bool URLSecurityManagerWhitelist::CanUseDefaultCredentials(
    const GURL& auth_origin) const  {
  if (whitelist_default_.get())
    return whitelist_default_->IsValid(auth_origin, HttpAuth::AUTH_SERVER);
  return false;
}

bool URLSecurityManagerWhitelist::CanDelegate(const GURL& auth_origin) const {
  if (whitelist_delegate_.get())
    return whitelist_delegate_->IsValid(auth_origin, HttpAuth::AUTH_SERVER);
  return false;
}

}  //  namespace net
