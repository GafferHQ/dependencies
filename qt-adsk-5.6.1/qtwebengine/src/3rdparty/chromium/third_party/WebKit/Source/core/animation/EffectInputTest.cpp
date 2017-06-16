// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/EffectInput.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/animation/AnimationTestHelper.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/testing/DummyPageHolder.h"
#include <gtest/gtest.h>
#include <v8.h>

namespace blink {

class AnimationEffectInputTest : public ::testing::Test {
protected:
    AnimationEffectInputTest()
        : pageHolder(DummyPageHolder::create())
        , document(pageHolder->document())
        , element(document.createElement("foo", ASSERT_NO_EXCEPTION))
        , m_isolate(v8::Isolate::GetCurrent())
        , m_scope(m_isolate)
    {
        document.documentElement()->appendChild(element.get());
    }

    OwnPtr<DummyPageHolder> pageHolder;
    Document& document;
    RefPtrWillBePersistent<Element> element;
    TrackExceptionState exceptionState;
    v8::Isolate* m_isolate;

private:
    V8TestingScope m_scope;
};

TEST_F(AnimationEffectInputTest, SortedOffsets)
{
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(m_isolate);
    v8::Local<v8::Object> keyframe2 = v8::Object::New(m_isolate);

    setV8ObjectPropertyAsString(m_isolate, keyframe1, "width", "100px");
    setV8ObjectPropertyAsString(m_isolate, keyframe1, "offset", "0");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "width", "0px");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "offset", "1");

    jsKeyframes.append(Dictionary(keyframe1, m_isolate, exceptionState));
    jsKeyframes.append(Dictionary(keyframe2, m_isolate, exceptionState));

    RefPtrWillBeRawPtr<EffectModel> animationEffect = EffectInput::convert(element.get(), jsKeyframes, exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    const KeyframeEffectModelBase& keyframeEffect = *toKeyframeEffectModelBase(animationEffect.get());
    EXPECT_EQ(1.0, keyframeEffect.getFrames()[1]->offset());
}

TEST_F(AnimationEffectInputTest, UnsortedOffsets)
{
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(m_isolate);
    v8::Local<v8::Object> keyframe2 = v8::Object::New(m_isolate);

    setV8ObjectPropertyAsString(m_isolate, keyframe1, "width", "0px");
    setV8ObjectPropertyAsString(m_isolate, keyframe1, "offset", "1");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "width", "100px");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "offset", "0");

    jsKeyframes.append(Dictionary(keyframe1, m_isolate, exceptionState));
    jsKeyframes.append(Dictionary(keyframe2, m_isolate, exceptionState));

    EffectInput::convert(element.get(), jsKeyframes, exceptionState);
    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_EQ(InvalidModificationError, exceptionState.code());
}

TEST_F(AnimationEffectInputTest, LooslySorted)
{
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(m_isolate);
    v8::Local<v8::Object> keyframe2 = v8::Object::New(m_isolate);
    v8::Local<v8::Object> keyframe3 = v8::Object::New(m_isolate);

    setV8ObjectPropertyAsString(m_isolate, keyframe1, "width", "100px");
    setV8ObjectPropertyAsString(m_isolate, keyframe1, "offset", "0");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "width", "200px");
    setV8ObjectPropertyAsString(m_isolate, keyframe3, "width", "0px");
    setV8ObjectPropertyAsString(m_isolate, keyframe3, "offset", "1");

    jsKeyframes.append(Dictionary(keyframe1, m_isolate, exceptionState));
    jsKeyframes.append(Dictionary(keyframe2, m_isolate, exceptionState));
    jsKeyframes.append(Dictionary(keyframe3, m_isolate, exceptionState));

    RefPtrWillBeRawPtr<EffectModel> animationEffect = EffectInput::convert(element.get(), jsKeyframes, exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    const KeyframeEffectModelBase& keyframeEffect = *toKeyframeEffectModelBase(animationEffect.get());
    EXPECT_EQ(1, keyframeEffect.getFrames()[2]->offset());
}

TEST_F(AnimationEffectInputTest, OutOfOrderWithNullOffsets)
{
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(m_isolate);
    v8::Local<v8::Object> keyframe2 = v8::Object::New(m_isolate);
    v8::Local<v8::Object> keyframe3 = v8::Object::New(m_isolate);
    v8::Local<v8::Object> keyframe4 = v8::Object::New(m_isolate);

    setV8ObjectPropertyAsString(m_isolate, keyframe1, "height", "100px");
    setV8ObjectPropertyAsString(m_isolate, keyframe1, "offset", "0.5");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "height", "150px");
    setV8ObjectPropertyAsString(m_isolate, keyframe3, "height", "200px");
    setV8ObjectPropertyAsString(m_isolate, keyframe3, "offset", "0");
    setV8ObjectPropertyAsString(m_isolate, keyframe4, "height", "300px");
    setV8ObjectPropertyAsString(m_isolate, keyframe4, "offset", "1");

    jsKeyframes.append(Dictionary(keyframe1, m_isolate, exceptionState));
    jsKeyframes.append(Dictionary(keyframe2, m_isolate, exceptionState));
    jsKeyframes.append(Dictionary(keyframe3, m_isolate, exceptionState));
    jsKeyframes.append(Dictionary(keyframe4, m_isolate, exceptionState));

    EffectInput::convert(element.get(), jsKeyframes, exceptionState);
    EXPECT_TRUE(exceptionState.hadException());
}

TEST_F(AnimationEffectInputTest, Invalid)
{
    // Not loosely sorted by offset, and there exists a keyframe with null offset.
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(m_isolate);
    v8::Local<v8::Object> keyframe2 = v8::Object::New(m_isolate);
    v8::Local<v8::Object> keyframe3 = v8::Object::New(m_isolate);

    setV8ObjectPropertyAsString(m_isolate, keyframe1, "width", "0px");
    setV8ObjectPropertyAsString(m_isolate, keyframe1, "offset", "1");
    setV8ObjectPropertyAsString(m_isolate, keyframe2, "width", "200px");
    setV8ObjectPropertyAsString(m_isolate, keyframe3, "width", "100px");
    setV8ObjectPropertyAsString(m_isolate, keyframe3, "offset", "0");

    jsKeyframes.append(Dictionary(keyframe1, m_isolate, exceptionState));
    jsKeyframes.append(Dictionary(keyframe2, m_isolate, exceptionState));
    jsKeyframes.append(Dictionary(keyframe3, m_isolate, exceptionState));

    EffectInput::convert(element.get(), jsKeyframes, exceptionState);
    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_EQ(InvalidModificationError, exceptionState.code());
}

} // namespace blink
