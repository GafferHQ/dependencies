// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLCompressedTextureETC1_h
#define WebGLCompressedTextureETC1_h

#include "modules/webgl/WebGLExtension.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class WebGLCompressedTextureETC1 final : public WebGLExtension {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<WebGLCompressedTextureETC1> create(WebGLRenderingContextBase*);
    static bool supported(WebGLRenderingContextBase*);
    static const char* extensionName();

    ~WebGLCompressedTextureETC1() override;
    WebGLExtensionName name() const override;

private:
    explicit WebGLCompressedTextureETC1(WebGLRenderingContextBase*);
};

} // namespace blink

#endif // WebGLCompressedTextureETC1_h
