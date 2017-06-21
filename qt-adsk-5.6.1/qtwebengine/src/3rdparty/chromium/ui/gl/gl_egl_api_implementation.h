// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_EGL_API_IMPLEMENTATION_H_
#define UI_GL_GL_EGL_API_IMPLEMENTATION_H_

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "gl_bindings.h"
#include "ui/gl/gl_export.h"

namespace base {
class CommandLine;
}
namespace gfx {

class GLContext;
struct GLWindowSystemBindingInfo;

void InitializeStaticGLBindingsEGL();
void InitializeDebugGLBindingsEGL();
void ClearGLBindingsEGL();
bool GetGLWindowSystemBindingInfoEGL(GLWindowSystemBindingInfo* info);

class GL_EXPORT EGLApiBase : public EGLApi {
 public:
  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_egl.h"

 protected:
  EGLApiBase();
  ~EGLApiBase() override;
  void InitializeBase(DriverEGL* driver);

  DriverEGL* driver_;
};

class GL_EXPORT RealEGLApi : public EGLApiBase {
 public:
  RealEGLApi();
  ~RealEGLApi() override;
  void Initialize(DriverEGL* driver);
  void InitializeWithCommandLine(DriverEGL* driver,
                                 base::CommandLine* command_line);

  const char* eglQueryStringFn(EGLDisplay dpy, EGLint name) override;

 private:
  // Filtered EGL_EXTENSIONS we return to eglQueryStringFn() calls.
  std::vector<std::string> disabled_exts_;
  std::map<EGLDisplay, std::string> filtered_exts_;
};


// Inserts a TRACE for every EGL call.
class GL_EXPORT TraceEGLApi : public EGLApi {
 public:
  TraceEGLApi(EGLApi* egl_api) : egl_api_(egl_api) { }
  ~TraceEGLApi() override;

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_egl.h"

 private:
  EGLApi* egl_api_;
};

}  // namespace gfx

#endif  // UI_GL_GL_EGL_API_IMPLEMENTATION_H_



