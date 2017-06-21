// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_X_UTIL_H_
#define CONTENT_COMMON_GPU_X_UTIL_H_

// Some X-Windows specific stuff. This can be included on any platform, and will
// be a NOP on non-Linux ones.

#include "build/build_config.h"
#include "content/common/gpu/gpu_config.h"

#if defined(USE_X11)

namespace content {

// Forward declares ------------------------------------------------------------
//
// X Windows headers do a lot of evil stuff, like "#define Status int" which
// will cause many problems when combined with our other header files (like
// ones that define a class local enum called "Status."
//
// These definitions are not Kosher, but allow us to remove this dependency and
// actually compile X at all.

typedef unsigned long XID;

extern "C" {

typedef struct _XDisplay Display;
typedef struct __GLXcontextRec *GLXContext;

}  // extern "C"

}  // namespace content

#endif  // USE_X11

#endif  // CONTENT_COMMON_GPU_X_UTIL_H_
