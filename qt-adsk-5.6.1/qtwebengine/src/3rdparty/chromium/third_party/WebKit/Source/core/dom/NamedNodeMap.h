/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2010, 2013 Apple Inc. All rights reserved.
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

#ifndef NamedNodeMap_h
#define NamedNodeMap_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/Element.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class Attr;
class ExceptionState;

class NamedNodeMap final : public NoBaseWillBeGarbageCollected<NamedNodeMap>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(NamedNodeMap);
    friend class Element;
public:
    static PassOwnPtrWillBeRawPtr<NamedNodeMap> create(Element* element)
    {
        return adoptPtrWillBeNoop(new NamedNodeMap(element));
    }

#if !ENABLE(OILPAN)
    void ref();
    void deref();
#endif

    // Public DOM interface.

    PassRefPtrWillBeRawPtr<Attr> getNamedItem(const AtomicString&) const;
    PassRefPtrWillBeRawPtr<Attr> removeNamedItem(const AtomicString& name, ExceptionState&);

    PassRefPtrWillBeRawPtr<Attr> getNamedItemNS(const AtomicString& namespaceURI, const AtomicString& localName) const;
    PassRefPtrWillBeRawPtr<Attr> removeNamedItemNS(const AtomicString& namespaceURI, const AtomicString& localName, ExceptionState&);

    PassRefPtrWillBeRawPtr<Attr> setNamedItem(Attr*, ExceptionState&);
    PassRefPtrWillBeRawPtr<Attr> setNamedItemNS(Attr*, ExceptionState&);

    PassRefPtrWillBeRawPtr<Attr> item(unsigned index) const;
    size_t length() const;

    Element* element() const { return m_element; }

    DECLARE_TRACE();

private:
    explicit NamedNodeMap(Element* element)
        : m_element(element)
    {
        // Only supports NamedNodeMaps with Element associated.
        ASSERT(m_element);
    }

    RawPtrWillBeMember<Element> m_element;
};

} // namespace blink

#endif // NamedNodeMap_h
