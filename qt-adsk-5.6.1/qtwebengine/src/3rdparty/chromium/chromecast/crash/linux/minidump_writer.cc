// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/minidump_writer.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "chromecast/base/path_utils.h"
#include "chromecast/base/process_utils.h"
#include "chromecast/crash/linux/dump_info.h"
#include "chromecast/crash/linux/minidump_generator.h"

namespace chromecast {

namespace {

const char kDumpStateSuffix[] = ".txt.gz";

const int kDefaultDumpIntervalHours = 24;
const int kDefaultMaxDumps = 5;
const int kDefaultMaxRecentDumps = 5;

// Fork and run dumpstate, saving results to minidump_name + ".txt.gz".
int DumpState(const std::string& minidump_name) {
  std::vector<std::string> argv;
  argv.push_back(GetBinPathASCII("dumpstate").value());
  argv.push_back("-w");
  argv.push_back("crash-request");
  argv.push_back("-z");
  argv.push_back("-o");
  argv.push_back(
      minidump_name);  // dumpstate appends ".txt.gz" to the filename.

  std::string log;
  if (!chromecast::GetAppOutput(argv, &log)) {
    LOG(ERROR) << "failed to execute dumpstate";
    return -1;
  }
  return 0;
}

}  // namespace

MinidumpWriter::MinidumpWriter(MinidumpGenerator* minidump_generator,
                               const std::string& minidump_filename,
                               const MinidumpParams& params,
                               const DumpStateCallback& dump_state_cb)
    : minidump_generator_(minidump_generator),
      minidump_path_(minidump_filename),
      params_(params),
      max_dumps_(kDefaultMaxDumps),
      dump_interval_(base::TimeDelta::FromHours(kDefaultDumpIntervalHours)),
      max_recent_dumps_(kDefaultMaxRecentDumps),
      dump_state_cb_(dump_state_cb) {
}

MinidumpWriter::MinidumpWriter(MinidumpGenerator* minidump_generator,
                               const std::string& minidump_filename,
                               const MinidumpParams& params)
    : MinidumpWriter(minidump_generator,
                     minidump_filename,
                     params,
                     base::Bind(&DumpState)) {
}

MinidumpWriter::~MinidumpWriter() {
}

int MinidumpWriter::DoWork() {
  // If path is not absolute, append it to |dump_path_|.
  if (!minidump_path_.value().empty() && minidump_path_.value()[0] != '/')
    minidump_path_ = dump_path_.Append(minidump_path_);

  // The path should be a file in the |dump_path_| directory.
  if (dump_path_ != minidump_path_.DirName()) {
    LOG(INFO) << "The absolute path: " << minidump_path_.value() << " is not"
              << "in the correct directory: " << dump_path_.value();
    return -1;
  }

  // Query if we are able to write another minidump.
  if (!CanWriteDump()) {
    LOG(INFO) << "Skipping writing of dump due to limits";
    return -1;
  }

  // Generate a minidump at the specified |minidump_path_|.
  if (!minidump_generator_->Generate(minidump_path_.value())) {
    LOG(ERROR) << "Generate minidump failed " << minidump_path_.value();
    return -1;
  }

  // Run the dumpstate callback.
  DCHECK(!dump_state_cb_.is_null());
  if (dump_state_cb_.Run(minidump_path_.value()) < 0) {
    LOG(ERROR) << "DumpState callback failed.";
    return -1;
  }

  // Add this entry to the lockfile.
  const DumpInfo info(minidump_path_.value(),
                      minidump_path_.value() + kDumpStateSuffix,
                      time(NULL),
                      params_);
  if (AddEntryToLockFile(info) < 0) {
    LOG(ERROR) << "lockfile logging failed";
    return -1;
  }

  return 0;
}

bool MinidumpWriter::CanWriteDump() {
  const auto& dumps = GetDumpMetadata();

  // If no more dumps can be written, return false.
  if (static_cast<int>(dumps.size()) >= max_dumps_)
    return false;

  // If too many dumps have been written recently, return false.
  time_t cur_time = time(0);
  int recent_dumps = 0;
  for (auto dump : dumps) {
    if (difftime(cur_time, dump->dump_time()) <= dump_interval_.InSecondsF()) {
      if (++recent_dumps >= max_recent_dumps_)
        return false;
    }
  }

  return true;
}

}  // namespace crash_manager
