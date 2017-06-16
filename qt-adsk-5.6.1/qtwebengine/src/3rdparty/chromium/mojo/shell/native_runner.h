// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_NATIVE_RUNNER_H_
#define MOJO_SHELL_NATIVE_RUNNER_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/application/public/interfaces/application.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

#if defined(OS_WIN)
#undef DELETE
#endif

namespace base {
class FilePath;
}

namespace mojo {
namespace shell {

enum class NativeApplicationCleanup { DELETE, DONT_DELETE };

// ApplicationManager requires implementations of NativeRunner and
// NativeRunnerFactory to run native applications.
class NativeRunner {
 public:
  virtual ~NativeRunner() {}

  // Loads the app in the file at |app_path| and runs it on some other
  // thread/process. If |cleanup| is |DELETE|, this takes ownership of the file.
  // |app_completed_callback| is posted (to the thread on which |Start()| was
  // called) after |MojoMain()| completes.
  // TODO(vtl): |app_path| and |cleanup| should probably be moved to the
  // factory's Create(). Rationale: The factory may need information from the
  // file to decide what kind of NativeRunner to make.
  virtual void Start(const base::FilePath& app_path,
                     NativeApplicationCleanup cleanup,
                     InterfaceRequest<Application> application_request,
                     const base::Closure& app_completed_callback) = 0;
};

class NativeRunnerFactory {
 public:
  // Options for running the native app. (This will contain, e.g., information
  // about the sandbox profile, etc.)
  struct Options {
    // Constructs with default options.
    Options() : force_in_process(false) {}

    bool force_in_process;
  };

  virtual ~NativeRunnerFactory() {}
  virtual scoped_ptr<NativeRunner> Create(const Options& options) = 0;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_NATIVE_RUNNER_H_
