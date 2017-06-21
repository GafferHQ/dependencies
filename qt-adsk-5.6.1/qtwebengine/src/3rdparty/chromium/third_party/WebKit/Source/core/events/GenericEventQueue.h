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

#ifndef GenericEventQueue_h
#define GenericEventQueue_h

#include "core/CoreExport.h"
#include "core/events/EventQueue.h"
#include "core/events/EventTarget.h"
#include "platform/Timer.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class CORE_EXPORT GenericEventQueue final : public EventQueue {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(GenericEventQueue);
public:
    static PassOwnPtrWillBeRawPtr<GenericEventQueue> create(EventTarget*);
    virtual ~GenericEventQueue();

    // EventQueue
    DECLARE_VIRTUAL_TRACE();
    virtual bool enqueueEvent(PassRefPtrWillBeRawPtr<Event>) override;
    virtual bool cancelEvent(Event*) override;
    virtual void close() override;

    void cancelAllEvents();
    bool hasPendingEvents() const;

private:
    explicit GenericEventQueue(EventTarget*);
    void timerFired(Timer<GenericEventQueue>*);

    RawPtrWillBeMember<EventTarget> m_owner;
    WillBeHeapVector<RefPtrWillBeMember<Event>> m_pendingEvents;
    Timer<GenericEventQueue> m_timer;

    bool m_isClosed;
};

}

#endif
