// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/dhcp_proxy_script_fetcher_factory.h"

#include "net/base/net_errors.h"
#include "net/proxy/dhcp_proxy_script_fetcher.h"

#if defined(OS_WIN)
#include "net/proxy/dhcp_proxy_script_fetcher_win.h"
#endif

namespace net {

DhcpProxyScriptFetcherFactory::DhcpProxyScriptFetcherFactory()
    : feature_enabled_(false) {
  set_enabled(true);
}

DhcpProxyScriptFetcher* DhcpProxyScriptFetcherFactory::Create(
    URLRequestContext* context) {
  if (!feature_enabled_) {
    return new DoNothingDhcpProxyScriptFetcher();
  } else {
    DCHECK(IsSupported());
    DhcpProxyScriptFetcher* ret = NULL;
#if defined(OS_WIN)
    ret = new DhcpProxyScriptFetcherWin(context);
#endif
    DCHECK(ret);
    return ret;
  }
}

void DhcpProxyScriptFetcherFactory::set_enabled(bool enabled) {
  if (IsSupported()) {
    feature_enabled_ = enabled;
  }
}

bool DhcpProxyScriptFetcherFactory::enabled() const {
  return feature_enabled_;
}

// static
bool DhcpProxyScriptFetcherFactory::IsSupported() {
#if defined(OS_WIN)
  return true;
#else
  return false;
#endif
}

}  // namespace net
