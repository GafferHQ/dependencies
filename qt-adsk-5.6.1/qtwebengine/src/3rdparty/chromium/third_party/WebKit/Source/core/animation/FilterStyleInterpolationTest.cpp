// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/FilterStyleInterpolation.h"

#include "core/css/CSSPrimitiveValue.h"

#include <gtest/gtest.h>

namespace blink {

class AnimationFilterStyleInterpolationTest : public ::testing::Test {
protected:
    static void roundTrip(const CSSValue& value)
    {
        CSSValueID functionType;
        ASSERT_TRUE(FilterStyleInterpolation::canCreateFrom(value, value));
        OwnPtrWillBeRawPtr<InterpolableValue> interpolableValue = FilterStyleInterpolation::toInterpolableValue(value, functionType);
        RefPtrWillBeRawPtr<CSSFunctionValue> result = FilterStyleInterpolation::fromInterpolableValue(*interpolableValue, functionType);
        ASSERT_TRUE(value.equals(*result));
    }
};

TEST_F(AnimationFilterStyleInterpolationTest, ZeroTest)
{
    RefPtrWillBeRawPtr<CSSFunctionValue> filter;
    filter = CSSFunctionValue::create(CSSValueBlur);
    filter->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    roundTrip(*filter);
}

TEST_F(AnimationFilterStyleInterpolationTest, SimpleTest)
{
    RefPtrWillBeRawPtr<CSSFunctionValue> filter;

    filter = CSSFunctionValue::create(CSSValueOpacity);
    filter->append(CSSPrimitiveValue::create(0.5, CSSPrimitiveValue::CSS_NUMBER));
    roundTrip(*filter);
}

}
