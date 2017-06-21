/*
 * Copyright (C) 2014 Google Inc. All Rights Reserved.
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
#include "core/events/TreeScopeEventContext.h"

#include "core/dom/StaticNodeList.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/EventPath.h"
#include "core/events/TouchEventContext.h"

namespace blink {

WillBeHeapVector<RefPtrWillBeMember<EventTarget>>& TreeScopeEventContext::ensureEventPath(EventPath& path)
{
    if (m_eventPath)
        return *m_eventPath;

    m_eventPath = adoptPtrWillBeNoop(new WillBeHeapVector<RefPtrWillBeMember<EventTarget>>());
    LocalDOMWindow* window = path.windowEventContext().window();
    m_eventPath->reserveCapacity(path.size() + (window ? 1 : 0));
    for (size_t i = 0; i < path.size(); ++i) {
        Node& rootNode = path[i].treeScopeEventContext().rootNode();
        if (rootNode.isShadowRoot() && toShadowRoot(rootNode).type() == ShadowRootType::Open)
            m_eventPath->append(path[i].node());
        else if (path[i].treeScopeEventContext().isInclusiveAncestorOf(*this))
            m_eventPath->append(path[i].node());
    }
    if (window)
        m_eventPath->append(window);
    return *m_eventPath;
}

TouchEventContext* TreeScopeEventContext::ensureTouchEventContext()
{
    if (!m_touchEventContext)
        m_touchEventContext = TouchEventContext::create();
    return m_touchEventContext.get();
}

PassRefPtrWillBeRawPtr<TreeScopeEventContext> TreeScopeEventContext::create(TreeScope& treeScope)
{
    return adoptRefWillBeNoop(new TreeScopeEventContext(treeScope));
}

TreeScopeEventContext::TreeScopeEventContext(TreeScope& treeScope)
    : m_treeScope(treeScope)
    , m_rootNode(treeScope.rootNode())
    , m_preOrder(-1)
    , m_postOrder(-1)
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(TreeScopeEventContext)

DEFINE_TRACE(TreeScopeEventContext)
{
    visitor->trace(m_treeScope);
    visitor->trace(m_rootNode);
    visitor->trace(m_target);
    visitor->trace(m_relatedTarget);
    visitor->trace(m_eventPath);
    visitor->trace(m_touchEventContext);
#if ENABLE(OILPAN)
    visitor->trace(m_children);
#endif
}

int TreeScopeEventContext::calculatePrePostOrderNumber(int orderNumber)
{
    m_preOrder = orderNumber;
    for (size_t i = 0; i < m_children.size(); ++i)
        orderNumber = m_children[i]->calculatePrePostOrderNumber(orderNumber + 1);
    m_postOrder = orderNumber + 1;
    return orderNumber + 1;
}

}
