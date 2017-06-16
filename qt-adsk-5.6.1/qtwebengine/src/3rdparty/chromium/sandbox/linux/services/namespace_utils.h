// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_NAMESPACE_UTILS_H_
#define SANDBOX_LINUX_SERVICES_NAMESPACE_UTILS_H_

#include <sys/types.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/template_util.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// Utility functions for using Linux namepaces.
class SANDBOX_EXPORT NamespaceUtils {
 public:
  COMPILE_ASSERT((base::is_same<uid_t, gid_t>::value), UidAndGidAreSameType);
  // generic_id_t can be used for either uid_t or gid_t.
  typedef uid_t generic_id_t;

  // Write a uid or gid mapping from |id| to |id| in |map_file|. This function
  // is async-signal-safe.
  static bool WriteToIdMapFile(const char* map_file,
                               generic_id_t id) WARN_UNUSED_RESULT;

  // Returns true if unprivileged namespaces of type |type| is supported
  // (meaning that both CLONE_NEWUSER and type are are supported).  |type| must
  // be one of CLONE_NEWIPC, CLONE_NEWNET, CLONE_NEWNS, CLONE_NEWPID,
  // CLONE_NEWUSER, or CLONE_NEWUTS. This relies on access to /proc, so it will
  // not work from within a sandbox.
  static bool KernelSupportsUnprivilegedNamespace(int type);

  // Returns true if the kernel supports denying setgroups in a user namespace.
  // On kernels where this is supported, DenySetgroups must be called before a
  // gid mapping can be added.
  static bool KernelSupportsDenySetgroups();

  // Disables setgroups() within the current user namespace. On Linux 3.18.2 and
  // later, this is required in order to write to /proc/self/gid_map without
  // having CAP_SETGID. Callers can determine whether is this needed with
  // KernelSupportsDenySetgroups. This function is async-signal-safe.
  static bool DenySetgroups() WARN_UNUSED_RESULT;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(NamespaceUtils);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_NAMESPACE_UTILS_H_
