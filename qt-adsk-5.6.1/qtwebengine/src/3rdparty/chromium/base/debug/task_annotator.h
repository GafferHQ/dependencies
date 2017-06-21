// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_TASK_ANNOTATOR_H_
#define BASE_DEBUG_TASK_ANNOTATOR_H_

#include "base/base_export.h"
#include "base/basictypes.h"

namespace base {
struct PendingTask;
namespace debug {

// Implements common debug annotations for posted tasks. This includes data
// such as task origins, queueing durations and memory usage.
class BASE_EXPORT TaskAnnotator {
 public:
  TaskAnnotator();
  ~TaskAnnotator();

  // Called to indicate that a task has been queued to run in the future.
  // |queue_function| is used as the trace flow event name.
  void DidQueueTask(const char* queue_function,
                    const PendingTask& pending_task);

  // Run a previously queued task. |queue_function| should match what was
  // passed into |DidQueueTask| for this task. |run_function| is used as the
  // name for the trace event that surrounds the task's execution.
  void RunTask(const char* queue_function,
               const char* run_function,
               const PendingTask& pending_task);

 private:
  // Creates a process-wide unique ID to represent this task in trace events.
  // This will be mangled with a Process ID hash to reduce the likelyhood of
  // colliding with TaskAnnotator pointers on other processes.
  uint64 GetTaskTraceID(const PendingTask& task) const;

  DISALLOW_COPY_AND_ASSIGN(TaskAnnotator);
};

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_TASK_ANNOTATOR_H_
