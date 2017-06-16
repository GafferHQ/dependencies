// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_CORE_TEST_BASE_H_
#define MOJO_EDK_SYSTEM_CORE_TEST_BASE_H_

#include "base/synchronization/lock.h"
#include "mojo/edk/embedder/simple_platform_support.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {

class Core;
class Awakable;

namespace test {

class CoreTestBase_MockHandleInfo;

class CoreTestBase : public testing::Test {
 public:
  using MockHandleInfo = CoreTestBase_MockHandleInfo;

  CoreTestBase();
  ~CoreTestBase() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  // |info| must remain alive until the returned handle is closed.
  MojoHandle CreateMockHandle(MockHandleInfo* info);

  Core* core() { return core_; }

 private:
  embedder::SimplePlatformSupport platform_support_;
  Core* core_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(CoreTestBase);
};

class CoreTestBase_MockHandleInfo {
 public:
  CoreTestBase_MockHandleInfo();
  ~CoreTestBase_MockHandleInfo();

  unsigned GetCtorCallCount() const;
  unsigned GetDtorCallCount() const;
  unsigned GetCloseCallCount() const;
  unsigned GetWriteMessageCallCount() const;
  unsigned GetReadMessageCallCount() const;
  unsigned GetWriteDataCallCount() const;
  unsigned GetBeginWriteDataCallCount() const;
  unsigned GetEndWriteDataCallCount() const;
  unsigned GetReadDataCallCount() const;
  unsigned GetBeginReadDataCallCount() const;
  unsigned GetEndReadDataCallCount() const;
  unsigned GetAddAwakableCallCount() const;
  unsigned GetRemoveAwakableCallCount() const;
  unsigned GetCancelAllAwakablesCallCount() const;

  size_t GetAddedAwakableSize() const;
  Awakable* GetAddedAwakableAt(unsigned i) const;

  // For use by |MockDispatcher|:
  void IncrementCtorCallCount();
  void IncrementDtorCallCount();
  void IncrementCloseCallCount();
  void IncrementWriteMessageCallCount();
  void IncrementReadMessageCallCount();
  void IncrementWriteDataCallCount();
  void IncrementBeginWriteDataCallCount();
  void IncrementEndWriteDataCallCount();
  void IncrementReadDataCallCount();
  void IncrementBeginReadDataCallCount();
  void IncrementEndReadDataCallCount();
  void IncrementAddAwakableCallCount();
  void IncrementRemoveAwakableCallCount();
  void IncrementCancelAllAwakablesCallCount();

  void AllowAddAwakable(bool alllow);
  bool IsAddAwakableAllowed() const;
  void AwakableWasAdded(Awakable*);

 private:
  mutable base::Lock lock_;  // Protects the following members.
  unsigned ctor_call_count_;
  unsigned dtor_call_count_;
  unsigned close_call_count_;
  unsigned write_message_call_count_;
  unsigned read_message_call_count_;
  unsigned write_data_call_count_;
  unsigned begin_write_data_call_count_;
  unsigned end_write_data_call_count_;
  unsigned read_data_call_count_;
  unsigned begin_read_data_call_count_;
  unsigned end_read_data_call_count_;
  unsigned add_awakable_call_count_;
  unsigned remove_awakable_call_count_;
  unsigned cancel_all_awakables_call_count_;

  bool add_awakable_allowed_;
  std::vector<Awakable*> added_awakables_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(CoreTestBase_MockHandleInfo);
};

}  // namespace test
}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_CORE_TEST_BASE_H_
