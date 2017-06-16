/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/events/EventDispatcher.h"

#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/events/EventDispatchMediator.h"
#include "core/events/MouseEvent.h"
#include "core/events/ScopedEventQueue.h"
#include "core/events/WindowEventContext.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/TraceEvent.h"
#include "wtf/RefPtr.h"

namespace blink {

bool EventDispatcher::dispatchEvent(Node& node, PassRefPtrWillBeRawPtr<EventDispatchMediator> mediator)
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("blink.debug"), "EventDispatcher::dispatchEvent");
    ASSERT(!EventDispatchForbiddenScope::isEventDispatchForbidden());
    EventDispatcher dispatcher(node, &mediator->event());
    return mediator->dispatchEvent(dispatcher);
}

EventDispatcher::EventDispatcher(Node& node, PassRefPtrWillBeRawPtr<Event> event)
    : m_node(node)
    , m_event(event)
#if ENABLE(ASSERT)
    , m_eventDispatched(false)
#endif
{
    ASSERT(m_event.get());
    m_view = node.document().view();
    m_event->initEventPath(*m_node);
}

void EventDispatcher::dispatchScopedEvent(Node& node, PassRefPtrWillBeRawPtr<EventDispatchMediator> mediator)
{
    // We need to set the target here because it can go away by the time we actually fire the event.
    mediator->event().setTarget(EventPath::eventTargetRespectingTargetRules(node));
    ScopedEventQueue::instance()->enqueueEventDispatchMediator(mediator);
}

void EventDispatcher::dispatchSimulatedClick(Node& node, Event* underlyingEvent, SimulatedClickMouseEventOptions mouseEventOptions)
{
    // This persistent vector doesn't cause leaks, because added Nodes are removed
    // before dispatchSimulatedClick() returns. This vector is here just to prevent
    // the code from running into an infinite recursion of dispatchSimulatedClick().
    DEFINE_STATIC_LOCAL(OwnPtrWillBePersistent<WillBeHeapHashSet<RawPtrWillBeMember<Node>>>, nodesDispatchingSimulatedClicks, (adoptPtrWillBeNoop(new WillBeHeapHashSet<RawPtrWillBeMember<Node>>())));

    if (isDisabledFormControl(&node))
        return;

    if (nodesDispatchingSimulatedClicks->contains(&node))
        return;

    nodesDispatchingSimulatedClicks->add(&node);

    if (mouseEventOptions == SendMouseOverUpDownEvents)
        EventDispatcher(node, SimulatedMouseEvent::create(EventTypeNames::mouseover, node.document().domWindow(), underlyingEvent)).dispatch();

    if (mouseEventOptions != SendNoEvents) {
        EventDispatcher(node, SimulatedMouseEvent::create(EventTypeNames::mousedown, node.document().domWindow(), underlyingEvent)).dispatch();
        node.setActive(true);
        EventDispatcher(node, SimulatedMouseEvent::create(EventTypeNames::mouseup, node.document().domWindow(), underlyingEvent)).dispatch();
    }
    // Some elements (e.g. the color picker) may set active state to true before
    // calling this method and expect the state to be reset during the call.
    node.setActive(false);

    // always send click
    EventDispatcher(node, SimulatedMouseEvent::create(EventTypeNames::click, node.document().domWindow(), underlyingEvent)).dispatch();

    nodesDispatchingSimulatedClicks->remove(&node);
}

bool EventDispatcher::dispatch()
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("blink.debug"), "EventDispatcher::dispatch");

#if ENABLE(ASSERT)
    ASSERT(!m_eventDispatched);
    m_eventDispatched = true;
#endif
    if (event().eventPath().isEmpty()) {
        // eventPath() can be empty if event path is shrinked by relataedTarget retargeting.
        return true;
    }
    m_event->eventPath().ensureWindowEventContext();

    m_event->setTarget(EventPath::eventTargetRespectingTargetRules(*m_node));
    ASSERT(!EventDispatchForbiddenScope::isEventDispatchForbidden());
    ASSERT(m_event->target());
    TRACE_EVENT1("devtools.timeline", "EventDispatch", "data", InspectorEventDispatchEvent::data(*m_event));
    void* preDispatchEventHandlerResult;
    if (dispatchEventPreProcess(preDispatchEventHandlerResult) == ContinueDispatching) {
        if (dispatchEventAtCapturing() == ContinueDispatching) {
            if (dispatchEventAtTarget() == ContinueDispatching)
                dispatchEventAtBubbling();
        }
    }
    dispatchEventPostProcess(preDispatchEventHandlerResult);

    // Ensure that after event dispatch, the event's target object is the
    // outermost shadow DOM boundary.
    m_event->setTarget(m_event->eventPath().windowEventContext().target());
    m_event->setCurrentTarget(0);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data", InspectorUpdateCountersEvent::data());

    return !m_event->defaultPrevented();
}

inline EventDispatchContinuation EventDispatcher::dispatchEventPreProcess(void*& preDispatchEventHandlerResult)
{
    // Give the target node a chance to do some work before DOM event handlers get a crack.
    preDispatchEventHandlerResult = m_node->preDispatchEventHandler(m_event.get());
    return (m_event->eventPath().isEmpty() || m_event->propagationStopped()) ? DoneDispatching : ContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::dispatchEventAtCapturing()
{
    // Trigger capturing event handlers, starting at the top and working our way down.
    m_event->setEventPhase(Event::CAPTURING_PHASE);

    if (m_event->eventPath().windowEventContext().handleLocalEvents(*m_event) && m_event->propagationStopped())
        return DoneDispatching;

    for (size_t i = m_event->eventPath().size() - 1; i > 0; --i) {
        const NodeEventContext& eventContext = m_event->eventPath()[i];
        if (eventContext.currentTargetSameAsTarget())
            continue;
        eventContext.handleLocalEvents(*m_event);
        if (m_event->propagationStopped())
            return DoneDispatching;
    }

    return ContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::dispatchEventAtTarget()
{
    m_event->setEventPhase(Event::AT_TARGET);
    m_event->eventPath()[0].handleLocalEvents(*m_event);
    return m_event->propagationStopped() ? DoneDispatching : ContinueDispatching;
}

inline void EventDispatcher::dispatchEventAtBubbling()
{
    // Trigger bubbling event handlers, starting at the bottom and working our way up.
    size_t size = m_event->eventPath().size();
    for (size_t i = 1; i < size; ++i) {
        const NodeEventContext& eventContext = m_event->eventPath()[i];
        if (eventContext.currentTargetSameAsTarget())
            m_event->setEventPhase(Event::AT_TARGET);
        else if (m_event->bubbles() && !m_event->cancelBubble())
            m_event->setEventPhase(Event::BUBBLING_PHASE);
        else
            continue;
        eventContext.handleLocalEvents(*m_event);
        if (m_event->propagationStopped())
            return;
    }
    if (m_event->bubbles() && !m_event->cancelBubble()) {
        m_event->setEventPhase(Event::BUBBLING_PHASE);
        m_event->eventPath().windowEventContext().handleLocalEvents(*m_event);
    }
}

inline void EventDispatcher::dispatchEventPostProcess(void* preDispatchEventHandlerResult)
{
    m_event->setTarget(EventPath::eventTargetRespectingTargetRules(*m_node));
    m_event->setCurrentTarget(0);
    m_event->setEventPhase(0);

    // Pass the data from the preDispatchEventHandler to the postDispatchEventHandler.
    m_node->postDispatchEventHandler(m_event.get(), preDispatchEventHandlerResult);

    // Call default event handlers. While the DOM does have a concept of preventing
    // default handling, the detail of which handlers are called is an internal
    // implementation detail and not part of the DOM.
    if (!m_event->defaultPrevented() && !m_event->defaultHandled()) {
        // Non-bubbling events call only one default event handler, the one for the target.
        m_node->willCallDefaultEventHandler(*m_event);
        m_node->defaultEventHandler(m_event.get());
        ASSERT(!m_event->defaultPrevented());
        if (m_event->defaultHandled())
            return;
        // For bubbling events, call default event handlers on the same targets in the
        // same order as the bubbling phase.
        if (m_event->bubbles()) {
            size_t size = m_event->eventPath().size();
            for (size_t i = 1; i < size; ++i) {
                m_event->eventPath()[i].node()->willCallDefaultEventHandler(*m_event);
                m_event->eventPath()[i].node()->defaultEventHandler(m_event.get());
                ASSERT(!m_event->defaultPrevented());
                if (m_event->defaultHandled())
                    return;
            }
        }
    }
}

}
