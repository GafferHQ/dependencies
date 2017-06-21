/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef InsertionPoint_h
#define InsertionPoint_h

#include "core/CoreExport.h"
#include "core/css/CSSSelectorList.h"
#include "core/dom/shadow/DistributedNodes.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLElement.h"

namespace blink {

class CORE_EXPORT InsertionPoint : public HTMLElement {
public:
    ~InsertionPoint() override;

    bool hasDistribution() const { return !m_distributedNodes.isEmpty(); }
    void setDistributedNodes(DistributedNodes&);
    void clearDistribution() { m_distributedNodes.clear(); }
    bool isActive() const;
    bool canBeActive() const;

    bool isShadowInsertionPoint() const;
    bool isContentInsertionPoint() const;

    PassRefPtrWillBeRawPtr<StaticNodeList> getDistributedNodes();

    virtual bool canAffectSelector() const { return false; }

    void attach(const AttachContext& = AttachContext()) override;
    void detach(const AttachContext& = AttachContext()) override;

    bool shouldUseFallbackElements() const;

    size_t distributedNodesSize() const { return m_distributedNodes.size(); }
    Node* distributedNodeAt(size_t index)  const { return m_distributedNodes.at(index).get(); }
    Node* firstDistributedNode() const { return m_distributedNodes.isEmpty() ? 0 : m_distributedNodes.first().get(); }
    Node* lastDistributedNode() const { return m_distributedNodes.isEmpty() ? 0 : m_distributedNodes.last().get(); }
    Node* distributedNodeNextTo(const Node* node) const { return m_distributedNodes.nextTo(node); }
    Node* distributedNodePreviousTo(const Node* node) const { return m_distributedNodes.previousTo(node); }

    DECLARE_VIRTUAL_TRACE();

protected:
    InsertionPoint(const QualifiedName&, Document&);
    bool layoutObjectIsNeeded(const ComputedStyle&) override;
    void childrenChanged(const ChildrenChange&) override;
    InsertionNotificationRequest insertedInto(ContainerNode*) override;
    void removedFrom(ContainerNode*) override;
    void willRecalcStyle(StyleRecalcChange) override;

private:
    bool isInsertionPoint() const = delete; // This will catch anyone doing an unnecessary check.

    DistributedNodes m_distributedNodes;
    bool m_registeredWithShadowRoot;
};

typedef WillBeHeapVector<RefPtrWillBeMember<InsertionPoint>, 1> DestinationInsertionPoints;

DEFINE_ELEMENT_TYPE_CASTS(InsertionPoint, isInsertionPoint());

inline bool isActiveInsertionPoint(const Node& node)
{
    return node.isInsertionPoint() && toInsertionPoint(node).isActive();
}

inline bool isActiveShadowInsertionPoint(const Node& node)
{
    return node.isInsertionPoint() && toInsertionPoint(node).isShadowInsertionPoint();
}

inline ElementShadow* shadowWhereNodeCanBeDistributed(const Node& node)
{
    Node* parent = node.parentNode();
    if (!parent)
        return 0;
    if (parent->isShadowRoot() && !toShadowRoot(parent)->isYoungest())
        return node.shadowHost()->shadow();
    if (isActiveInsertionPoint(*parent))
        return node.shadowHost()->shadow();
    if (parent->isElementNode())
        return toElement(parent)->shadow();
    return 0;
}

const InsertionPoint* resolveReprojection(const Node*);

void collectDestinationInsertionPoints(const Node&, WillBeHeapVector<RawPtrWillBeMember<InsertionPoint>, 8>& results);

} // namespace blink

#endif // InsertionPoint_h
