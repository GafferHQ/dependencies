// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/InterpolationEffect.h"

#include <gtest/gtest.h>

namespace blink {

namespace {

class SampleInterpolation : public Interpolation {
public:
    static PassRefPtrWillBeRawPtr<Interpolation> create(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end)
    {
        return adoptRefWillBeNoop(new SampleInterpolation(start, end));
    }

    virtual PropertyHandle property() const override
    {
        return PropertyHandle(CSSPropertyBackgroundColor);
    }
private:
    SampleInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end)
        : Interpolation(start, end)
    {
    }
};

const double duration = 1.0;

} // namespace

class AnimationInterpolationEffectTest : public ::testing::Test {
protected:
    InterpolableValue* interpolationValue(Interpolation& interpolation)
    {
        return interpolation.getCachedValueForTesting();
    }

    double getInterpolableNumber(PassRefPtrWillBeRawPtr<Interpolation> value)
    {
        return toInterpolableNumber(interpolationValue(*value.get()))->value();
    }
};

TEST_F(AnimationInterpolationEffectTest, SingleInterpolation)
{
    RefPtrWillBeRawPtr<InterpolationEffect> interpolationEffect = InterpolationEffect::create();
    interpolationEffect->addInterpolation(SampleInterpolation::create(InterpolableNumber::create(0), InterpolableNumber::create(10)),
        RefPtr<TimingFunction>(), 0, 1, -1, 2);

    OwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation>>> activeInterpolations = nullptr;
    interpolationEffect->getActiveInterpolations(-2, duration, activeInterpolations);
    EXPECT_EQ(0ul, activeInterpolations->size());

    interpolationEffect->getActiveInterpolations(-0.5, duration, activeInterpolations);
    EXPECT_EQ(1ul, activeInterpolations->size());
    EXPECT_EQ(-5, getInterpolableNumber(activeInterpolations->at(0)));

    interpolationEffect->getActiveInterpolations(0.5, duration, activeInterpolations);
    EXPECT_EQ(1ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(5, getInterpolableNumber(activeInterpolations->at(0)));

    interpolationEffect->getActiveInterpolations(1.5, duration, activeInterpolations);
    EXPECT_EQ(1ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(15, getInterpolableNumber(activeInterpolations->at(0)));

    interpolationEffect->getActiveInterpolations(3, duration, activeInterpolations);
    EXPECT_EQ(0ul, activeInterpolations->size());
}

TEST_F(AnimationInterpolationEffectTest, MultipleInterpolations)
{
    RefPtrWillBeRawPtr<InterpolationEffect> interpolationEffect = InterpolationEffect::create();
    interpolationEffect->addInterpolation(SampleInterpolation::create(InterpolableNumber::create(10), InterpolableNumber::create(15)),
        RefPtr<TimingFunction>(), 1, 2, 1, 3);
    interpolationEffect->addInterpolation(SampleInterpolation::create(InterpolableNumber::create(0), InterpolableNumber::create(1)),
        LinearTimingFunction::shared(), 0, 1, 0, 1);
    interpolationEffect->addInterpolation(SampleInterpolation::create(InterpolableNumber::create(1), InterpolableNumber::create(6)),
        CubicBezierTimingFunction::preset(CubicBezierTimingFunction::Ease), 0.5, 1.5, 0.5, 1.5);

    OwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation>>> activeInterpolations = nullptr;
    interpolationEffect->getActiveInterpolations(-0.5, duration, activeInterpolations);
    EXPECT_EQ(0ul, activeInterpolations->size());

    interpolationEffect->getActiveInterpolations(0, duration, activeInterpolations);
    EXPECT_EQ(1ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(0, getInterpolableNumber(activeInterpolations->at(0)));

    interpolationEffect->getActiveInterpolations(0.5, duration, activeInterpolations);
    EXPECT_EQ(2ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(0.5f, getInterpolableNumber(activeInterpolations->at(0)));
    EXPECT_FLOAT_EQ(1, getInterpolableNumber(activeInterpolations->at(1)));

    interpolationEffect->getActiveInterpolations(1, duration, activeInterpolations);
    EXPECT_EQ(2ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(10, getInterpolableNumber(activeInterpolations->at(0)));
    EXPECT_FLOAT_EQ(5.0282884f, getInterpolableNumber(activeInterpolations->at(1)));

    interpolationEffect->getActiveInterpolations(1, duration * 1000, activeInterpolations);
    EXPECT_EQ(2ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(10, getInterpolableNumber(activeInterpolations->at(0)));
    EXPECT_FLOAT_EQ(5.0120168f, getInterpolableNumber(activeInterpolations->at(1)));

    interpolationEffect->getActiveInterpolations(1.5, duration, activeInterpolations);
    EXPECT_EQ(1ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(12.5f, getInterpolableNumber(activeInterpolations->at(0)));

    interpolationEffect->getActiveInterpolations(2, duration, activeInterpolations);
    EXPECT_EQ(1ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(15, getInterpolableNumber(activeInterpolations->at(0)));
}

}
