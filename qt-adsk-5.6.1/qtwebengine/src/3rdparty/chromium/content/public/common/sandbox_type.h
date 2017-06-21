// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SANDBOX_TYPE_H_
#define CONTENT_PUBLIC_COMMON_SANDBOX_TYPE_H_

namespace content {

// Defines the sandbox types known within content. Embedders can add additional
// sandbox types with IDs starting with SANDBOX_TYPE_AFTER_LAST_TYPE.

enum SandboxType {
  // Not a valid sandbox type.
  SANDBOX_TYPE_INVALID = -1,

  SANDBOX_TYPE_FIRST_TYPE = 0,  // Placeholder to ease iteration.

  SANDBOX_TYPE_RENDERER = SANDBOX_TYPE_FIRST_TYPE,

  // Utility process is as restrictive as the worker process except full
  // access is allowed to one configurable directory.
  SANDBOX_TYPE_UTILITY,

  // GPU process.
  SANDBOX_TYPE_GPU,

  // The PPAPI plugin process.
  SANDBOX_TYPE_PPAPI,

  SANDBOX_TYPE_AFTER_LAST_TYPE,  // Placeholder to ease iteration.
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SANDBOX_TYPE_H_
