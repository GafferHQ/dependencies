/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "core/events/DOMWindowEventQueue.h"

#include "core/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/SuspendableTimer.h"
#include "core/inspector/InspectorInstrumentation.h"

namespace blink {

class DOMWindowEventQueueTimer final : public NoBaseWillBeGarbageCollectedFinalized<DOMWindowEventQueueTimer>, public SuspendableTimer {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(DOMWindowEventQueueTimer);
    WTF_MAKE_NONCOPYABLE(DOMWindowEventQueueTimer);
public:
    DOMWindowEventQueueTimer(DOMWindowEventQueue* eventQueue, ExecutionContext* context)
        : SuspendableTimer(context)
        , m_eventQueue(eventQueue)
    {
    }

    // Eager finalization is needed to promptly stop this timer object.
    // (see DOMTimer comment for more.)
    EAGERLY_FINALIZE();
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_eventQueue);
        SuspendableTimer::trace(visitor);
    }

private:
    virtual void fired() { m_eventQueue->pendingEventTimerFired(); }

    RawPtrWillBeMember<DOMWindowEventQueue> m_eventQueue;
};

PassRefPtrWillBeRawPtr<DOMWindowEventQueue> DOMWindowEventQueue::create(ExecutionContext* context)
{
    return adoptRefWillBeNoop(new DOMWindowEventQueue(context));
}

DOMWindowEventQueue::DOMWindowEventQueue(ExecutionContext* context)
    : m_pendingEventTimer(adoptPtrWillBeNoop(new DOMWindowEventQueueTimer(this, context)))
    , m_isClosed(false)
{
    m_pendingEventTimer->suspendIfNeeded();
}

DOMWindowEventQueue::~DOMWindowEventQueue()
{
}

DEFINE_TRACE(DOMWindowEventQueue)
{
#if ENABLE(OILPAN)
    visitor->trace(m_pendingEventTimer);
    visitor->trace(m_queuedEvents);
#endif
    EventQueue::trace(visitor);
}

bool DOMWindowEventQueue::enqueueEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    if (m_isClosed)
        return false;

    ASSERT(event->target());
    InspectorInstrumentation::didEnqueueEvent(event->target(), event.get());

    bool wasAdded = m_queuedEvents.add(event).isNewEntry;
    ASSERT_UNUSED(wasAdded, wasAdded); // It should not have already been in the list.

    if (!m_pendingEventTimer->isActive())
        m_pendingEventTimer->startOneShot(0, FROM_HERE);

    return true;
}

bool DOMWindowEventQueue::cancelEvent(Event* event)
{
    WillBeHeapListHashSet<RefPtrWillBeMember<Event>, 16>::iterator it = m_queuedEvents.find(event);
    bool found = it != m_queuedEvents.end();
    if (found) {
        InspectorInstrumentation::didRemoveEvent(event->target(), event);
        m_queuedEvents.remove(it);
    }
    if (m_queuedEvents.isEmpty())
        m_pendingEventTimer->stop();
    return found;
}

void DOMWindowEventQueue::close()
{
    m_isClosed = true;
    m_pendingEventTimer->stop();
    if (InspectorInstrumentation::hasFrontends()) {
        for (const auto& queuedEvent : m_queuedEvents) {
            RefPtrWillBeRawPtr<Event> event = queuedEvent;
            if (event)
                InspectorInstrumentation::didRemoveEvent(event->target(), event.get());
        }
    }
    m_queuedEvents.clear();
}

void DOMWindowEventQueue::pendingEventTimerFired()
{
    ASSERT(!m_pendingEventTimer->isActive());
    ASSERT(!m_queuedEvents.isEmpty());

    // Insert a marker for where we should stop.
    ASSERT(!m_queuedEvents.contains(nullptr));
    bool wasAdded = m_queuedEvents.add(nullptr).isNewEntry;
    ASSERT_UNUSED(wasAdded, wasAdded); // It should not have already been in the list.

    RefPtrWillBeRawPtr<DOMWindowEventQueue> protector(this);

    while (!m_queuedEvents.isEmpty()) {
        WillBeHeapListHashSet<RefPtrWillBeMember<Event>, 16>::iterator iter = m_queuedEvents.begin();
        RefPtrWillBeRawPtr<Event> event = *iter;
        m_queuedEvents.remove(iter);
        if (!event)
            break;
        dispatchEvent(event.get());
        InspectorInstrumentation::didRemoveEvent(event->target(), event.get());
    }
}

void DOMWindowEventQueue::dispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    EventTarget* eventTarget = event->target();
    if (eventTarget->toDOMWindow())
        eventTarget->toDOMWindow()->dispatchEvent(event, nullptr);
    else
        eventTarget->dispatchEvent(event);
}

}
