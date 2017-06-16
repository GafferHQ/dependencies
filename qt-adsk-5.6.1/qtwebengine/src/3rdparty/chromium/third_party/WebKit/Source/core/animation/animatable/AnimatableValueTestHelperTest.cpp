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

#include "core/layout/ClipPathOperation.h"
#include "core/style/BasicShapes.h"
#include "platform/transforms/ScaleTransformOperation.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>
#include <string>


namespace blink {

class AnimationAnimatableValueTestHelperTest : public ::testing::Test {
protected:
    ::std::string PrintToString(PassRefPtrWillBeRawPtr<AnimatableValue> animValue)
    {
        return ::testing::PrintToString(*animValue.get());
    }
};

TEST_F(AnimationAnimatableValueTestHelperTest, PrintTo)
{
    EXPECT_THAT(
        PrintToString(AnimatableClipPathOperation::create(ShapeClipPathOperation::create(BasicShapeCircle::create().get()).get())),
        testing::StartsWith("AnimatableClipPathOperation")
        );

    EXPECT_EQ(
        ::std::string("AnimatableColor(rgba(0, 0, 0, 0), #ff0000)"),
        PrintToString(AnimatableColor::create(Color(0x000000FF), Color(0xFFFF0000))));

    EXPECT_THAT(
        PrintToString(const_cast<AnimatableValue*>(AnimatableValue::neutralValue())),
        testing::StartsWith("AnimatableNeutral@"));

    EXPECT_THAT(
        PrintToString(AnimatableShapeValue::create(ShapeValue::createShapeValue(BasicShapeCircle::create().get(), ContentBox).get())),
        testing::StartsWith("AnimatableShapeValue@"));

    RefPtr<SVGDashArray> l2 = SVGDashArray::create();
    l2->append(Length(1, blink::Fixed));
    l2->append(Length(2, blink::Percent));
    EXPECT_EQ(
        ::std::string("AnimatableStrokeDasharrayList(1+0%, 0+2%)"),
        PrintToString(AnimatableStrokeDasharrayList::create(l2, 1)));

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, blink::Fixed), Length(0, blink::Fixed), TransformOperation::TranslateX));
    EXPECT_EQ(
        ::std::string("AnimatableTransform([1 0 0 1 2 0])"),
        PrintToString(AnimatableTransform::create(operations1)));

    TransformOperations operations2;
    operations2.operations().append(ScaleTransformOperation::create(1, 1, 1, TransformOperation::Scale3D));
    EXPECT_EQ(
        ::std::string("AnimatableTransform([1 0 0 1 0 0])"),
        PrintToString(AnimatableTransform::create(operations2)));

    EXPECT_EQ(
        ::std::string("AnimatableUnknown(none)"),
        PrintToString(AnimatableUnknown::create(CSSPrimitiveValue::createIdentifier(CSSValueNone).get())));

    EXPECT_EQ(
        ::std::string("AnimatableVisibility(VISIBLE)"),
        PrintToString(AnimatableVisibility::create(VISIBLE)));
}

} // namespace blink
