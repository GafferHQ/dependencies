// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/task_queue_manager.h"

#include "base/bind.h"
#include "base/threading/thread.h"
#include "components/scheduler/child/scheduler_message_loop_delegate.h"
#include "components/scheduler/child/task_queue.h"
#include "components/scheduler/child/task_queue_selector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace scheduler {

namespace {

class SelectorForTest : public TaskQueueSelector {
 public:
  SelectorForTest() {}

  void RegisterWorkQueues(
      const std::vector<const base::TaskQueue*>& work_queues) override {
    work_queues_ = work_queues;
  }

  bool SelectWorkQueueToService(size_t* out_queue_index) override {
    // Choose the oldest task, if any.
    bool found_one = false;
    for (size_t i = 0; i < work_queues_.size(); i++) {
      if (work_queues_[i]->empty())
        continue;
      // Note: the < comparison is correct due to the fact that the PendingTask
      // operator inverts its comparison operation in order to work well in a
      // heap based priority queue.
      if (!found_one ||
          work_queues_[*out_queue_index]->front() < work_queues_[i]->front())
        *out_queue_index = i;
      found_one = true;
    }
    CHECK(found_one);
    return found_one;
  }

  void SetTaskQueueSelectorObserver(Observer* observer) override {}

  void AsValueInto(base::trace_event::TracedValue* state) const override {}

 private:
  std::vector<const base::TaskQueue*> work_queues_;

  DISALLOW_COPY_AND_ASSIGN(SelectorForTest);
};

}  // namespace

class TaskQueueManagerPerfTest : public testing::Test {
 public:
  TaskQueueManagerPerfTest()
      : num_queues_(0),
        max_tasks_in_flight_(0),
        num_tasks_in_flight_(0),
        num_tasks_to_post_(0),
        num_tasks_to_run_(0) {}

  void Initialize(size_t num_queues) {
    num_queues_ = num_queues;
    message_loop_.reset(new base::MessageLoop());
    selector_ = make_scoped_ptr(new SelectorForTest);
    manager_ = make_scoped_ptr(new TaskQueueManager(
        num_queues, SchedulerMessageLoopDelegate::Create(message_loop_.get()),
        selector_.get(), "fake.category", "fake.category.debug"));
  }

  void TestDelayedTask() {
    if (--num_tasks_to_run_ == 0) {
      message_loop_->Quit();
    }

    num_tasks_in_flight_--;
    // NOTE there are only up to max_tasks_in_flight_ pending delayed tasks at
    // any one time.  Thanks to the lower_num_tasks_to_post going to zero if
    // there are a lot of tasks in flight, the total number of task in flight at
    // any one time is very variable.
    unsigned int lower_num_tasks_to_post =
        num_tasks_in_flight_ < (max_tasks_in_flight_ / 2) ? 1 : 0;
    unsigned int max_tasks_to_post =
        num_tasks_to_post_ % 2 ? lower_num_tasks_to_post : 10;
    for (unsigned int i = 0;
         i < max_tasks_to_post && num_tasks_in_flight_ < max_tasks_in_flight_ &&
             num_tasks_to_post_ > 0;
         i++) {
      // Choose a queue weighted towards queue 0.
      unsigned int queue = num_tasks_to_post_ % (num_queues_ + 1);
      if (queue == num_queues_) {
        queue = 0;
      }
      // Simulate a mix of short and longer delays.
      unsigned int delay =
          num_tasks_to_post_ % 2 ? 1 : (10 + num_tasks_to_post_ % 10);
      scoped_refptr<base::SingleThreadTaskRunner> runner =
          manager_->TaskRunnerForQueue(queue);
      runner->PostDelayedTask(
          FROM_HERE, base::Bind(&TaskQueueManagerPerfTest::TestDelayedTask,
                                base::Unretained(this)),
          base::TimeDelta::FromMicroseconds(delay));
      num_tasks_in_flight_++;
      num_tasks_to_post_--;
    }
  }

  void ResetAndCallTestDelayedTask(unsigned int num_tasks_to_run) {
    num_tasks_in_flight_ = 1;
    num_tasks_to_post_ = num_tasks_to_run;
    num_tasks_to_run_ = num_tasks_to_run;
    TestDelayedTask();
  }

  void Benchmark(const std::string& trace, const base::Closure& test_task) {
    base::TimeTicks start = base::TimeTicks::Now();
    base::TimeTicks now;
    unsigned long long num_iterations = 0;
    do {
      test_task.Run();
      message_loop_->Run();
      now = base::TimeTicks::Now();
      num_iterations++;
    } while (now - start < base::TimeDelta::FromSeconds(5));
    perf_test::PrintResult(
        "task", "", trace,
        (now - start).InMicroseconds() / static_cast<double>(num_iterations),
        "us/run", true);
  }

  size_t num_queues_;
  unsigned int max_tasks_in_flight_;
  unsigned int num_tasks_in_flight_;
  unsigned int num_tasks_to_post_;
  unsigned int num_tasks_to_run_;
  scoped_ptr<SelectorForTest> selector_;
  scoped_ptr<TaskQueueManager> manager_;
  scoped_ptr<base::MessageLoop> message_loop_;
};

TEST_F(TaskQueueManagerPerfTest, RunTenThousandDelayedTasks_OneQueue) {
  Initialize(1u);

  max_tasks_in_flight_ = 200;
  Benchmark("run 10000 delayed tasks with one queue",
            base::Bind(&TaskQueueManagerPerfTest::ResetAndCallTestDelayedTask,
                       base::Unretained(this), 10000));
}

TEST_F(TaskQueueManagerPerfTest, RunTenThousandDelayedTasks_FourQueues) {
  Initialize(4u);

  max_tasks_in_flight_ = 200;
  Benchmark("run 10000 delayed tasks with four queues",
            base::Bind(&TaskQueueManagerPerfTest::ResetAndCallTestDelayedTask,
                       base::Unretained(this), 10000));
}

TEST_F(TaskQueueManagerPerfTest, RunTenThousandDelayedTasks_EightQueues) {
  Initialize(8u);

  max_tasks_in_flight_ = 200;
  Benchmark("run 10000 delayed tasks with eight queues",
            base::Bind(&TaskQueueManagerPerfTest::ResetAndCallTestDelayedTask,
                       base::Unretained(this), 10000));
}

// TODO(alexclarke): Add additional tests with different mixes of non-delayed vs
// delayed tasks.

}  // namespace scheduler
