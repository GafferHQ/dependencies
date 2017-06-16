/*
 * Copyright (C) 2012 Victor Carbune (victor@rosedu.org)
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

#include "core/events/GenericEventQueue.h"

#include "core/events/Event.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "platform/TraceEvent.h"

namespace blink {

PassOwnPtrWillBeRawPtr<GenericEventQueue> GenericEventQueue::create(EventTarget* owner)
{
    return adoptPtrWillBeNoop(new GenericEventQueue(owner));
}

GenericEventQueue::GenericEventQueue(EventTarget* owner)
    : m_owner(owner)
    , m_timer(this, &GenericEventQueue::timerFired)
    , m_isClosed(false)
{
}

GenericEventQueue::~GenericEventQueue()
{
}

DEFINE_TRACE(GenericEventQueue)
{
    visitor->trace(m_owner);
    visitor->trace(m_pendingEvents);
    EventQueue::trace(visitor);
}

bool GenericEventQueue::enqueueEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    if (m_isClosed)
        return false;

    if (event->target() == m_owner)
        event->setTarget(nullptr);

    TRACE_EVENT_ASYNC_BEGIN1("event", "GenericEventQueue:enqueueEvent", event.get(), "type", event->type().ascii());
    InspectorInstrumentation::didEnqueueEvent(event->target() ? event->target() : m_owner.get(), event.get());
    m_pendingEvents.append(event);

    if (!m_timer.isActive())
        m_timer.startOneShot(0, FROM_HERE);

    return true;
}

bool GenericEventQueue::cancelEvent(Event* event)
{
    bool found = m_pendingEvents.contains(event);

    if (found) {
        InspectorInstrumentation::didRemoveEvent(event->target() ? event->target() : m_owner.get(), event);
        m_pendingEvents.remove(m_pendingEvents.find(event));
        TRACE_EVENT_ASYNC_END2("event", "GenericEventQueue:enqueueEvent", event, "type", event->type().ascii(), "status", "cancelled");
    }

    if (m_pendingEvents.isEmpty())
        m_timer.stop();

    return found;
}

void GenericEventQueue::timerFired(Timer<GenericEventQueue>*)
{
    ASSERT(!m_timer.isActive());
    ASSERT(!m_pendingEvents.isEmpty());

    WillBeHeapVector<RefPtrWillBeMember<Event>> pendingEvents;
    m_pendingEvents.swap(pendingEvents);

    RefPtrWillBeRawPtr<EventTarget> protect(m_owner.get());
    for (const auto& pendingEvent : pendingEvents) {
        Event* event = pendingEvent.get();
        EventTarget* target = event->target() ? event->target() : m_owner.get();
        CString type(event->type().ascii());
        TRACE_EVENT_ASYNC_STEP_INTO1("event", "GenericEventQueue:enqueueEvent", event, "dispatch", "type", type);
        target->dispatchEvent(pendingEvent);
        TRACE_EVENT_ASYNC_END1("event", "GenericEventQueue:enqueueEvent", event, "type", type);
        InspectorInstrumentation::didRemoveEvent(target, event);
    }
}

void GenericEventQueue::close()
{
    m_isClosed = true;
    cancelAllEvents();
}

void GenericEventQueue::cancelAllEvents()
{
    m_timer.stop();

    for (const auto& pendingEvent : m_pendingEvents) {
        Event* event = pendingEvent.get();
        TRACE_EVENT_ASYNC_END2("event", "GenericEventQueue:enqueueEvent", event, "type", event->type().ascii(), "status", "cancelled");
        InspectorInstrumentation::didRemoveEvent(event->target() ? event->target() : m_owner.get(), event);
    }
    m_pendingEvents.clear();
}

bool GenericEventQueue::hasPendingEvents() const
{
    return m_pendingEvents.size();
}

}
