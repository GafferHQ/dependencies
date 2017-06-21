/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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

#ifndef LineLayoutState_h
#define LineLayoutState_h

#include "core/layout/LayoutBlockFlow.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

// Like LayoutState for layout(), LineLayoutState keeps track of global information
// during an entire linebox tree layout pass (aka layoutInlineChildren).
class LineLayoutState {
public:
    LineLayoutState(bool fullLayout, LayoutUnit& paintInvalidationLogicalTop, LayoutUnit& paintInvalidationLogicalBottom, LayoutFlowThread* flowThread)
        : m_lastFloat(nullptr)
        , m_endLine(nullptr)
        , m_floatIndex(0)
        , m_endLineLogicalTop(0)
        , m_endLineMatched(false)
        , m_checkForFloatsFromLastLine(false)
        , m_hasInlineChild(false)
        , m_isFullLayout(fullLayout)
        , m_paintInvalidationLogicalTop(paintInvalidationLogicalTop)
        , m_paintInvalidationLogicalBottom(paintInvalidationLogicalBottom)
        , m_adjustedLogicalLineTop(0)
        , m_usesPaintInvalidationBounds(false)
        , m_flowThread(flowThread)
    { }

    void markForFullLayout() { m_isFullLayout = true; }
    bool isFullLayout() const { return m_isFullLayout; }

    bool usesPaintInvalidationBounds() const { return m_usesPaintInvalidationBounds; }

    void setPaintInvalidationRange(LayoutUnit logicalHeight)
    {
        m_usesPaintInvalidationBounds = true;
        m_paintInvalidationLogicalTop = m_paintInvalidationLogicalBottom = logicalHeight;
    }

    void updatePaintInvalidationRangeFromBox(RootInlineBox* box, LayoutUnit paginationDelta = 0)
    {
        m_usesPaintInvalidationBounds = true;
        m_paintInvalidationLogicalTop = std::min(m_paintInvalidationLogicalTop, box->logicalTopVisualOverflow() + std::min<LayoutUnit>(paginationDelta, 0));
        m_paintInvalidationLogicalBottom = std::max(m_paintInvalidationLogicalBottom, box->logicalBottomVisualOverflow() + std::max<LayoutUnit>(paginationDelta, 0));
    }

    bool endLineMatched() const { return m_endLineMatched; }
    void setEndLineMatched(bool endLineMatched) { m_endLineMatched = endLineMatched; }

    bool checkForFloatsFromLastLine() const { return m_checkForFloatsFromLastLine; }
    void setCheckForFloatsFromLastLine(bool check) { m_checkForFloatsFromLastLine = check; }

    bool hasInlineChild() const { return m_hasInlineChild; }
    void setHasInlineChild(bool hasInlineChild) { m_hasInlineChild = hasInlineChild; }


    LineInfo& lineInfo() { return m_lineInfo; }
    const LineInfo& lineInfo() const { return m_lineInfo; }

    LayoutUnit endLineLogicalTop() const { return m_endLineLogicalTop; }
    void setEndLineLogicalTop(LayoutUnit logicalTop) { m_endLineLogicalTop = logicalTop; }

    RootInlineBox* endLine() const { return m_endLine; }
    void setEndLine(RootInlineBox* line) { m_endLine = line; }

    FloatingObject* lastFloat() const { return m_lastFloat; }
    void setLastFloat(FloatingObject* lastFloat) { m_lastFloat = lastFloat; }

    Vector<LayoutBlockFlow::FloatWithRect>& floats() { return m_floats; }

    unsigned floatIndex() const { return m_floatIndex; }
    void setFloatIndex(unsigned floatIndex) { m_floatIndex = floatIndex; }

    LayoutUnit adjustedLogicalLineTop() const { return m_adjustedLogicalLineTop; }
    void setAdjustedLogicalLineTop(LayoutUnit value) { m_adjustedLogicalLineTop = value; }

    LayoutFlowThread* flowThread() const { return m_flowThread; }

private:
    Vector<LayoutBlockFlow::FloatWithRect> m_floats;
    FloatingObject* m_lastFloat;
    RootInlineBox* m_endLine;
    LineInfo m_lineInfo;
    unsigned m_floatIndex;
    LayoutUnit m_endLineLogicalTop;
    bool m_endLineMatched;
    bool m_checkForFloatsFromLastLine;
    // Used as a performance optimization to avoid doing a full paint invalidation when our floats
    // change but we don't have any inline children.
    bool m_hasInlineChild;

    bool m_isFullLayout;

    // FIXME: Should this be a range object instead of two ints?
    LayoutUnit& m_paintInvalidationLogicalTop;
    LayoutUnit& m_paintInvalidationLogicalBottom;

    LayoutUnit m_adjustedLogicalLineTop;

    bool m_usesPaintInvalidationBounds;

    LayoutFlowThread* m_flowThread;
};

}

#endif // LineLayoutState_h
