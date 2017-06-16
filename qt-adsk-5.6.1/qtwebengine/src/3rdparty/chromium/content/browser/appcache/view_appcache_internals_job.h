// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_
#define CONTENT_BROWSER_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_

#include "base/basictypes.h"

namespace net {
class NetworkDelegate;
class URLRequest;
class URLRequestJob;
}

namespace content {
class AppCacheServiceImpl;

class ViewAppCacheInternalsJobFactory {
 public:
  static net::URLRequestJob* CreateJobForRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      AppCacheServiceImpl* service);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ViewAppCacheInternalsJobFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_
