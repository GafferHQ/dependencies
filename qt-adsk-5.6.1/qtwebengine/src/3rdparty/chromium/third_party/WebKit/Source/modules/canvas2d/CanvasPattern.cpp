/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
#include "modules/canvas2d/CanvasPattern.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "wtf/text/WTFString.h"

namespace blink {

Pattern::RepeatMode CanvasPattern::parseRepetitionType(const String& type,
    ExceptionState& exceptionState)
{
    if (type.isEmpty() || type == "repeat")
        return Pattern::RepeatModeXY;

    if (type == "no-repeat")
        return Pattern::RepeatModeNone;

    if (type == "repeat-x")
        return Pattern::RepeatModeX;

    if (type == "repeat-y")
        return Pattern::RepeatModeY;

    exceptionState.throwDOMException(SyntaxError, "The provided type ('" + type + "') is not one of 'repeat', 'no-repeat', 'repeat-x', or 'repeat-y'.");
    return Pattern::RepeatModeNone;
}

CanvasPattern::CanvasPattern(PassRefPtr<Image> image, Pattern::RepeatMode repeat, bool originClean)
    : m_pattern(Pattern::createBitmapPattern(image, repeat))
    , m_originClean(originClean)
{
}

void CanvasPattern::setTransform(SVGMatrixTearOff* transform)
{
    pattern()->setPatternSpaceTransform(transform ? transform->value() : AffineTransform(1, 0, 0, 1, 0, 0));
}

}
