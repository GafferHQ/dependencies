// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTBlendMinMax_h
#define EXTBlendMinMax_h

#include "modules/webgl/WebGLExtension.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class EXTBlendMinMax final : public WebGLExtension {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<EXTBlendMinMax> create(WebGLRenderingContextBase*);
    static bool supported(WebGLRenderingContextBase*);
    static const char* extensionName();

    ~EXTBlendMinMax() override;
    WebGLExtensionName name() const override;

private:
    explicit EXTBlendMinMax(WebGLRenderingContextBase*);
};

} // namespace blink

#endif // EXTBlendMinMax_h
