// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_MOCK_ALLOW_URL_SECURITY_MANAGER_H_
#define NET_HTTP_MOCK_ALLOW_URL_SECURITY_MANAGER_H_

#include "net/http/url_security_manager.h"

namespace net {

// An URLSecurityManager which is very permissive and which should only be used
// in unit testing.
class MockAllowURLSecurityManager : public URLSecurityManager {
 public:
  MockAllowURLSecurityManager();
  ~MockAllowURLSecurityManager() override;

  bool CanUseDefaultCredentials(const GURL& auth_origin) const override;
  bool CanDelegate(const GURL& auth_origin) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAllowURLSecurityManager);
};

}  // namespace net

#endif  // NET_HTTP_MOCK_ALLOW_URL_SECURITY_MANAGER_H_
