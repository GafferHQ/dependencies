// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/animatable/AnimatableValueKeyframe.h"

#include "core/animation/LegacyStyleInterpolation.h"

namespace blink {

AnimatableValueKeyframe::AnimatableValueKeyframe(const AnimatableValueKeyframe& copyFrom)
    : Keyframe(copyFrom.m_offset, copyFrom.m_composite, copyFrom.m_easing)
{
    for (PropertyValueMap::const_iterator iter = copyFrom.m_propertyValues.begin(); iter != copyFrom.m_propertyValues.end(); ++iter)
        setPropertyValue(iter->key, iter->value.get());
}

PropertyHandleSet AnimatableValueKeyframe::properties() const
{
    // This is not used in time-critical code, so we probably don't need to
    // worry about caching this result.
    PropertyHandleSet properties;
    for (PropertyValueMap::const_iterator iter = m_propertyValues.begin(); iter != m_propertyValues.end(); ++iter)
        properties.add(PropertyHandle(*iter.keys()));
    return properties;
}

PassRefPtrWillBeRawPtr<Keyframe> AnimatableValueKeyframe::clone() const
{
    return adoptRefWillBeNoop(new AnimatableValueKeyframe(*this));
}

PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> AnimatableValueKeyframe::createPropertySpecificKeyframe(PropertyHandle property) const
{
    return adoptPtrWillBeNoop(new PropertySpecificKeyframe(offset(), &easing(), propertyValue(property.cssProperty()), composite()));
}

DEFINE_TRACE(AnimatableValueKeyframe)
{
#if ENABLE(OILPAN)
    visitor->trace(m_propertyValues);
#endif
    Keyframe::trace(visitor);
}

AnimatableValueKeyframe::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, const AnimatableValue* value, EffectModel::CompositeOperation op)
    : Keyframe::PropertySpecificKeyframe(offset, easing, op)
    , m_value(const_cast<AnimatableValue*>(value))
{ }

AnimatableValueKeyframe::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, PassRefPtrWillBeRawPtr<AnimatableValue> value)
    : Keyframe::PropertySpecificKeyframe(offset, easing, EffectModel::CompositeReplace)
    , m_value(value)
{
    ASSERT(!isNull(m_offset));
}

PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> AnimatableValueKeyframe::PropertySpecificKeyframe::cloneWithOffset(double offset) const
{
    Keyframe::PropertySpecificKeyframe* theClone = new PropertySpecificKeyframe(offset, m_easing, m_value);
    return adoptPtrWillBeNoop(theClone);
}

PassRefPtrWillBeRawPtr<Interpolation> AnimatableValueKeyframe::PropertySpecificKeyframe::maybeCreateInterpolation(PropertyHandle property, Keyframe::PropertySpecificKeyframe& end, Element*, const ComputedStyle*) const
{
    AnimatableValuePropertySpecificKeyframe& to = toAnimatableValuePropertySpecificKeyframe(end);
    return LegacyStyleInterpolation::create(value(), to.value(), property.cssProperty());
}

PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> AnimatableValueKeyframe::PropertySpecificKeyframe::neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const
{
    return adoptPtrWillBeNoop(new AnimatableValueKeyframe::PropertySpecificKeyframe(offset, easing, AnimatableValue::neutralValue(), EffectModel::CompositeAdd));
}

DEFINE_TRACE(AnimatableValueKeyframe::PropertySpecificKeyframe)
{
    visitor->trace(m_value);
    Keyframe::PropertySpecificKeyframe::trace(visitor);
}

}
