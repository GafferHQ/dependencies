// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SUID_SETUID_SANDBOX_HOST_H_
#define SANDBOX_LINUX_SUID_SETUID_SANDBOX_HOST_H_

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/launch.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// Helper class to use the setuid sandbox. This class is to be used
// before launching the setuid helper.
// This class is difficult to use. It has been created by refactoring very old
// code scathered through the Chromium code base.
//
// A typical use for "A" launching a sandboxed process "B" would be:
// 1. A calls SetupLaunchEnvironment()
// 2. A sets up a base::CommandLine and then amends it with
//    PrependWrapper() (or manually, by relying on GetSandboxBinaryPath()).
// 3. A uses SetupLaunchOptions() to arrange for a dummy descriptor for the
//    setuid sandbox ABI.
// 4. A launches B with base::LaunchProcess, using the amended
// base::CommandLine.
// (The remaining steps are described within setuid_sandbox_client.h.)
class SANDBOX_EXPORT SetuidSandboxHost {
 public:
  // All instantation should go through this factory method.
  static SetuidSandboxHost* Create();
  ~SetuidSandboxHost();

  // The setuid sandbox may still be disabled via the environment.
  // This is tracked in crbug.com/245376.
  bool IsDisabledViaEnvironment();
  // Get the sandbox binary path. This method knows about the
  // CHROME_DEVEL_SANDBOX environment variable used for user-managed builds. If
  // the sandbox binary cannot be found, it will return an empty FilePath.
  base::FilePath GetSandboxBinaryPath();
  // Modify |cmd_line| to launch via the setuid sandbox. Crash if the setuid
  // sandbox binary cannot be found.  |cmd_line| must not be NULL.
  void PrependWrapper(base::CommandLine* cmd_line);
  // Set-up the launch options for launching via the setuid sandbox.  Caller is
  // responsible for keeping |dummy_fd| alive until LaunchProcess() completes.
  // |options| and |fds_to_remap| must not be NULL.
  // (Keeping |dummy_fd| alive is an unfortunate historical artifact of the
  // chrome-sandbox ABI.)
  void SetupLaunchOptions(base::LaunchOptions* options,
                          base::FileHandleMappingVector* fds_to_remap,
                          base::ScopedFD* dummy_fd);
  // Set-up the environment. This should be done prior to launching the setuid
  // helper.
  void SetupLaunchEnvironment();

 private:
  explicit SetuidSandboxHost(base::Environment* env);

  // Holds the environment. Will never be NULL.
  scoped_ptr<base::Environment> env_;

  DISALLOW_COPY_AND_ASSIGN(SetuidSandboxHost);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SUID_SETUID_SANDBOX_HOST_H_
