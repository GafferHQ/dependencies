// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_DUMMY_MINIDUMP_GENERATOR_H_
#define CHROMECAST_CRASH_LINUX_DUMMY_MINIDUMP_GENERATOR_H_

#include <string>

#include "base/macros.h"
#include "chromecast/crash/linux/minidump_generator.h"

namespace chromecast {

class DummyMinidumpGenerator : public MinidumpGenerator {
 public:
  // A dummy minidump generator to move an existing minidump into
  // crash_uploader's monitoring path ($HOME/minidumps). The path is monitored
  // with file lock-control, so that third process should not write to it
  // directly.
  explicit DummyMinidumpGenerator(const std::string& existing_minidump_path);

  // MinidumpGenerator implementation:
  bool Generate(const std::string& minidump_path) override;

  // Provide access to internal utility for testing.
  bool CopyAndDeleteForTest(const std::string& dest_path) {
    return CopyAndDelete(dest_path);
  }

 private:
  // Copies the contents of the file at |existing_minidump_path_| to the file at
  // |dest_path|. If the copy operation succeeds, delete the file at
  // |existing_minidump_path_|. Returns false if |existing_minidump_path_| can't
  // be opened, or if the copy operation fails. Ideally, we would use Chrome
  // utilities for a task like this, but we must ensure that this operation can
  // occur on any thread (regardless of IO restrictions).
  bool CopyAndDelete(const std::string& dest_path);

  const std::string existing_minidump_path_;

  DISALLOW_COPY_AND_ASSIGN(DummyMinidumpGenerator);
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_DUMMY_MINIDUMP_GENERATOR_H_
