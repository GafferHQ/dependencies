// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_LINUX_BPF_UTILITY_POLICY_LINUX_H_
#define CONTENT_COMMON_SANDBOX_LINUX_BPF_UTILITY_POLICY_LINUX_H_

#include "content/common/sandbox_linux/sandbox_bpf_base_policy_linux.h"

namespace content {

// This policy can be used by utility processes.
class UtilityProcessPolicy : public SandboxBPFBasePolicy {
 public:
  UtilityProcessPolicy();
  ~UtilityProcessPolicy() override;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilityProcessPolicy);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_LINUX_BPF_UTILITY_POLICY_LINUX_H_
