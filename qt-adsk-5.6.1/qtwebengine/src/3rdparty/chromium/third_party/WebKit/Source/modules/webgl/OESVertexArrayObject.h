/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef OESVertexArrayObject_h
#define OESVertexArrayObject_h

#include "modules/webgl/WebGLExtension.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class WebGLRenderingContextBase;
class WebGLVertexArrayObjectOES;

class OESVertexArrayObject final : public WebGLExtension {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<OESVertexArrayObject> create(WebGLRenderingContextBase*);
    static bool supported(WebGLRenderingContextBase*);
    static const char* extensionName();

    ~OESVertexArrayObject() override;
    WebGLExtensionName name() const override;

    PassRefPtrWillBeRawPtr<WebGLVertexArrayObjectOES> createVertexArrayOES();
    void deleteVertexArrayOES(WebGLVertexArrayObjectOES*);
    GLboolean isVertexArrayOES(WebGLVertexArrayObjectOES*);
    void bindVertexArrayOES(WebGLVertexArrayObjectOES*);

private:
    explicit OESVertexArrayObject(WebGLRenderingContextBase*);
};

} // namespace blink

#endif // OESVertexArrayObject_h
