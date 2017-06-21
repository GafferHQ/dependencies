/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollingCoordinator_h
#define ScrollingCoordinator_h

#include "core/CoreExport.h"
#include "core/layout/LayoutObject.h"
#include "platform/PlatformWheelEvent.h"
#include "platform/geometry/IntRect.h"
#include "platform/scroll/ScrollTypes.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace blink {
class WebScrollbarLayer;
}

namespace blink {

typedef unsigned MainThreadScrollingReasons;

class LocalFrame;
class FrameView;
class GraphicsLayer;
class Page;
class Region;
class ScrollableArea;

class CORE_EXPORT ScrollingCoordinator final : public NoBaseWillBeGarbageCollectedFinalized<ScrollingCoordinator> {
    WTF_MAKE_NONCOPYABLE(ScrollingCoordinator);
public:
    static PassOwnPtrWillBeRawPtr<ScrollingCoordinator> create(Page*);

    ~ScrollingCoordinator();
    DECLARE_TRACE();

    void willBeDestroyed();

    // Return whether this scrolling coordinator handles scrolling for the given frame view.
    bool coordinatesScrollingForFrameView(FrameView*) const;

    // Called when any frame has done its layout.
    void notifyLayoutUpdated();

    void updateAfterCompositingChangeIfNeeded();

    void updateHaveWheelEventHandlers();
    void updateHaveScrollEventHandlers();

    // Should be called whenever scrollable area set changes.
    void scrollableAreasDidChange();

    // Should be called whenever the slow repaint objects counter changes between zero and one.
    void frameViewHasSlowRepaintObjectsDidChange(FrameView*);

    // Should be called whenever the set of fixed objects changes.
    void frameViewFixedObjectsDidChange(FrameView*);

    // Should be called whenever the root layer for the given frame view changes.
    void frameViewRootLayerDidChange(FrameView*);

#if OS(MACOSX)
    // Dispatched by the scrolling tree during handleWheelEvent. This is required as long as scrollbars are painted on the main thread.
    void handleWheelEventPhase(PlatformWheelEventPhase);
#endif

    enum MainThreadScrollingReasonFlags {
        HasSlowRepaintObjects = 1 << 0,
        HasNonLayerViewportConstrainedObjects = 1 << 1,
        ThreadedScrollingDisabled = 1 << 2
    };

    MainThreadScrollingReasons mainThreadScrollingReasons() const;
    bool shouldUpdateScrollLayerPositionOnMainThread() const { return mainThreadScrollingReasons() != 0; }

    PassOwnPtr<WebScrollbarLayer> createSolidColorScrollbarLayer(ScrollbarOrientation, int thumbThickness, int trackStart, bool isLeftSideVerticalScrollbar);

    void willDestroyScrollableArea(ScrollableArea*);
    // Returns true if the coordinator handled this change.
    bool scrollableAreaScrollLayerDidChange(ScrollableArea*);
    void scrollableAreaScrollbarLayerDidChange(ScrollableArea*, ScrollbarOrientation);
    void setLayerIsContainerForFixedPositionLayers(GraphicsLayer*, bool);
    void updateLayerPositionConstraint(DeprecatedPaintLayer*);
    void touchEventTargetRectsDidChange();
    void willDestroyLayer(DeprecatedPaintLayer*);

    void updateScrollParentForGraphicsLayer(GraphicsLayer* child, DeprecatedPaintLayer* parent);
    void updateClipParentForGraphicsLayer(GraphicsLayer* child, DeprecatedPaintLayer* parent);

    static String mainThreadScrollingReasonsAsText(MainThreadScrollingReasons);
    String mainThreadScrollingReasonsAsText() const;
    Region computeShouldHandleScrollGestureOnMainThreadRegion(const LocalFrame*, const IntPoint& frameLocation) const;

    void updateTouchEventTargetRectsIfNeeded();

    // For testing purposes only. This ScrollingCoordinator is reused between layout test, and must be reset
    // for the results to be valid.
    void reset();

protected:
    explicit ScrollingCoordinator(Page*);

    bool isForRootLayer(ScrollableArea*) const;
    bool isForMainFrame(ScrollableArea*) const;
    bool isForViewport(ScrollableArea*) const;

    RawPtrWillBeMember<Page> m_page;

    // Dirty flags used to idenfity what really needs to be computed after compositing is updated.
    bool m_scrollGestureRegionIsDirty;
    bool m_touchEventTargetRectsAreDirty;
    bool m_shouldScrollOnMainThreadDirty;

private:
    bool shouldUpdateAfterCompositingChange() const { return m_scrollGestureRegionIsDirty || m_touchEventTargetRectsAreDirty || m_shouldScrollOnMainThreadDirty || frameViewIsDirty(); }

    void setShouldUpdateScrollLayerPositionOnMainThread(MainThreadScrollingReasons);

    bool hasVisibleSlowRepaintViewportConstrainedObjects(FrameView*) const;

    void setShouldHandleScrollGestureOnMainThreadRegion(const Region&);
    void setTouchEventTargetRects(LayerHitTestRects&);
    void computeTouchEventTargetRects(LayerHitTestRects&);

    WebScrollbarLayer* addWebScrollbarLayer(ScrollableArea*, ScrollbarOrientation, PassOwnPtr<WebScrollbarLayer>);
    WebScrollbarLayer* getWebScrollbarLayer(ScrollableArea*, ScrollbarOrientation);
    void removeWebScrollbarLayer(ScrollableArea*, ScrollbarOrientation);

    bool frameViewIsDirty() const;

    using ScrollbarMap = WillBeHeapHashMap<RawPtrWillBeMember<ScrollableArea>, OwnPtr<WebScrollbarLayer>>;
    ScrollbarMap m_horizontalScrollbars;
    ScrollbarMap m_verticalScrollbars;
    HashSet<const DeprecatedPaintLayer*> m_layersWithTouchRects;
    bool m_wasFrameScrollable;

    MainThreadScrollingReasons m_lastMainThreadScrollingReasons;
};

} // namespace blink

#endif // ScrollingCoordinator_h
