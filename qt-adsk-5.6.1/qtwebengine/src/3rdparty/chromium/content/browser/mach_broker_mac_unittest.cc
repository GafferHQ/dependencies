// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mach_broker_mac.h"

#include "base/synchronization/lock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MachBrokerTest : public testing::Test {
 public:
  // Helper function to acquire/release locks and call |PlaceholderForPid()|.
  void AddPlaceholderForPid(base::ProcessHandle pid, int child_process_id) {
    base::AutoLock lock(broker_.GetLock());
    broker_.AddPlaceholderForPid(pid, child_process_id);
  }

  void InvalidateChildProcessId(int child_process_id) {
    broker_.InvalidateChildProcessId(child_process_id);
  }

  int GetChildProcessCount(int child_process_id) {
    return broker_.child_process_id_map_.count(child_process_id);
  }

  // Helper function to acquire/release locks and call |FinalizePid()|.
  void FinalizePid(base::ProcessHandle pid,
                   mach_port_t task_port) {
    base::AutoLock lock(broker_.GetLock());
    broker_.FinalizePid(pid, task_port);
  }

 protected:
  MachBroker broker_;
};

TEST_F(MachBrokerTest, Locks) {
  // Acquire and release the locks.  Nothing bad should happen.
  base::AutoLock lock(broker_.GetLock());
}

TEST_F(MachBrokerTest, AddPlaceholderAndFinalize) {
  // Add a placeholder for PID 1.
  AddPlaceholderForPid(1, 1);
  EXPECT_EQ(0u, broker_.TaskForPid(1));

  // Finalize PID 1.
  FinalizePid(1, 100u);
  EXPECT_EQ(100u, broker_.TaskForPid(1));

  // Should be no entry for PID 2.
  EXPECT_EQ(0u, broker_.TaskForPid(2));
}

TEST_F(MachBrokerTest, InvalidateChildProcessId) {
  // Add a placeholder for PID 1 and child process id 50.
  AddPlaceholderForPid(1, 50);
  FinalizePid(1, 100u);

  EXPECT_EQ(100u, broker_.TaskForPid(1));
  InvalidateChildProcessId(50);
  EXPECT_EQ(0u, broker_.TaskForPid(1));
}

TEST_F(MachBrokerTest, ValidateChildProcessIdMap) {
  // Add a placeholder for PID 1 and child process id 50.
  AddPlaceholderForPid(1, 50);
  FinalizePid(1, 100u);

  EXPECT_EQ(1, GetChildProcessCount(50));
  InvalidateChildProcessId(50);
  EXPECT_EQ(0, GetChildProcessCount(50));
}

TEST_F(MachBrokerTest, FinalizeUnknownPid) {
  // Finalizing an entry for an unknown pid should not add it to the map.
  FinalizePid(1u, 100u);
  EXPECT_EQ(0u, broker_.TaskForPid(1u));
}

}  // namespace content
