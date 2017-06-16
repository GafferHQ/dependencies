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

#include "minidump/minidump_memory_writer.h"

#include <windows.h>
#include <dbghelp.h>
#include <stdint.h>

#include "base/basictypes.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "minidump/minidump_extensions.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/minidump_stream_writer.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_memory_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "snapshot/test/test_memory_snapshot.h"
#include "util/file/string_file.h"
#include "util/stdlib/pointer_container.h"

namespace crashpad {
namespace test {
namespace {

const MinidumpStreamType kBogusStreamType =
    static_cast<MinidumpStreamType>(1234);

// expected_streams is the expected number of streams in the file. The memory
// list must be the last stream. If there is another stream, it must come first,
// have stream type kBogusStreamType, and have zero-length data.
void GetMemoryListStream(const std::string& file_contents,
                         const MINIDUMP_MEMORY_LIST** memory_list,
                         const uint32_t expected_streams) {
  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kMemoryListStreamOffset =
      kDirectoryOffset + expected_streams * sizeof(MINIDUMP_DIRECTORY);
  const size_t kMemoryDescriptorsOffset =
      kMemoryListStreamOffset + sizeof(MINIDUMP_MEMORY_LIST);

  ASSERT_GE(file_contents.size(), kMemoryDescriptorsOffset);

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(file_contents, &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, expected_streams, 0));
  ASSERT_TRUE(directory);

  size_t directory_index = 0;
  if (expected_streams > 1) {
    ASSERT_EQ(kBogusStreamType, directory[directory_index].StreamType);
    ASSERT_EQ(0u, directory[directory_index].Location.DataSize);
    ASSERT_EQ(kMemoryListStreamOffset, directory[directory_index].Location.Rva);
    ++directory_index;
  }

  ASSERT_EQ(kMinidumpStreamTypeMemoryList,
            directory[directory_index].StreamType);
  EXPECT_EQ(kMemoryListStreamOffset, directory[directory_index].Location.Rva);

  *memory_list = MinidumpWritableAtLocationDescriptor<MINIDUMP_MEMORY_LIST>(
      file_contents, directory[directory_index].Location);
  ASSERT_TRUE(memory_list);
}

TEST(MinidumpMemoryWriter, EmptyMemoryList) {
  MinidumpFileWriter minidump_file_writer;
  auto memory_list_writer = make_scoped_ptr(new MinidumpMemoryListWriter());

  minidump_file_writer.AddStream(memory_list_writer.Pass());

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_MEMORY_LIST),
            string_file.string().size());

  const MINIDUMP_MEMORY_LIST* memory_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetMemoryListStream(string_file.string(), &memory_list, 1));

  EXPECT_EQ(0u, memory_list->NumberOfMemoryRanges);
}

TEST(MinidumpMemoryWriter, OneMemoryRegion) {
  MinidumpFileWriter minidump_file_writer;
  auto memory_list_writer = make_scoped_ptr(new MinidumpMemoryListWriter());

  const uint64_t kBaseAddress = 0xfedcba9876543210;
  const uint64_t kSize = 0x1000;
  const uint8_t kValue = 'm';

  auto memory_writer = make_scoped_ptr(
      new TestMinidumpMemoryWriter(kBaseAddress, kSize, kValue));
  memory_list_writer->AddMemory(memory_writer.Pass());

  minidump_file_writer.AddStream(memory_list_writer.Pass());

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MEMORY_LIST* memory_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetMemoryListStream(string_file.string(), &memory_list, 1));

  MINIDUMP_MEMORY_DESCRIPTOR expected;
  expected.StartOfMemoryRange = kBaseAddress;
  expected.Memory.DataSize = kSize;
  expected.Memory.Rva =
      sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
      sizeof(MINIDUMP_MEMORY_LIST) +
      memory_list->NumberOfMemoryRanges * sizeof(MINIDUMP_MEMORY_DESCRIPTOR);
  ExpectMinidumpMemoryDescriptorAndContents(&expected,
                                            &memory_list->MemoryRanges[0],
                                            string_file.string(),
                                            kValue,
                                            true);
}

TEST(MinidumpMemoryWriter, TwoMemoryRegions) {
  MinidumpFileWriter minidump_file_writer;
  auto memory_list_writer = make_scoped_ptr(new MinidumpMemoryListWriter());

  const uint64_t kBaseAddress0 = 0xc0ffee;
  const uint64_t kSize0 = 0x0100;
  const uint8_t kValue0 = '6';
  const uint64_t kBaseAddress1 = 0xfac00fac;
  const uint64_t kSize1 = 0x0200;
  const uint8_t kValue1 = '!';

  auto memory_writer_0 = make_scoped_ptr(
      new TestMinidumpMemoryWriter(kBaseAddress0, kSize0, kValue0));
  memory_list_writer->AddMemory(memory_writer_0.Pass());
  auto memory_writer_1 = make_scoped_ptr(
      new TestMinidumpMemoryWriter(kBaseAddress1, kSize1, kValue1));
  memory_list_writer->AddMemory(memory_writer_1.Pass());

  minidump_file_writer.AddStream(memory_list_writer.Pass());

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MEMORY_LIST* memory_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetMemoryListStream(string_file.string(), &memory_list, 1));

  EXPECT_EQ(2u, memory_list->NumberOfMemoryRanges);

  MINIDUMP_MEMORY_DESCRIPTOR expected;

  {
    SCOPED_TRACE("region 0");

    expected.StartOfMemoryRange = kBaseAddress0;
    expected.Memory.DataSize = kSize0;
    expected.Memory.Rva =
        sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
        sizeof(MINIDUMP_MEMORY_LIST) +
        memory_list->NumberOfMemoryRanges * sizeof(MINIDUMP_MEMORY_DESCRIPTOR);
    ExpectMinidumpMemoryDescriptorAndContents(&expected,
                                              &memory_list->MemoryRanges[0],
                                              string_file.string(),
                                              kValue0,
                                              false);
  }

  {
    SCOPED_TRACE("region 1");

    expected.StartOfMemoryRange = kBaseAddress1;
    expected.Memory.DataSize = kSize1;
    expected.Memory.Rva = memory_list->MemoryRanges[0].Memory.Rva +
                          memory_list->MemoryRanges[0].Memory.DataSize;
    ExpectMinidumpMemoryDescriptorAndContents(&expected,
                                              &memory_list->MemoryRanges[1],
                                              string_file.string(),
                                              kValue1,
                                              true);
  }
}

class TestMemoryStream final : public internal::MinidumpStreamWriter {
 public:
  TestMemoryStream(uint64_t base_address, size_t size, uint8_t value)
      : MinidumpStreamWriter(), memory_(base_address, size, value) {}

  ~TestMemoryStream() override {}

  TestMinidumpMemoryWriter* memory() {
    return &memory_;
  }

  // MinidumpStreamWriter:
  MinidumpStreamType StreamType() const override {
    return kBogusStreamType;
  }

 protected:
  // MinidumpWritable:
  size_t SizeOfObject() override {
    EXPECT_GE(state(), kStateFrozen);
    return 0;
  }

  std::vector<MinidumpWritable*> Children() override {
    EXPECT_GE(state(), kStateFrozen);
    std::vector<MinidumpWritable*> children(1, memory());
    return children;
  }

  bool WriteObject(FileWriterInterface* file_writer) override {
    EXPECT_EQ(kStateWritable, state());
    return true;
  }

 private:
  TestMinidumpMemoryWriter memory_;

  DISALLOW_COPY_AND_ASSIGN(TestMemoryStream);
};

TEST(MinidumpMemoryWriter, ExtraMemory) {
  // This tests MinidumpMemoryListWriter::AddExtraMemory(). That method adds
  // a MinidumpMemoryWriter to the MinidumpMemoryListWriter without making the
  // memory writer a child of the memory list writer.
  MinidumpFileWriter minidump_file_writer;

  const uint64_t kBaseAddress0 = 0x1000;
  const size_t kSize0 = 0x0400;
  const uint8_t kValue0 = '1';
  auto test_memory_stream =
      make_scoped_ptr(new TestMemoryStream(kBaseAddress0, kSize0, kValue0));

  auto memory_list_writer = make_scoped_ptr(new MinidumpMemoryListWriter());
  memory_list_writer->AddExtraMemory(test_memory_stream->memory());

  minidump_file_writer.AddStream(test_memory_stream.Pass());

  const uint64_t kBaseAddress1 = 0x2000;
  const size_t kSize1 = 0x0400;
  const uint8_t kValue1 = 'm';

  auto memory_writer = make_scoped_ptr(
      new TestMinidumpMemoryWriter(kBaseAddress1, kSize1, kValue1));
  memory_list_writer->AddMemory(memory_writer.Pass());

  minidump_file_writer.AddStream(memory_list_writer.Pass());

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MEMORY_LIST* memory_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetMemoryListStream(string_file.string(), &memory_list, 2));

  EXPECT_EQ(2u, memory_list->NumberOfMemoryRanges);

  MINIDUMP_MEMORY_DESCRIPTOR expected;

  {
    SCOPED_TRACE("region 0");

    expected.StartOfMemoryRange = kBaseAddress0;
    expected.Memory.DataSize = kSize0;
    expected.Memory.Rva =
        sizeof(MINIDUMP_HEADER) + 2 * sizeof(MINIDUMP_DIRECTORY) +
        sizeof(MINIDUMP_MEMORY_LIST) +
        memory_list->NumberOfMemoryRanges * sizeof(MINIDUMP_MEMORY_DESCRIPTOR);
    ExpectMinidumpMemoryDescriptorAndContents(&expected,
                                              &memory_list->MemoryRanges[0],
                                              string_file.string(),
                                              kValue0,
                                              false);
  }

  {
    SCOPED_TRACE("region 1");

    expected.StartOfMemoryRange = kBaseAddress1;
    expected.Memory.DataSize = kSize1;
    expected.Memory.Rva = memory_list->MemoryRanges[0].Memory.Rva +
                          memory_list->MemoryRanges[0].Memory.DataSize;
    ExpectMinidumpMemoryDescriptorAndContents(&expected,
                                              &memory_list->MemoryRanges[1],
                                              string_file.string(),
                                              kValue1,
                                              true);
  }
}

TEST(MinidumpMemoryWriter, AddFromSnapshot) {
  MINIDUMP_MEMORY_DESCRIPTOR expect_memory_descriptors[3] = {};
  uint8_t values[arraysize(expect_memory_descriptors)] = {};

  expect_memory_descriptors[0].StartOfMemoryRange = 0;
  expect_memory_descriptors[0].Memory.DataSize = 0x1000;
  values[0] = 0x01;

  expect_memory_descriptors[1].StartOfMemoryRange = 0x1000;
  expect_memory_descriptors[1].Memory.DataSize = 0x2000;
  values[1] = 0xf4;

  expect_memory_descriptors[2].StartOfMemoryRange = 0x7654321000000000;
  expect_memory_descriptors[2].Memory.DataSize = 0x800;
  values[2] = 0xa9;

  PointerVector<TestMemorySnapshot> memory_snapshots_owner;
  std::vector<const MemorySnapshot*> memory_snapshots;
  for (size_t index = 0;
       index < arraysize(expect_memory_descriptors);
       ++index) {
    TestMemorySnapshot* memory_snapshot = new TestMemorySnapshot();
    memory_snapshots_owner.push_back(memory_snapshot);
    memory_snapshot->SetAddress(
        expect_memory_descriptors[index].StartOfMemoryRange);
    memory_snapshot->SetSize(expect_memory_descriptors[index].Memory.DataSize);
    memory_snapshot->SetValue(values[index]);
    memory_snapshots.push_back(memory_snapshot);
  }

  auto memory_list_writer = make_scoped_ptr(new MinidumpMemoryListWriter());
  memory_list_writer->AddFromSnapshot(memory_snapshots);

  MinidumpFileWriter minidump_file_writer;
  minidump_file_writer.AddStream(memory_list_writer.Pass());

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MEMORY_LIST* memory_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetMemoryListStream(string_file.string(), &memory_list, 1));

  ASSERT_EQ(3u, memory_list->NumberOfMemoryRanges);

  for (size_t index = 0; index < memory_list->NumberOfMemoryRanges; ++index) {
    SCOPED_TRACE(base::StringPrintf("index %" PRIuS, index));
    ExpectMinidumpMemoryDescriptorAndContents(
        &expect_memory_descriptors[index],
        &memory_list->MemoryRanges[index],
        string_file.string(),
        values[index],
        index == memory_list->NumberOfMemoryRanges - 1);
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
