//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderbufferD3d.h: Defines the RenderbufferD3D class which implements RenderbufferImpl.

#ifndef LIBANGLE_RENDERER_D3D_RENDERBUFFERD3D_H_
#define LIBANGLE_RENDERER_D3D_RENDERBUFFERD3D_H_

#include "angle_gl.h"

#include "common/angleutils.h"
#include "libANGLE/renderer/RenderbufferImpl.h"

namespace rx
{
class RendererD3D;
class RenderTargetD3D;
class SwapChainD3D;

class RenderbufferD3D : public RenderbufferImpl
{
  public:
    RenderbufferD3D(RendererD3D *renderer);
    virtual ~RenderbufferD3D();

    static RenderbufferD3D *makeRenderbufferD3D(RenderbufferImpl *renderbuffer);

    virtual gl::Error setStorage(GLenum internalformat, size_t width, size_t height) override;
    virtual gl::Error setStorageMultisample(size_t samples, GLenum internalformat, size_t width, size_t height) override;

    RenderTargetD3D *getRenderTarget();
    unsigned int getRenderTargetSerial() const;

  private:
    RendererD3D *mRenderer;
    RenderTargetD3D *mRenderTarget;
};
}

#endif // LIBANGLE_RENDERER_D3D_RENDERBUFFERD3D_H_
