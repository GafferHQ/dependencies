// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/webthread_impl_for_renderer_scheduler.h"

#include "components/scheduler/child/task_queue.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "components/scheduler/renderer/renderer_web_scheduler_impl.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace scheduler {

WebThreadImplForRendererScheduler::WebThreadImplForRendererScheduler(
    RendererScheduler* scheduler)
    : web_scheduler_(new RendererWebSchedulerImpl(scheduler)),
      task_runner_(scheduler->DefaultTaskRunner()),
      idle_task_runner_(scheduler->IdleTaskRunner()),
      scheduler_(scheduler),
      thread_id_(base::PlatformThread::CurrentId()) {
}

WebThreadImplForRendererScheduler::~WebThreadImplForRendererScheduler() {
}

blink::PlatformThreadId WebThreadImplForRendererScheduler::threadId() const {
  return thread_id_;
}

blink::WebScheduler* WebThreadImplForRendererScheduler::scheduler() const {
  return web_scheduler_.get();
}

base::SingleThreadTaskRunner* WebThreadImplForRendererScheduler::TaskRunner()
    const {
  return task_runner_.get();
}

SingleThreadIdleTaskRunner* WebThreadImplForRendererScheduler::IdleTaskRunner()
    const {
  return idle_task_runner_.get();
}

void WebThreadImplForRendererScheduler::AddTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  scheduler_->AddTaskObserver(observer);
}

void WebThreadImplForRendererScheduler::RemoveTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  scheduler_->RemoveTaskObserver(observer);
}

}  // namespace scheduler
