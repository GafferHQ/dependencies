/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/animation/animatable/AnimatableValueTestHelper.h"

namespace blink {

bool operator==(const AnimatableValue& a, const AnimatableValue& b)
{
    return a.equals(b);
}

void PrintTo(const AnimatableClipPathOperation& animValue, ::std::ostream* os)
{
    *os << "AnimatableClipPathOperation@" << &animValue;
}

void PrintTo(const AnimatableColor& animColor, ::std::ostream* os)
{
    *os << "AnimatableColor("
        << animColor.color().serialized().utf8().data() << ", "
        << animColor.visitedLinkColor().serialized().utf8().data() << ")";
}

void PrintTo(const AnimatableImage& animImage, ::std::ostream* os)
{
    PrintTo(*(animImage.toCSSValue()), os, "AnimatableImage");
}

void PrintTo(const AnimatableNeutral& animValue, ::std::ostream* os)
{
    *os << "AnimatableNeutral@" << &animValue;
}

void PrintTo(const AnimatableRepeatable& animValue, ::std::ostream* os)
{
    *os << "AnimatableRepeatable(";

    const WillBeHeapVector<RefPtrWillBeMember<AnimatableValue>> v = animValue.values();
    for (WillBeHeapVector<RefPtrWillBeMember<AnimatableValue>>::const_iterator it = v.begin(); it != v.end(); ++it) {
        PrintTo(*(it->get()), os);
        if (it+1 != v.end())
            *os << ", ";
    }
    *os << ")";
}

void PrintTo(const AnimatableShapeValue& animValue, ::std::ostream* os)
{
    *os << "AnimatableShapeValue@" << &animValue;
}

void PrintTo(const AnimatableStrokeDasharrayList& animValue, ::std::ostream* os)
{
    *os << "AnimatableStrokeDasharrayList(";
    RefPtr<SVGDashArray> list = animValue.toSVGDashArray(1);
    size_t length = list->size();
    for (size_t i = 0; i < length; ++i) {
        const Length& dashLength = list->at(i);
        PixelsAndPercent pixelsAndPercent = dashLength.pixelsAndPercent();
        *os << pixelsAndPercent.pixels << '+';
        *os << pixelsAndPercent.percent << '%';

        if (i != length-1)
            *os << ", ";
    }
    *os << ")";
}

void PrintTo(const AnimatableTransform& animTransform, ::std::ostream* os)
{
    TransformOperations ops = animTransform.transformOperations();

    *os << "AnimatableTransform(";
    // FIXME: TransformOperations should really have it's own pretty-printer
    // then we could just call that.
    // FIXME: Output useful names not just the raw matrixes.
    for (unsigned i = 0; i < ops.size(); i++) {
        const TransformOperation* op = ops.at(i);

        TransformationMatrix matrix;
        op->apply(matrix, FloatSize(1.0, 1.0));

        *os << "[";
        if (matrix.isAffine()) {
            *os << matrix.a();
            *os << " " << matrix.b();
            *os << " " << matrix.c();
            *os << " " << matrix.d();
            *os << " " << matrix.e();
            *os << " " << matrix.f();
        } else {
            *os << matrix.m11();
            *os << " " << matrix.m12();
            *os << " " << matrix.m13();
            *os << " " << matrix.m14();
            *os << " ";
            *os << " " << matrix.m21();
            *os << " " << matrix.m22();
            *os << " " << matrix.m23();
            *os << " " << matrix.m24();
            *os << " ";
            *os << " " << matrix.m31();
            *os << " " << matrix.m32();
            *os << " " << matrix.m33();
            *os << " " << matrix.m34();
            *os << " ";
            *os << " " << matrix.m41();
            *os << " " << matrix.m42();
            *os << " " << matrix.m43();
            *os << " " << matrix.m44();
        }
        *os << "]";
        if (i < ops.size() - 1)
            *os << ", ";
    }
    *os << ")";
}

void PrintTo(const AnimatableUnknown& animUnknown, ::std::ostream* os)
{
    PrintTo(*(animUnknown.toCSSValue().get()), os, "AnimatableUnknown");
}

void PrintTo(const AnimatableVisibility& animVisibility, ::std::ostream* os)
{
    *os << "AnimatableVisibility(";
    switch (animVisibility.visibility()) {
    case VISIBLE:
        *os << "VISIBLE";
        break;
    case HIDDEN:
        *os << "HIDDEN";
        break;
    case COLLAPSE:
        *os << "COLLAPSE";
        break;
    default:
        *os << "Unknown Visbilility - update switch in AnimatableValueTestHelper.h";
    }
    *os << ")";
}

void PrintTo(const AnimatableValue& animValue, ::std::ostream* os)
{
    if (animValue.isClipPathOperation())
        PrintTo(toAnimatableClipPathOperation(animValue), os);
    else if (animValue.isColor())
        PrintTo(toAnimatableColor(animValue), os);
    else if (animValue.isImage())
        PrintTo(toAnimatableImage(animValue), os);
    else if (animValue.isNeutral())
        PrintTo(static_cast<const AnimatableNeutral&>(animValue), os);
    else if (animValue.isRepeatable())
        PrintTo(toAnimatableRepeatable(animValue), os);
    else if (animValue.isSVGPaint())
        PrintTo(toAnimatableSVGPaint(animValue), os);
    else if (animValue.isShapeValue())
        PrintTo(toAnimatableShapeValue(animValue), os);
    else if (animValue.isStrokeDasharrayList())
        PrintTo(toAnimatableStrokeDasharrayList(animValue), os);
    else if (animValue.isTransform())
        PrintTo(toAnimatableTransform(animValue), os);
    else if (animValue.isUnknown())
        PrintTo(toAnimatableUnknown(animValue), os);
    else if (animValue.isVisibility())
        PrintTo(toAnimatableVisibility(animValue), os);
    else
        *os << "Unknown AnimatableValue - update ifelse chain in AnimatableValueTestHelper.h";
}

} // namespace blink
