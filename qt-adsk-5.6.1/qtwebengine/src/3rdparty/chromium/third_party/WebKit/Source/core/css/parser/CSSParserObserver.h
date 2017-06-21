/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 - 2010  Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#ifndef CSSParserObserver_h
#define CSSParserObserver_h

#include "core/css/CSSPropertySourceData.h"
#include "wtf/Vector.h"

namespace blink {

class CSSParserToken;
class CSSParserTokenRange;

// This is only for the inspector and shouldn't be used elsewhere.
class CSSParserObserver {
    STACK_ALLOCATED();
public:
    virtual void startRuleHeader(StyleRule::Type, unsigned offset) = 0;
    virtual void endRuleHeader(unsigned offset) = 0;
    virtual void observeSelector(unsigned startOffset, unsigned endOffset) = 0;
    virtual void startRuleBody(unsigned offset) = 0;
    virtual void endRuleBody(unsigned offset) = 0;
    virtual void observeProperty(unsigned startOffset, unsigned endOffset, bool isImportant, bool isParsed) = 0;
    virtual void observeComment(unsigned startOffset, unsigned endOffset) = 0;
    // TODO(timloh): Unused, should be removed
    virtual void startMediaQueryExp(unsigned offset) = 0;
    virtual void endMediaQueryExp(unsigned offset) = 0;
    virtual void startMediaQuery() = 0;
    virtual void endMediaQuery() = 0;
};

} // namespace blink

#endif // CSSParserObserver_h
