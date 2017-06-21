/*
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "core/layout/CounterNode.h"

#include "core/layout/LayoutCounter.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

CounterNode::CounterNode(LayoutObject& o, bool hasResetType, int value)
    : m_hasResetType(hasResetType)
    , m_value(value)
    , m_countInParent(0)
    , m_owner(o)
    , m_rootLayoutObject(nullptr)
    , m_parent(nullptr)
    , m_previousSibling(nullptr)
    , m_nextSibling(nullptr)
    , m_firstChild(nullptr)
    , m_lastChild(nullptr)
{
}

CounterNode::~CounterNode()
{
    // Ideally this would be an assert and this would never be reached. In reality this happens a lot
    // so we need to handle these cases. The node is still connected to the tree so we need to detach it.
    if (m_parent || m_previousSibling || m_nextSibling || m_firstChild || m_lastChild) {
        CounterNode* oldParent = nullptr;
        CounterNode* oldPreviousSibling = nullptr;
        // Instead of calling removeChild() we do this safely as the tree is likely broken if we get here.
        if (m_parent) {
            if (m_parent->m_firstChild == this)
                m_parent->m_firstChild = m_nextSibling;
            if (m_parent->m_lastChild == this)
                m_parent->m_lastChild = m_previousSibling;
            oldParent = m_parent;
            m_parent = nullptr;
        }
        if (m_previousSibling) {
            if (m_previousSibling->m_nextSibling == this)
                m_previousSibling->m_nextSibling = m_nextSibling;
            oldPreviousSibling = m_previousSibling;
            m_previousSibling = nullptr;
        }
        if (m_nextSibling) {
            if (m_nextSibling->m_previousSibling == this)
                m_nextSibling->m_previousSibling = oldPreviousSibling;
            m_nextSibling = nullptr;
        }
        if (m_firstChild) {
            // The node's children are reparented to the old parent.
            for (CounterNode* child = m_firstChild; child; ) {
                CounterNode* nextChild = child->m_nextSibling;
                CounterNode* nextSibling = nullptr;
                child->m_parent = oldParent;
                if (oldPreviousSibling) {
                    nextSibling = oldPreviousSibling->m_nextSibling;
                    child->m_previousSibling = oldPreviousSibling;
                    oldPreviousSibling->m_nextSibling = child;
                    child->m_nextSibling = nextSibling;
                    nextSibling->m_previousSibling = child;
                    oldPreviousSibling = child;
                }
                child = nextChild;
            }
        }
    }
    resetLayoutObjects();
}

PassRefPtr<CounterNode> CounterNode::create(LayoutObject& owner, bool hasResetType, int value)
{
    return adoptRef(new CounterNode(owner, hasResetType, value));
}

CounterNode* CounterNode::nextInPreOrderAfterChildren(const CounterNode* stayWithin) const
{
    if (this == stayWithin)
        return nullptr;

    const CounterNode* current = this;
    CounterNode* next = current->m_nextSibling;
    for (; !next; next = current->m_nextSibling) {
        current = current->m_parent;
        if (!current || current == stayWithin)
            return nullptr;
    }
    return next;
}

CounterNode* CounterNode::nextInPreOrder(const CounterNode* stayWithin) const
{
    if (CounterNode* next = m_firstChild)
        return next;

    return nextInPreOrderAfterChildren(stayWithin);
}

CounterNode* CounterNode::lastDescendant() const
{
    CounterNode* last = m_lastChild;
    if (!last)
        return nullptr;

    while (CounterNode* lastChild = last->m_lastChild)
        last = lastChild;

    return last;
}

CounterNode* CounterNode::previousInPreOrder() const
{
    CounterNode* previous = m_previousSibling;
    if (!previous)
        return m_parent;

    while (CounterNode* lastChild = previous->m_lastChild)
        previous = lastChild;

    return previous;
}

int CounterNode::computeCountInParent() const
{
    int increment = actsAsReset() ? 0 : m_value;
    if (m_previousSibling)
        return m_previousSibling->m_countInParent + increment;
    ASSERT(m_parent->m_firstChild == this);
    return m_parent->m_value + increment;
}

void CounterNode::addLayoutObject(LayoutCounter* value)
{
    if (!value) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (value->m_counterNode) {
        ASSERT_NOT_REACHED();
        value->m_counterNode->removeLayoutObject(value);
    }
    ASSERT(!value->m_nextForSameCounter);
    for (LayoutCounter* iterator = m_rootLayoutObject; iterator; iterator = iterator->m_nextForSameCounter) {
        if (iterator == value) {
            ASSERT_NOT_REACHED();
            return;
        }
    }
    value->m_nextForSameCounter = m_rootLayoutObject;
    m_rootLayoutObject = value;
    if (value->m_counterNode != this) {
        if (value->m_counterNode) {
            ASSERT_NOT_REACHED();
            value->m_counterNode->removeLayoutObject(value);
        }
        value->m_counterNode = this;
    }
}

void CounterNode::removeLayoutObject(LayoutCounter* value)
{
    if (!value) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (value->m_counterNode && value->m_counterNode != this) {
        ASSERT_NOT_REACHED();
        value->m_counterNode->removeLayoutObject(value);
    }
    LayoutCounter* previous = nullptr;
    for (LayoutCounter* iterator = m_rootLayoutObject; iterator; iterator = iterator->m_nextForSameCounter) {
        if (iterator == value) {
            if (previous)
                previous->m_nextForSameCounter = value->m_nextForSameCounter;
            else
                m_rootLayoutObject = value->m_nextForSameCounter;
            value->m_nextForSameCounter = nullptr;
            value->m_counterNode = nullptr;
            return;
        }
        previous = iterator;
    }
    ASSERT_NOT_REACHED();
}

void CounterNode::resetLayoutObjects()
{
    while (m_rootLayoutObject)
        m_rootLayoutObject->invalidate(); // This makes m_rootLayoutObject point to the next layoutObject if any since it disconnects the m_rootLayoutObject from this.
}

void CounterNode::resetThisAndDescendantsLayoutObjects()
{
    CounterNode* node = this;
    do {
        node->resetLayoutObjects();
        node = node->nextInPreOrder(this);
    } while (node);
}

void CounterNode::recount()
{
    for (CounterNode* node = this; node; node = node->m_nextSibling) {
        int oldCount = node->m_countInParent;
        int newCount = node->computeCountInParent();
        if (oldCount == newCount)
            break;
        node->m_countInParent = newCount;
        node->resetThisAndDescendantsLayoutObjects();
    }
}

void CounterNode::insertAfter(CounterNode* newChild, CounterNode* refChild, const AtomicString& identifier)
{
    ASSERT(newChild);
    ASSERT(!newChild->m_parent);
    ASSERT(!newChild->m_previousSibling);
    ASSERT(!newChild->m_nextSibling);
    // If the refChild is not our child we can not complete the request. This hardens against bugs in LayoutCounter.
    // When layoutObjects are reparented it may request that we insert counter nodes improperly.
    if (refChild && refChild->m_parent != this)
        return;

    if (newChild->m_hasResetType) {
        while (m_lastChild != refChild)
            LayoutCounter::destroyCounterNode(m_lastChild->owner(), identifier);
    }

    CounterNode* next;

    if (refChild) {
        next = refChild->m_nextSibling;
        refChild->m_nextSibling = newChild;
    } else {
        next = m_firstChild;
        m_firstChild = newChild;
    }

    newChild->m_parent = this;
    newChild->m_previousSibling = refChild;

    if (next) {
        ASSERT(next->m_previousSibling == refChild);
        next->m_previousSibling = newChild;
        newChild->m_nextSibling = next;
    } else {
        ASSERT(m_lastChild == refChild);
        m_lastChild = newChild;
    }

    if (!newChild->m_firstChild || newChild->m_hasResetType) {
        newChild->m_countInParent = newChild->computeCountInParent();
        newChild->resetThisAndDescendantsLayoutObjects();
        if (next)
            next->recount();
        return;
    }

    // The code below handles the case when a formerly root increment counter is loosing its root position
    // and therefore its children become next siblings.
    CounterNode* last = newChild->m_lastChild;
    CounterNode* first = newChild->m_firstChild;

    if (first) {
        ASSERT(last);
        newChild->m_nextSibling = first;
        if (m_lastChild == newChild)
            m_lastChild = last;

        first->m_previousSibling = newChild;

        // The case when the original next sibling of the inserted node becomes a child of
        // one of the former children of the inserted node is not handled as it is believed
        // to be impossible since:
        // 1. if the increment counter node lost it's root position as a result of another
        //    counter node being created, it will be inserted as the last child so next is null.
        // 2. if the increment counter node lost it's root position as a result of a layoutObject being
        //    inserted into the document's layout tree, all its former children counters are attached
        //    to children of the inserted layoutObject and hence cannot be in scope for counter nodes
        //    attached to layoutObjects that were already in the document's layout tree.
        last->m_nextSibling = next;
        if (next) {
            ASSERT(next->m_previousSibling == newChild);
            next->m_previousSibling = last;
        } else {
            m_lastChild = last;
        }
        for (next = first; ; next = next->m_nextSibling) {
            next->m_parent = this;
            if (last == next)
                break;
        }
    }
    newChild->m_firstChild = nullptr;
    newChild->m_lastChild = nullptr;
    newChild->m_countInParent = newChild->computeCountInParent();
    newChild->resetLayoutObjects();
    first->recount();
}

void CounterNode::removeChild(CounterNode* oldChild)
{
    ASSERT(oldChild);
    ASSERT(!oldChild->m_firstChild);
    ASSERT(!oldChild->m_lastChild);

    CounterNode* next = oldChild->m_nextSibling;
    CounterNode* previous = oldChild->m_previousSibling;

    oldChild->m_nextSibling = nullptr;
    oldChild->m_previousSibling = nullptr;
    oldChild->m_parent = nullptr;

    if (previous) {
        previous->m_nextSibling = next;
    } else {
        ASSERT(m_firstChild == oldChild);
        m_firstChild = next;
    }

    if (next) {
        next->m_previousSibling = previous;
    } else {
        ASSERT(m_lastChild == oldChild);
        m_lastChild = previous;
    }

    if (next)
        next->recount();
}

#ifndef NDEBUG

static void showTreeAndMark(const CounterNode* node)
{
    const CounterNode* root = node;
    while (root->parent())
        root = root->parent();

    for (const CounterNode* current = root; current; current = current->nextInPreOrder()) {
        fprintf(stderr, "%c", (current == node) ? '*' : ' ');
        for (const CounterNode* parent = current; parent && parent != root; parent = parent->parent())
            fprintf(stderr, "    ");
        fprintf(stderr, "%p %s: %d %d P:%p PS:%p NS:%p R:%p\n",
            current, current->actsAsReset() ? "reset____" : "increment", current->value(),
            current->countInParent(), current->parent(), current->previousSibling(),
            current->nextSibling(), &current->owner());
    }
    fflush(stderr);
}

#endif

} // namespace blink

#ifndef NDEBUG

void showCounterTree(const blink::CounterNode* counter)
{
    if (counter)
        showTreeAndMark(counter);
    else
        fprintf(stderr, "Cannot showCounterTree for (nil).\n");
}

#endif
