/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef HTMLDetailsElement_h
#define HTMLDetailsElement_h

#include "core/html/HTMLElement.h"

namespace blink {

template<typename T> class EventSender;
typedef EventSender<HTMLDetailsElement> DetailsEventSender;

class HTMLDetailsElement final : public HTMLElement {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<HTMLDetailsElement> create(Document&);
    void toggleOpen();
    ~HTMLDetailsElement() override;

    void dispatchPendingEvent(DetailsEventSender*);

    Element* findMainSummary() const;

private:
    explicit HTMLDetailsElement(Document&);

    LayoutObject* createLayoutObject(const ComputedStyle&) override;
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void didAddUserAgentShadowRoot(ShadowRoot&) override;
    bool isInteractiveContent() const override;

    bool m_isOpen;
};

} // namespace blink

#endif // HTMLDetailsElement_h
