// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_WEBTHREAD_IMPL_FOR_WORKER_SCHEDULER_H_
#define COMPONENTS_SCHEDULER_CHILD_WEBTHREAD_IMPL_FOR_WORKER_SCHEDULER_H_

#include "components/scheduler/child/task_queue_manager.h"
#include "components/scheduler/child/webthread_base.h"

namespace base {
class WaitableEvent;
};

namespace blink {
class WebScheduler;
};

namespace scheduler {
class SingleThreadIdleTaskRunner;
class WebSchedulerImpl;
class WorkerScheduler;

class SCHEDULER_EXPORT WebThreadImplForWorkerScheduler
    : public WebThreadBase,
      public base::MessageLoop::DestructionObserver {
 public:
  explicit WebThreadImplForWorkerScheduler(const char* name);
  virtual ~WebThreadImplForWorkerScheduler();

  // blink::WebThread implementation.
  virtual blink::WebScheduler* scheduler() const;
  blink::PlatformThreadId threadId() const override;

  // WebThreadBase implementation.
  base::SingleThreadTaskRunner* TaskRunner() const override;
  scheduler::SingleThreadIdleTaskRunner* IdleTaskRunner() const override;

  // base::MessageLoop::DestructionObserver implementation.
  void WillDestroyCurrentMessageLoop() override;

 private:
  void AddTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;
  void RemoveTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;

  void InitOnThread(base::WaitableEvent* completion);

  scoped_ptr<base::Thread> thread_;
  scoped_ptr<scheduler::WorkerScheduler> worker_scheduler_;
  scoped_ptr<scheduler::WebSchedulerImpl> web_scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<scheduler::SingleThreadIdleTaskRunner> idle_task_runner_;
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_WEBTHREAD_IMPL_FOR_WORKER_SCHEDULER_H_
