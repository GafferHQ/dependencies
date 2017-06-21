/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/layout/LayoutFlexibleBox.h"

#include "core/frame/UseCounter.h"
#include "core/layout/LayoutView.h"
#include "core/layout/TextAutosizer.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"
#include "wtf/MathExtras.h"
#include <limits>

namespace blink {

struct LayoutFlexibleBox::LineContext {
    LineContext(LayoutUnit crossAxisOffset, LayoutUnit crossAxisExtent, size_t numberOfChildren, LayoutUnit maxAscent)
        : crossAxisOffset(crossAxisOffset)
        , crossAxisExtent(crossAxisExtent)
        , numberOfChildren(numberOfChildren)
        , maxAscent(maxAscent)
    {
    }

    LayoutUnit crossAxisOffset;
    LayoutUnit crossAxisExtent;
    size_t numberOfChildren;
    LayoutUnit maxAscent;
};

struct LayoutFlexibleBox::Violation {
    Violation(LayoutBox* child, LayoutUnit childSize)
        : child(child)
        , childSize(childSize)
    {
    }

    LayoutBox* child;
    LayoutUnit childSize;
};


LayoutFlexibleBox::LayoutFlexibleBox(Element* element)
    : LayoutBlock(element)
    , m_orderIterator(this)
    , m_numberOfInFlowChildrenOnFirstLine(-1)
{
    ASSERT(!childrenInline());
}

LayoutFlexibleBox::~LayoutFlexibleBox()
{
}

LayoutFlexibleBox* LayoutFlexibleBox::createAnonymous(Document* document)
{
    LayoutFlexibleBox* layoutObject = new LayoutFlexibleBox(nullptr);
    layoutObject->setDocumentForAnonymous(document);
    return layoutObject;
}

void LayoutFlexibleBox::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    // FIXME: We're ignoring flex-basis here and we shouldn't. We can't start honoring it though until
    // the flex shorthand stops setting it to 0.
    // See https://bugs.webkit.org/show_bug.cgi?id=116117 and http://crbug.com/240765.
    for (LayoutBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;

        LayoutUnit margin = marginIntrinsicLogicalWidthForChild(*child);

        LayoutUnit minPreferredLogicalWidth;
        LayoutUnit maxPreferredLogicalWidth;
        bool hasOrthogonalWritingMode = child->isHorizontalWritingMode() != isHorizontalWritingMode();
        if (hasOrthogonalWritingMode) {
            minPreferredLogicalWidth = maxPreferredLogicalWidth = child->computeLogicalHeightWithoutLayout();
        } else {
            minPreferredLogicalWidth = child->minPreferredLogicalWidth();
            maxPreferredLogicalWidth = child->maxPreferredLogicalWidth();
        }
        ASSERT(minPreferredLogicalWidth >= 0);
        ASSERT(maxPreferredLogicalWidth >= 0);
        minPreferredLogicalWidth += margin;
        maxPreferredLogicalWidth += margin;
        if (!isColumnFlow()) {
            maxLogicalWidth += maxPreferredLogicalWidth;
            if (isMultiline()) {
                // For multiline, the min preferred width is if you put a break between each item.
                minLogicalWidth = std::max(minLogicalWidth, minPreferredLogicalWidth);
            } else {
                minLogicalWidth += minPreferredLogicalWidth;
            }
        } else {
            minLogicalWidth = std::max(minPreferredLogicalWidth, minLogicalWidth);
            maxLogicalWidth = std::max(maxPreferredLogicalWidth, maxLogicalWidth);
        }
    }

    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

    LayoutUnit scrollbarWidth = intrinsicScrollbarLogicalWidth();
    maxLogicalWidth += scrollbarWidth;
    minLogicalWidth += scrollbarWidth;
}

static int synthesizedBaselineFromContentBox(const LayoutBox& box, LineDirectionMode direction)
{
    if (direction == HorizontalLine) {
        return box.size().height() - box.borderBottom() - box.paddingBottom() - box.verticalScrollbarWidth();
    }
    return box.size().width() - box.borderLeft() - box.paddingLeft() - box.horizontalScrollbarHeight();
}

int LayoutFlexibleBox::baselinePosition(FontBaseline, bool, LineDirectionMode direction, LinePositionMode mode) const
{
    ASSERT(mode == PositionOnContainingLine);
    int baseline = firstLineBoxBaseline();
    if (baseline == -1)
        baseline = synthesizedBaselineFromContentBox(*this, direction);

    return beforeMarginInLineDirection(direction) + baseline;
}

int LayoutFlexibleBox::firstLineBoxBaseline() const
{
    if (isWritingModeRoot() || m_numberOfInFlowChildrenOnFirstLine <= 0)
        return -1;
    LayoutBox* baselineChild = nullptr;
    int childNumber = 0;
    for (LayoutBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (child->isOutOfFlowPositioned())
            continue;
        if (alignmentForChild(*child) == ItemPositionBaseline && !hasAutoMarginsInCrossAxis(*child)) {
            baselineChild = child;
            break;
        }
        if (!baselineChild)
            baselineChild = child;

        ++childNumber;
        if (childNumber == m_numberOfInFlowChildrenOnFirstLine)
            break;
    }

    if (!baselineChild)
        return -1;

    if (!isColumnFlow() && hasOrthogonalFlow(*baselineChild))
        return crossAxisExtentForChild(*baselineChild) + baselineChild->logicalTop();
    if (isColumnFlow() && !hasOrthogonalFlow(*baselineChild))
        return mainAxisExtentForChild(*baselineChild) + baselineChild->logicalTop();

    int baseline = baselineChild->firstLineBoxBaseline();
    if (baseline == -1) {
        // FIXME: We should pass |direction| into firstLineBoxBaseline and stop bailing out if we're a writing mode root.
        // This would also fix some cases where the flexbox is orthogonal to its container.
        LineDirectionMode direction = isHorizontalWritingMode() ? HorizontalLine : VerticalLine;
        return synthesizedBaselineFromContentBox(*baselineChild, direction) + baselineChild->logicalTop();
    }

    return baseline + baselineChild->logicalTop();
}

int LayoutFlexibleBox::inlineBlockBaseline(LineDirectionMode direction) const
{
    int baseline = firstLineBoxBaseline();
    if (baseline != -1)
        return baseline;

    int marginAscent = direction == HorizontalLine ? marginTop() : marginRight();
    return synthesizedBaselineFromContentBox(*this, direction) + marginAscent;
}

void LayoutFlexibleBox::removeChild(LayoutObject* child)
{
    LayoutBlock::removeChild(child);
    m_intrinsicSizeAlongMainAxis.remove(child);
}

void LayoutFlexibleBox::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    LayoutBlock::styleDidChange(diff, oldStyle);

    if (oldStyle && oldStyle->alignItemsPosition() == ItemPositionStretch && diff.needsFullLayout()) {
        // Flex items that were previously stretching need to be relayed out so we can compute new available cross axis space.
        // This is only necessary for stretching since other alignment values don't change the size of the box.
        for (LayoutBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            ItemPosition previousAlignment = ComputedStyle::resolveAlignment(*oldStyle, child->styleRef(), ItemPositionStretch);
            if (previousAlignment == ItemPositionStretch && previousAlignment != ComputedStyle::resolveAlignment(styleRef(), child->styleRef(), ItemPositionStretch))
                child->setChildNeedsLayout(MarkOnlyThis);
        }
    }
}

void LayoutFlexibleBox::layoutBlock(bool relayoutChildren)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    if (updateLogicalWidthAndColumnWidth())
        relayoutChildren = true;

    SubtreeLayoutScope layoutScope(*this);
    LayoutUnit previousHeight = logicalHeight();
    setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());

    {
        TextAutosizer::LayoutScope textAutosizerLayoutScope(this);
        LayoutState state(*this, locationOffset());

        m_numberOfInFlowChildrenOnFirstLine = -1;

        LayoutBlock::startDelayUpdateScrollInfo();

        prepareOrderIteratorAndMargins();

        ChildFrameRects oldChildRects;
        appendChildFrameRects(oldChildRects);

        layoutFlexItems(relayoutChildren, layoutScope);

        LayoutBlock::finishDelayUpdateScrollInfo();

        if (logicalHeight() != previousHeight)
            relayoutChildren = true;

        layoutPositionedObjects(relayoutChildren || isDocumentElement());

        // FIXME: css3/flexbox/repaint-rtl-column.html seems to issue paint invalidations for more overflow than it needs to.
        computeOverflow(clientLogicalBottomAfterRepositioning());
    }

    updateLayerTransformAfterLayout();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    updateScrollInfoAfterLayout();

    clearNeedsLayout();
}

void LayoutFlexibleBox::appendChildFrameRects(ChildFrameRects& childFrameRects)
{
    for (LayoutBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (!child->isOutOfFlowPositioned())
            childFrameRects.append(child->frameRect());
    }
}

void LayoutFlexibleBox::paintChildren(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    BlockPainter::paintChildrenOfFlexibleBox(*this, paintInfo, paintOffset);
}

void LayoutFlexibleBox::repositionLogicalHeightDependentFlexItems(Vector<LineContext>& lineContexts)
{
    LayoutUnit crossAxisStartEdge = lineContexts.isEmpty() ? LayoutUnit() : lineContexts[0].crossAxisOffset;
    alignFlexLines(lineContexts);

    alignChildren(lineContexts);

    if (style()->flexWrap() == FlexWrapReverse)
        flipForWrapReverse(lineContexts, crossAxisStartEdge);

    // direction:rtl + flex-direction:column means the cross-axis direction is flipped.
    flipForRightToLeftColumn();
}

LayoutUnit LayoutFlexibleBox::clientLogicalBottomAfterRepositioning()
{
    LayoutUnit maxChildLogicalBottom = 0;
    for (LayoutBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;
        LayoutUnit childLogicalBottom = logicalTopForChild(*child) + logicalHeightForChild(*child) + marginAfterForChild(*child);
        maxChildLogicalBottom = std::max(maxChildLogicalBottom, childLogicalBottom);
    }
    return std::max(clientLogicalBottom(), maxChildLogicalBottom + paddingAfter());
}

bool LayoutFlexibleBox::hasOrthogonalFlow(LayoutBox& child) const
{
    // FIXME: If the child is a flexbox, then we need to check isHorizontalFlow.
    return isHorizontalFlow() != child.isHorizontalWritingMode();
}

bool LayoutFlexibleBox::isColumnFlow() const
{
    return style()->isColumnFlexDirection();
}

bool LayoutFlexibleBox::isHorizontalFlow() const
{
    if (isHorizontalWritingMode())
        return !isColumnFlow();
    return isColumnFlow();
}

bool LayoutFlexibleBox::isLeftToRightFlow() const
{
    if (isColumnFlow())
        return style()->writingMode() == TopToBottomWritingMode || style()->writingMode() == LeftToRightWritingMode;
    return style()->isLeftToRightDirection() ^ (style()->flexDirection() == FlowRowReverse);
}

bool LayoutFlexibleBox::isMultiline() const
{
    return style()->flexWrap() != FlexNoWrap;
}

Length LayoutFlexibleBox::flexBasisForChild(LayoutBox& child) const
{
    Length flexLength = child.style()->flexBasis();
    if (flexLength.isAuto())
        flexLength = isHorizontalFlow() ? child.style()->width() : child.style()->height();
    return flexLength;
}

LayoutUnit LayoutFlexibleBox::crossAxisExtentForChild(LayoutBox& child) const
{
    return isHorizontalFlow() ? child.size().height() : child.size().width();
}

static inline LayoutUnit constrainedChildIntrinsicContentLogicalHeight(LayoutBox& child)
{
    LayoutUnit childIntrinsicContentLogicalHeight = child.intrinsicContentLogicalHeight();
    return child.constrainLogicalHeightByMinMax(childIntrinsicContentLogicalHeight + child.borderAndPaddingLogicalHeight(), childIntrinsicContentLogicalHeight);
}

LayoutUnit LayoutFlexibleBox::childIntrinsicHeight(LayoutBox& child) const
{
    if (child.isHorizontalWritingMode() && needToStretchChildLogicalHeight(child))
        return constrainedChildIntrinsicContentLogicalHeight(child);
    return child.size().height();
}

LayoutUnit LayoutFlexibleBox::childIntrinsicWidth(LayoutBox& child) const
{
    if (!child.isHorizontalWritingMode() && needToStretchChildLogicalHeight(child))
        return constrainedChildIntrinsicContentLogicalHeight(child);
    return child.size().width();
}

bool LayoutFlexibleBox::mainAxisExtentIsDefinite() const
{
    return isColumnFlow() ? hasDefiniteLogicalHeight() : hasDefiniteLogicalWidth();
}

LayoutUnit LayoutFlexibleBox::crossAxisIntrinsicExtentForChild(LayoutBox& child) const
{
    return isHorizontalFlow() ? childIntrinsicHeight(child) : childIntrinsicWidth(child);
}

LayoutUnit LayoutFlexibleBox::mainAxisExtentForChild(LayoutBox& child) const
{
    return isHorizontalFlow() ? child.size().width() : child.size().height();
}

LayoutUnit LayoutFlexibleBox::crossAxisExtent() const
{
    return isHorizontalFlow() ? size().height() : size().width();
}

LayoutUnit LayoutFlexibleBox::mainAxisExtent() const
{
    return isHorizontalFlow() ? size().width() : size().height();
}

LayoutUnit LayoutFlexibleBox::crossAxisContentExtent() const
{
    return isHorizontalFlow() ? contentHeight() : contentWidth();
}

LayoutUnit LayoutFlexibleBox::mainAxisContentExtent(LayoutUnit contentLogicalHeight)
{
    if (isColumnFlow()) {
        LogicalExtentComputedValues computedValues;
        LayoutUnit borderPaddingAndScrollbar = borderAndPaddingLogicalHeight() + scrollbarLogicalHeight();
        LayoutUnit borderBoxLogicalHeight = contentLogicalHeight + borderPaddingAndScrollbar;
        computeLogicalHeight(borderBoxLogicalHeight, logicalTop(), computedValues);
        if (computedValues.m_extent == LayoutUnit::max())
            return computedValues.m_extent;
        return std::max(LayoutUnit(), computedValues.m_extent - borderPaddingAndScrollbar);
    }
    return contentLogicalWidth();
}

LayoutUnit LayoutFlexibleBox::computeMainAxisExtentForChild(LayoutBox& child, SizeType sizeType, const Length& size)
{
    // If we have a horizontal flow, that means the main size is the width.
    // That's the logical width for horizontal writing modes, and the logical height in vertical writing modes.
    // For a vertical flow, main size is the height, so it's the inverse.
    // So we need the logical width if we have a horizontal flow and horizontal writing mode, or vertical flow and vertical writing mode.
    // Otherwise we need the logical height.
    if (isHorizontalFlow() != child.styleRef().isHorizontalWritingMode()) {
        // We don't have to check for "auto" here - computeContentLogicalHeight will just return -1 for that case anyway.
        if (size.isIntrinsic())
            child.layoutIfNeeded();
        return child.computeContentLogicalHeight(sizeType, size, child.logicalHeight() - child.borderAndPaddingLogicalHeight()) + child.scrollbarLogicalHeight();
    }
    return child.computeLogicalWidthUsing(sizeType, size, contentLogicalWidth(), this) - child.borderAndPaddingLogicalWidth();
}

WritingMode LayoutFlexibleBox::transformedWritingMode() const
{
    WritingMode mode = style()->writingMode();
    if (!isColumnFlow())
        return mode;

    switch (mode) {
    case TopToBottomWritingMode:
    case BottomToTopWritingMode:
        return style()->isLeftToRightDirection() ? LeftToRightWritingMode : RightToLeftWritingMode;
    case LeftToRightWritingMode:
    case RightToLeftWritingMode:
        return style()->isLeftToRightDirection() ? TopToBottomWritingMode : BottomToTopWritingMode;
    }
    ASSERT_NOT_REACHED();
    return TopToBottomWritingMode;
}

LayoutUnit LayoutFlexibleBox::flowAwareBorderStart() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? borderLeft() : borderRight();
    return isLeftToRightFlow() ? borderTop() : borderBottom();
}

LayoutUnit LayoutFlexibleBox::flowAwareBorderEnd() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? borderRight() : borderLeft();
    return isLeftToRightFlow() ? borderBottom() : borderTop();
}

LayoutUnit LayoutFlexibleBox::flowAwareBorderBefore() const
{
    switch (transformedWritingMode()) {
    case TopToBottomWritingMode:
        return borderTop();
    case BottomToTopWritingMode:
        return borderBottom();
    case LeftToRightWritingMode:
        return borderLeft();
    case RightToLeftWritingMode:
        return borderRight();
    }
    ASSERT_NOT_REACHED();
    return borderTop();
}

LayoutUnit LayoutFlexibleBox::flowAwareBorderAfter() const
{
    switch (transformedWritingMode()) {
    case TopToBottomWritingMode:
        return borderBottom();
    case BottomToTopWritingMode:
        return borderTop();
    case LeftToRightWritingMode:
        return borderRight();
    case RightToLeftWritingMode:
        return borderLeft();
    }
    ASSERT_NOT_REACHED();
    return borderTop();
}

LayoutUnit LayoutFlexibleBox::flowAwarePaddingStart() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? paddingLeft() : paddingRight();
    return isLeftToRightFlow() ? paddingTop() : paddingBottom();
}

LayoutUnit LayoutFlexibleBox::flowAwarePaddingEnd() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? paddingRight() : paddingLeft();
    return isLeftToRightFlow() ? paddingBottom() : paddingTop();
}

LayoutUnit LayoutFlexibleBox::flowAwarePaddingBefore() const
{
    switch (transformedWritingMode()) {
    case TopToBottomWritingMode:
        return paddingTop();
    case BottomToTopWritingMode:
        return paddingBottom();
    case LeftToRightWritingMode:
        return paddingLeft();
    case RightToLeftWritingMode:
        return paddingRight();
    }
    ASSERT_NOT_REACHED();
    return paddingTop();
}

LayoutUnit LayoutFlexibleBox::flowAwarePaddingAfter() const
{
    switch (transformedWritingMode()) {
    case TopToBottomWritingMode:
        return paddingBottom();
    case BottomToTopWritingMode:
        return paddingTop();
    case LeftToRightWritingMode:
        return paddingRight();
    case RightToLeftWritingMode:
        return paddingLeft();
    }
    ASSERT_NOT_REACHED();
    return paddingTop();
}

LayoutUnit LayoutFlexibleBox::flowAwareMarginStartForChild(LayoutBox& child) const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? child.marginLeft() : child.marginRight();
    return isLeftToRightFlow() ? child.marginTop() : child.marginBottom();
}

LayoutUnit LayoutFlexibleBox::flowAwareMarginEndForChild(LayoutBox& child) const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? child.marginRight() : child.marginLeft();
    return isLeftToRightFlow() ? child.marginBottom() : child.marginTop();
}

LayoutUnit LayoutFlexibleBox::flowAwareMarginBeforeForChild(LayoutBox& child) const
{
    switch (transformedWritingMode()) {
    case TopToBottomWritingMode:
        return child.marginTop();
    case BottomToTopWritingMode:
        return child.marginBottom();
    case LeftToRightWritingMode:
        return child.marginLeft();
    case RightToLeftWritingMode:
        return child.marginRight();
    }
    ASSERT_NOT_REACHED();
    return marginTop();
}

LayoutUnit LayoutFlexibleBox::crossAxisMarginExtentForChild(LayoutBox& child) const
{
    return isHorizontalFlow() ? child.marginHeight() : child.marginWidth();
}

LayoutUnit LayoutFlexibleBox::crossAxisScrollbarExtent() const
{
    return isHorizontalFlow() ? horizontalScrollbarHeight() : verticalScrollbarWidth();
}

LayoutUnit LayoutFlexibleBox::crossAxisScrollbarExtentForChild(LayoutBox& child) const
{
    return isHorizontalFlow() ? child.horizontalScrollbarHeight() : child.verticalScrollbarWidth();
}

LayoutPoint LayoutFlexibleBox::flowAwareLocationForChild(LayoutBox& child) const
{
    return isHorizontalFlow() ? child.location() : child.location().transposedPoint();
}

void LayoutFlexibleBox::setFlowAwareLocationForChild(LayoutBox& child, const LayoutPoint& location)
{
    if (isHorizontalFlow())
        child.setLocationAndUpdateOverflowControlsIfNeeded(location);
    else
        child.setLocationAndUpdateOverflowControlsIfNeeded(location.transposedPoint());
}

LayoutUnit LayoutFlexibleBox::mainAxisBorderAndPaddingExtentForChild(LayoutBox& child) const
{
    return isHorizontalFlow() ? child.borderAndPaddingWidth() : child.borderAndPaddingHeight();
}

bool LayoutFlexibleBox::mainAxisLengthIsIndefinite(const Length& flexBasis) const
{
    return flexBasis.isAuto() || (flexBasis.hasPercent() && !mainAxisExtentIsDefinite());
}

bool LayoutFlexibleBox::childFlexBaseSizeRequiresLayout(LayoutBox& child) const
{
    return mainAxisLengthIsIndefinite(flexBasisForChild(child)) && hasOrthogonalFlow(child);
}

LayoutUnit LayoutFlexibleBox::computeInnerFlexBaseSizeForChild(LayoutBox& child, ChildLayoutType childLayoutType)
{
    child.clearOverrideSize();

    if (child.isImage() || child.isVideo() || child.isCanvas())
        UseCounter::count(document(), UseCounter::AspectRatioFlexItem);

    Length flexBasis = flexBasisForChild(child);
    if (mainAxisLengthIsIndefinite(flexBasis)) {
        LayoutUnit mainAxisExtent;
        if (hasOrthogonalFlow(child)) {
            if (childLayoutType == NeverLayout)
                return LayoutUnit();

            if (child.needsLayout() || childLayoutType == ForceLayout || !m_intrinsicSizeAlongMainAxis.contains(&child)) {
                m_intrinsicSizeAlongMainAxis.remove(&child);
                child.forceChildLayout();
                m_intrinsicSizeAlongMainAxis.set(&child, child.logicalHeight());
            }
            mainAxisExtent = m_intrinsicSizeAlongMainAxis.get(&child);
        } else {
            mainAxisExtent = child.maxPreferredLogicalWidth();
        }
        ASSERT(mainAxisExtent - mainAxisBorderAndPaddingExtentForChild(child) >= 0);
        return mainAxisExtent - mainAxisBorderAndPaddingExtentForChild(child);
    }
    return std::max(LayoutUnit(), computeMainAxisExtentForChild(child, MainOrPreferredSize, flexBasis));
}

void LayoutFlexibleBox::layoutFlexItems(bool relayoutChildren, SubtreeLayoutScope& layoutScope)
{
    Vector<LineContext> lineContexts;
    OrderedFlexItemList orderedChildren;
    LayoutUnit sumFlexBaseSize;
    double totalFlexGrow;
    double totalWeightedFlexShrink;
    LayoutUnit sumHypotheticalMainSize;

    Vector<LayoutUnit, 16> childSizes;

    m_orderIterator.first();
    LayoutUnit crossAxisOffset = flowAwareBorderBefore() + flowAwarePaddingBefore();
    while (computeNextFlexLine(orderedChildren, sumFlexBaseSize, totalFlexGrow, totalWeightedFlexShrink, sumHypotheticalMainSize, relayoutChildren)) {
        LayoutUnit containerMainInnerSize = mainAxisContentExtent(sumHypotheticalMainSize);
        LayoutUnit availableFreeSpace = containerMainInnerSize - sumFlexBaseSize;
        FlexSign flexSign = (sumHypotheticalMainSize < containerMainInnerSize) ? PositiveFlexibility : NegativeFlexibility;
        InflexibleFlexItemSize inflexibleItems;
        childSizes.reserveCapacity(orderedChildren.size());
        while (!resolveFlexibleLengths(flexSign, orderedChildren, availableFreeSpace, totalFlexGrow, totalWeightedFlexShrink, inflexibleItems, childSizes)) {
            ASSERT(totalFlexGrow >= 0 && totalWeightedFlexShrink >= 0);
            ASSERT(inflexibleItems.size() > 0);
        }

        layoutAndPlaceChildren(crossAxisOffset, orderedChildren, childSizes, availableFreeSpace, relayoutChildren, layoutScope, lineContexts);
    }
    if (hasLineIfEmpty()) {
        // Even if computeNextFlexLine returns true, the flexbox might not have
        // a line because all our children might be out of flow positioned.
        // Instead of just checking if we have a line, make sure the flexbox
        // has at least a line's worth of height to cover this case.
        LayoutUnit minHeight = minimumLogicalHeightForEmptyLine();
        if (size().height() < minHeight)
            setLogicalHeight(minHeight);
    }

    updateLogicalHeight();
    repositionLogicalHeightDependentFlexItems(lineContexts);
}

LayoutUnit LayoutFlexibleBox::autoMarginOffsetInMainAxis(const OrderedFlexItemList& children, LayoutUnit& availableFreeSpace)
{
    if (availableFreeSpace <= 0)
        return 0;

    int numberOfAutoMargins = 0;
    bool isHorizontal = isHorizontalFlow();
    for (size_t i = 0; i < children.size(); ++i) {
        LayoutBox* child = children[i];
        if (child->isOutOfFlowPositioned())
            continue;
        if (isHorizontal) {
            if (child->style()->marginLeft().isAuto())
                ++numberOfAutoMargins;
            if (child->style()->marginRight().isAuto())
                ++numberOfAutoMargins;
        } else {
            if (child->style()->marginTop().isAuto())
                ++numberOfAutoMargins;
            if (child->style()->marginBottom().isAuto())
                ++numberOfAutoMargins;
        }
    }
    if (!numberOfAutoMargins)
        return 0;

    LayoutUnit sizeOfAutoMargin = availableFreeSpace / numberOfAutoMargins;
    availableFreeSpace = 0;
    return sizeOfAutoMargin;
}

void LayoutFlexibleBox::updateAutoMarginsInMainAxis(LayoutBox& child, LayoutUnit autoMarginOffset)
{
    ASSERT(autoMarginOffset >= 0);

    if (isHorizontalFlow()) {
        if (child.style()->marginLeft().isAuto())
            child.setMarginLeft(autoMarginOffset);
        if (child.style()->marginRight().isAuto())
            child.setMarginRight(autoMarginOffset);
    } else {
        if (child.style()->marginTop().isAuto())
            child.setMarginTop(autoMarginOffset);
        if (child.style()->marginBottom().isAuto())
            child.setMarginBottom(autoMarginOffset);
    }
}

bool LayoutFlexibleBox::hasAutoMarginsInCrossAxis(LayoutBox& child) const
{
    if (isHorizontalFlow())
        return child.style()->marginTop().isAuto() || child.style()->marginBottom().isAuto();
    return child.style()->marginLeft().isAuto() || child.style()->marginRight().isAuto();
}

LayoutUnit LayoutFlexibleBox::availableAlignmentSpaceForChild(LayoutUnit lineCrossAxisExtent, LayoutBox& child)
{
    ASSERT(!child.isOutOfFlowPositioned());
    LayoutUnit childCrossExtent = crossAxisMarginExtentForChild(child) + crossAxisExtentForChild(child);
    return lineCrossAxisExtent - childCrossExtent;
}

LayoutUnit LayoutFlexibleBox::availableAlignmentSpaceForChildBeforeStretching(LayoutUnit lineCrossAxisExtent, LayoutBox& child)
{
    ASSERT(!child.isOutOfFlowPositioned());
    LayoutUnit childCrossExtent = crossAxisMarginExtentForChild(child) + crossAxisIntrinsicExtentForChild(child);
    return lineCrossAxisExtent - childCrossExtent;
}

bool LayoutFlexibleBox::updateAutoMarginsInCrossAxis(LayoutBox& child, LayoutUnit availableAlignmentSpace)
{
    ASSERT(!child.isOutOfFlowPositioned());
    ASSERT(availableAlignmentSpace >= 0);

    bool isHorizontal = isHorizontalFlow();
    Length topOrLeft = isHorizontal ? child.style()->marginTop() : child.style()->marginLeft();
    Length bottomOrRight = isHorizontal ? child.style()->marginBottom() : child.style()->marginRight();
    if (topOrLeft.isAuto() && bottomOrRight.isAuto()) {
        adjustAlignmentForChild(child, availableAlignmentSpace / 2);
        if (isHorizontal) {
            child.setMarginTop(availableAlignmentSpace / 2);
            child.setMarginBottom(availableAlignmentSpace / 2);
        } else {
            child.setMarginLeft(availableAlignmentSpace / 2);
            child.setMarginRight(availableAlignmentSpace / 2);
        }
        return true;
    }
    bool shouldAdjustTopOrLeft = true;
    if (isColumnFlow() && !child.style()->isLeftToRightDirection()) {
        // For column flows, only make this adjustment if topOrLeft corresponds to the "before" margin,
        // so that flipForRightToLeftColumn will do the right thing.
        shouldAdjustTopOrLeft = false;
    }
    if (!isColumnFlow() && child.style()->isFlippedBlocksWritingMode()) {
        // If we are a flipped writing mode, we need to adjust the opposite side. This is only needed
        // for row flows because this only affects the block-direction axis.
        shouldAdjustTopOrLeft = false;
    }

    if (topOrLeft.isAuto()) {
        if (shouldAdjustTopOrLeft)
            adjustAlignmentForChild(child, availableAlignmentSpace);

        if (isHorizontal)
            child.setMarginTop(availableAlignmentSpace);
        else
            child.setMarginLeft(availableAlignmentSpace);
        return true;
    }
    if (bottomOrRight.isAuto()) {
        if (!shouldAdjustTopOrLeft)
            adjustAlignmentForChild(child, availableAlignmentSpace);

        if (isHorizontal)
            child.setMarginBottom(availableAlignmentSpace);
        else
            child.setMarginRight(availableAlignmentSpace);
        return true;
    }
    return false;
}

LayoutUnit LayoutFlexibleBox::marginBoxAscentForChild(LayoutBox& child)
{
    LayoutUnit ascent = child.firstLineBoxBaseline();
    if (ascent == -1)
        ascent = crossAxisExtentForChild(child);
    return ascent + flowAwareMarginBeforeForChild(child);
}

LayoutUnit LayoutFlexibleBox::computeChildMarginValue(Length margin)
{
    // When resolving the margins, we use the content size for resolving percent and calc (for percents in calc expressions) margins.
    // Fortunately, percent margins are always computed with respect to the block's width, even for margin-top and margin-bottom.
    LayoutUnit availableSize = contentLogicalWidth();
    return minimumValueForLength(margin, availableSize);
}

void LayoutFlexibleBox::prepareOrderIteratorAndMargins()
{
    OrderIteratorPopulator populator(m_orderIterator);

    for (LayoutBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        populator.collectChild(child);

        if (child->isOutOfFlowPositioned())
            continue;

        // Before running the flex algorithm, 'auto' has a margin of 0.
        // Also, if we're not auto sizing, we don't do a layout that computes the start/end margins.
        if (isHorizontalFlow()) {
            child->setMarginLeft(computeChildMarginValue(child->style()->marginLeft()));
            child->setMarginRight(computeChildMarginValue(child->style()->marginRight()));
        } else {
            child->setMarginTop(computeChildMarginValue(child->style()->marginTop()));
            child->setMarginBottom(computeChildMarginValue(child->style()->marginBottom()));
        }
    }
}

LayoutUnit LayoutFlexibleBox::adjustChildSizeForMinAndMax(LayoutBox& child, LayoutUnit childSize, bool childShrunk)
{
    Length max = isHorizontalFlow() ? child.style()->maxWidth() : child.style()->maxHeight();
    LayoutUnit maxExtent = -1;
    if (max.isSpecifiedOrIntrinsic()) {
        maxExtent = computeMainAxisExtentForChild(child, MaxSize, max);
        if (maxExtent != -1 && childSize > maxExtent)
            childSize = maxExtent;
    }

    Length min = isHorizontalFlow() ? child.style()->minWidth() : child.style()->minHeight();
    LayoutUnit minExtent = 0;
    if (min.isSpecifiedOrIntrinsic()) {
        minExtent = computeMainAxisExtentForChild(child, MinSize, min);
        // computeMainAxisExtentForChild can return -1 when the child has a percentage
        // min size, but we have an indefinite size in that axis.
        minExtent = std::max(LayoutUnit(), minExtent);
    } else if (childShrunk && min.isAuto() && mainAxisOverflowForChild(child) == OVISIBLE) {
        // css-flexbox section 4.5
        LayoutUnit contentSize = computeMainAxisExtentForChild(child, MinSize, Length(MinContent));
        ASSERT(contentSize >= 0);
        if (maxExtent != -1 && contentSize > maxExtent)
            contentSize = maxExtent;

        Length mainSize = isHorizontalFlow() ? child.styleRef().width() : child.styleRef().height();
        if (!mainAxisLengthIsIndefinite(mainSize)) {
            LayoutUnit resolvedMainSize = computeMainAxisExtentForChild(child, MainOrPreferredSize, mainSize);
            ASSERT(resolvedMainSize >= 0);
            LayoutUnit specifiedSize = maxExtent != -1 ? std::min(resolvedMainSize, maxExtent) : resolvedMainSize;

            minExtent = std::min(specifiedSize, contentSize);
        } else {
            minExtent = contentSize;
        }
        // TODO(cbiesinger): Implement aspect ratio handling (here, transferred size) - crbug.com/249112
    }
    ASSERT(minExtent >= 0);
    return std::max(childSize, minExtent);
}

bool LayoutFlexibleBox::computeNextFlexLine(OrderedFlexItemList& orderedChildren, LayoutUnit& sumFlexBaseSize, double& totalFlexGrow, double& totalWeightedFlexShrink, LayoutUnit& sumHypotheticalMainSize, bool relayoutChildren)
{
    orderedChildren.clear();
    sumFlexBaseSize = 0;
    totalFlexGrow = totalWeightedFlexShrink = 0;
    sumHypotheticalMainSize = 0;

    if (!m_orderIterator.currentChild())
        return false;

    LayoutUnit lineBreakLength = mainAxisContentExtent(LayoutUnit::max());

    bool lineHasInFlowItem = false;

    for (LayoutBox* child = m_orderIterator.currentChild(); child; child = m_orderIterator.next()) {
        if (child->isOutOfFlowPositioned()) {
            orderedChildren.append(child);
            continue;
        }

        LayoutUnit childInnerFlexBaseSize = computeInnerFlexBaseSizeForChild(*child, relayoutChildren ? ForceLayout : LayoutIfNeeded);
        LayoutUnit childMainAxisMarginBorderPadding = mainAxisBorderAndPaddingExtentForChild(*child)
            + (isHorizontalFlow() ? child->marginWidth() : child->marginHeight());
        LayoutUnit childOuterFlexBaseSize = childInnerFlexBaseSize + childMainAxisMarginBorderPadding;

        LayoutUnit childMinMaxAppliedMainAxisExtent = adjustChildSizeForMinAndMax(*child, childInnerFlexBaseSize);
        LayoutUnit childHypotheticalMainSize = childMinMaxAppliedMainAxisExtent + childMainAxisMarginBorderPadding;

        if (isMultiline() && sumHypotheticalMainSize + childHypotheticalMainSize > lineBreakLength && lineHasInFlowItem)
            break;
        orderedChildren.append(child);
        lineHasInFlowItem  = true;
        sumFlexBaseSize += childOuterFlexBaseSize;
        totalFlexGrow += child->style()->flexGrow();
        totalWeightedFlexShrink += child->style()->flexShrink() * childInnerFlexBaseSize;
        sumHypotheticalMainSize += childHypotheticalMainSize;
    }
    return true;
}

void LayoutFlexibleBox::freezeViolations(const Vector<Violation>& violations, LayoutUnit& availableFreeSpace, double& totalFlexGrow, double& totalWeightedFlexShrink, InflexibleFlexItemSize& inflexibleItems)
{
    for (size_t i = 0; i < violations.size(); ++i) {
        LayoutBox* child = violations[i].child;
        LayoutUnit childSize = violations[i].childSize;
        LayoutUnit childInnerFlexBaseSize = computeInnerFlexBaseSizeForChild(*child);
        availableFreeSpace -= childSize - childInnerFlexBaseSize;
        totalFlexGrow -= child->style()->flexGrow();
        totalWeightedFlexShrink -= child->style()->flexShrink() * childInnerFlexBaseSize;
        inflexibleItems.set(child, childSize);
    }
}

// Returns true if we successfully ran the algorithm and sized the flex items.
bool LayoutFlexibleBox::resolveFlexibleLengths(FlexSign flexSign, const OrderedFlexItemList& children, LayoutUnit& availableFreeSpace, double& totalFlexGrow, double& totalWeightedFlexShrink, InflexibleFlexItemSize& inflexibleItems, Vector<LayoutUnit, 16>& childSizes)
{
    childSizes.resize(0);
    LayoutUnit totalViolation = 0;
    LayoutUnit usedFreeSpace = 0;
    Vector<Violation> minViolations;
    Vector<Violation> maxViolations;
    for (size_t i = 0; i < children.size(); ++i) {
        LayoutBox* child = children[i];
        if (child->isOutOfFlowPositioned()) {
            childSizes.append(0);
            continue;
        }

        if (inflexibleItems.contains(child)) {
            childSizes.append(inflexibleItems.get(child));
        } else {
            LayoutUnit childInnerFlexBaseSize = computeInnerFlexBaseSizeForChild(*child);
            LayoutUnit childSize = childInnerFlexBaseSize;
            double extraSpace = 0;
            bool childShrunk = false;
            if (availableFreeSpace > 0 && totalFlexGrow > 0 && flexSign == PositiveFlexibility && std::isfinite(totalFlexGrow)) {
                extraSpace = availableFreeSpace * child->style()->flexGrow() / totalFlexGrow;
            } else if (availableFreeSpace < 0 && totalWeightedFlexShrink > 0 && flexSign == NegativeFlexibility && std::isfinite(totalWeightedFlexShrink) && child->style()->flexShrink()) {
                extraSpace = availableFreeSpace * child->style()->flexShrink() * childInnerFlexBaseSize / totalWeightedFlexShrink;
                childShrunk = true;
            }
            if (std::isfinite(extraSpace))
                childSize += LayoutUnit::fromFloatRound(extraSpace);

            LayoutUnit adjustedChildSize = adjustChildSizeForMinAndMax(*child, childSize, childShrunk);
            ASSERT(adjustedChildSize >= 0);
            childSizes.append(adjustedChildSize);
            usedFreeSpace += adjustedChildSize - childInnerFlexBaseSize;

            LayoutUnit violation = adjustedChildSize - childSize;
            if (violation > 0)
                minViolations.append(Violation(child, adjustedChildSize));
            else if (violation < 0)
                maxViolations.append(Violation(child, adjustedChildSize));
            totalViolation += violation;
        }
    }

    if (totalViolation)
        freezeViolations(totalViolation < 0 ? maxViolations : minViolations, availableFreeSpace, totalFlexGrow, totalWeightedFlexShrink, inflexibleItems);
    else
        availableFreeSpace -= usedFreeSpace;

    return !totalViolation;
}

static LayoutUnit initialJustifyContentOffset(LayoutUnit availableFreeSpace, ContentPosition justifyContent, ContentDistributionType justifyContentDistribution, unsigned numberOfChildren)
{
    if (justifyContent == ContentPositionFlexEnd)
        return availableFreeSpace;
    if (justifyContent == ContentPositionCenter)
        return availableFreeSpace / 2;
    if (justifyContentDistribution == ContentDistributionSpaceAround) {
        if (availableFreeSpace > 0 && numberOfChildren)
            return availableFreeSpace / (2 * numberOfChildren);

        return availableFreeSpace / 2;
    }
    return 0;
}

static LayoutUnit justifyContentSpaceBetweenChildren(LayoutUnit availableFreeSpace, ContentDistributionType justifyContentDistribution, unsigned numberOfChildren)
{
    if (availableFreeSpace > 0 && numberOfChildren > 1) {
        if (justifyContentDistribution == ContentDistributionSpaceBetween)
            return availableFreeSpace / (numberOfChildren - 1);
        if (justifyContentDistribution == ContentDistributionSpaceAround)
            return availableFreeSpace / numberOfChildren;
    }
    return 0;
}

void LayoutFlexibleBox::setOverrideMainAxisSizeForChild(LayoutBox& child, LayoutUnit childPreferredSize)
{
    if (hasOrthogonalFlow(child))
        child.setOverrideLogicalContentHeight(childPreferredSize - child.borderAndPaddingLogicalHeight());
    else
        child.setOverrideLogicalContentWidth(childPreferredSize - child.borderAndPaddingLogicalWidth());
}

void LayoutFlexibleBox::prepareChildForPositionedLayout(LayoutBox& child, LayoutUnit mainAxisOffset, LayoutUnit crossAxisOffset, PositionedLayoutMode layoutMode)
{
    ASSERT(child.isOutOfFlowPositioned());
    child.containingBlock()->insertPositionedObject(&child);
    DeprecatedPaintLayer* childLayer = child.layer();
    LayoutUnit inlinePosition = isColumnFlow() ? crossAxisOffset : mainAxisOffset;
    if (layoutMode == FlipForRowReverse && style()->flexDirection() == FlowRowReverse)
        inlinePosition = mainAxisExtent() - mainAxisOffset;
    childLayer->setStaticInlinePosition(inlinePosition);

    LayoutUnit staticBlockPosition = isColumnFlow() ? mainAxisOffset : crossAxisOffset;
    if (childLayer->staticBlockPosition() != staticBlockPosition) {
        childLayer->setStaticBlockPosition(staticBlockPosition);
        if (child.style()->hasStaticBlockPosition(style()->isHorizontalWritingMode()))
            child.setChildNeedsLayout(MarkOnlyThis);
    }
}

ItemPosition LayoutFlexibleBox::alignmentForChild(LayoutBox& child) const
{
    ItemPosition align = ComputedStyle::resolveAlignment(styleRef(), child.styleRef(), ItemPositionStretch);

    if (align == ItemPositionBaseline && hasOrthogonalFlow(child))
        align = ItemPositionFlexStart;

    if (style()->flexWrap() == FlexWrapReverse) {
        if (align == ItemPositionFlexStart)
            align = ItemPositionFlexEnd;
        else if (align == ItemPositionFlexEnd)
            align = ItemPositionFlexStart;
    }

    return align;
}

size_t LayoutFlexibleBox::numberOfInFlowPositionedChildren(const OrderedFlexItemList& children) const
{
    size_t count = 0;
    for (size_t i = 0; i < children.size(); ++i) {
        LayoutBox* child = children[i];
        if (!child->isOutOfFlowPositioned())
            ++count;
    }
    return count;
}

void LayoutFlexibleBox::resetAutoMarginsAndLogicalTopInCrossAxis(LayoutBox& child)
{
    if (hasAutoMarginsInCrossAxis(child)) {
        child.updateLogicalHeight();
        if (isHorizontalFlow()) {
            if (child.style()->marginTop().isAuto())
                child.setMarginTop(0);
            if (child.style()->marginBottom().isAuto())
                child.setMarginBottom(0);
        } else {
            if (child.style()->marginLeft().isAuto())
                child.setMarginLeft(0);
            if (child.style()->marginRight().isAuto())
                child.setMarginRight(0);
        }
    }
}

bool LayoutFlexibleBox::needToStretchChildLogicalHeight(LayoutBox& child) const
{
    // This function is a little bit magical. It relies on the fact that blocks intrinsically
    // "stretch" themselves in their inline axis, i.e. a <div> has an implicit width: 100%.
    // Therefore, we never need to stretch an item if we're a vertical flow, because the child
    // will automatically stretch itself.
    // TODO(cbiesinger): this code is wrong when the child has an orthogonal flow and we're vertical. crbug.com/482766
    if (alignmentForChild(child) != ItemPositionStretch)
        return false;

    return isHorizontalFlow() && child.style()->height().isAuto();
}

EOverflow LayoutFlexibleBox::mainAxisOverflowForChild(LayoutBox& child) const
{
    if (isHorizontalFlow())
        return child.styleRef().overflowX();
    return child.styleRef().overflowY();
}

void LayoutFlexibleBox::layoutAndPlaceChildren(LayoutUnit& crossAxisOffset, const OrderedFlexItemList& children, const Vector<LayoutUnit, 16>& childSizes, LayoutUnit availableFreeSpace, bool relayoutChildren, SubtreeLayoutScope& layoutScope, Vector<LineContext>& lineContexts)
{
    ASSERT(childSizes.size() == children.size());

    size_t numberOfChildrenForJustifyContent = numberOfInFlowPositionedChildren(children);
    LayoutUnit autoMarginOffset = autoMarginOffsetInMainAxis(children, availableFreeSpace);
    LayoutUnit mainAxisOffset = flowAwareBorderStart() + flowAwarePaddingStart();
    mainAxisOffset += initialJustifyContentOffset(availableFreeSpace, style()->justifyContentPosition(), style()->justifyContentDistribution(), numberOfChildrenForJustifyContent);
    if (style()->flexDirection() == FlowRowReverse)
        mainAxisOffset += isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    LayoutUnit totalMainExtent = mainAxisExtent();
    LayoutUnit maxAscent = 0, maxDescent = 0; // Used when align-items: baseline.
    LayoutUnit maxChildCrossAxisExtent = 0;
    size_t seenInFlowPositionedChildren = 0;
    bool shouldFlipMainAxis = !isColumnFlow() && !isLeftToRightFlow();
    for (size_t i = 0; i < children.size(); ++i) {
        LayoutBox* child = children[i];

        if (child->isOutOfFlowPositioned()) {
            prepareChildForPositionedLayout(*child, mainAxisOffset, crossAxisOffset, FlipForRowReverse);
            continue;
        }

        // FIXME Investigate if this can be removed based on other flags. crbug.com/370010
        child->setMayNeedPaintInvalidation();

        LayoutUnit childPreferredSize = childSizes[i] + mainAxisBorderAndPaddingExtentForChild(*child);
        setOverrideMainAxisSizeForChild(*child, childPreferredSize);
        if (childPreferredSize != mainAxisExtentForChild(*child)) {
            child->setChildNeedsLayout(MarkOnlyThis);
        } else {
            // To avoid double applying margin changes in updateAutoMarginsInCrossAxis, we reset the margins here.
            resetAutoMarginsAndLogicalTopInCrossAxis(*child);
        }
        // We may have already forced relayout for orthogonal flowing children in computeInnerFlexBaseSizeForChild.
        bool forceChildRelayout = relayoutChildren && !childFlexBaseSizeRequiresLayout(*child);
        updateBlockChildDirtyBitsBeforeLayout(forceChildRelayout, *child);
        if (!child->needsLayout())
            child->markForPaginationRelayoutIfNeeded(layoutScope);
        child->layoutIfNeeded();

        updateAutoMarginsInMainAxis(*child, autoMarginOffset);

        LayoutUnit childCrossAxisMarginBoxExtent;
        if (alignmentForChild(*child) == ItemPositionBaseline && !hasAutoMarginsInCrossAxis(*child)) {
            LayoutUnit ascent = marginBoxAscentForChild(*child);
            LayoutUnit descent = (crossAxisMarginExtentForChild(*child) + crossAxisExtentForChild(*child)) - ascent;

            maxAscent = std::max(maxAscent, ascent);
            maxDescent = std::max(maxDescent, descent);

            childCrossAxisMarginBoxExtent = maxAscent + maxDescent;
        } else {
            childCrossAxisMarginBoxExtent = crossAxisIntrinsicExtentForChild(*child) + crossAxisMarginExtentForChild(*child) + crossAxisScrollbarExtentForChild(*child);
        }
        if (!isColumnFlow())
            setLogicalHeight(std::max(logicalHeight(), crossAxisOffset + flowAwareBorderAfter() + flowAwarePaddingAfter() + childCrossAxisMarginBoxExtent + crossAxisScrollbarExtent()));
        maxChildCrossAxisExtent = std::max(maxChildCrossAxisExtent, childCrossAxisMarginBoxExtent);

        mainAxisOffset += flowAwareMarginStartForChild(*child);

        LayoutUnit childMainExtent = mainAxisExtentForChild(*child);
        // In an RTL column situation, this will apply the margin-right/margin-end on the left.
        // This will be fixed later in flipForRightToLeftColumn.
        LayoutPoint childLocation(shouldFlipMainAxis ? totalMainExtent - mainAxisOffset - childMainExtent : mainAxisOffset,
            crossAxisOffset + flowAwareMarginBeforeForChild(*child));

        setFlowAwareLocationForChild(*child, childLocation);
        mainAxisOffset += childMainExtent + flowAwareMarginEndForChild(*child);

        ++seenInFlowPositionedChildren;
        if (seenInFlowPositionedChildren < numberOfChildrenForJustifyContent)
            mainAxisOffset += justifyContentSpaceBetweenChildren(availableFreeSpace, style()->justifyContentDistribution(), numberOfChildrenForJustifyContent);
    }

    if (isColumnFlow())
        setLogicalHeight(mainAxisOffset + flowAwareBorderEnd() + flowAwarePaddingEnd() + scrollbarLogicalHeight());

    if (style()->flexDirection() == FlowColumnReverse) {
        // We have to do an extra pass for column-reverse to reposition the flex items since the start depends
        // on the height of the flexbox, which we only know after we've positioned all the flex items.
        updateLogicalHeight();
        layoutColumnReverse(children, crossAxisOffset, availableFreeSpace);
    }

    if (m_numberOfInFlowChildrenOnFirstLine == -1)
        m_numberOfInFlowChildrenOnFirstLine = seenInFlowPositionedChildren;
    lineContexts.append(LineContext(crossAxisOffset, maxChildCrossAxisExtent, children.size(), maxAscent));
    crossAxisOffset += maxChildCrossAxisExtent;
}

void LayoutFlexibleBox::layoutColumnReverse(const OrderedFlexItemList& children, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace)
{
    // This is similar to the logic in layoutAndPlaceChildren, except we place the children
    // starting from the end of the flexbox. We also don't need to layout anything since we're
    // just moving the children to a new position.
    size_t numberOfChildrenForJustifyContent = numberOfInFlowPositionedChildren(children);
    LayoutUnit mainAxisOffset = logicalHeight() - flowAwareBorderEnd() - flowAwarePaddingEnd();
    mainAxisOffset -= initialJustifyContentOffset(availableFreeSpace, style()->justifyContentPosition(), style()->justifyContentDistribution(), numberOfChildrenForJustifyContent);
    mainAxisOffset -= isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    size_t seenInFlowPositionedChildren = 0;
    for (size_t i = 0; i < children.size(); ++i) {
        LayoutBox* child = children[i];

        if (child->isOutOfFlowPositioned()) {
            child->layer()->setStaticBlockPosition(mainAxisOffset);
            continue;
        }
        mainAxisOffset -= mainAxisExtentForChild(*child) + flowAwareMarginEndForChild(*child);

        setFlowAwareLocationForChild(*child, LayoutPoint(mainAxisOffset, crossAxisOffset + flowAwareMarginBeforeForChild(*child)));

        mainAxisOffset -= flowAwareMarginStartForChild(*child);

        ++seenInFlowPositionedChildren;
        if (seenInFlowPositionedChildren < numberOfChildrenForJustifyContent)
            mainAxisOffset -= justifyContentSpaceBetweenChildren(availableFreeSpace, style()->justifyContentDistribution(), numberOfChildrenForJustifyContent);
    }
}

static LayoutUnit initialAlignContentOffset(LayoutUnit availableFreeSpace, ContentPosition alignContent, ContentDistributionType alignContentDistribution, unsigned numberOfLines)
{
    if (numberOfLines <= 1)
        return 0;
    if (alignContent == ContentPositionFlexEnd)
        return availableFreeSpace;
    if (alignContent == ContentPositionCenter)
        return availableFreeSpace / 2;
    if (alignContentDistribution == ContentDistributionSpaceAround) {
        if (availableFreeSpace > 0 && numberOfLines)
            return availableFreeSpace / (2 * numberOfLines);
        if (availableFreeSpace < 0)
            return availableFreeSpace / 2;
    }
    return 0;
}

static LayoutUnit alignContentSpaceBetweenChildren(LayoutUnit availableFreeSpace, ContentDistributionType alignContentDistribution, unsigned numberOfLines)
{
    if (availableFreeSpace > 0 && numberOfLines > 1) {
        if (alignContentDistribution == ContentDistributionSpaceBetween)
            return availableFreeSpace / (numberOfLines - 1);
        if (alignContentDistribution == ContentDistributionSpaceAround || alignContentDistribution == ContentDistributionStretch)
            return availableFreeSpace / numberOfLines;
    }
    return 0;
}

void LayoutFlexibleBox::alignFlexLines(Vector<LineContext>& lineContexts)
{
    // If we have a single line flexbox or a multiline line flexbox with only one flex line,
    // the line height is all the available space.
    // For flex-direction: row, this means we need to use the height, so we do this after calling updateLogicalHeight.
    if (lineContexts.size() == 1) {
        lineContexts[0].crossAxisExtent = crossAxisContentExtent();
        return;
    }

    if (style()->alignContentPosition() == ContentPositionFlexStart)
        return;

    LayoutUnit availableCrossAxisSpace = crossAxisContentExtent();
    for (size_t i = 0; i < lineContexts.size(); ++i)
        availableCrossAxisSpace -= lineContexts[i].crossAxisExtent;

    LayoutBox* child = m_orderIterator.first();
    LayoutUnit lineOffset = initialAlignContentOffset(availableCrossAxisSpace, style()->alignContentPosition(), style()->alignContentDistribution(), lineContexts.size());
    for (unsigned lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        lineContexts[lineNumber].crossAxisOffset += lineOffset;
        for (size_t childNumber = 0; childNumber < lineContexts[lineNumber].numberOfChildren; ++childNumber, child = m_orderIterator.next())
            adjustAlignmentForChild(*child, lineOffset);

        if (style()->alignContentDistribution() == ContentDistributionStretch && availableCrossAxisSpace > 0)
            lineContexts[lineNumber].crossAxisExtent += availableCrossAxisSpace / static_cast<unsigned>(lineContexts.size());

        lineOffset += alignContentSpaceBetweenChildren(availableCrossAxisSpace, style()->alignContentDistribution(), lineContexts.size());
    }
}

void LayoutFlexibleBox::adjustAlignmentForChild(LayoutBox& child, LayoutUnit delta)
{
    if (child.isOutOfFlowPositioned()) {
        LayoutUnit staticInlinePosition = child.layer()->staticInlinePosition();
        LayoutUnit staticBlockPosition = child.layer()->staticBlockPosition();
        LayoutUnit mainAxis = isColumnFlow() ? staticBlockPosition : staticInlinePosition;
        LayoutUnit crossAxis = isColumnFlow() ? staticInlinePosition : staticBlockPosition;
        crossAxis += delta;
        prepareChildForPositionedLayout(child, mainAxis, crossAxis, NoFlipForRowReverse);
        return;
    }

    setFlowAwareLocationForChild(child, flowAwareLocationForChild(child) + LayoutSize(0, delta));
}

void LayoutFlexibleBox::alignChildren(const Vector<LineContext>& lineContexts)
{
    // Keep track of the space between the baseline edge and the after edge of the box for each line.
    Vector<LayoutUnit> minMarginAfterBaselines;

    LayoutBox* child = m_orderIterator.first();
    for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        LayoutUnit minMarginAfterBaseline = LayoutUnit::max();
        LayoutUnit lineCrossAxisExtent = lineContexts[lineNumber].crossAxisExtent;
        LayoutUnit maxAscent = lineContexts[lineNumber].maxAscent;

        for (size_t childNumber = 0; childNumber < lineContexts[lineNumber].numberOfChildren; ++childNumber, child = m_orderIterator.next()) {
            ASSERT(child);
            if (child->isOutOfFlowPositioned()) {
                if (style()->flexWrap() == FlexWrapReverse)
                    adjustAlignmentForChild(*child, lineCrossAxisExtent);
                continue;
            }

            if (updateAutoMarginsInCrossAxis(*child, std::max(LayoutUnit(), availableAlignmentSpaceForChild(lineCrossAxisExtent, *child))))
                continue;

            switch (alignmentForChild(*child)) {
            case ItemPositionAuto:
                ASSERT_NOT_REACHED();
                break;
            case ItemPositionStretch: {
                applyStretchAlignmentToChild(*child, lineCrossAxisExtent);
                // Since wrap-reverse flips cross start and cross end, strech children should be aligned with the cross end.
                if (style()->flexWrap() == FlexWrapReverse)
                    adjustAlignmentForChild(*child, availableAlignmentSpaceForChild(lineCrossAxisExtent, *child));
                break;
            }
            case ItemPositionFlexStart:
                break;
            case ItemPositionFlexEnd:
                adjustAlignmentForChild(*child, availableAlignmentSpaceForChild(lineCrossAxisExtent, *child));
                break;
            case ItemPositionCenter:
                adjustAlignmentForChild(*child, availableAlignmentSpaceForChild(lineCrossAxisExtent, *child) / 2);
                break;
            case ItemPositionBaseline: {
                // FIXME: If we get here in columns, we want the use the descent, except we currently can't get the ascent/descent of orthogonal children.
                // https://bugs.webkit.org/show_bug.cgi?id=98076
                LayoutUnit ascent = marginBoxAscentForChild(*child);
                LayoutUnit startOffset = maxAscent - ascent;
                adjustAlignmentForChild(*child, startOffset);

                if (style()->flexWrap() == FlexWrapReverse)
                    minMarginAfterBaseline = std::min(minMarginAfterBaseline, availableAlignmentSpaceForChild(lineCrossAxisExtent, *child) - startOffset);
                break;
            }
            case ItemPositionLastBaseline:
            case ItemPositionSelfStart:
            case ItemPositionSelfEnd:
            case ItemPositionStart:
            case ItemPositionEnd:
            case ItemPositionLeft:
            case ItemPositionRight:
                // FIXME: File a bug about implementing that. The extended grammar
                // is not enabled by default so we shouldn't hit this codepath.
                ASSERT_NOT_REACHED();
                break;
            }
        }
        minMarginAfterBaselines.append(minMarginAfterBaseline);
    }

    if (style()->flexWrap() != FlexWrapReverse)
        return;

    // wrap-reverse flips the cross axis start and end. For baseline alignment, this means we
    // need to align the after edge of baseline elements with the after edge of the flex line.
    child = m_orderIterator.first();
    for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        LayoutUnit minMarginAfterBaseline = minMarginAfterBaselines[lineNumber];
        for (size_t childNumber = 0; childNumber < lineContexts[lineNumber].numberOfChildren; ++childNumber, child = m_orderIterator.next()) {
            ASSERT(child);
            if (alignmentForChild(*child) == ItemPositionBaseline && !hasAutoMarginsInCrossAxis(*child) && minMarginAfterBaseline)
                adjustAlignmentForChild(*child, minMarginAfterBaseline);
        }
    }
}

void LayoutFlexibleBox::applyStretchAlignmentToChild(LayoutBox& child, LayoutUnit lineCrossAxisExtent)
{
    if (!isColumnFlow() && child.style()->logicalHeight().isAuto()) {
        // FIXME: If the child has orthogonal flow, then it already has an override height set, so use it.
        if (!hasOrthogonalFlow(child)) {
            LayoutUnit heightBeforeStretching = needToStretchChildLogicalHeight(child) ? constrainedChildIntrinsicContentLogicalHeight(child) : child.logicalHeight();
            LayoutUnit stretchedLogicalHeight = std::max(child.borderAndPaddingLogicalHeight(), heightBeforeStretching + availableAlignmentSpaceForChildBeforeStretching(lineCrossAxisExtent, child));
            ASSERT(!child.needsLayout());
            LayoutUnit desiredLogicalHeight = child.constrainLogicalHeightByMinMax(stretchedLogicalHeight, heightBeforeStretching - child.borderAndPaddingLogicalHeight());

            // FIXME: Can avoid laying out here in some cases. See https://webkit.org/b/87905.
            bool childNeedsRelayout = desiredLogicalHeight != child.logicalHeight();
            if (childNeedsRelayout || !child.hasOverrideLogicalContentHeight())
                child.setOverrideLogicalContentHeight(desiredLogicalHeight - child.borderAndPaddingLogicalHeight());
            if (childNeedsRelayout) {
                child.setLogicalHeight(0);
                // We cache the child's intrinsic content logical height to avoid it being reset to the stretched height.
                // FIXME: This is fragile. LayoutBoxes should be smart enough to determine their intrinsic content logical
                // height correctly even when there's an overrideHeight.
                LayoutUnit childIntrinsicContentLogicalHeight = child.intrinsicContentLogicalHeight();
                child.forceChildLayout();
                child.setIntrinsicContentLogicalHeight(childIntrinsicContentLogicalHeight);
            }
        }
    } else if (isColumnFlow() && child.style()->logicalWidth().isAuto()) {
        // FIXME: If the child doesn't have orthogonal flow, then it already has an override width set, so use it.
        if (hasOrthogonalFlow(child)) {
            LayoutUnit childWidth = std::max<LayoutUnit>(0, lineCrossAxisExtent - crossAxisMarginExtentForChild(child));
            childWidth = child.constrainLogicalWidthByMinMax(childWidth, childWidth, this);

            if (childWidth != child.logicalWidth()) {
                child.setOverrideLogicalContentWidth(childWidth - child.borderAndPaddingLogicalWidth());
                child.forceChildLayout();
            }
        }
    }
}

void LayoutFlexibleBox::flipForRightToLeftColumn()
{
    if (style()->isLeftToRightDirection() || !isColumnFlow())
        return;

    LayoutUnit crossExtent = crossAxisExtent();
    for (LayoutBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (child->isOutOfFlowPositioned())
            continue;
        LayoutPoint location = flowAwareLocationForChild(*child);
        // For vertical flows, setFlowAwareLocationForChild will transpose x and y,
        // so using the y axis for a column cross axis extent is correct.
        location.setY(crossExtent - crossAxisExtentForChild(*child) - location.y());
        setFlowAwareLocationForChild(*child, location);
    }
}

void LayoutFlexibleBox::flipForWrapReverse(const Vector<LineContext>& lineContexts, LayoutUnit crossAxisStartEdge)
{
    LayoutUnit contentExtent = crossAxisContentExtent();
    LayoutBox* child = m_orderIterator.first();
    for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        for (size_t childNumber = 0; childNumber < lineContexts[lineNumber].numberOfChildren; ++childNumber, child = m_orderIterator.next()) {
            ASSERT(child);
            LayoutUnit lineCrossAxisExtent = lineContexts[lineNumber].crossAxisExtent;
            LayoutUnit originalOffset = lineContexts[lineNumber].crossAxisOffset - crossAxisStartEdge;
            LayoutUnit newOffset = contentExtent - originalOffset - lineCrossAxisExtent;
            adjustAlignmentForChild(*child, newOffset - originalOffset);
        }
    }
}

}
