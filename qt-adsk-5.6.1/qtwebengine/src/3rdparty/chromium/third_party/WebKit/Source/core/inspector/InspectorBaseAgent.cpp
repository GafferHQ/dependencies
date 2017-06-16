/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/inspector/InspectorBaseAgent.h"

#include "core/inspector/InspectorState.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

InspectorAgent::InspectorAgent(const String& name)
    : m_name(name)
{
}

InspectorAgent::~InspectorAgent()
{
#if ENABLE(OILPAN)
    m_state = nullptr;
#endif
}

DEFINE_TRACE(InspectorAgent)
{
    visitor->trace(m_instrumentingAgents);
    visitor->trace(m_state);
}

void InspectorAgent::appended(InstrumentingAgents* instrumentingAgents, InspectorState* inspectorState)
{
    m_instrumentingAgents = instrumentingAgents;
    m_state = inspectorState;
    init();
}

InspectorAgentRegistry::InspectorAgentRegistry(InstrumentingAgents* instrumentingAgents, InspectorCompositeState* inspectorState)
    : m_instrumentingAgents(instrumentingAgents)
    , m_inspectorState(inspectorState)
{
}

void InspectorAgentRegistry::append(PassOwnPtrWillBeRawPtr<InspectorAgent> agent)
{
    agent->appended(m_instrumentingAgents, m_inspectorState->createAgentState(agent->name()));
    m_agents.append(agent);
}

void InspectorAgentRegistry::setFrontend(InspectorFrontend* frontend)
{
    for (size_t i = 0; i < m_agents.size(); i++)
        m_agents[i]->setFrontend(frontend);
}

void InspectorAgentRegistry::clearFrontend()
{
    for (size_t i = 0; i < m_agents.size(); i++)
        m_agents[i]->clearFrontend();
}

void InspectorAgentRegistry::restore()
{
    for (size_t i = 0; i < m_agents.size(); i++)
        m_agents[i]->restore();
}

void InspectorAgentRegistry::registerInDispatcher(InspectorBackendDispatcher* dispatcher)
{
    for (size_t i = 0; i < m_agents.size(); i++)
        m_agents[i]->registerInDispatcher(dispatcher);
}

void InspectorAgentRegistry::discardAgents()
{
    for (size_t i = 0; i < m_agents.size(); i++)
        m_agents[i]->discardAgent();
}

void InspectorAgentRegistry::flushPendingProtocolNotifications()
{
    for (size_t i = 0; i < m_agents.size(); i++)
        m_agents[i]->flushPendingProtocolNotifications();
}

DEFINE_TRACE(InspectorAgentRegistry)
{
    visitor->trace(m_instrumentingAgents);
    visitor->trace(m_inspectorState);
#if ENABLE(OILPAN)
    visitor->trace(m_agents);
#endif
}

void InspectorAgentRegistry::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    for (size_t i = 0; i < m_agents.size(); i++)
        m_agents[i]->didCommitLoadForLocalFrame(frame);
}

} // namespace blink

