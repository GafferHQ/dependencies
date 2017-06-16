//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Surface.h: Defines the egl::Surface class, representing a drawing surface
// such as the client area of a window, including any back buffers.
// Implements EGLSurface and related functionality. [EGL 1.4] section 2.2 page 3.

#ifndef LIBANGLE_SURFACE_H_
#define LIBANGLE_SURFACE_H_

#include <EGL/egl.h>

#include "common/angleutils.h"
#include "libANGLE/Error.h"
#include "libANGLE/RefCountObject.h"

namespace gl
{
class Texture;
}

namespace rx
{
class SurfaceImpl;
}

namespace egl
{
class AttributeMap;
class Display;
struct Config;

class Surface final : public RefCountObject
{
  public:
    Surface(rx::SurfaceImpl *impl, EGLint surfaceType, const egl::Config *config, const AttributeMap &attributes);

    rx::SurfaceImpl *getImplementation() { return mImplementation; }
    const rx::SurfaceImpl *getImplementation() const { return mImplementation; }

    EGLint getType() const;

    Error swap();
    Error postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height);
    Error querySurfacePointerANGLE(EGLint attribute, void **value);
    Error bindTexImage(gl::Texture *texture, EGLint buffer);
    Error releaseTexImage(EGLint buffer);

    EGLint isPostSubBufferSupported() const;

    void setSwapInterval(EGLint interval);

    const Config *getConfig() const;

    // width and height can change with client window resizing
    EGLint getWidth() const;
    EGLint getHeight() const;
    EGLint getPixelAspectRatio() const;
    EGLenum getRenderBuffer() const;
    EGLenum getSwapBehavior() const;
    EGLenum getTextureFormat() const;
    EGLenum getTextureTarget() const;

    gl::Texture *getBoundTexture() const { return mTexture; }

    EGLint isFixedSize() const;

  private:
    virtual ~Surface();

    rx::SurfaceImpl *mImplementation;

    EGLint mType;

    const egl::Config *mConfig;

    bool mPostSubBufferRequested;

    bool mFixedSize;
    size_t mFixedWidth;
    size_t mFixedHeight;

    EGLenum mTextureFormat;
    EGLenum mTextureTarget;

    EGLint mPixelAspectRatio;      // Display aspect ratio
    EGLenum mRenderBuffer;         // Render buffer
    EGLenum mSwapBehavior;         // Buffer swap behavior

    gl::Texture *mTexture;
};

}

#endif   // LIBANGLE_SURFACE_H_
