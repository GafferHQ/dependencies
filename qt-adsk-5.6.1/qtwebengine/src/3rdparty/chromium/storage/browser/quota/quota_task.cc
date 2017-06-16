// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_task.h"

#include <algorithm>
#include <functional>

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"

using base::TaskRunner;

namespace storage {

// QuotaTask ---------------------------------------------------------------

QuotaTask::~QuotaTask() {
}

void QuotaTask::Start() {
  DCHECK(observer_);
  observer()->RegisterTask(this);
  Run();
}

QuotaTask::QuotaTask(QuotaTaskObserver* observer)
    : observer_(observer),
      original_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      delete_scheduled_(false) {
}

void QuotaTask::CallCompleted() {
  DCHECK(original_task_runner_->BelongsToCurrentThread());
  if (observer_) {
    observer_->UnregisterTask(this);
    Completed();
  }
}

void QuotaTask::Abort() {
  DCHECK(original_task_runner_->BelongsToCurrentThread());
  observer_ = NULL;
  Aborted();
}

void QuotaTask::DeleteSoon() {
  DCHECK(original_task_runner_->BelongsToCurrentThread());
  if (delete_scheduled_)
    return;
  delete_scheduled_ = true;
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

// QuotaTaskObserver -------------------------------------------------------

QuotaTaskObserver::~QuotaTaskObserver() {
  std::for_each(running_quota_tasks_.begin(),
                running_quota_tasks_.end(),
                std::mem_fun(&QuotaTask::Abort));
}

QuotaTaskObserver::QuotaTaskObserver() {
}

void QuotaTaskObserver::RegisterTask(QuotaTask* task) {
  running_quota_tasks_.insert(task);
}

void QuotaTaskObserver::UnregisterTask(QuotaTask* task) {
  DCHECK(running_quota_tasks_.find(task) != running_quota_tasks_.end());
  running_quota_tasks_.erase(task);
}

}  // namespace storage
