// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_descriptor_info_impl.h"

#include <fcntl.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/posix/eintr_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Get a safe file descriptor for test purposes.
// TODO(morrita) Merge with things in file_descriptor_set_posix_unittest.cc
int GetSafeFd() {
  return open("/dev/null", O_RDONLY);
}

// Returns true if fd was already closed.  Closes fd if not closed.
// TODO(morrita) Merge with things in file_descriptor_set_posix_unittest.cc
bool VerifyClosed(int fd) {
  const int duped = dup(fd);
  if (duped != -1) {
    EXPECT_NE(IGNORE_EINTR(close(duped)), -1);
    EXPECT_NE(IGNORE_EINTR(close(fd)), -1);
    return false;
  }
  return true;
}

}  // namespace

namespace content {

typedef testing::Test FileDescriptorInfoTest;

TEST_F(FileDescriptorInfoTest, Transfer) {
  int testingId = 42;
  scoped_ptr<FileDescriptorInfo> target(FileDescriptorInfoImpl::Create());
  base::ScopedFD fd(GetSafeFd());

  int raw_fd = fd.get();
  target->Transfer(testingId, fd.Pass());
  ASSERT_EQ(1U, target->GetMappingSize());
  ASSERT_EQ(target->GetFDAt(0), raw_fd);
  ASSERT_EQ(target->GetIDAt(0), testingId);

  target.reset();

  ASSERT_TRUE(VerifyClosed(raw_fd));
}

TEST_F(FileDescriptorInfoTest, Share) {
  int testingId = 42;
  scoped_ptr<FileDescriptorInfo> target(FileDescriptorInfoImpl::Create());
  base::ScopedFD fd(GetSafeFd());

  int raw_fd = fd.get();
  target->Share(testingId, fd.get());
  ASSERT_EQ(1U, target->GetMappingSize());
  ASSERT_EQ(target->GetFDAt(0), raw_fd);
  ASSERT_EQ(target->GetIDAt(0), testingId);

  target.reset();

  ASSERT_TRUE(!VerifyClosed(fd.release()));
}

TEST_F(FileDescriptorInfoTest, GetMappingWithIDAdjustment) {
  int testingId1 = 42;
  int testingId2 = 43;
  scoped_ptr<FileDescriptorInfo> target(FileDescriptorInfoImpl::Create());

  target->Transfer(testingId1, base::ScopedFD(GetSafeFd()));
  target->Transfer(testingId2, base::ScopedFD(GetSafeFd()));

  base::FileHandleMappingVector mapping =
      target->GetMappingWithIDAdjustment(100);
  ASSERT_EQ(mapping[0].second, 142);
  ASSERT_EQ(mapping[1].second, 143);
}

}  // namespace content
