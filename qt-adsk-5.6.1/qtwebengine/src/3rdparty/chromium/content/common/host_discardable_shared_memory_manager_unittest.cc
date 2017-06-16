// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/host_discardable_shared_memory_manager.h"

#include "content/public/common/child_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class TestDiscardableSharedMemory : public base::DiscardableSharedMemory {
 public:
  TestDiscardableSharedMemory() {}

  explicit TestDiscardableSharedMemory(base::SharedMemoryHandle handle)
      : DiscardableSharedMemory(handle) {}

  void SetNow(base::Time now) { now_ = now; }

 private:
  // Overriden from base::DiscardableSharedMemory:
  base::Time Now() const override { return now_; }

  base::Time now_;
};

class TestHostDiscardableSharedMemoryManager
    : public HostDiscardableSharedMemoryManager {
 public:
  TestHostDiscardableSharedMemoryManager()
      : enforce_memory_policy_pending_(false) {}

  void SetNow(base::Time now) { now_ = now; }

  void set_enforce_memory_policy_pending(bool enforce_memory_policy_pending) {
    enforce_memory_policy_pending_ = enforce_memory_policy_pending;
  }
  bool enforce_memory_policy_pending() const {
    return enforce_memory_policy_pending_;
  }

 private:
  // Overriden from HostDiscardableSharedMemoryManager:
  base::Time Now() const override { return now_; }
  void ScheduleEnforceMemoryPolicy() override {
    enforce_memory_policy_pending_ = true;
  }

  base::Time now_;
  bool enforce_memory_policy_pending_;
};

class HostDiscardableSharedMemoryManagerTest : public testing::Test {
 protected:
  // Overridden from testing::Test:
  void SetUp() override {
    manager_.reset(new TestHostDiscardableSharedMemoryManager);
  }

  scoped_ptr<TestHostDiscardableSharedMemoryManager> manager_;
};

TEST_F(HostDiscardableSharedMemoryManagerTest, AllocateForChild) {
  const int kDataSize = 1024;
  uint8 data[kDataSize];
  memset(data, 0x80, kDataSize);

  base::SharedMemoryHandle shared_handle;
  manager_->AllocateLockedDiscardableSharedMemoryForChild(
      base::GetCurrentProcessHandle(), ChildProcessHost::kInvalidUniqueID,
      kDataSize, 0, &shared_handle);
  ASSERT_TRUE(base::SharedMemory::IsHandleValid(shared_handle));

  TestDiscardableSharedMemory memory(shared_handle);
  bool rv = memory.Map(kDataSize);
  ASSERT_TRUE(rv);

  memcpy(memory.memory(), data, kDataSize);
  memory.SetNow(base::Time::FromDoubleT(1));
  memory.Unlock(0, 0);

  ASSERT_EQ(base::DiscardableSharedMemory::SUCCESS, memory.Lock(0, 0));
  EXPECT_EQ(memcmp(data, memory.memory(), kDataSize), 0);
  memory.Unlock(0, 0);
}

TEST_F(HostDiscardableSharedMemoryManagerTest, Purge) {
  const int kDataSize = 1024;

  base::SharedMemoryHandle shared_handle1;
  manager_->AllocateLockedDiscardableSharedMemoryForChild(
      base::GetCurrentProcessHandle(), ChildProcessHost::kInvalidUniqueID,
      kDataSize, 1, &shared_handle1);
  ASSERT_TRUE(base::SharedMemory::IsHandleValid(shared_handle1));

  TestDiscardableSharedMemory memory1(shared_handle1);
  bool rv = memory1.Map(kDataSize);
  ASSERT_TRUE(rv);

  base::SharedMemoryHandle shared_handle2;
  manager_->AllocateLockedDiscardableSharedMemoryForChild(
      base::GetCurrentProcessHandle(), ChildProcessHost::kInvalidUniqueID,
      kDataSize, 2, &shared_handle2);
  ASSERT_TRUE(base::SharedMemory::IsHandleValid(shared_handle2));

  TestDiscardableSharedMemory memory2(shared_handle2);
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);

  // Enough memory for both allocations.
  manager_->SetNow(base::Time::FromDoubleT(1));
  manager_->SetMemoryLimit(memory1.mapped_size() + memory2.mapped_size());

  memory1.SetNow(base::Time::FromDoubleT(2));
  memory1.Unlock(0, 0);
  memory2.SetNow(base::Time::FromDoubleT(2));
  memory2.Unlock(0, 0);

  // Manager should not have to schedule another call to EnforceMemoryPolicy().
  manager_->SetNow(base::Time::FromDoubleT(3));
  manager_->EnforceMemoryPolicy();
  EXPECT_FALSE(manager_->enforce_memory_policy_pending());

  // Memory should still be resident.
  EXPECT_TRUE(memory1.IsMemoryResident());
  EXPECT_TRUE(memory2.IsMemoryResident());

  auto lock_rv = memory1.Lock(0, 0);
  EXPECT_EQ(base::DiscardableSharedMemory::SUCCESS, lock_rv);
  lock_rv = memory2.Lock(0, 0);
  EXPECT_EQ(base::DiscardableSharedMemory::SUCCESS, lock_rv);

  memory1.SetNow(base::Time::FromDoubleT(4));
  memory1.Unlock(0, 0);
  memory2.SetNow(base::Time::FromDoubleT(5));
  memory2.Unlock(0, 0);

  // Just enough memory for one allocation.
  manager_->SetNow(base::Time::FromDoubleT(6));
  manager_->SetMemoryLimit(memory2.mapped_size());
  EXPECT_FALSE(manager_->enforce_memory_policy_pending());

  // LRU allocation should still be resident.
  EXPECT_FALSE(memory1.IsMemoryResident());
  EXPECT_TRUE(memory2.IsMemoryResident());

  lock_rv = memory1.Lock(0, 0);
  EXPECT_EQ(base::DiscardableSharedMemory::FAILED, lock_rv);
  lock_rv = memory2.Lock(0, 0);
  EXPECT_EQ(base::DiscardableSharedMemory::SUCCESS, lock_rv);
}

TEST_F(HostDiscardableSharedMemoryManagerTest, EnforceMemoryPolicy) {
  const int kDataSize = 1024;

  base::SharedMemoryHandle shared_handle;
  manager_->AllocateLockedDiscardableSharedMemoryForChild(
      base::GetCurrentProcessHandle(), ChildProcessHost::kInvalidUniqueID,
      kDataSize, 0, &shared_handle);
  ASSERT_TRUE(base::SharedMemory::IsHandleValid(shared_handle));

  TestDiscardableSharedMemory memory(shared_handle);
  bool rv = memory.Map(kDataSize);
  ASSERT_TRUE(rv);

  // Not enough memory for one allocation.
  manager_->SetNow(base::Time::FromDoubleT(1));
  manager_->SetMemoryLimit(memory.mapped_size() - 1);
  // We need to enforce memory policy as our memory usage is currently above
  // the limit.
  EXPECT_TRUE(manager_->enforce_memory_policy_pending());

  manager_->set_enforce_memory_policy_pending(false);
  manager_->SetNow(base::Time::FromDoubleT(2));
  manager_->EnforceMemoryPolicy();
  // Still need to enforce memory policy as nothing can be purged.
  EXPECT_TRUE(manager_->enforce_memory_policy_pending());

  memory.SetNow(base::Time::FromDoubleT(3));
  memory.Unlock(0, 0);

  manager_->set_enforce_memory_policy_pending(false);
  manager_->SetNow(base::Time::FromDoubleT(4));
  manager_->EnforceMemoryPolicy();
  // Memory policy should have successfully been enforced.
  EXPECT_FALSE(manager_->enforce_memory_policy_pending());

  EXPECT_EQ(base::DiscardableSharedMemory::FAILED, memory.Lock(0, 0));
}

}  // namespace
}  // namespace content
