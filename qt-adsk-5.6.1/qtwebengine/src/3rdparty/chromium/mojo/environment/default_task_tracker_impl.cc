// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/environment/default_task_tracker_impl.h"

namespace mojo {
namespace internal {
namespace {

TaskTrackingId StartTracking(const char* function_name,
                             const char* file_name,
                             int line_number,
                             const void* program_counter) {
  return TaskTrackingId(0);
}

void EndTracking(const TaskTrackingId id) {
}

void SetEnabled(bool enabled) {
}

const TaskTracker kDefaultTaskTracker = {&StartTracking,
                                         &EndTracking,
                                         &SetEnabled};

}  // namespace

const TaskTracker* GetDefaultTaskTracker() {
  return &kDefaultTaskTracker;
}

}  // namespace internal
}  // namespace mojo
