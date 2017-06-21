/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/graphics/GraphicsLayer.h"

#include "platform/scroll/ScrollableArea.h"
#include "platform/transforms/Matrix3DTransformOperation.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebFloatAnimationCurve.h"
#include "public/platform/WebGraphicsContext3D.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebUnitTestSupport.h"
#include "wtf/PassOwnPtr.h"
#include <gtest/gtest.h>

namespace blink {

namespace {

class MockGraphicsLayerClient : public GraphicsLayerClient {
public:
    void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& inClip) override { }
    String debugName(const GraphicsLayer*) override { return String(); }
};

class GraphicsLayerForTesting : public GraphicsLayer {
public:
    explicit GraphicsLayerForTesting(GraphicsLayerClient* client)
        : GraphicsLayer(client) { }

    WebLayer* contentsLayer() const override { return GraphicsLayer::contentsLayer(); }
};

} // anonymous namespace

class GraphicsLayerTest : public testing::Test {
public:
    GraphicsLayerTest()
    {
        m_clipLayer = adoptPtr(new GraphicsLayerForTesting(&m_client));
        m_scrollElasticityLayer = adoptPtr(new GraphicsLayerForTesting(&m_client));
        m_graphicsLayer = adoptPtr(new GraphicsLayerForTesting(&m_client));
        m_clipLayer->addChild(m_scrollElasticityLayer.get());
        m_scrollElasticityLayer->addChild(m_graphicsLayer.get());
        m_graphicsLayer->platformLayer()->setScrollClipLayer(
            m_clipLayer->platformLayer());
        m_platformLayer = m_graphicsLayer->platformLayer();
        m_layerTreeView = adoptPtr(Platform::current()->unitTestSupport()->createLayerTreeViewForTesting());
        ASSERT(m_layerTreeView);
        m_layerTreeView->setRootLayer(*m_clipLayer->platformLayer());
        m_layerTreeView->registerViewportLayers(
            m_scrollElasticityLayer->platformLayer(), m_clipLayer->platformLayer(), m_graphicsLayer->platformLayer(), 0);
        m_layerTreeView->setViewportSize(WebSize(1, 1));
    }

    ~GraphicsLayerTest() override
    {
        m_graphicsLayer.clear();
        m_layerTreeView.clear();
    }

protected:
    WebLayer* m_platformLayer;
    OwnPtr<GraphicsLayerForTesting> m_graphicsLayer;
    OwnPtr<GraphicsLayerForTesting> m_scrollElasticityLayer;
    OwnPtr<GraphicsLayerForTesting> m_clipLayer;

private:
    OwnPtr<WebLayerTreeView> m_layerTreeView;
    MockGraphicsLayerClient m_client;
};

TEST_F(GraphicsLayerTest, updateLayerShouldFlattenTransformWithAnimations)
{
    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());

    OwnPtr<WebFloatAnimationCurve> curve = adoptPtr(Platform::current()->compositorSupport()->createFloatAnimationCurve());
    curve->add(WebFloatKeyframe(0.0, 0.0));
    OwnPtr<WebCompositorAnimation> floatAnimation(adoptPtr(Platform::current()->compositorSupport()->createAnimation(*curve, WebCompositorAnimation::TargetPropertyOpacity)));
    int animationId = floatAnimation->id();
    ASSERT_TRUE(m_platformLayer->addAnimation(floatAnimation.leakPtr()));

    ASSERT_TRUE(m_platformLayer->hasActiveAnimation());

    m_graphicsLayer->setShouldFlattenTransform(false);

    m_platformLayer = m_graphicsLayer->platformLayer();
    ASSERT_TRUE(m_platformLayer);

    ASSERT_TRUE(m_platformLayer->hasActiveAnimation());
    m_platformLayer->removeAnimation(animationId);
    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());

    m_graphicsLayer->setShouldFlattenTransform(true);

    m_platformLayer = m_graphicsLayer->platformLayer();
    ASSERT_TRUE(m_platformLayer);

    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());
}

class FakeScrollableArea : public NoBaseWillBeGarbageCollectedFinalized<FakeScrollableArea>, public ScrollableArea {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(FakeScrollableArea);
public:
    static PassOwnPtrWillBeRawPtr<FakeScrollableArea> create()
    {
        return adoptPtrWillBeNoop(new FakeScrollableArea);
    }

    bool isActive() const override { return false; }
    int scrollSize(ScrollbarOrientation) const override { return 100; }
    bool isScrollCornerVisible() const override { return false; }
    IntRect scrollCornerRect() const override { return IntRect(); }
    int visibleWidth() const override { return 10; }
    int visibleHeight() const override { return 10; }
    IntSize contentsSize() const override { return IntSize(100, 100); }
    bool scrollbarsCanBeActive() const override { return false; }
    IntRect scrollableAreaBoundingBox() const override { return IntRect(); }
    void invalidateScrollbarRect(Scrollbar*, const IntRect&) override { }
    void invalidateScrollCornerRect(const IntRect&) override { }
    bool userInputScrollable(ScrollbarOrientation) const override { return true; }
    bool shouldPlaceVerticalScrollbarOnLeft() const override { return false; }
    int pageStep(ScrollbarOrientation) const override { return 0; }
    IntPoint minimumScrollPosition() const override { return IntPoint(); }
    IntPoint maximumScrollPosition() const override
    {
        return IntPoint(contentsSize().width() - visibleWidth(), contentsSize().height() - visibleHeight());
    }

    void setScrollOffset(const IntPoint& scrollOffset, ScrollType) override { m_scrollPosition = scrollOffset; }
    void setScrollOffset(const DoublePoint& scrollOffset, ScrollType) override { m_scrollPosition = scrollOffset; }
    DoublePoint scrollPositionDouble() const override { return m_scrollPosition; }
    IntPoint scrollPosition() const override { return flooredIntPoint(m_scrollPosition); }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        ScrollableArea::trace(visitor);
    }

private:
    DoublePoint m_scrollPosition;
};

TEST_F(GraphicsLayerTest, applyScrollToScrollableArea)
{
    OwnPtrWillBeRawPtr<FakeScrollableArea> scrollableArea = FakeScrollableArea::create();
    m_graphicsLayer->setScrollableArea(scrollableArea.get(), false);

    WebDoublePoint scrollPosition(7, 9);
    m_platformLayer->setScrollPositionDouble(scrollPosition);
    m_graphicsLayer->didScroll();

    EXPECT_FLOAT_EQ(scrollPosition.x, scrollableArea->scrollPositionDouble().x());
    EXPECT_FLOAT_EQ(scrollPosition.y, scrollableArea->scrollPositionDouble().y());
}

} // namespace blink
