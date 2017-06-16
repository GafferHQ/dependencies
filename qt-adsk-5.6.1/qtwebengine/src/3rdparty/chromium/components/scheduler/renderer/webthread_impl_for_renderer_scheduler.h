// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_WEBTHREAD_IMPL_FOR_RENDERER_SCHEDULER_H_
#define COMPONENTS_SCHEDULER_RENDERER_WEBTHREAD_IMPL_FOR_RENDERER_SCHEDULER_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "components/scheduler/child/webthread_base.h"

namespace blink {
class WebScheduler;
};

namespace scheduler {
class RendererScheduler;
class WebSchedulerImpl;

class SCHEDULER_EXPORT WebThreadImplForRendererScheduler
    : public WebThreadBase {
 public:
  explicit WebThreadImplForRendererScheduler(RendererScheduler* scheduler);
  virtual ~WebThreadImplForRendererScheduler();

  // blink::WebThread implementation.
  blink::WebScheduler* scheduler() const;
  blink::PlatformThreadId threadId() const override;

  // WebThreadBase implementation.
  base::SingleThreadTaskRunner* TaskRunner() const override;
  SingleThreadIdleTaskRunner* IdleTaskRunner() const override;

 private:
  void AddTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;
  void RemoveTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;

  scoped_ptr<WebSchedulerImpl> web_scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  RendererScheduler* scheduler_;  // Not owned.
  blink::PlatformThreadId thread_id_;
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_WEBTHREAD_IMPL_FOR_RENDERER_SCHEDULER_H_
