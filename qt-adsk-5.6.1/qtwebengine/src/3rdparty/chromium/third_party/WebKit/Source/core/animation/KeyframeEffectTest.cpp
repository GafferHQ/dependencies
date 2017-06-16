// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/KeyframeEffect.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/UnionTypesCore.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8KeyframeEffectOptions.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/AnimationEffectTiming.h"
#include "core/animation/AnimationTestHelper.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/Timing.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/testing/DummyPageHolder.h"
#include <gtest/gtest.h>
#include <v8.h>

namespace blink {

class KeyframeEffectTest : public ::testing::Test {
protected:
    KeyframeEffectTest()
        : pageHolder(DummyPageHolder::create())
        , document(pageHolder->document())
        , element(document.createElement("foo", ASSERT_NO_EXCEPTION))
    {
        document.animationClock().resetTimeForTesting(document.timeline().zeroTime());
        document.documentElement()->appendChild(element.get());
        EXPECT_EQ(0, document.timeline().currentTime());
    }

    OwnPtr<DummyPageHolder> pageHolder;
    Document& document;
    RefPtrWillBePersistent<Element> element;
    TrackExceptionState exceptionState;
};

class AnimationKeyframeEffectV8Test : public KeyframeEffectTest {
protected:
    AnimationKeyframeEffectV8Test()
        : m_isolate(v8::Isolate::GetCurrent())
        , m_scope(m_isolate)
    {
    }

    template<typename T>
    static PassRefPtrWillBeRawPtr<KeyframeEffect> createAnimation(Element* element, Vector<Dictionary> keyframeDictionaryVector, T timingInput, ExceptionState& exceptionState)
    {
        return KeyframeEffect::create(element, keyframeDictionaryVector, timingInput, exceptionState);
    }
    static PassRefPtrWillBeRawPtr<KeyframeEffect> createAnimation(Element* element, Vector<Dictionary> keyframeDictionaryVector, ExceptionState& exceptionState)
    {
        return KeyframeEffect::create(element, keyframeDictionaryVector, exceptionState);
    }

    v8::Isolate* m_isolate;

private:
    V8TestingScope m_scope;
};

TEST_F(AnimationKeyframeEffectV8Test, CanCreateAnAnimation)
{
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(m_isolate);
    v8::Local<v8::Object> keyframe2 = v8::Object::New(m_isolate);

    setV8ObjectPropertyAsString(m_isolate, keyframe1, "width", "100px");
    setV8ObjectPropertyAsString(m_isolate, keyframe1, "offset", "0");
    setV8ObjectPropertyAsString(m_isolate, keyframe1, "easing", "ease-in-out");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "width", "0px");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "offset", "1");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "easing", "cubic-bezier(1, 1, 0.3, 0.3)");

    jsKeyframes.append(Dictionary(keyframe1, m_isolate, exceptionState));
    jsKeyframes.append(Dictionary(keyframe2, m_isolate, exceptionState));

    String value1;
    ASSERT_TRUE(DictionaryHelper::get(jsKeyframes[0], "width", value1));
    ASSERT_EQ("100px", value1);

    String value2;
    ASSERT_TRUE(DictionaryHelper::get(jsKeyframes[1], "width", value2));
    ASSERT_EQ("0px", value2);

    RefPtrWillBeRawPtr<KeyframeEffect> animation = createAnimation(element.get(), jsKeyframes, 0, exceptionState);

    Element* target = animation->target();
    EXPECT_EQ(*element.get(), *target);

    const KeyframeVector keyframes = toKeyframeEffectModelBase(animation->model())->getFrames();

    EXPECT_EQ(0, keyframes[0]->offset());
    EXPECT_EQ(1, keyframes[1]->offset());

    const CSSValue* keyframe1Width = toStringKeyframe(keyframes[0].get())->cssPropertyValue(CSSPropertyWidth);
    const CSSValue* keyframe2Width = toStringKeyframe(keyframes[1].get())->cssPropertyValue(CSSPropertyWidth);
    ASSERT(keyframe1Width);
    ASSERT(keyframe2Width);

    EXPECT_EQ("100px", keyframe1Width->cssText());
    EXPECT_EQ("0px", keyframe2Width->cssText());

    EXPECT_EQ(*(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseInOut)), keyframes[0]->easing());
    EXPECT_EQ(*(CubicBezierTimingFunction::create(1, 1, 0.3, 0.3).get()), keyframes[1]->easing());
}

TEST_F(AnimationKeyframeEffectV8Test, CanSetDuration)
{
    Vector<Dictionary, 0> jsKeyframes;
    double duration = 2000;

    RefPtrWillBeRawPtr<KeyframeEffect> animation = createAnimation(element.get(), jsKeyframes, duration, exceptionState);

    EXPECT_EQ(duration / 1000, animation->specifiedTiming().iterationDuration);
}

TEST_F(AnimationKeyframeEffectV8Test, CanOmitSpecifiedDuration)
{
    Vector<Dictionary, 0> jsKeyframes;
    RefPtrWillBeRawPtr<KeyframeEffect> animation = createAnimation(element.get(), jsKeyframes, exceptionState);
    EXPECT_TRUE(std::isnan(animation->specifiedTiming().iterationDuration));
}

TEST_F(AnimationKeyframeEffectV8Test, NegativeDurationIsAuto)
{
    Vector<Dictionary, 0> jsKeyframes;
    RefPtrWillBeRawPtr<KeyframeEffect> animation = createAnimation(element.get(), jsKeyframes, -2, exceptionState);
    EXPECT_TRUE(std::isnan(animation->specifiedTiming().iterationDuration));
}

TEST_F(AnimationKeyframeEffectV8Test, MismatchedKeyframePropertyRaisesException)
{
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(m_isolate);
    v8::Local<v8::Object> keyframe2 = v8::Object::New(m_isolate);

    setV8ObjectPropertyAsString(m_isolate, keyframe1, "width", "100px");
    setV8ObjectPropertyAsString(m_isolate, keyframe1, "offset", "0");

    // Height property appears only in keyframe2
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "height", "100px");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "width", "0px");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "offset", "1");

    jsKeyframes.append(Dictionary(keyframe1, m_isolate, exceptionState));
    jsKeyframes.append(Dictionary(keyframe2, m_isolate, exceptionState));

    createAnimation(element.get(), jsKeyframes, 0, exceptionState);

    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_EQ(NotSupportedError, exceptionState.code());
}

TEST_F(AnimationKeyframeEffectV8Test, MissingOffsetZeroRaisesException)
{
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(m_isolate);
    v8::Local<v8::Object> keyframe2 = v8::Object::New(m_isolate);

    setV8ObjectPropertyAsString(m_isolate, keyframe1, "width", "100px");
    setV8ObjectPropertyAsString(m_isolate, keyframe1, "offset", "0.1");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "width", "0px");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "offset", "1");

    jsKeyframes.append(Dictionary(keyframe1, m_isolate, exceptionState));
    jsKeyframes.append(Dictionary(keyframe2, m_isolate, exceptionState));

    createAnimation(element.get(), jsKeyframes, 0, exceptionState);

    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_EQ(NotSupportedError, exceptionState.code());
}

TEST_F(AnimationKeyframeEffectV8Test, MissingOffsetOneRaisesException)
{
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(m_isolate);
    v8::Local<v8::Object> keyframe2 = v8::Object::New(m_isolate);

    setV8ObjectPropertyAsString(m_isolate, keyframe1, "width", "100px");
    setV8ObjectPropertyAsString(m_isolate, keyframe1, "offset", "0");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "width", "0px");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "offset", "0.1");

    jsKeyframes.append(Dictionary(keyframe1, m_isolate, exceptionState));
    jsKeyframes.append(Dictionary(keyframe2, m_isolate, exceptionState));

    createAnimation(element.get(), jsKeyframes, 0, exceptionState);

    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_EQ(NotSupportedError, exceptionState.code());
}

TEST_F(AnimationKeyframeEffectV8Test, MissingOffsetZeroAndOneRaisesException)
{
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(m_isolate);
    v8::Local<v8::Object> keyframe2 = v8::Object::New(m_isolate);

    setV8ObjectPropertyAsString(m_isolate, keyframe1, "width", "100px");
    setV8ObjectPropertyAsString(m_isolate, keyframe1, "offset", "0.1");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "width", "0px");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "offset", "0.2");

    jsKeyframes.append(Dictionary(keyframe1, m_isolate, exceptionState));
    jsKeyframes.append(Dictionary(keyframe2, m_isolate, exceptionState));

    createAnimation(element.get(), jsKeyframes, 0, exceptionState);

    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_EQ(NotSupportedError, exceptionState.code());
}

TEST_F(AnimationKeyframeEffectV8Test, SpecifiedGetters)
{
    Vector<Dictionary, 0> jsKeyframes;

    v8::Local<v8::Object> timingInput = v8::Object::New(m_isolate);
    setV8ObjectPropertyAsNumber(m_isolate, timingInput, "delay", 2);
    setV8ObjectPropertyAsNumber(m_isolate, timingInput, "endDelay", 0.5);
    setV8ObjectPropertyAsString(m_isolate, timingInput, "fill", "backwards");
    setV8ObjectPropertyAsNumber(m_isolate, timingInput, "iterationStart", 2);
    setV8ObjectPropertyAsNumber(m_isolate, timingInput, "iterations", 10);
    setV8ObjectPropertyAsNumber(m_isolate, timingInput, "playbackRate", 2);
    setV8ObjectPropertyAsString(m_isolate, timingInput, "direction", "reverse");
    setV8ObjectPropertyAsString(m_isolate, timingInput, "easing", "step-start");
    KeyframeEffectOptions timingInputDictionary;
    V8KeyframeEffectOptions::toImpl(m_isolate, timingInput, timingInputDictionary, exceptionState);

    RefPtrWillBeRawPtr<KeyframeEffect> animation = createAnimation(element.get(), jsKeyframes, timingInputDictionary, exceptionState);

    RefPtrWillBeRawPtr<AnimationEffectTiming> specified = animation->timing();
    EXPECT_EQ(2, specified->delay());
    EXPECT_EQ(0.5, specified->endDelay());
    EXPECT_EQ("backwards", specified->fill());
    EXPECT_EQ(2, specified->iterationStart());
    EXPECT_EQ(10, specified->iterations());
    EXPECT_EQ(2, specified->playbackRate());
    EXPECT_EQ("reverse", specified->direction());
    EXPECT_EQ("step-start", specified->easing());
}

TEST_F(AnimationKeyframeEffectV8Test, SpecifiedDurationGetter)
{
    Vector<Dictionary, 0> jsKeyframes;

    v8::Local<v8::Object> timingInputWithDuration = v8::Object::New(m_isolate);
    setV8ObjectPropertyAsNumber(m_isolate, timingInputWithDuration, "duration", 2.5);
    KeyframeEffectOptions timingInputDictionaryWithDuration;
    V8KeyframeEffectOptions::toImpl(m_isolate, timingInputWithDuration, timingInputDictionaryWithDuration, exceptionState);

    RefPtrWillBeRawPtr<KeyframeEffect> animationWithDuration = createAnimation(element.get(), jsKeyframes, timingInputDictionaryWithDuration, exceptionState);

    RefPtrWillBeRawPtr<AnimationEffectTiming> specifiedWithDuration = animationWithDuration->timing();
    UnrestrictedDoubleOrString duration;
    specifiedWithDuration->duration(duration);
    EXPECT_TRUE(duration.isUnrestrictedDouble());
    EXPECT_EQ(2.5, duration.getAsUnrestrictedDouble());
    EXPECT_FALSE(duration.isString());


    v8::Local<v8::Object> timingInputNoDuration = v8::Object::New(m_isolate);
    KeyframeEffectOptions timingInputDictionaryNoDuration;
    V8KeyframeEffectOptions::toImpl(m_isolate, timingInputNoDuration, timingInputDictionaryNoDuration, exceptionState);

    RefPtrWillBeRawPtr<KeyframeEffect> animationNoDuration = createAnimation(element.get(), jsKeyframes, timingInputDictionaryNoDuration, exceptionState);

    RefPtrWillBeRawPtr<AnimationEffectTiming> specifiedNoDuration = animationNoDuration->timing();
    UnrestrictedDoubleOrString duration2;
    specifiedNoDuration->duration(duration2);
    EXPECT_FALSE(duration2.isUnrestrictedDouble());
    EXPECT_TRUE(duration2.isString());
    EXPECT_EQ("auto", duration2.getAsString());
}

TEST_F(AnimationKeyframeEffectV8Test, SpecifiedSetters)
{
    Vector<Dictionary, 0> jsKeyframes;
    v8::Local<v8::Object> timingInput = v8::Object::New(m_isolate);
    KeyframeEffectOptions timingInputDictionary;
    V8KeyframeEffectOptions::toImpl(m_isolate, timingInput, timingInputDictionary, exceptionState);
    RefPtrWillBeRawPtr<KeyframeEffect> animation = createAnimation(element.get(), jsKeyframes, timingInputDictionary, exceptionState);

    RefPtrWillBeRawPtr<AnimationEffectTiming> specified = animation->timing();

    EXPECT_EQ(0, specified->delay());
    specified->setDelay(2);
    EXPECT_EQ(2, specified->delay());

    EXPECT_EQ(0, specified->endDelay());
    specified->setEndDelay(0.5);
    EXPECT_EQ(0.5, specified->endDelay());

    EXPECT_EQ("auto", specified->fill());
    specified->setFill("backwards");
    EXPECT_EQ("backwards", specified->fill());

    EXPECT_EQ(0, specified->iterationStart());
    specified->setIterationStart(2);
    EXPECT_EQ(2, specified->iterationStart());

    EXPECT_EQ(1, specified->iterations());
    specified->setIterations(10);
    EXPECT_EQ(10, specified->iterations());

    EXPECT_EQ(1, specified->playbackRate());
    specified->setPlaybackRate(2);
    EXPECT_EQ(2, specified->playbackRate());

    EXPECT_EQ("normal", specified->direction());
    specified->setDirection("reverse");
    EXPECT_EQ("reverse", specified->direction());

    EXPECT_EQ("linear", specified->easing());
    specified->setEasing("step-start");
    EXPECT_EQ("step-start", specified->easing());
}

TEST_F(AnimationKeyframeEffectV8Test, SetSpecifiedDuration)
{
    Vector<Dictionary, 0> jsKeyframes;
    v8::Local<v8::Object> timingInput = v8::Object::New(m_isolate);
    KeyframeEffectOptions timingInputDictionary;
    V8KeyframeEffectOptions::toImpl(m_isolate, timingInput, timingInputDictionary, exceptionState);
    RefPtrWillBeRawPtr<KeyframeEffect> animation = createAnimation(element.get(), jsKeyframes, timingInputDictionary, exceptionState);

    RefPtrWillBeRawPtr<AnimationEffectTiming> specified = animation->timing();

    UnrestrictedDoubleOrString duration;
    specified->duration(duration);
    EXPECT_FALSE(duration.isUnrestrictedDouble());
    EXPECT_TRUE(duration.isString());
    EXPECT_EQ("auto", duration.getAsString());

    UnrestrictedDoubleOrString inDuration;
    inDuration.setUnrestrictedDouble(2.5);
    specified->setDuration(inDuration);
    UnrestrictedDoubleOrString duration2;
    specified->duration(duration2);
    EXPECT_TRUE(duration2.isUnrestrictedDouble());
    EXPECT_EQ(2.5, duration2.getAsUnrestrictedDouble());
    EXPECT_FALSE(duration2.isString());
}

TEST_F(KeyframeEffectTest, TimeToEffectChange)
{
    Timing timing;
    timing.iterationDuration = 100;
    timing.startDelay = 100;
    timing.endDelay = 100;
    timing.fillMode = Timing::FillModeNone;
    RefPtrWillBeRawPtr<KeyframeEffect> animation = KeyframeEffect::create(0, nullptr, timing);
    RefPtrWillBeRawPtr<Animation> player = document.timeline().play(animation.get());
    double inf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(100, animation->timeToForwardsEffectChange());
    EXPECT_EQ(inf, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(100);
    EXPECT_EQ(100, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(199);
    EXPECT_EQ(1, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(200);
    // End-exclusive.
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(300);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(100, animation->timeToReverseEffectChange());
}

TEST_F(KeyframeEffectTest, TimeToEffectChangeWithPlaybackRate)
{
    Timing timing;
    timing.iterationDuration = 100;
    timing.startDelay = 100;
    timing.endDelay = 100;
    timing.playbackRate = 2;
    timing.fillMode = Timing::FillModeNone;
    RefPtrWillBeRawPtr<KeyframeEffect> animation = KeyframeEffect::create(0, nullptr, timing);
    RefPtrWillBeRawPtr<Animation> player = document.timeline().play(animation.get());
    double inf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(100, animation->timeToForwardsEffectChange());
    EXPECT_EQ(inf, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(100);
    EXPECT_EQ(50, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(149);
    EXPECT_EQ(1, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(150);
    // End-exclusive.
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(200);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(50, animation->timeToReverseEffectChange());
}

TEST_F(KeyframeEffectTest, TimeToEffectChangeWithNegativePlaybackRate)
{
    Timing timing;
    timing.iterationDuration = 100;
    timing.startDelay = 100;
    timing.endDelay = 100;
    timing.playbackRate = -2;
    timing.fillMode = Timing::FillModeNone;
    RefPtrWillBeRawPtr<KeyframeEffect> animation = KeyframeEffect::create(0, nullptr, timing);
    RefPtrWillBeRawPtr<Animation> player = document.timeline().play(animation.get());
    double inf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(100, animation->timeToForwardsEffectChange());
    EXPECT_EQ(inf, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(100);
    EXPECT_EQ(50, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(149);
    EXPECT_EQ(1, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(150);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(200);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(50, animation->timeToReverseEffectChange());
}

TEST_F(KeyframeEffectTest, ElementDestructorClearsAnimationTarget)
{
    // This test expects incorrect behaviour should be removed once Element
    // and KeyframeEffect are moved to Oilpan. See crbug.com/362404 for context.
    Timing timing;
    timing.iterationDuration = 5;
    RefPtrWillBeRawPtr<KeyframeEffect> animation = KeyframeEffect::create(element.get(), nullptr, timing);
    EXPECT_EQ(element.get(), animation->target());
    document.timeline().play(animation.get());
    pageHolder.clear();
    element.clear();
#if !ENABLE(OILPAN)
    EXPECT_EQ(0, animation->target());
#endif
}

} // namespace blink
