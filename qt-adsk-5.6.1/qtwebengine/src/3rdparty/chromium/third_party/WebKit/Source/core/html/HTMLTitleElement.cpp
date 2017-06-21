/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2010 Apple Inc. All rights reserved.
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
#include "core/html/HTMLTitleElement.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/HTMLNames.h"
#include "core/dom/ChildListMutationScope.h"
#include "core/dom/Document.h"
#include "core/dom/Text.h"
#include "core/style/ComputedStyle.h"
#include "core/style/StyleInheritedData.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

inline HTMLTitleElement::HTMLTitleElement(Document& document)
    : HTMLElement(titleTag, document)
    , m_ignoreTitleUpdatesWhenChildrenChange(false)
{
}

DEFINE_NODE_FACTORY(HTMLTitleElement)

Node::InsertionNotificationRequest HTMLTitleElement::insertedInto(ContainerNode* insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    if (inDocument() && !isInShadowTree())
        document().setTitleElement(this);
    return InsertionDone;
}

void HTMLTitleElement::removedFrom(ContainerNode* insertionPoint)
{
    HTMLElement::removedFrom(insertionPoint);
    if (insertionPoint->inDocument() && !insertionPoint->isInShadowTree())
        document().removeTitle(this);
}

void HTMLTitleElement::childrenChanged(const ChildrenChange& change)
{
    HTMLElement::childrenChanged(change);
    if (inDocument() && !isInShadowTree() && !m_ignoreTitleUpdatesWhenChildrenChange)
        document().setTitleElement(this);
}

String HTMLTitleElement::text() const
{
    StringBuilder result;

    for (Node *n = firstChild(); n; n = n->nextSibling()) {
        if (n->isTextNode())
            result.append(toText(n)->data());
    }

    return result.toString();
}

void HTMLTitleElement::setText(const String &value)
{
    RefPtrWillBeRawPtr<Node> protectFromMutationEvents(this);
    ChildListMutationScope mutation(*this);

    // Avoid calling Document::setTitleElement() during intermediate steps.
    m_ignoreTitleUpdatesWhenChildrenChange = !value.isEmpty();
    removeChildren(OmitSubtreeModifiedEvent);
    m_ignoreTitleUpdatesWhenChildrenChange = false;

    if (!value.isEmpty())
        appendChild(document().createTextNode(value.impl()), IGNORE_EXCEPTION);
}

}
