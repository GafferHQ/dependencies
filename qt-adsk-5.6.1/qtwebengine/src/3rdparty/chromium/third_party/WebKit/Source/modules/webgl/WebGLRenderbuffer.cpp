/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "modules/webgl/WebGLRenderbuffer.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

PassRefPtrWillBeRawPtr<WebGLRenderbuffer> WebGLRenderbuffer::create(WebGLRenderingContextBase* ctx)
{
    return adoptRefWillBeNoop(new WebGLRenderbuffer(ctx));
}

WebGLRenderbuffer::~WebGLRenderbuffer()
{
#if ENABLE(OILPAN)
    // This render buffer (heap) object must finalize itself.
    m_emulatedStencilBuffer.clear();
#endif
    // Always call detach here to ensure that platform object deletion
    // happens with Oilpan enabled. It keeps the code regular to do it
    // with or without Oilpan enabled.
    //
    // See comment in WebGLBuffer's destructor for additional
    // information on why this is done for WebGLSharedObject-derived
    // objects.
    detachAndDeleteObject();
}

WebGLRenderbuffer::WebGLRenderbuffer(WebGLRenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx)
    , m_internalFormat(GL_RGBA4)
    , m_width(0)
    , m_height(0)
    , m_hasEverBeenBound(false)
{
    setObject(ctx->webContext()->createRenderbuffer());
}

void WebGLRenderbuffer::deleteObjectImpl(WebGraphicsContext3D* context3d)
{
    context3d->deleteRenderbuffer(m_object);
    m_object = 0;
    deleteEmulatedStencilBuffer(context3d);
}

void WebGLRenderbuffer::deleteEmulatedStencilBuffer(WebGraphicsContext3D* context3d)
{
    if (!m_emulatedStencilBuffer)
        return;
    m_emulatedStencilBuffer->deleteObject(context3d);
    m_emulatedStencilBuffer.clear();
}

DEFINE_TRACE(WebGLRenderbuffer)
{
    visitor->trace(m_emulatedStencilBuffer);
    WebGLSharedPlatform3DObject::trace(visitor);
}

} // namespace blink
