// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/task_runners.h"

#include "base/threading/sequenced_worker_pool.h"

namespace mojo {
namespace runner {

namespace {

const size_t kMaxBlockingPoolThreads = 3;

scoped_ptr<base::Thread> CreateIOThread(const char* name) {
  scoped_ptr<base::Thread> thread(new base::Thread(name));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  thread->StartWithOptions(options);
  return thread.Pass();
}

}  // namespace

TaskRunners::TaskRunners(
    const scoped_refptr<base::SingleThreadTaskRunner>& shell_runner)
    : shell_runner_(shell_runner),
      io_thread_(CreateIOThread("io_thread")),
      blocking_pool_(new base::SequencedWorkerPool(kMaxBlockingPoolThreads,
                                                   "blocking_pool")) {
}

TaskRunners::~TaskRunners() {
  blocking_pool_->Shutdown();
}

}  // namespace runner
}  // namespace mojo
