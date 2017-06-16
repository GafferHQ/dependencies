// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/DoubleStyleInterpolation.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/StylePropertySet.h"
#include "wtf/MathExtras.h"

#include <gtest/gtest.h>

namespace blink {

class AnimationDoubleStyleInterpolationTest : public ::testing::Test {
protected:
    static PassOwnPtrWillBeRawPtr<InterpolableValue> doubleToInterpolableValue(const CSSValue& value)
    {
        return DoubleStyleInterpolation::doubleToInterpolableValue(value);
    }

    static PassOwnPtrWillBeRawPtr<InterpolableValue> motionRotationToInterpolableValue(const CSSValue& value)
    {
        return DoubleStyleInterpolation::motionRotationToInterpolableValue(value);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToDouble(InterpolableValue* value, bool isNumber, InterpolationRange clamp)
    {
        return DoubleStyleInterpolation::interpolableValueToDouble(value, isNumber, clamp);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToMotionRotation(InterpolableValue* value, bool flag)
    {
        return DoubleStyleInterpolation::interpolableValueToMotionRotation(value, flag);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> roundTrip(PassRefPtrWillBeRawPtr<CSSValue> value)
    {
        return interpolableValueToDouble(doubleToInterpolableValue(*value).get(), toCSSPrimitiveValue(value.get())->primitiveType() == CSSPrimitiveValue::CSS_NUMBER, RangeAll);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> roundTripMotionRotation(PassRefPtrWillBeRawPtr<CSSValue> value, bool flag)
    {
        return interpolableValueToMotionRotation(motionRotationToInterpolableValue(*value).get(), flag);
    }

    static void testPrimitiveValue(RefPtrWillBeRawPtr<CSSValue> value, double doubleValue, CSSPrimitiveValue::UnitType unitType)
    {
        EXPECT_TRUE(value->isPrimitiveValue());
        EXPECT_EQ(doubleValue, toCSSPrimitiveValue(value.get())->getDoubleValue());
        EXPECT_EQ(unitType, toCSSPrimitiveValue(value.get())->primitiveType());
    }

    static void testValueListMotionRotation(RefPtrWillBeRawPtr<CSSValue> value, double doubleValue, bool flag)
    {
        EXPECT_TRUE(value->isValueList());
        const CSSValueList& list = toCSSValueList(*value);
        const CSSValue* item;
        if (flag) {
            EXPECT_EQ(2U, list.length());
            item = list.item(0);
            EXPECT_TRUE(item->isPrimitiveValue());
            EXPECT_EQ(CSSValueAuto, toCSSPrimitiveValue(item)->getValueID());
            item = list.item(1);
        } else {
            EXPECT_EQ(1U, list.length());
            item = list.item(0);
        }
        EXPECT_TRUE(item->isPrimitiveValue());
        EXPECT_EQ(doubleValue, toCSSPrimitiveValue(item)->getDoubleValue());
        EXPECT_EQ(CSSPrimitiveValue::CSS_DEG, toCSSPrimitiveValue(item)->primitiveType());
    }

    static InterpolableValue* getCachedValue(Interpolation& interpolation)
    {
        return interpolation.getCachedValueForTesting();
    }
};

TEST_F(AnimationDoubleStyleInterpolationTest, ZeroValue)
{
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    testPrimitiveValue(value, 0, CSSPrimitiveValue::CSS_NUMBER);
}

TEST_F(AnimationDoubleStyleInterpolationTest, AngleValue)
{
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_DEG));
    testPrimitiveValue(value, 10, CSSPrimitiveValue::CSS_DEG);

    value = roundTrip(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_RAD));
    testPrimitiveValue(value, rad2deg(10.0), CSSPrimitiveValue::CSS_DEG);

    value = roundTrip(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_GRAD));
    testPrimitiveValue(value, grad2deg(10.0), CSSPrimitiveValue::CSS_DEG);
}

TEST_F(AnimationDoubleStyleInterpolationTest, Clamping)
{
    RefPtrWillBeRawPtr<Interpolation> interpolableDouble = DoubleStyleInterpolation::create(
        *CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER),
        *CSSPrimitiveValue::create(0.6, CSSPrimitiveValue::CSS_NUMBER),
        CSSPropertyLineHeight,
        CSSPrimitiveValue::CSS_NUMBER,
        RangeAll);
    interpolableDouble->interpolate(0, 0.4);
    // progVal = start*(1-prog) + end*prog
    EXPECT_EQ(0.24, toInterpolableNumber(getCachedValue(*interpolableDouble))->value());
}

TEST_F(AnimationDoubleStyleInterpolationTest, ZeroValueFixedMotionRotation)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_DEG));
    RefPtrWillBeRawPtr<CSSValue> value = roundTripMotionRotation(list.release(), false);
    testValueListMotionRotation(value, 0, false);
}

TEST_F(AnimationDoubleStyleInterpolationTest, ZeroValueAutoMotionRotation)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(CSSPrimitiveValue::createIdentifier(CSSValueAuto));
    list->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_DEG));
    RefPtrWillBeRawPtr<CSSValue> value = roundTripMotionRotation(list.release(), true);
    testValueListMotionRotation(value, 0, true);
}

TEST_F(AnimationDoubleStyleInterpolationTest, ValueFixedMotionRotation)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(CSSPrimitiveValue::create(90, CSSPrimitiveValue::CSS_DEG));
    RefPtrWillBeRawPtr<CSSValue> value = roundTripMotionRotation(list.release(), false);
    testValueListMotionRotation(value, 90, false);
}

TEST_F(AnimationDoubleStyleInterpolationTest, ValueAutoMotionRotation)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(CSSPrimitiveValue::createIdentifier(CSSValueAuto));
    list->append(CSSPrimitiveValue::create(90, CSSPrimitiveValue::CSS_DEG));
    RefPtrWillBeRawPtr<CSSValue> value = roundTripMotionRotation(list.release(), true);
    testValueListMotionRotation(value, 90, true);
}

}
