// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "content/child/child_discardable_shared_memory_manager.h"
#include "content/child/child_gpu_memory_buffer_manager.h"
#include "content/child/child_thread_impl.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"
#include "content/common/host_discardable_shared_memory_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "url/gurl.h"

namespace content {
namespace {

class ChildThreadImplBrowserTest : public ContentBrowserTest {
 public:
  ChildThreadImplBrowserTest()
      : child_gpu_memory_buffer_manager_(nullptr),
        child_discardable_shared_memory_manager_(nullptr) {}

  // Overridden from BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
  }
  void SetUpOnMainThread() override {
    NavigateToURL(shell(), GURL(url::kAboutBlankURL));
    PostTaskToInProcessRendererAndWait(
        base::Bind(&ChildThreadImplBrowserTest::SetUpOnChildThread, this));
  }

  ChildGpuMemoryBufferManager* child_gpu_memory_buffer_manager() {
    return child_gpu_memory_buffer_manager_;
  }

  ChildDiscardableSharedMemoryManager*
  child_discardable_shared_memory_manager() {
    return child_discardable_shared_memory_manager_;
  }

 private:
  void SetUpOnChildThread() {
    child_gpu_memory_buffer_manager_ =
        ChildThreadImpl::current()->gpu_memory_buffer_manager();
    child_discardable_shared_memory_manager_ =
        ChildThreadImpl::current()->discardable_shared_memory_manager();
  }

  ChildGpuMemoryBufferManager* child_gpu_memory_buffer_manager_;
  ChildDiscardableSharedMemoryManager* child_discardable_shared_memory_manager_;
};

IN_PROC_BROWSER_TEST_F(ChildThreadImplBrowserTest,
                       DISABLED_LockDiscardableMemory) {
  const size_t kSize = 1024 * 1024;  // 1MiB.

  scoped_ptr<base::DiscardableMemory> memory =
      child_discardable_shared_memory_manager()
          ->AllocateLockedDiscardableMemory(kSize);

  ASSERT_TRUE(memory);
  void* addr = memory->data();
  ASSERT_NE(nullptr, addr);

  memory->Unlock();

  // Purge all unlocked memory.
  HostDiscardableSharedMemoryManager::current()->SetMemoryLimit(0);

  // Should fail as memory should have been purged.
  EXPECT_FALSE(memory->Lock());
}

IN_PROC_BROWSER_TEST_F(ChildThreadImplBrowserTest,
                       DISABLED_DiscardableMemoryAddressSpace) {
  const size_t kLargeSize = 4 * 1024 * 1024;   // 4MiB.
  const size_t kNumberOfInstances = 1024 + 1;  // >4GiB total.

  ScopedVector<base::DiscardableMemory> instances;
  for (size_t i = 0; i < kNumberOfInstances; ++i) {
    scoped_ptr<base::DiscardableMemory> memory =
        child_discardable_shared_memory_manager()
            ->AllocateLockedDiscardableMemory(kLargeSize);
    ASSERT_TRUE(memory);
    void* addr = memory->data();
    ASSERT_NE(nullptr, addr);
    memory->Unlock();
    instances.push_back(memory.Pass());
  }
}

IN_PROC_BROWSER_TEST_F(ChildThreadImplBrowserTest,
                       DISABLED_ReleaseFreeDiscardableMemory) {
  const size_t kSize = 1024 * 1024;  // 1MiB.

  scoped_ptr<base::DiscardableMemory> memory =
      child_discardable_shared_memory_manager()
          ->AllocateLockedDiscardableMemory(kSize);

  EXPECT_TRUE(memory);
  memory.reset();

  EXPECT_GE(HostDiscardableSharedMemoryManager::current()->GetBytesAllocated(),
            kSize);

  child_discardable_shared_memory_manager()->ReleaseFreeMemory();

  // Busy wait for host memory usage to be reduced.
  base::TimeTicks end =
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(5);
  while (base::TimeTicks::Now() < end) {
    if (!HostDiscardableSharedMemoryManager::current()->GetBytesAllocated())
      break;
  }

  EXPECT_LT(base::TimeTicks::Now(), end);
}

enum NativeBufferFlag { kDisableNativeBuffers, kEnableNativeBuffers };

class ChildThreadImplGpuMemoryBufferBrowserTest
    : public ChildThreadImplBrowserTest,
      public testing::WithParamInterface<
          ::testing::tuple<NativeBufferFlag, gfx::GpuMemoryBuffer::Format>> {
 public:
  // Overridden from BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChildThreadImplBrowserTest::SetUpCommandLine(command_line);
    NativeBufferFlag native_buffer_flag = ::testing::get<0>(GetParam());
    switch (native_buffer_flag) {
      case kEnableNativeBuffers:
        command_line->AppendSwitch(switches::kEnableNativeGpuMemoryBuffers);
        break;
      case kDisableNativeBuffers:
        break;
    }
  }
};

IN_PROC_BROWSER_TEST_P(ChildThreadImplGpuMemoryBufferBrowserTest,
                       DISABLED_Map) {
  gfx::GpuMemoryBuffer::Format format = ::testing::get<1>(GetParam());
  gfx::Size buffer_size(4, 4);

  scoped_ptr<gfx::GpuMemoryBuffer> buffer =
      child_gpu_memory_buffer_manager()->AllocateGpuMemoryBuffer(
          buffer_size, format, gfx::GpuMemoryBuffer::MAP);
  ASSERT_TRUE(buffer);
  EXPECT_EQ(format, buffer->GetFormat());

  size_t num_planes =
      GpuMemoryBufferImpl::NumberOfPlanesForGpuMemoryBufferFormat(format);

  // Map buffer planes.
  scoped_ptr<void* []> planes(new void* [num_planes]);
  bool rv = buffer->Map(planes.get());
  ASSERT_TRUE(rv);
  EXPECT_TRUE(buffer->IsMapped());

  // Get strides.
  scoped_ptr<int[]> strides(new int[num_planes]);
  buffer->GetStride(strides.get());

  // Write to buffer and check result.
  for (size_t plane = 0; plane < num_planes; ++plane) {
    size_t row_size_in_bytes = 0;
    EXPECT_TRUE(GpuMemoryBufferImpl::RowSizeInBytes(buffer_size.width(), format,
                                                    plane, &row_size_in_bytes));

    scoped_ptr<char[]> data(new char[row_size_in_bytes]);
    memset(data.get(), 0x2a + plane, row_size_in_bytes);
    size_t height = buffer_size.height() /
                    GpuMemoryBufferImpl::SubsamplingFactor(format, plane);
    for (size_t y = 0; y < height; ++y) {
      // Copy |data| to row |y| of |plane| and verify result.
      memcpy(static_cast<char*>(planes[plane]) + y * strides[plane], data.get(),
             row_size_in_bytes);
      EXPECT_EQ(memcmp(static_cast<char*>(planes[plane]) + y * strides[plane],
                       data.get(), row_size_in_bytes),
                0);
    }
  }

  buffer->Unmap();
  EXPECT_FALSE(buffer->IsMapped());
}

INSTANTIATE_TEST_CASE_P(
    ChildThreadImplGpuMemoryBufferBrowserTests,
    ChildThreadImplGpuMemoryBufferBrowserTest,
    ::testing::Combine(::testing::Values(kDisableNativeBuffers,
                                         kEnableNativeBuffers),
                       // These formats are guaranteed to work on all platforms.
                       ::testing::Values(gfx::GpuMemoryBuffer::R_8,
                                         gfx::GpuMemoryBuffer::RGBA_4444,
                                         gfx::GpuMemoryBuffer::RGBA_8888,
                                         gfx::GpuMemoryBuffer::BGRA_8888,
                                         gfx::GpuMemoryBuffer::YUV_420)));

}  // namespace
}  // namespace content
