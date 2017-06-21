// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/events/AnimationPlayerEvent.h"

namespace blink {

AnimationPlayerEvent::AnimationPlayerEvent()
    : m_currentTime(0.0)
    , m_timelineTime(0.0)
{
}

AnimationPlayerEvent::AnimationPlayerEvent(const AtomicString& type, double currentTime, double timelineTime)
    : Event(type, false, false)
    , m_currentTime(currentTime)
    , m_timelineTime(timelineTime)
{
}

AnimationPlayerEvent::AnimationPlayerEvent(const AtomicString& type, const AnimationPlayerEventInit& initializer)
    : Event(type, initializer)
    , m_currentTime(0.0)
    , m_timelineTime(0.0)
{
    if (initializer.hasCurrentTime())
        m_currentTime = initializer.currentTime();
    if (initializer.hasTimelineTime())
        m_timelineTime = initializer.timelineTime();
}

AnimationPlayerEvent::~AnimationPlayerEvent()
{
}

double AnimationPlayerEvent::currentTime() const
{
    return m_currentTime;
}

double AnimationPlayerEvent::timelineTime() const
{
    return m_timelineTime;
}

const AtomicString& AnimationPlayerEvent::interfaceName() const
{
    return EventNames::AnimationPlayerEvent;
}

DEFINE_TRACE(AnimationPlayerEvent)
{
    Event::trace(visitor);
}

} // namespace blink
