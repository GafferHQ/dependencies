/*
 * Copyright (C) 2009 Apple Inc.  All rights reserved.
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

#ifndef LayoutObjectChildList_h
#define LayoutObjectChildList_h

#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class LayoutObject;

class LayoutObjectChildList {
    DISALLOW_ALLOCATION();
public:
    LayoutObjectChildList()
        : m_firstChild(nullptr)
        , m_lastChild(nullptr)
    {
    }

    LayoutObject* firstChild() const { return m_firstChild; }
    LayoutObject* lastChild() const { return m_lastChild; }

    // FIXME: Temporary while LayoutBox still exists. Eventually this will just happen during insert/append/remove methods on the child list, and nobody
    // will need to manipulate firstChild or lastChild directly.
    void setFirstChild(LayoutObject* child) { m_firstChild = child; }
    void setLastChild(LayoutObject* child) { m_lastChild = child; }

    void destroyLeftoverChildren();

    LayoutObject* removeChildNode(LayoutObject* owner, LayoutObject*, bool notifyLayoutObject = true);
    void insertChildNode(LayoutObject* owner, LayoutObject* newChild, LayoutObject* beforeChild, bool notifyLayoutObject = true);
    void appendChildNode(LayoutObject* owner, LayoutObject* newChild, bool notifyLayoutObject = true)
    {
        insertChildNode(owner, newChild, 0, notifyLayoutObject);
    }

private:
    void invalidatePaintOnRemoval(const LayoutObject& oldChild);

    LayoutObject* m_firstChild;
    LayoutObject* m_lastChild;
};

} // namespace blink

#endif // LayoutObjectChildList_h
