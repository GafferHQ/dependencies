// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/bpf_utility_policy_linux.h"

#include <errno.h>

#include "base/basictypes.h"
#include "build/build_config.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

using sandbox::SyscallSets;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::ResultExpr;

namespace content {

UtilityProcessPolicy::UtilityProcessPolicy() {
}
UtilityProcessPolicy::~UtilityProcessPolicy() {
}

ResultExpr UtilityProcessPolicy::EvaluateSyscall(int sysno) const {
  // TODO(mdempsky): For now, this is just a copy of the renderer
  // policy, which happens to work well for utility processes too.  It
  // should be possible to limit further though.  In particular, the
  // entries below annotated with bug references are most likely
  // unnecessary.

  switch (sysno) {
    case __NR_ioctl:
      return sandbox::RestrictIoctl();
    // Allow the system calls below.
    case __NR_fdatasync:
    case __NR_fsync:
#if defined(__i386__) || defined(__x86_64__)
    case __NR_getrlimit:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_ugetrlimit:
#endif
    case __NR_pread64:
    case __NR_pwrite64:
    case __NR_sysinfo:
    case __NR_times:
    case __NR_uname:
      return Allow();
    default:
      // Default on the content baseline policy.
      return SandboxBPFBasePolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace content
