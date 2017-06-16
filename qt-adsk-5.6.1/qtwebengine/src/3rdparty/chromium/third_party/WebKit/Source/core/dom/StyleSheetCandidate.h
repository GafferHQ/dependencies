/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#ifndef StyleSheetCandidate_h
#define StyleSheetCandidate_h

#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;
class Node;
class StyleSheet;

class StyleSheetCandidate {
    STACK_ALLOCATED();
public:
    enum Type {
        HTMLLink,
        HTMLStyle,
        SVGStyle,
        Pi
    };

    StyleSheetCandidate(Node& node)
        : m_node(node)
        , m_type(typeOf(node))
    { }

    bool isXSL() const;
    bool isImport() const;
    bool isAlternate() const;
    bool isEnabledViaScript() const;
    bool isEnabledAndLoading() const;
    bool hasPreferrableName(const String& currentPreferrableName) const;
    bool canBeActivated(const String& currentPreferrableName) const;
    bool isCSSStyle() const;

    StyleSheet* sheet() const;
    AtomicString title() const;
    Document* importedDocument() const;

private:
    bool isElement() const { return m_type != Pi; }
    bool isHTMLLink() const { return m_type == HTMLLink; }
    Node& node() const { return *m_node; }

    static Type typeOf(Node&);

    RawPtrWillBeMember<Node> m_node;
    Type m_type;
};

}

#endif

