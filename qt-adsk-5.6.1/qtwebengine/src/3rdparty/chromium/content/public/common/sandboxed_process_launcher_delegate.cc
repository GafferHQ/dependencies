// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandboxed_process_launcher_delegate.h"

namespace content {

#if defined(OS_WIN)
bool SandboxedProcessLauncherDelegate::ShouldLaunchElevated() {
  return false;
}

bool SandboxedProcessLauncherDelegate::ShouldSandbox() {
  return true;
}

#elif(OS_POSIX)
bool SandboxedProcessLauncherDelegate::ShouldUseZygote() {
  return false;
}

base::EnvironmentMap SandboxedProcessLauncherDelegate::GetEnvironment() {
  return base::EnvironmentMap();
}
#endif

SandboxType SandboxedProcessLauncherDelegate::GetSandboxType() {
  return SANDBOX_TYPE_INVALID;
}

}  // namespace content
