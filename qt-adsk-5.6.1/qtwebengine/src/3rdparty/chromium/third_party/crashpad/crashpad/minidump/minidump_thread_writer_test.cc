// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "minidump/minidump_thread_writer.h"

#include <windows.h>
#include <dbghelp.h>
#include <sys/types.h>

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "minidump/minidump_context_writer.h"
#include "minidump/minidump_memory_writer.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/minidump_thread_id_map.h"
#include "minidump/test/minidump_context_test_util.h"
#include "minidump/test/minidump_memory_writer_test_util.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "snapshot/test/test_cpu_context.h"
#include "snapshot/test/test_memory_snapshot.h"
#include "snapshot/test/test_thread_snapshot.h"
#include "test/gtest_death_check.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

// This returns the MINIDUMP_THREAD_LIST stream in |thread_list|. If
// |memory_list| is not nullptr, a MINIDUMP_MEMORY_LIST stream is also expected
// in |file_contents|, and that stream will be returned in |memory_list|.
void GetThreadListStream(const std::string& file_contents,
                         const MINIDUMP_THREAD_LIST** thread_list,
                         const MINIDUMP_MEMORY_LIST** memory_list) {
  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const uint32_t kExpectedStreams = memory_list ? 2 : 1;
  const size_t kThreadListStreamOffset =
      kDirectoryOffset + kExpectedStreams * sizeof(MINIDUMP_DIRECTORY);
  const size_t kThreadsOffset =
      kThreadListStreamOffset + sizeof(MINIDUMP_THREAD_LIST);

  ASSERT_GE(file_contents.size(), kThreadsOffset);

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(file_contents, &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, kExpectedStreams, 0));
  ASSERT_TRUE(directory);

  ASSERT_EQ(kMinidumpStreamTypeThreadList, directory[0].StreamType);
  EXPECT_EQ(kThreadListStreamOffset, directory[0].Location.Rva);

  *thread_list = MinidumpWritableAtLocationDescriptor<MINIDUMP_THREAD_LIST>(
      file_contents, directory[0].Location);
  ASSERT_TRUE(thread_list);

  if (memory_list) {
    ASSERT_EQ(kMinidumpStreamTypeMemoryList, directory[1].StreamType);

    *memory_list = MinidumpWritableAtLocationDescriptor<MINIDUMP_MEMORY_LIST>(
        file_contents, directory[1].Location);
    ASSERT_TRUE(*memory_list);
  }
}

TEST(MinidumpThreadWriter, EmptyThreadList) {
  MinidumpFileWriter minidump_file_writer;
  auto thread_list_writer = make_scoped_ptr(new MinidumpThreadListWriter());

  minidump_file_writer.AddStream(thread_list_writer.Pass());

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_LIST),
            string_file.string().size());

  const MINIDUMP_THREAD_LIST* thread_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadListStream(string_file.string(), &thread_list, nullptr));

  EXPECT_EQ(0u, thread_list->NumberOfThreads);
}

// The MINIDUMP_THREADs |expected| and |observed| are compared against each
// other using gtest assertions. If |stack| is not nullptr, |observed| is
// expected to contain a populated MINIDUMP_MEMORY_DESCRIPTOR in its Stack
// field, otherwise, its Stack field is expected to be zeroed out. The memory
// descriptor will be placed in |stack|. |observed| must contain a populated
// ThreadContext field. The context will be recovered from |file_contents| and
// stored in |context_base|.
void ExpectThread(const MINIDUMP_THREAD* expected,
                  const MINIDUMP_THREAD* observed,
                  const std::string& file_contents,
                  const MINIDUMP_MEMORY_DESCRIPTOR** stack,
                  const void** context_base) {
  EXPECT_EQ(expected->ThreadId, observed->ThreadId);
  EXPECT_EQ(expected->SuspendCount, observed->SuspendCount);
  EXPECT_EQ(expected->PriorityClass, observed->PriorityClass);
  EXPECT_EQ(expected->Priority, observed->Priority);
  EXPECT_EQ(expected->Teb, observed->Teb);

  EXPECT_EQ(expected->Stack.StartOfMemoryRange,
            observed->Stack.StartOfMemoryRange);
  EXPECT_EQ(expected->Stack.Memory.DataSize, observed->Stack.Memory.DataSize);
  if (stack) {
    ASSERT_NE(0u, observed->Stack.Memory.DataSize);
    ASSERT_NE(0u, observed->Stack.Memory.Rva);
    ASSERT_GE(file_contents.size(),
              observed->Stack.Memory.Rva + observed->Stack.Memory.DataSize);
    *stack = &observed->Stack;
  } else {
    EXPECT_EQ(0u, observed->Stack.StartOfMemoryRange);
    EXPECT_EQ(0u, observed->Stack.Memory.DataSize);
    EXPECT_EQ(0u, observed->Stack.Memory.Rva);
  }

  EXPECT_EQ(expected->ThreadContext.DataSize, observed->ThreadContext.DataSize);
  ASSERT_NE(0u, observed->ThreadContext.DataSize);
  ASSERT_NE(0u, observed->ThreadContext.Rva);
  ASSERT_GE(file_contents.size(),
            observed->ThreadContext.Rva + expected->ThreadContext.DataSize);
  *context_base = &file_contents[observed->ThreadContext.Rva];
}

TEST(MinidumpThreadWriter, OneThread_x86_NoStack) {
  MinidumpFileWriter minidump_file_writer;
  auto thread_list_writer = make_scoped_ptr(new MinidumpThreadListWriter());

  const uint32_t kThreadID = 0x11111111;
  const uint32_t kSuspendCount = 1;
  const uint32_t kPriorityClass = 0x20;
  const uint32_t kPriority = 10;
  const uint64_t kTEB = 0x55555555;
  const uint32_t kSeed = 123;

  auto thread_writer = make_scoped_ptr(new MinidumpThreadWriter());
  thread_writer->SetThreadID(kThreadID);
  thread_writer->SetSuspendCount(kSuspendCount);
  thread_writer->SetPriorityClass(kPriorityClass);
  thread_writer->SetPriority(kPriority);
  thread_writer->SetTEB(kTEB);

  auto context_x86_writer = make_scoped_ptr(new MinidumpContextX86Writer());
  InitializeMinidumpContextX86(context_x86_writer->context(), kSeed);
  thread_writer->SetContext(context_x86_writer.Pass());

  thread_list_writer->AddThread(thread_writer.Pass());
  minidump_file_writer.AddStream(thread_list_writer.Pass());

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_LIST) + 1 * sizeof(MINIDUMP_THREAD) +
                1 * sizeof(MinidumpContextX86),
            string_file.string().size());

  const MINIDUMP_THREAD_LIST* thread_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadListStream(string_file.string(), &thread_list, nullptr));

  EXPECT_EQ(1u, thread_list->NumberOfThreads);

  MINIDUMP_THREAD expected = {};
  expected.ThreadId = kThreadID;
  expected.SuspendCount = kSuspendCount;
  expected.PriorityClass = kPriorityClass;
  expected.Priority = kPriority;
  expected.Teb = kTEB;
  expected.ThreadContext.DataSize = sizeof(MinidumpContextX86);

  const MinidumpContextX86* observed_context = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      ExpectThread(&expected,
                   &thread_list->Threads[0],
                   string_file.string(),
                   nullptr,
                   reinterpret_cast<const void**>(&observed_context)));

  ASSERT_NO_FATAL_FAILURE(
      ExpectMinidumpContextX86(kSeed, observed_context, false));
}

TEST(MinidumpThreadWriter, OneThread_AMD64_Stack) {
  MinidumpFileWriter minidump_file_writer;
  auto thread_list_writer = make_scoped_ptr(new MinidumpThreadListWriter());

  const uint32_t kThreadID = 0x22222222;
  const uint32_t kSuspendCount = 2;
  const uint32_t kPriorityClass = 0x30;
  const uint32_t kPriority = 20;
  const uint64_t kTEB = 0x5555555555555555;
  const uint64_t kMemoryBase = 0x765432100000;
  const size_t kMemorySize = 32;
  const uint8_t kMemoryValue = 99;
  const uint32_t kSeed = 456;

  auto thread_writer = make_scoped_ptr(new MinidumpThreadWriter());
  thread_writer->SetThreadID(kThreadID);
  thread_writer->SetSuspendCount(kSuspendCount);
  thread_writer->SetPriorityClass(kPriorityClass);
  thread_writer->SetPriority(kPriority);
  thread_writer->SetTEB(kTEB);

  auto memory_writer = make_scoped_ptr(
      new TestMinidumpMemoryWriter(kMemoryBase, kMemorySize, kMemoryValue));
  thread_writer->SetStack(memory_writer.Pass());

  MSVC_SUPPRESS_WARNING(4316);  // Object allocated on heap may not be aligned.
  auto context_amd64_writer = make_scoped_ptr(new MinidumpContextAMD64Writer());
  InitializeMinidumpContextAMD64(context_amd64_writer->context(), kSeed);
  thread_writer->SetContext(context_amd64_writer.Pass());

  thread_list_writer->AddThread(thread_writer.Pass());
  minidump_file_writer.AddStream(thread_list_writer.Pass());

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_LIST) + 1 * sizeof(MINIDUMP_THREAD) +
                1 * sizeof(MinidumpContextAMD64) + kMemorySize,
            string_file.string().size());

  const MINIDUMP_THREAD_LIST* thread_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadListStream(string_file.string(), &thread_list, nullptr));

  EXPECT_EQ(1u, thread_list->NumberOfThreads);

  MINIDUMP_THREAD expected = {};
  expected.ThreadId = kThreadID;
  expected.SuspendCount = kSuspendCount;
  expected.PriorityClass = kPriorityClass;
  expected.Priority = kPriority;
  expected.Teb = kTEB;
  expected.Stack.StartOfMemoryRange = kMemoryBase;
  expected.Stack.Memory.DataSize = kMemorySize;
  expected.ThreadContext.DataSize = sizeof(MinidumpContextAMD64);

  const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack = nullptr;
  const MinidumpContextAMD64* observed_context = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      ExpectThread(&expected,
                   &thread_list->Threads[0],
                   string_file.string(),
                   &observed_stack,
                   reinterpret_cast<const void**>(&observed_context)));

  ASSERT_NO_FATAL_FAILURE(
      ExpectMinidumpMemoryDescriptorAndContents(&expected.Stack,
                                                observed_stack,
                                                string_file.string(),
                                                kMemoryValue,
                                                true));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMinidumpContextAMD64(kSeed, observed_context, false));
}

TEST(MinidumpThreadWriter, ThreeThreads_x86_MemoryList) {
  MinidumpFileWriter minidump_file_writer;
  auto thread_list_writer = make_scoped_ptr(new MinidumpThreadListWriter());
  auto memory_list_writer = make_scoped_ptr(new MinidumpMemoryListWriter());
  thread_list_writer->SetMemoryListWriter(memory_list_writer.get());

  const uint32_t kThreadID0 = 1111111;
  const uint32_t kSuspendCount0 = 111111;
  const uint32_t kPriorityClass0 = 11111;
  const uint32_t kPriority0 = 1111;
  const uint64_t kTEB0 = 111;
  const uint64_t kMemoryBase0 = 0x1110;
  const size_t kMemorySize0 = 16;
  const uint8_t kMemoryValue0 = 11;
  const uint32_t kSeed0 = 1;

  auto thread_writer_0 = make_scoped_ptr(new MinidumpThreadWriter());
  thread_writer_0->SetThreadID(kThreadID0);
  thread_writer_0->SetSuspendCount(kSuspendCount0);
  thread_writer_0->SetPriorityClass(kPriorityClass0);
  thread_writer_0->SetPriority(kPriority0);
  thread_writer_0->SetTEB(kTEB0);

  auto memory_writer_0 = make_scoped_ptr(
      new TestMinidumpMemoryWriter(kMemoryBase0, kMemorySize0, kMemoryValue0));
  thread_writer_0->SetStack(memory_writer_0.Pass());

  auto context_x86_writer_0 = make_scoped_ptr(new MinidumpContextX86Writer());
  InitializeMinidumpContextX86(context_x86_writer_0->context(), kSeed0);
  thread_writer_0->SetContext(context_x86_writer_0.Pass());

  thread_list_writer->AddThread(thread_writer_0.Pass());

  const uint32_t kThreadID1 = 2222222;
  const uint32_t kSuspendCount1 = 222222;
  const uint32_t kPriorityClass1 = 22222;
  const uint32_t kPriority1 = 2222;
  const uint64_t kTEB1 = 222;
  const uint64_t kMemoryBase1 = 0x2220;
  const size_t kMemorySize1 = 32;
  const uint8_t kMemoryValue1 = 22;
  const uint32_t kSeed1 = 2;

  auto thread_writer_1 = make_scoped_ptr(new MinidumpThreadWriter());
  thread_writer_1->SetThreadID(kThreadID1);
  thread_writer_1->SetSuspendCount(kSuspendCount1);
  thread_writer_1->SetPriorityClass(kPriorityClass1);
  thread_writer_1->SetPriority(kPriority1);
  thread_writer_1->SetTEB(kTEB1);

  auto memory_writer_1 = make_scoped_ptr(
      new TestMinidumpMemoryWriter(kMemoryBase1, kMemorySize1, kMemoryValue1));
  thread_writer_1->SetStack(memory_writer_1.Pass());

  auto context_x86_writer_1 = make_scoped_ptr(new MinidumpContextX86Writer());
  InitializeMinidumpContextX86(context_x86_writer_1->context(), kSeed1);
  thread_writer_1->SetContext(context_x86_writer_1.Pass());

  thread_list_writer->AddThread(thread_writer_1.Pass());

  const uint32_t kThreadID2 = 3333333;
  const uint32_t kSuspendCount2 = 333333;
  const uint32_t kPriorityClass2 = 33333;
  const uint32_t kPriority2 = 3333;
  const uint64_t kTEB2 = 333;
  const uint64_t kMemoryBase2 = 0x3330;
  const size_t kMemorySize2 = 48;
  const uint8_t kMemoryValue2 = 33;
  const uint32_t kSeed2 = 3;

  auto thread_writer_2 = make_scoped_ptr(new MinidumpThreadWriter());
  thread_writer_2->SetThreadID(kThreadID2);
  thread_writer_2->SetSuspendCount(kSuspendCount2);
  thread_writer_2->SetPriorityClass(kPriorityClass2);
  thread_writer_2->SetPriority(kPriority2);
  thread_writer_2->SetTEB(kTEB2);

  auto memory_writer_2 = make_scoped_ptr(
      new TestMinidumpMemoryWriter(kMemoryBase2, kMemorySize2, kMemoryValue2));
  thread_writer_2->SetStack(memory_writer_2.Pass());

  auto context_x86_writer_2 = make_scoped_ptr(new MinidumpContextX86Writer());
  InitializeMinidumpContextX86(context_x86_writer_2->context(), kSeed2);
  thread_writer_2->SetContext(context_x86_writer_2.Pass());

  thread_list_writer->AddThread(thread_writer_2.Pass());

  minidump_file_writer.AddStream(thread_list_writer.Pass());
  minidump_file_writer.AddStream(memory_list_writer.Pass());

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + 2 * sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_LIST) + 3 * sizeof(MINIDUMP_THREAD) +
                sizeof(MINIDUMP_MEMORY_LIST) +
                3 * sizeof(MINIDUMP_MEMORY_DESCRIPTOR) +
                3 * sizeof(MinidumpContextX86) + kMemorySize0 + kMemorySize1 +
                kMemorySize2 + 12,  // 12 for alignment
            string_file.string().size());

  const MINIDUMP_THREAD_LIST* thread_list = nullptr;
  const MINIDUMP_MEMORY_LIST* memory_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadListStream(string_file.string(), &thread_list, &memory_list));

  EXPECT_EQ(3u, thread_list->NumberOfThreads);
  EXPECT_EQ(3u, memory_list->NumberOfMemoryRanges);

  {
    SCOPED_TRACE("thread 0");

    MINIDUMP_THREAD expected = {};
    expected.ThreadId = kThreadID0;
    expected.SuspendCount = kSuspendCount0;
    expected.PriorityClass = kPriorityClass0;
    expected.Priority = kPriority0;
    expected.Teb = kTEB0;
    expected.Stack.StartOfMemoryRange = kMemoryBase0;
    expected.Stack.Memory.DataSize = kMemorySize0;
    expected.ThreadContext.DataSize = sizeof(MinidumpContextX86);

    const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack = nullptr;
    const MinidumpContextX86* observed_context = nullptr;
    ASSERT_NO_FATAL_FAILURE(
        ExpectThread(&expected,
                     &thread_list->Threads[0],
                     string_file.string(),
                     &observed_stack,
                     reinterpret_cast<const void**>(&observed_context)));

    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpMemoryDescriptorAndContents(&expected.Stack,
                                                  observed_stack,
                                                  string_file.string(),
                                                  kMemoryValue0,
                                                  false));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpContextX86(kSeed0, observed_context, false));
    ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptor(
        observed_stack, &memory_list->MemoryRanges[0]));
  }

  {
    SCOPED_TRACE("thread 1");

    MINIDUMP_THREAD expected = {};
    expected.ThreadId = kThreadID1;
    expected.SuspendCount = kSuspendCount1;
    expected.PriorityClass = kPriorityClass1;
    expected.Priority = kPriority1;
    expected.Teb = kTEB1;
    expected.Stack.StartOfMemoryRange = kMemoryBase1;
    expected.Stack.Memory.DataSize = kMemorySize1;
    expected.ThreadContext.DataSize = sizeof(MinidumpContextX86);

    const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack = nullptr;
    const MinidumpContextX86* observed_context = nullptr;
    ASSERT_NO_FATAL_FAILURE(
        ExpectThread(&expected,
                     &thread_list->Threads[1],
                     string_file.string(),
                     &observed_stack,
                     reinterpret_cast<const void**>(&observed_context)));

    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpMemoryDescriptorAndContents(&expected.Stack,
                                                  observed_stack,
                                                  string_file.string(),
                                                  kMemoryValue1,
                                                  false));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpContextX86(kSeed1, observed_context, false));
    ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptor(
        observed_stack, &memory_list->MemoryRanges[1]));
  }

  {
    SCOPED_TRACE("thread 2");

    MINIDUMP_THREAD expected = {};
    expected.ThreadId = kThreadID2;
    expected.SuspendCount = kSuspendCount2;
    expected.PriorityClass = kPriorityClass2;
    expected.Priority = kPriority2;
    expected.Teb = kTEB2;
    expected.Stack.StartOfMemoryRange = kMemoryBase2;
    expected.Stack.Memory.DataSize = kMemorySize2;
    expected.ThreadContext.DataSize = sizeof(MinidumpContextX86);

    const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack = nullptr;
    const MinidumpContextX86* observed_context = nullptr;
    ASSERT_NO_FATAL_FAILURE(
        ExpectThread(&expected,
                     &thread_list->Threads[2],
                     string_file.string(),
                     &observed_stack,
                     reinterpret_cast<const void**>(&observed_context)));

    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpMemoryDescriptorAndContents(&expected.Stack,
                                                  observed_stack,
                                                  string_file.string(),
                                                  kMemoryValue2,
                                                  true));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpContextX86(kSeed2, observed_context, false));
    ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptor(
        observed_stack, &memory_list->MemoryRanges[2]));
  }
}

struct InitializeFromSnapshotX86Traits {
  using MinidumpContextType = MinidumpContextX86;
  static void InitializeCPUContext(CPUContext* context, uint32_t seed) {
    return InitializeCPUContextX86(context, seed);
  }
  static void ExpectMinidumpContext(
      uint32_t expect_seed, const MinidumpContextX86* observed, bool snapshot) {
    return ExpectMinidumpContextX86(expect_seed, observed, snapshot);
  }
};

struct InitializeFromSnapshotAMD64Traits {
  using MinidumpContextType = MinidumpContextAMD64;
  static void InitializeCPUContext(CPUContext* context, uint32_t seed) {
    return InitializeCPUContextX86_64(context, seed);
  }
  static void ExpectMinidumpContext(uint32_t expect_seed,
                                    const MinidumpContextAMD64* observed,
                                    bool snapshot) {
    return ExpectMinidumpContextAMD64(expect_seed, observed, snapshot);
  }
};

struct InitializeFromSnapshotNoContextTraits {
  using MinidumpContextType = MinidumpContextX86;
  static void InitializeCPUContext(CPUContext* context, uint32_t seed) {
    context->architecture = kCPUArchitectureUnknown;
  }
  static void ExpectMinidumpContext(uint32_t expect_seed,
                                    const MinidumpContextX86* observed,
                                    bool snapshot) {
    FAIL();
  }
};

template <typename Traits>
void RunInitializeFromSnapshotTest(bool thread_id_collision) {
  using MinidumpContextType = typename Traits::MinidumpContextType;
  MINIDUMP_THREAD expect_threads[3] = {};
  uint64_t thread_ids[arraysize(expect_threads)] = {};
  uint8_t memory_values[arraysize(expect_threads)] = {};
  uint32_t context_seeds[arraysize(expect_threads)] = {};

  expect_threads[0].ThreadId = 1;
  expect_threads[0].SuspendCount = 2;
  expect_threads[0].Priority = 3;
  expect_threads[0].Teb = 0x0123456789abcdef;
  expect_threads[0].Stack.StartOfMemoryRange = 0x1000;
  expect_threads[0].Stack.Memory.DataSize = 0x100;
  expect_threads[0].ThreadContext.DataSize = sizeof(MinidumpContextType);
  memory_values[0] = 'A';
  context_seeds[0] = 0x80000000;

  // The thread at index 1 has no stack.
  expect_threads[1].ThreadId = 11;
  expect_threads[1].SuspendCount = 12;
  expect_threads[1].Priority = 13;
  expect_threads[1].Teb = 0xfedcba9876543210;
  expect_threads[1].ThreadContext.DataSize = sizeof(MinidumpContextType);
  context_seeds[1] = 0x40000001;

  expect_threads[2].ThreadId = 21;
  expect_threads[2].SuspendCount = 22;
  expect_threads[2].Priority = 23;
  expect_threads[2].Teb = 0x1111111111111111;
  expect_threads[2].Stack.StartOfMemoryRange = 0x3000;
  expect_threads[2].Stack.Memory.DataSize = 0x300;
  expect_threads[2].ThreadContext.DataSize = sizeof(MinidumpContextType);
  memory_values[2] = 'd';
  context_seeds[2] = 0x20000002;

  if (thread_id_collision) {
    thread_ids[0] = 0x0123456700000001;
    thread_ids[1] = 0x89abcdef00000001;
    thread_ids[2] = 4;
    expect_threads[0].ThreadId = 0;
    expect_threads[1].ThreadId = 1;
    expect_threads[2].ThreadId = 2;
  } else {
    thread_ids[0] = 1;
    thread_ids[1] = 11;
    thread_ids[2] = 22;
    expect_threads[0].ThreadId = static_cast<uint32_t>(thread_ids[0]);
    expect_threads[1].ThreadId = static_cast<uint32_t>(thread_ids[1]);
    expect_threads[2].ThreadId = static_cast<uint32_t>(thread_ids[2]);
  }

  PointerVector<TestThreadSnapshot> thread_snapshots_owner;
  std::vector<const ThreadSnapshot*> thread_snapshots;
  for (size_t index = 0; index < arraysize(expect_threads); ++index) {
    TestThreadSnapshot* thread_snapshot = new TestThreadSnapshot();
    thread_snapshots_owner.push_back(thread_snapshot);

    thread_snapshot->SetThreadID(thread_ids[index]);
    thread_snapshot->SetSuspendCount(expect_threads[index].SuspendCount);
    thread_snapshot->SetPriority(expect_threads[index].Priority);
    thread_snapshot->SetThreadSpecificDataAddress(expect_threads[index].Teb);

    if (expect_threads[index].Stack.Memory.DataSize) {
      auto memory_snapshot = make_scoped_ptr(new TestMemorySnapshot());
      memory_snapshot->SetAddress(
          expect_threads[index].Stack.StartOfMemoryRange);
      memory_snapshot->SetSize(expect_threads[index].Stack.Memory.DataSize);
      memory_snapshot->SetValue(memory_values[index]);
      thread_snapshot->SetStack(memory_snapshot.Pass());
    }

    Traits::InitializeCPUContext(thread_snapshot->MutableContext(),
                                 context_seeds[index]);

    thread_snapshots.push_back(thread_snapshot);
  }

  auto thread_list_writer = make_scoped_ptr(new MinidumpThreadListWriter());
  auto memory_list_writer = make_scoped_ptr(new MinidumpMemoryListWriter());
  thread_list_writer->SetMemoryListWriter(memory_list_writer.get());
  MinidumpThreadIDMap thread_id_map;
  thread_list_writer->InitializeFromSnapshot(thread_snapshots, &thread_id_map);

  MinidumpFileWriter minidump_file_writer;
  minidump_file_writer.AddStream(thread_list_writer.Pass());
  minidump_file_writer.AddStream(memory_list_writer.Pass());

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_THREAD_LIST* thread_list = nullptr;
  const MINIDUMP_MEMORY_LIST* memory_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadListStream(string_file.string(), &thread_list, &memory_list));

  ASSERT_EQ(3u, thread_list->NumberOfThreads);
  ASSERT_EQ(2u, memory_list->NumberOfMemoryRanges);

  size_t memory_index = 0;
  for (size_t index = 0; index < thread_list->NumberOfThreads; ++index) {
    SCOPED_TRACE(base::StringPrintf("index %" PRIuS, index));

    const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack = nullptr;
    const MINIDUMP_MEMORY_DESCRIPTOR** observed_stack_p =
        expect_threads[index].Stack.Memory.DataSize ? &observed_stack : nullptr;
    const MinidumpContextType* observed_context = nullptr;
    ASSERT_NO_FATAL_FAILURE(
        ExpectThread(&expect_threads[index],
                     &thread_list->Threads[index],
                     string_file.string(),
                     observed_stack_p,
                     reinterpret_cast<const void**>(&observed_context)));

    ASSERT_NO_FATAL_FAILURE(Traits::ExpectMinidumpContext(
        context_seeds[index], observed_context, true));

    if (observed_stack_p) {
      ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptorAndContents(
          &expect_threads[index].Stack,
          observed_stack,
          string_file.string(),
          memory_values[index],
          index == thread_list->NumberOfThreads - 1));

      ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptor(
          observed_stack, &memory_list->MemoryRanges[memory_index]));

      ++memory_index;
    }
  }
}

TEST(MinidumpThreadWriter, InitializeFromSnapshot_x86) {
  RunInitializeFromSnapshotTest<InitializeFromSnapshotX86Traits>(false);
}

TEST(MinidumpThreadWriter, InitializeFromSnapshot_AMD64) {
  RunInitializeFromSnapshotTest<InitializeFromSnapshotAMD64Traits>(false);
}

TEST(MinidumpThreadWriter, InitializeFromSnapshot_ThreadIDCollision) {
  RunInitializeFromSnapshotTest<InitializeFromSnapshotX86Traits>(true);
}

TEST(MinidumpThreadWriterDeathTest, NoContext) {
  MinidumpFileWriter minidump_file_writer;
  auto thread_list_writer = make_scoped_ptr(new MinidumpThreadListWriter());

  auto thread_writer = make_scoped_ptr(new MinidumpThreadWriter());

  thread_list_writer->AddThread(thread_writer.Pass());
  minidump_file_writer.AddStream(thread_list_writer.Pass());

  StringFile string_file;
  ASSERT_DEATH_CHECK(minidump_file_writer.WriteEverything(&string_file),
                     "context_");
}

TEST(MinidumpThreadWriterDeathTest, InitializeFromSnapshot_NoContext) {
  ASSERT_DEATH_CHECK(
      RunInitializeFromSnapshotTest<InitializeFromSnapshotNoContextTraits>(
          false), "context_");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
