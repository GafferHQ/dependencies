// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_render_process_observer.h"

#include "chromecast/renderer/media/capabilities_message_filter.h"
#include "chromecast/renderer/media/cma_message_filter_proxy.h"
#include "content/public/renderer/render_thread.h"

namespace chromecast {
namespace shell {

CastRenderProcessObserver::CastRenderProcessObserver(
    const std::vector<scoped_refptr<IPC::MessageFilter>>&
        platform_message_filters)
    : platform_message_filters_(platform_message_filters) {
  content::RenderThread* thread = content::RenderThread::Get();
  thread->AddObserver(this);
  CreateCustomFilters();
}

CastRenderProcessObserver::~CastRenderProcessObserver() {
  // CastRenderProcessObserver outlives content::RenderThread.
  // No need to explicitly call RemoveObserver in teardown.
}

void CastRenderProcessObserver::CreateCustomFilters() {
  content::RenderThread* thread = content::RenderThread::Get();
#if !defined(OS_ANDROID)
  cma_message_filter_proxy_ =
      new media::CmaMessageFilterProxy(thread->GetIOMessageLoopProxy());
  thread->AddFilter(cma_message_filter_proxy_.get());
#endif  // !defined(OS_ANDROID)
  capabilities_message_filter_ = new CapabilitiesMessageFilter;
  thread->AddFilter(capabilities_message_filter_.get());
  for (const auto& filter : platform_message_filters_) {
    thread->AddFilter(filter.get());
  }
}

void CastRenderProcessObserver::OnRenderProcessShutdown() {
  content::RenderThread* thread = content::RenderThread::Get();
#if !defined(OS_ANDROID)
  if (cma_message_filter_proxy_.get()) {
    thread->RemoveFilter(cma_message_filter_proxy_.get());
    cma_message_filter_proxy_ = nullptr;
  }
#endif  // !defined(OS_ANDROID)
  if (capabilities_message_filter_.get()) {
    thread->RemoveFilter(capabilities_message_filter_.get());
    capabilities_message_filter_ = nullptr;
  }
  for (auto& filter : platform_message_filters_) {
    if (filter.get()) {
      thread->RemoveFilter(filter.get());
      filter = nullptr;
    }
  }
  platform_message_filters_.clear();
}

}  // namespace shell
}  // namespace chromecast
