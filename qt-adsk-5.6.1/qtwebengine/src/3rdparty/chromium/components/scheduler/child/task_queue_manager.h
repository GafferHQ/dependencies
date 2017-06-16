// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_TASK_QUEUE_MANAGER_H_
#define CONTENT_RENDERER_SCHEDULER_TASK_QUEUE_MANAGER_H_

#include "base/atomic_sequence_num.h"
#include "base/debug/task_annotator.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/pending_task.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "components/scheduler/child/task_queue_selector.h"
#include "components/scheduler/scheduler_export.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
class TracedValue;
}
class TickClock;
}

namespace scheduler {
class TaskQueue;
namespace internal {
class LazyNow;
class TaskQueueImpl;
}
class NestableSingleThreadTaskRunner;
class TaskQueueSelector;

// The task queue manager provides N task queues and a selector interface for
// choosing which task queue to service next. Each task queue consists of two
// sub queues:
//
// 1. Incoming task queue. Tasks that are posted get immediately appended here.
//    When a task is appended into an empty incoming queue, the task manager
//    work function (DoWork) is scheduled to run on the main task runner.
//
// 2. Work queue. If a work queue is empty when DoWork() is entered, tasks from
//    the incoming task queue (if any) are moved here. The work queues are
//    registered with the selector as input to the scheduling decision.
//
class SCHEDULER_EXPORT TaskQueueManager : public TaskQueueSelector::Observer {
 public:
  // Keep TaskQueue::PumpPolicyToString in sync with this enum.
  enum class PumpPolicy {
    // Tasks posted to an incoming queue with an AUTO pump policy will be
    // automatically scheduled for execution or transferred to the work queue
    // automatically.
    AUTO,
    // Tasks posted to an incoming queue with an AFTER_WAKEUP pump policy
    // will be scheduled for execution or transferred to the work queue
    // automatically but only after another queue has executed a task.
    AFTER_WAKEUP,
    // Tasks posted to an incoming queue with a MANUAL will not be
    // automatically scheduled for execution or transferred to the work queue.
    // Instead, the selector should call PumpQueue() when necessary to bring
    // in new tasks for execution.
    MANUAL,
    // Must be last entry.
    PUMP_POLICY_COUNT,
    FIRST_PUMP_POLICY = AUTO,
  };

  // Keep TaskQueue::WakeupPolicyToString in sync with this enum.
  enum class WakeupPolicy {
    // Tasks run on a queue with CAN_WAKE_OTHER_QUEUES wakeup policy can
    // cause queues with the AFTER_WAKEUP PumpPolicy to be woken up.
    CAN_WAKE_OTHER_QUEUES,
    // Tasks run on a queue with DONT_WAKE_OTHER_QUEUES won't cause queues
    // with the AFTER_WAKEUP PumpPolicy to be woken up.
    DONT_WAKE_OTHER_QUEUES,
    // Must be last entry.
    WAKEUP_POLICY_COUNT,
    FIRST_WAKEUP_POLICY = CAN_WAKE_OTHER_QUEUES,
  };

  enum class QueueState {
    // A queue in the EMPTY state is empty and has no tasks in either the
    // work or incoming task queue.
    EMPTY,
    // A queue in the NEEDS_PUMPING state has no tasks in the work task queue,
    // but has tasks in the incoming task queue which can be pumped to make them
    // runnable.
    NEEDS_PUMPING,
    // A queue in the HAS_WORK state has tasks in the work task queue which
    // are runnable.
    HAS_WORK,
  };

  // Create a task queue manager with |task_queue_count| task queues.
  // |main_task_runner| identifies the thread on which where the tasks are
  // eventually run. |selector| is used to choose which task queue to service.
  // It should outlive this class.  Category strings must have application
  // lifetime (statics or literals). They may not include " chars.
  TaskQueueManager(
      size_t task_queue_count,
      scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner,
      TaskQueueSelector* selector,
      const char* disabled_by_default_tracing_category,
      const char* disabled_by_default_verbose_tracing_category);
  ~TaskQueueManager() override;

  // Returns the task runner which targets the queue selected by |queue_index|.
  scoped_refptr<TaskQueue> TaskRunnerForQueue(size_t queue_index) const;

  // Sets the pump policy for the |queue_index| to |pump_policy|. By
  // default queues are created with AUTO_PUMP_POLICY.
  void SetPumpPolicy(size_t queue_index, PumpPolicy pump_policy);

  // Sets the wakeup policy for the |queue_index| to |wakeup_policy|. By
  // default queues are created with CAN_WAKE_OTHER_QUEUES.
  void SetWakeupPolicy(size_t queue_index, WakeupPolicy wakeup_policy);

  // Reloads new tasks from the incoming queue for |queue_index| into the work
  // queue, regardless of whether the work queue is empty or not. After this,
  // this function ensures that the tasks in the work queue, if any, are
  // scheduled for execution.
  //
  // This function only needs to be called if automatic pumping is disabled
  // for |queue_index|. See |SetQueueAutoPumpPolicy|. By default automatic
  // pumping is enabled for all queues.
  void PumpQueue(size_t queue_index);

  // Returns true if there no tasks in either the work or incoming task queue
  // identified by |queue_index|. Note that this function involves taking a
  // lock, so calling it has some overhead.
  bool IsQueueEmpty(size_t queue_index) const;

  // Returns the QueueState of the queue identified by |queue_index|. Note that
  // this function involves taking a lock, so calling it has some overhead.
  QueueState GetQueueState(size_t queue_index) const;

  // Returns the time of the next pending delayed task in any queue.  Ignores
  // any delayed tasks whose delay has expired. Returns a null TimeTicks object
  // if no tasks are pending.  NOTE this is somewhat expensive since every queue
  // will get locked.
  base::TimeTicks NextPendingDelayedTaskRunTime();

  // Set the name |queue_index| for tracing purposes. |name| must be a pointer
  // to a static string.
  void SetQueueName(size_t queue_index, const char* name);

  // Set the number of tasks executed in a single invocation of the task queue
  // manager. Increasing the batch size can reduce the overhead of yielding
  // back to the main message loop -- at the cost of potentially delaying other
  // tasks posted to the main loop. The batch size is 1 by default.
  void SetWorkBatchSize(int work_batch_size);

  // These functions can only be called on the same thread that the task queue
  // manager executes its tasks on.
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer);
  void RemoveTaskObserver(base::MessageLoop::TaskObserver* task_observer);

  void SetTimeSourceForTesting(scoped_ptr<base::TickClock> time_source);

  // Returns a bitmap where a bit is set iff a task on the corresponding queue
  // was run since the last call to GetAndClearTaskWasRunOnQueueBitmap.
  uint64 GetAndClearTaskWasRunOnQueueBitmap();

 private:
  // TaskQueueSelector::Observer implementation:
  void OnTaskQueueEnabled() override;

  friend class internal::LazyNow;
  friend class internal::TaskQueueImpl;
  friend class TaskQueueManagerTest;

  class DeletionSentinel : public base::RefCounted<DeletionSentinel> {
   private:
    friend class base::RefCounted<DeletionSentinel>;
    ~DeletionSentinel() {}
  };

  // Called by the task queue to register a new pending task.
  void DidQueueTask(const base::PendingTask& pending_task);

  // Post a task to call DoWork() on the main task runner.  Only one pending
  // DoWork is allowed from the main thread, to prevent an explosion of pending
  // DoWorks.
  void MaybePostDoWorkOnMainRunner();

  // Use the selector to choose a pending task and run it.
  void DoWork(bool posted_from_main_thread);

  // Delayed Tasks with run_times <= Now() are enqueued onto the work queue.
  // Reloads any empty work queues which have automatic pumping enabled and
  // which are eligible to be auto pumped based on the |previous_task| which was
  // run and |should_trigger_wakeup| . Call with an empty |previous_task| if no
  // task was just run. Returns true if any work queue has tasks after doing
  // this.
  bool UpdateWorkQueues(bool should_trigger_wakeup,
                        const base::PendingTask* previous_task);

  // Chooses the next work queue to service. Returns true if |out_queue_index|
  // indicates the queue from which the next task should be run, false to
  // avoid running any tasks.
  bool SelectWorkQueueToService(size_t* out_queue_index);

  // Runs a single nestable task from the work queue designated by
  // |queue_index|. If |has_previous_task| is true, |previous_task| should
  // contain the previous task in this work batch. Non-nestable task are
  // reposted on the run loop. The queue must not be empty.
  // Returns true if the TaskQueueManager got deleted, and false otherwise.
  bool ProcessTaskFromWorkQueue(size_t queue_index,
                                bool has_previous_task,
                                base::PendingTask* previous_task);

  bool RunsTasksOnCurrentThread() const;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay);
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay);
  internal::TaskQueueImpl* Queue(size_t queue_index) const;

  base::TimeTicks Now() const;

  int GetNextSequenceNumber();

  scoped_refptr<base::trace_event::ConvertableToTraceFormat>
  AsValueWithSelectorResult(bool should_run, size_t selected_queue) const;
  static const char* PumpPolicyToString(PumpPolicy pump_policy);
  static const char* WakeupPolicyToString(WakeupPolicy wakeup_policy);

  std::vector<scoped_refptr<internal::TaskQueueImpl>> queues_;
  base::AtomicSequenceNumber task_sequence_num_;
  base::debug::TaskAnnotator task_annotator_;

  base::ThreadChecker main_thread_checker_;
  scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner_;
  TaskQueueSelector* selector_;

  base::Closure do_work_from_main_thread_closure_;
  base::Closure do_work_from_other_thread_closure_;

  uint64 task_was_run_bitmap_;

  // The pending_dowork_count_ is only tracked on the main thread since that's
  // where re-entrant problems happen.
  int pending_dowork_count_;

  int work_batch_size_;

  scoped_ptr<base::TickClock> time_source_;

  base::ObserverList<base::MessageLoop::TaskObserver> task_observers_;

  const char* disabled_by_default_tracing_category_;

  scoped_refptr<DeletionSentinel> deletion_sentinel_;
  base::WeakPtrFactory<TaskQueueManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueManager);
};

}  // namespace scheduler

#endif  // CONTENT_RENDERER_SCHEDULER_TASK_QUEUE_MANAGER_H_
