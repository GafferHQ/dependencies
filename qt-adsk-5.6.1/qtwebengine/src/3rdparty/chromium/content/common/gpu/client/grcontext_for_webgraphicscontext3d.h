// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GRCONTEXT_FOR_WEBGRAPHICSCONTEXT3D_H_
#define CONTENT_COMMON_GPU_CLIENT_GRCONTEXT_FOR_WEBGRAPHICSCONTEXT3D_H_

#include "base/macros.h"
#include "skia/ext/refptr.h"

class GrContext;

namespace gpu_blink {
class WebGraphicsContext3DImpl;
}

namespace content {

// This class binds an offscreen GrContext to an offscreen context3d. The
// context3d is used by the GrContext so must be valid as long as this class
// is alive.
class GrContextForWebGraphicsContext3D {
 public:
  explicit GrContextForWebGraphicsContext3D(
      gpu_blink::WebGraphicsContext3DImpl* context3d);
  virtual ~GrContextForWebGraphicsContext3D();

  GrContext* get() { return gr_context_.get(); }

  void OnLostContext();
  void FreeGpuResources();

 private:
  skia::RefPtr<class GrContext> gr_context_;

  DISALLOW_COPY_AND_ASSIGN(GrContextForWebGraphicsContext3D);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GRCONTEXT_FOR_WEBGRAPHICSCONTEXT3D_H_
