/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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
#include "core/css/StyleSheetList.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLStyleElement.h"
#include "wtf/text/WTFString.h"

namespace blink {

using namespace HTMLNames;

StyleSheetList::StyleSheetList(TreeScope* treeScope)
    : m_treeScope(treeScope)
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(StyleSheetList);

inline const WillBeHeapVector<RefPtrWillBeMember<StyleSheet>>& StyleSheetList::styleSheets()
{
#if !ENABLE(OILPAN)
    if (!m_treeScope)
        return m_detachedStyleSheets;
#endif
    return document()->styleEngine().styleSheetsForStyleSheetList(*m_treeScope);
}

#if !ENABLE(OILPAN)
void StyleSheetList::detachFromDocument()
{
    m_detachedStyleSheets = document()->styleEngine().styleSheetsForStyleSheetList(*m_treeScope);
    m_treeScope = nullptr;
}
#endif

unsigned StyleSheetList::length()
{
    return styleSheets().size();
}

StyleSheet* StyleSheetList::item(unsigned index)
{
    const WillBeHeapVector<RefPtrWillBeMember<StyleSheet>>& sheets = styleSheets();
    return index < sheets.size() ? sheets[index].get() : 0;
}

HTMLStyleElement* StyleSheetList::getNamedItem(const AtomicString& name) const
{
#if !ENABLE(OILPAN)
    if (!m_treeScope)
        return 0;
#endif

    // IE also supports retrieving a stylesheet by name, using the name/id of the <style> tag
    // (this is consistent with all the other collections)
    // ### Bad implementation because returns a single element (are IDs always unique?)
    // and doesn't look for name attribute.
    // But unicity of stylesheet ids is good practice anyway ;)
    // FIXME: We should figure out if we should change this or fix the spec.
    Element* element = m_treeScope->getElementById(name);
    return isHTMLStyleElement(element) ? toHTMLStyleElement(element) : 0;
}

CSSStyleSheet* StyleSheetList::anonymousNamedGetter(const AtomicString& name)
{
    if (document())
        UseCounter::count(*document(), UseCounter::StyleSheetListAnonymousNamedGetter);
    HTMLStyleElement* item = getNamedItem(name);
    if (!item)
        return 0;
    return item->sheet();
}

DEFINE_TRACE(StyleSheetList)
{
    visitor->trace(m_treeScope);
}

} // namespace blink
