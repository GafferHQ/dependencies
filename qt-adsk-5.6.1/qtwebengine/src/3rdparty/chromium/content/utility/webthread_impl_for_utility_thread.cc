// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/webthread_impl_for_utility_thread.h"

#include "base/thread_task_runner_handle.h"

namespace content {

WebThreadImplForUtilityThread::WebThreadImplForUtilityThread()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      thread_id_(base::PlatformThread::CurrentId()) {
}

WebThreadImplForUtilityThread::~WebThreadImplForUtilityThread() {
}

blink::WebScheduler* WebThreadImplForUtilityThread::scheduler() const {
  NOTIMPLEMENTED();
  return nullptr;
}

blink::PlatformThreadId WebThreadImplForUtilityThread::threadId() const {
  return thread_id_;
}

base::SingleThreadTaskRunner* WebThreadImplForUtilityThread::TaskRunner()
    const {
  return task_runner_.get();
}

scheduler::SingleThreadIdleTaskRunner*
WebThreadImplForUtilityThread::IdleTaskRunner() const {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace content
