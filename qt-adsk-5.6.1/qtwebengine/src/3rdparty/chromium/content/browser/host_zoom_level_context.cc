// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/host_zoom_level_context.h"

#include "base/files/file_path.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

HostZoomLevelContext::HostZoomLevelContext(
    scoped_ptr<ZoomLevelDelegate> zoom_level_delegate)
    : host_zoom_map_impl_(new HostZoomMapImpl()),
      zoom_level_delegate_(zoom_level_delegate.Pass()) {
  if (zoom_level_delegate_)
    zoom_level_delegate_->InitHostZoomMap(host_zoom_map_impl_.get());
}

HostZoomLevelContext::~HostZoomLevelContext() {}

void HostZoomLevelContext::DeleteOnCorrectThread() const {
  if (BrowserThread::IsMessageLoopValid(BrowserThread::UI) &&
      !BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
    return;
  }
  delete this;
}

}  // namespace content
