// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SCOPED_CGL_H_
#define UI_GL_SCOPED_CGL_H_

#include <OpenGL/OpenGL.h>

#include "base/mac/scoped_typeref.h"
#include "ui/gl/gl_export.h"

namespace base {

template<>
struct ScopedTypeRefTraits<CGLContextObj> {
  static void Retain(CGLContextObj object) {
    CGLRetainContext(object);
  }
  static void Release(CGLContextObj object) {
    CGLReleaseContext(object);
  }
};

template<>
struct ScopedTypeRefTraits<CGLPixelFormatObj> {
  static void Retain(CGLPixelFormatObj object) {
    CGLRetainPixelFormat(object);
  }
  static void Release(CGLPixelFormatObj object) {
    CGLReleasePixelFormat(object);
  }
};

}  // namespace base

namespace gfx {

class GL_EXPORT ScopedCGLSetCurrentContext {
 public:
  explicit ScopedCGLSetCurrentContext(CGLContextObj context);
  ~ScopedCGLSetCurrentContext();
 private:
  // Note that if a context is destroyed when it is current, then the current
  // context is changed to NULL. Take out a reference on |previous_context_| to
  // preserve this behavior (when this falls out of scope, |previous_context_|
  // will be made current, then released, so NULL will be current if that
  // release destroys the context).
  base::ScopedTypeRef<CGLContextObj> previous_context_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCGLSetCurrentContext);
};

}  // namespace gfx

#endif  // UI_GL_SCOPED_CGL_H_
