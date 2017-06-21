/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "platform/graphics/Image.h"

#include "platform/graphics/GraphicsLayer.h"
#include "wtf/PassOwnPtr.h"
#include <gtest/gtest.h>

namespace blink {

namespace {

class MockGraphicsLayerClient : public GraphicsLayerClient {
public:
    void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect&) override { }
    String debugName(const GraphicsLayer*) override { return String(); }
};

class TestImage : public Image {
public:
    static PassRefPtr<TestImage> create(const IntSize& size, bool isOpaque)
    {
        return adoptRef(new TestImage(size, isOpaque));
    }

    TestImage(const IntSize& size, bool isOpaque)
        : Image(0)
        , m_size(size)
    {
        m_bitmap.allocN32Pixels(size.width(), size.height(), isOpaque);
        m_bitmap.eraseColor(SK_ColorTRANSPARENT);
    }

    bool isBitmapImage() const override
    {
        return true;
    }

    bool currentFrameKnownToBeOpaque() override
    {
        return m_bitmap.isOpaque();
    }

    IntSize size() const override
    {
        return m_size;
    }

    bool bitmapForCurrentFrame(SkBitmap* bitmap) override
    {
        if (m_size.isZero())
            return false;

        *bitmap = m_bitmap;
        return true;
    }

    // Stub implementations of pure virtual Image functions.
    void destroyDecodedData(bool) override
    {
    }

    void draw(SkCanvas*, const SkPaint&, const FloatRect&, const FloatRect&, RespectImageOrientationEnum, ImageClampingMode) override
    {
    }

private:
    IntSize m_size;
    SkBitmap m_bitmap;
};

class GraphicsLayerForTesting : public GraphicsLayer {
public:
    explicit GraphicsLayerForTesting(GraphicsLayerClient* client)
        : GraphicsLayer(client) { }

    WebLayer* contentsLayer() const { return GraphicsLayer::contentsLayer(); }
};

} // anonymous namespace

TEST(ImageLayerChromiumTest, imageLayerContentReset)
{
    MockGraphicsLayerClient client;
    OwnPtr<GraphicsLayerForTesting> graphicsLayer = adoptPtr(new GraphicsLayerForTesting(&client));
    ASSERT_TRUE(graphicsLayer.get());

    ASSERT_FALSE(graphicsLayer->hasContentsLayer());
    ASSERT_FALSE(graphicsLayer->contentsLayer());

    RefPtr<Image> image = TestImage::create(IntSize(100, 100), false);
    ASSERT_TRUE(image.get());

    graphicsLayer->setContentsToImage(image.get());
    ASSERT_TRUE(graphicsLayer->hasContentsLayer());
    ASSERT_TRUE(graphicsLayer->contentsLayer());

    graphicsLayer->setContentsToImage(0);
    ASSERT_FALSE(graphicsLayer->hasContentsLayer());
    ASSERT_FALSE(graphicsLayer->contentsLayer());
}

TEST(ImageLayerChromiumTest, opaqueImages)
{
    MockGraphicsLayerClient client;
    OwnPtr<GraphicsLayerForTesting> graphicsLayer = adoptPtr(new GraphicsLayerForTesting(&client));
    ASSERT_TRUE(graphicsLayer.get());

    RefPtr<Image> opaqueImage = TestImage::create(IntSize(100, 100), true /* opaque */);
    ASSERT_TRUE(opaqueImage.get());
    RefPtr<Image> nonOpaqueImage = TestImage::create(IntSize(100, 100), false /* opaque */);
    ASSERT_TRUE(nonOpaqueImage.get());

    ASSERT_FALSE(graphicsLayer->contentsLayer());

    graphicsLayer->setContentsToImage(opaqueImage.get());
    ASSERT_TRUE(graphicsLayer->contentsLayer()->opaque());

    graphicsLayer->setContentsToImage(nonOpaqueImage.get());
    ASSERT_FALSE(graphicsLayer->contentsLayer()->opaque());
}

} // namespace blink
