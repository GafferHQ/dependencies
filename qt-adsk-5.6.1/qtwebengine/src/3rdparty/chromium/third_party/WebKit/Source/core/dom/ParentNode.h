/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#ifndef ParentNode_h
#define ParentNode_h

#include "core/dom/ContainerNode.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLCollection.h"
#include "platform/heap/Handle.h"

namespace blink {

class ParentNode {
public:
    static PassRefPtrWillBeRawPtr<HTMLCollection> children(ContainerNode& node)
    {
        return node.children();
    }

    static Element* firstElementChild(ContainerNode& node)
    {
        return ElementTraversal::firstChild(node);
    }

    static Element* lastElementChild(ContainerNode& node)
    {
        return ElementTraversal::lastChild(node);
    }

    static unsigned childElementCount(ContainerNode& node)
    {
        unsigned count = 0;
        for (Element* child = ElementTraversal::firstChild(node); child; child = ElementTraversal::nextSibling(*child))
            ++count;
        return count;
    }

    static void prepend(Node& node, const HeapVector<NodeOrString>& nodes, ExceptionState& exceptionState)
    {
        return node.prepend(nodes, exceptionState);
    }

    static void append(Node& node, const HeapVector<NodeOrString>& nodes, ExceptionState& exceptionState)
    {
        return node.append(nodes, exceptionState);
    }

    static PassRefPtrWillBeRawPtr<Element> querySelector(ContainerNode& node, const AtomicString& selectors, ExceptionState& exceptionState)
    {
        return node.querySelector(selectors, exceptionState);
    }

    static PassRefPtrWillBeRawPtr<StaticElementList> querySelectorAll(ContainerNode& node, const AtomicString& selectors, ExceptionState& exceptionState)
    {
        return node.querySelectorAll(selectors, exceptionState);
    }
};

} // namespace blink

#endif // ParentNode_h
