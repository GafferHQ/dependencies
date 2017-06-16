// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_WEBTHREAD_BASE_H_
#define COMPONENTS_SCHEDULER_CHILD_WEBTHREAD_BASE_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "components/scheduler/scheduler_export.h"
#include "third_party/WebKit/public/platform/WebThread.h"

namespace blink {
class WebTraceLocation;
}

namespace scheduler {
class SingleThreadIdleTaskRunner;

class SCHEDULER_EXPORT WebThreadBase : public blink::WebThread {
 public:
  virtual ~WebThreadBase();

  // blink::WebThread implementation.
  virtual bool isCurrentThread() const;
  virtual blink::PlatformThreadId threadId() const = 0;

  virtual void postTask(const blink::WebTraceLocation& location, Task* task);
  virtual void postDelayedTask(const blink::WebTraceLocation& location,
                               Task* task,
                               long long delay_ms);
  virtual void postIdleTask(const blink::WebTraceLocation& location,
                            IdleTask* idle_task);
  virtual void postIdleTaskAfterWakeup(const blink::WebTraceLocation& location,
                                       IdleTask* idle_task);

  virtual void addTaskObserver(TaskObserver* observer);
  virtual void removeTaskObserver(TaskObserver* observer);

  // Returns the base::Bind-compatible task runner for posting tasks to this
  // thread. Can be called from any thread.
  virtual base::SingleThreadTaskRunner* TaskRunner() const = 0;

  // Returns the base::Bind-compatible task runner for posting idle tasks to
  // this thread. Can be called from any thread.
  virtual scheduler::SingleThreadIdleTaskRunner* IdleTaskRunner() const = 0;

 protected:
  class TaskObserverAdapter;

  WebThreadBase();

  virtual void AddTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer);
  virtual void RemoveTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer);

  static void RunWebThreadTask(scoped_ptr<blink::WebThread::Task> task);
  static void RunWebThreadIdleTask(
      scoped_ptr<blink::WebThread::IdleTask> idle_task,
      base::TimeTicks deadline);

 private:
  typedef std::map<TaskObserver*, TaskObserverAdapter*> TaskObserverMap;
  TaskObserverMap task_observer_map_;
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_WEBTHREAD_BASE_H_
