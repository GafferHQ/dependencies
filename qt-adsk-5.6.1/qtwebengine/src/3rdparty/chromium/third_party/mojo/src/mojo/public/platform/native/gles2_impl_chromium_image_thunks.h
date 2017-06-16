// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_PLATFORM_NATIVE_GLES2_IMPL_CHROMIUM_IMAGE_THUNKS_H_
#define MOJO_PUBLIC_PLATFORM_NATIVE_GLES2_IMPL_CHROMIUM_IMAGE_THUNKS_H_

#include <stddef.h>

#include "mojo/public/c/gles2/chromium_image.h"

// Specifies the frozen API for the GLES2 CHROMIUM_image extension.
#pragma pack(push, 8)
struct MojoGLES2ImplChromiumImageThunks {
  size_t size;  // Should be set to sizeof(*this).

#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) \
  ReturnType(*Function) PARAMETERS;
#include "mojo/public/c/gles2/gles2_call_visitor_chromium_image_autogen.h"
#undef VISIT_GL_CALL
};
#pragma pack(pop)

// Intended to be called from the embedder to get the embedder's implementation
// of GLES2.
inline MojoGLES2ImplChromiumImageThunks
MojoMakeGLES2ImplChromiumImageThunks() {
  MojoGLES2ImplChromiumImageThunks gles2_impl_chromium_image_thunks = {
      sizeof(MojoGLES2ImplChromiumImageThunks),
#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) gl##Function,
#include "mojo/public/c/gles2/gles2_call_visitor_chromium_image_autogen.h"
#undef VISIT_GL_CALL
  };

  return gles2_impl_chromium_image_thunks;
}

// Use this type for the function found by dynamically discovering it in
// a DSO linked with mojo_system.
// The contents of |gles2_impl_chromium_image_thunks| are copied.
typedef size_t (*MojoSetGLES2ImplChromiumImageThunksFn)(
    const MojoGLES2ImplChromiumImageThunks* thunks);

#endif  // MOJO_PUBLIC_PLATFORM_NATIVE_GLES2_IMPL_CHROMIUM_IMAGE_THUNKS_H_
