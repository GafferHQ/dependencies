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

#include "minidump/minidump_file_writer.h"

#include "base/logging.h"
#include "minidump/minidump_crashpad_info_writer.h"
#include "minidump/minidump_exception_writer.h"
#include "minidump/minidump_memory_writer.h"
#include "minidump/minidump_misc_info_writer.h"
#include "minidump/minidump_module_writer.h"
#include "minidump/minidump_system_info_writer.h"
#include "minidump/minidump_thread_id_map.h"
#include "minidump/minidump_thread_writer.h"
#include "minidump/minidump_writer_util.h"
#include "snapshot/process_snapshot.h"
#include "util/file/file_writer.h"
#include "util/numeric/safe_assignment.h"

namespace crashpad {

MinidumpFileWriter::MinidumpFileWriter()
    : MinidumpWritable(), header_(), streams_(), stream_types_() {
  // Don’t set the signature field right away. Leave it set to 0, so that a
  // partially-written minidump file isn’t confused for a complete and valid
  // one. The header will be rewritten in WriteToFile().
  header_.Signature = 0;

  header_.Version = MINIDUMP_VERSION;
  header_.CheckSum = 0;
  header_.Flags = MiniDumpNormal;
}

MinidumpFileWriter::~MinidumpFileWriter() {
}

void MinidumpFileWriter::InitializeFromSnapshot(
    const ProcessSnapshot* process_snapshot) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK_EQ(header_.Signature, 0u);
  DCHECK_EQ(header_.TimeDateStamp, 0u);
  DCHECK_EQ(header_.Flags, MiniDumpNormal);
  DCHECK(streams_.empty());

  // This time is truncated to an integer number of seconds, not rounded, for
  // compatibility with the truncation of process_snapshot->ProcessStartTime()
  // done by MinidumpMiscInfoWriter::InitializeFromSnapshot(). Handling both
  // timestamps in the same way allows the highest-fidelity computation of
  // process uptime as the difference between the two values.
  timeval snapshot_time;
  process_snapshot->SnapshotTime(&snapshot_time);
  SetTimestamp(snapshot_time.tv_sec);

  const SystemSnapshot* system_snapshot = process_snapshot->System();
  auto system_info = make_scoped_ptr(new MinidumpSystemInfoWriter());
  system_info->InitializeFromSnapshot(system_snapshot);
  AddStream(system_info.Pass());

  auto misc_info = make_scoped_ptr(new MinidumpMiscInfoWriter());
  misc_info->InitializeFromSnapshot(process_snapshot);
  AddStream(misc_info.Pass());

  auto memory_list = make_scoped_ptr(new MinidumpMemoryListWriter());
  auto thread_list = make_scoped_ptr(new MinidumpThreadListWriter());
  thread_list->SetMemoryListWriter(memory_list.get());
  MinidumpThreadIDMap thread_id_map;
  thread_list->InitializeFromSnapshot(process_snapshot->Threads(),
                                      &thread_id_map);
  AddStream(thread_list.Pass());

  const ExceptionSnapshot* exception_snapshot = process_snapshot->Exception();
  if (exception_snapshot) {
    auto exception = make_scoped_ptr(new MinidumpExceptionWriter());
    exception->InitializeFromSnapshot(exception_snapshot, thread_id_map);
    AddStream(exception.Pass());
  }

  auto module_list = make_scoped_ptr(new MinidumpModuleListWriter());
  module_list->InitializeFromSnapshot(process_snapshot->Modules());
  AddStream(module_list.Pass());

  auto crashpad_info = make_scoped_ptr(new MinidumpCrashpadInfoWriter());
  crashpad_info->InitializeFromSnapshot(process_snapshot);

  // Since the MinidumpCrashpadInfo stream is an extension, it’s safe to not add
  // it to the minidump file if it wouldn’t carry any useful information.
  if (crashpad_info->IsUseful()) {
    AddStream(crashpad_info.Pass());
  }

  AddStream(memory_list.Pass());
}

void MinidumpFileWriter::SetTimestamp(time_t timestamp) {
  DCHECK_EQ(state(), kStateMutable);

  internal::MinidumpWriterUtil::AssignTimeT(&header_.TimeDateStamp, timestamp);
}

void MinidumpFileWriter::AddStream(
    scoped_ptr<internal::MinidumpStreamWriter> stream) {
  DCHECK_EQ(state(), kStateMutable);

  MinidumpStreamType stream_type = stream->StreamType();

  auto rv = stream_types_.insert(stream_type);
  CHECK(rv.second) << "stream_type " << stream_type << " already present";

  streams_.push_back(stream.release());

  DCHECK_EQ(streams_.size(), stream_types_.size());
}

bool MinidumpFileWriter::WriteEverything(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateMutable);

  FileOffset start_offset = file_writer->Seek(0, SEEK_CUR);
  if (start_offset < 0) {
    return false;
  }

  if (!MinidumpWritable::WriteEverything(file_writer)) {
    return false;
  }

  FileOffset end_offset = file_writer->Seek(0, SEEK_CUR);
  if (end_offset < 0) {
    return false;
  }

  // Now that the entire minidump file has been completely written, go back to
  // the beginning and rewrite the header with the correct signature to identify
  // it as a valid minidump file.
  header_.Signature = MINIDUMP_SIGNATURE;

  if (file_writer->Seek(start_offset, SEEK_SET) != 0) {
    return false;
  }

  if (!file_writer->Write(&header_, sizeof(header_))) {
    return false;
  }

  // Seek back to the end of the file, in case some non-minidump content will be
  // written to the file after the minidump content.
  return file_writer->Seek(end_offset, SEEK_SET);
}

bool MinidumpFileWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);

  if (!MinidumpWritable::Freeze()) {
    return false;
  }

  size_t stream_count = streams_.size();
  CHECK_EQ(stream_count, stream_types_.size());

  if (!AssignIfInRange(&header_.NumberOfStreams, stream_count)) {
    LOG(ERROR) << "stream_count " << stream_count << " out of range";
    return false;
  }

  return true;
}

size_t MinidumpFileWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);
  DCHECK_EQ(streams_.size(), stream_types_.size());

  return sizeof(header_) + streams_.size() * sizeof(MINIDUMP_DIRECTORY);
}

std::vector<internal::MinidumpWritable*> MinidumpFileWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);
  DCHECK_EQ(streams_.size(), stream_types_.size());

  std::vector<MinidumpWritable*> children;
  for (internal::MinidumpStreamWriter* stream : streams_) {
    children.push_back(stream);
  }

  return children;
}

bool MinidumpFileWriter::WillWriteAtOffsetImpl(FileOffset offset) {
  DCHECK_EQ(state(), kStateFrozen);
  DCHECK_EQ(offset, 0);
  DCHECK_EQ(streams_.size(), stream_types_.size());

  auto directory_offset = streams_.empty() ? 0 : offset + sizeof(header_);
  if (!AssignIfInRange(&header_.StreamDirectoryRva, directory_offset)) {
    LOG(ERROR) << "offset " << directory_offset << " out of range";
    return false;
  }

  return MinidumpWritable::WillWriteAtOffsetImpl(offset);
}

bool MinidumpFileWriter::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);
  DCHECK_EQ(streams_.size(), stream_types_.size());

  WritableIoVec iov;
  iov.iov_base = &header_;
  iov.iov_len = sizeof(header_);
  std::vector<WritableIoVec> iovecs(1, iov);

  for (internal::MinidumpStreamWriter* stream : streams_) {
    iov.iov_base = stream->DirectoryListEntry();
    iov.iov_len = sizeof(MINIDUMP_DIRECTORY);
    iovecs.push_back(iov);
  }

  return file_writer->WriteIoVec(&iovecs);
}

}  // namespace crashpad
