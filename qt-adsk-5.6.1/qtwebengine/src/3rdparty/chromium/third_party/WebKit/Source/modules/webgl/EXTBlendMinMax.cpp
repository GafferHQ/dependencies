// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "modules/webgl/EXTBlendMinMax.h"

namespace blink {

EXTBlendMinMax::EXTBlendMinMax(WebGLRenderingContextBase* context)
    : WebGLExtension(context)
{
    context->extensionsUtil()->ensureExtensionEnabled("GL_EXT_blend_minmax");
}

EXTBlendMinMax::~EXTBlendMinMax()
{
}

WebGLExtensionName EXTBlendMinMax::name() const
{
    return EXTBlendMinMaxName;
}

PassRefPtrWillBeRawPtr<EXTBlendMinMax> EXTBlendMinMax::create(WebGLRenderingContextBase* context)
{
    return adoptRefWillBeNoop(new EXTBlendMinMax(context));
}

bool EXTBlendMinMax::supported(WebGLRenderingContextBase* context)
{
    return context->extensionsUtil()->supportsExtension("GL_EXT_blend_minmax");
}

const char* EXTBlendMinMax::extensionName()
{
    return "EXT_blend_minmax";
}

} // namespace blink
