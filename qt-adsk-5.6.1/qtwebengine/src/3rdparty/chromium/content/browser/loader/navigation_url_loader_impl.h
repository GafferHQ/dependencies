// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/loader/navigation_url_loader.h"

namespace net {
struct RedirectInfo;
}

namespace content {

class NavigationURLLoaderImplCore;
class StreamHandle;
struct ResourceResponse;

class NavigationURLLoaderImpl : public NavigationURLLoader {
 public:
  // The caller is responsible for ensuring that |delegate| outlives the loader.
  NavigationURLLoaderImpl(BrowserContext* browser_context,
                          int frame_tree_node_id,
                          scoped_ptr<NavigationRequestInfo> request_info,
                          NavigationURLLoaderDelegate* delegate);
  ~NavigationURLLoaderImpl() override;

  // Called in response to OnRequestRedirected to continue processing the
  // request.
  void FollowRedirect() override;

 private:
  friend class NavigationURLLoaderImplCore;

  // Notifies the delegate of a redirect.
  void NotifyRequestRedirected(const net::RedirectInfo& redirect_info,
                               const scoped_refptr<ResourceResponse>& response);

  // Notifies the delegate that the response has started.
  void NotifyResponseStarted(const scoped_refptr<ResourceResponse>& response,
                             scoped_ptr<StreamHandle> body);

  // Notifies the delegate the request failed to return a response.
  void NotifyRequestFailed(bool in_cache, int net_error);

  // Notifies the delegate the begin navigation request was handled and a
  // potential first network request is about to be made.
  void NotifyRequestStarted(base::TimeTicks timestamp);

  NavigationURLLoaderDelegate* delegate_;

  // |core_| is deleted on the IO thread in a subsequent task when the
  // NavigationURLLoaderImpl goes out of scope.
  NavigationURLLoaderImplCore* core_;

  base::WeakPtrFactory<NavigationURLLoaderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_
