// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/page/PrintContext.h"

#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/layout/LayoutView.h"
#include "core/loader/EmptyClients.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "core/paint/DeprecatedPaintLayerPainter.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/testing/SkiaForCoreTesting.h"
#include "platform/text/TextStream.h"
#include <gtest/gtest.h>

namespace blink {

const int kPageWidth = 800;
const int kPageHeight = 600;

class MockPrintContext : public PrintContext {
public:
    MockPrintContext(LocalFrame* frame) : PrintContext(frame) { }

    void outputLinkedDestinations(SkCanvas* canvas, const IntRect& pageRect)
    {
        PrintContext::outputLinkedDestinations(canvas, pageRect);
    }
};

class MockCanvas : public SkCanvas {
public:
    enum OperationType {
        DrawRect,
        DrawPoint
    };

    struct Operation {
        OperationType type;
        SkRect rect;
    };

    MockCanvas() : SkCanvas(kPageWidth, kPageHeight) { }

    virtual void onDrawRect(const SkRect& rect, const SkPaint& paint) override
    {
        if (!paint.getAnnotation())
            return;
        Operation operation = { DrawRect, rect };
        getTotalMatrix().mapRect(&operation.rect);
        m_recordedOperations.append(operation);
    }

    virtual void onDrawPoints(PointMode mode, size_t count, const SkPoint pts[], const SkPaint& paint) override
    {
        if (!paint.getAnnotation())
            return;
        ASSERT_EQ(1u, count); // Only called from drawPoint().
        SkPoint point = getTotalMatrix().mapXY(pts[0].x(), pts[0].y());
        Operation operation = { DrawPoint, SkRect::MakeXYWH(point.x(), point.y(), 0, 0) };
        m_recordedOperations.append(operation);
    }

    const Vector<Operation>& recordedOperations() const { return m_recordedOperations; }

private:
    Vector<Operation> m_recordedOperations;
};

class PrintContextTest : public testing::Test {
protected:
    PrintContextTest(PassOwnPtr<FrameLoaderClient> frameLoaderClient = PassOwnPtr<FrameLoaderClient>())
        : m_pageHolder(DummyPageHolder::create(IntSize(kPageWidth, kPageHeight), nullptr, frameLoaderClient))
        , m_printContext(adoptPtrWillBeNoop(new MockPrintContext(document().frame()))) { }

    Document& document() const { return m_pageHolder->document(); }
    MockPrintContext& printContext() { return *m_printContext.get(); }

    void setBodyInnerHTML(String bodyContent)
    {
        document().body()->setAttribute(HTMLNames::styleAttr, "margin: 0");
        document().body()->setInnerHTML(bodyContent, ASSERT_NO_EXCEPTION);
    }

    void printSinglePage(SkCanvas& canvas)
    {
        IntRect pageRect(0, 0, kPageWidth, kPageHeight);
        document().view()->updateAllLifecyclePhases();
        document().setPrinting(true);
        SkPictureBuilder pictureBuilder(pageRect);
        GraphicsContext& context = pictureBuilder.context();
        context.setPrinting(true);
        DeprecatedPaintLayer& rootLayer = *document().view()->layoutView()->layer();
        DeprecatedPaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(pageRect), PaintBehaviorNormal, LayoutSize());
        DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
        printContext().begin(kPageWidth, kPageHeight);
        printContext().end();
        pictureBuilder.endRecording()->playback(&canvas);
        printContext().outputLinkedDestinations(&canvas, pageRect);
        document().setPrinting(false);
    }

    static String absoluteBlockHtmlForLink(int x, int y, int width, int height, const char* url, const char* children = nullptr)
    {
        TextStream ts;
        ts << "<a style='position: absolute; left: " << x << "px; top: " << y << "px; width: " << width << "px; height: " << height
            << "px' href='" << url << "'>" << (children ? children : url) << "</a>";
        return ts.release();
    }

    static String inlineHtmlForLink(const char* url, const char* children = nullptr)
    {
        TextStream ts;
        ts << "<a href='" << url << "'>" << (children ? children : url) << "</a>";
        return ts.release();
    }

    static String htmlForAnchor(int x, int y, const char* name)
    {
        TextStream ts;
        ts << "<a name='" << name << "' style='position: absolute; left: " << x << "px; top: " << y << "px'>" << name << "</a>";
        return ts.release();
    }

    void testBasicLinkTarget();

private:
    OwnPtr<DummyPageHolder> m_pageHolder;
    OwnPtrWillBePersistent<MockPrintContext> m_printContext;
};

class SingleChildFrameLoaderClient : public EmptyFrameLoaderClient {
public:
    SingleChildFrameLoaderClient() : m_child(nullptr) { }

    virtual Frame* firstChild() const override { return m_child; }
    virtual Frame* lastChild() const override { return m_child; }

    void setChild(Frame* child) { m_child = child; }

private:
    Frame* m_child;
};

class FrameLoaderClientWithParent : public EmptyFrameLoaderClient {
public:
    FrameLoaderClientWithParent(Frame* parent) : m_parent(parent) { }

    virtual Frame* parent() const override { return m_parent; }

private:
    Frame* m_parent;
};

class PrintContextFrameTest : public PrintContextTest {
public:
    PrintContextFrameTest() : PrintContextTest(adoptPtr(new SingleChildFrameLoaderClient())) { }
};

#define EXPECT_SKRECT_EQ(expectedX, expectedY, expectedWidth, expectedHeight, actualRect) \
    EXPECT_EQ(expectedX, actualRect.x()); \
    EXPECT_EQ(expectedY, actualRect.y()); \
    EXPECT_EQ(expectedWidth, actualRect.width()); \
    EXPECT_EQ(expectedHeight, actualRect.height());

void PrintContextTest::testBasicLinkTarget()
{
    MockCanvas canvas;
    setBodyInnerHTML(absoluteBlockHtmlForLink(50, 60, 70, 80, "http://www.google.com")
        + absoluteBlockHtmlForLink(150, 160, 170, 180, "http://www.google.com#fragment"));
    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(2u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[0].rect);
    EXPECT_EQ(MockCanvas::DrawRect, operations[1].type);
    EXPECT_SKRECT_EQ(150, 160, 170, 180, operations[1].rect);
    // We should also check if the annotation is correct but Skia doesn't export
    // SkAnnotation API.
}

class PrintContextTestWithSlimmingPaint : public PrintContextTest {
public:
    PrintContextTestWithSlimmingPaint()
        : m_originalSlimmingPaintEnabled(RuntimeEnabledFeatures::slimmingPaintEnabled()) { }

protected:
    virtual void SetUp() override
    {
        PrintContextTest::SetUp();
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(true);
    }
    virtual void TearDown() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(m_originalSlimmingPaintEnabled);
        PrintContextTest::TearDown();
    }

private:
    bool m_originalSlimmingPaintEnabled;
};

TEST_F(PrintContextTest, LinkTarget)
{
    testBasicLinkTarget();
}

TEST_F(PrintContextTestWithSlimmingPaint, LinkTargetBasic)
{
    testBasicLinkTarget();
}

TEST_F(PrintContextTest, LinkTargetComplex)
{
    MockCanvas canvas;
    setBodyInnerHTML("<div>"
        // Link in anonymous block before a block.
        + inlineHtmlForLink("http://www.google.com", "<img style='width: 111; height: 10'>")
        + "<div> " + inlineHtmlForLink("http://www.google1.com", "<img style='width: 122; height: 20'>") + "</div>"
        // Link in anonymous block after a block, containing another block
        + inlineHtmlForLink("http://www.google2.com", "<div style='width:133; height: 30'>BLOCK</div>")
        // Link embedded in inlines
        + "<span><b><i><img style='width: 40px; height: 40px'><br>"
        + inlineHtmlForLink("http://www.google3.com", "<img style='width: 144px; height: 40px'>")
        + "</i></b></span>"
        // Link embedded in relatively positioned inline
        + "<span style='position: relative; top: 50px; left: 50px'><b><i><img style='width: 1px; height: 40px'><br>"
        + inlineHtmlForLink("http://www.google3.com", "<img style='width: 155px; height: 50px'>")
        + "</i></b></span>"
        + "</div>");
    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(5u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(0, 0, 111, 10, operations[0].rect);
    EXPECT_EQ(MockCanvas::DrawRect, operations[1].type);
    EXPECT_SKRECT_EQ(0, 10, 122, 20, operations[1].rect);
    EXPECT_EQ(MockCanvas::DrawRect, operations[2].type);
    EXPECT_SKRECT_EQ(0, 30, 133, 30, operations[2].rect);
    EXPECT_EQ(MockCanvas::DrawRect, operations[3].type);
    EXPECT_SKRECT_EQ(0, 100, 144, 40, operations[3].rect);
    EXPECT_EQ(MockCanvas::DrawRect, operations[4].type);
    EXPECT_SKRECT_EQ(50, 190, 155, 50, operations[4].rect);
}

TEST_F(PrintContextTest, LinkTargetSvg)
{
    MockCanvas canvas;
    setBodyInnerHTML("<svg width='100' height='100'>"
        "<a xlink:href='http://www.w3.org'><rect x='20' y='20' width='50' height='50'/></a>"
        "<text x='10' y='90'><a xlink:href='http://www.google.com'><tspan>google</tspan></a></text>"
        "</svg>");
    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(2u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(20, 20, 50, 50, operations[0].rect);
    EXPECT_EQ(MockCanvas::DrawRect, operations[1].type);
    EXPECT_EQ(10, operations[1].rect.x());
    EXPECT_GE(90, operations[1].rect.y());
}

TEST_F(PrintContextTest, LinkedTarget)
{
    MockCanvas canvas;
    document().setBaseURLOverride(KURL(ParsedURLString, "http://a.com/"));
    setBodyInnerHTML(absoluteBlockHtmlForLink(50, 60, 70, 80, "#fragment") // Generates a Link_Named_Dest_Key annotation
        + absoluteBlockHtmlForLink(150, 160, 170, 180, "#not-found") // Generates no annotation
        + htmlForAnchor(250, 260, "fragment") // Generates a Define_Named_Dest_Key annotation
        + htmlForAnchor(350, 360, "fragment-not-used")); // Generates no annotation
    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(2u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[0].rect);
    EXPECT_EQ(MockCanvas::DrawPoint, operations[1].type);
    EXPECT_SKRECT_EQ(250, 260, 0, 0, operations[1].rect);
}

TEST_F(PrintContextTest, LinkTargetBoundingBox)
{
    MockCanvas canvas;
    setBodyInnerHTML(absoluteBlockHtmlForLink(50, 60, 70, 20, "http://www.google.com", "<img style='width: 200px; height: 100px'>"));
    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(1u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(50, 60, 200, 100, operations[0].rect);
}

TEST_F(PrintContextFrameTest, WithSubframe)
{
    MockCanvas canvas;
    document().setBaseURLOverride(KURL(ParsedURLString, "http://a.com/"));
    setBodyInnerHTML("<style>::-webkit-scrollbar { display: none }</style>"
        "<iframe id='frame' src='http://b.com/' width='500' height='500'"
        " style='border-width: 5px; margin: 5px; position: absolute; top: 90px; left: 90px'></iframe>");

    HTMLIFrameElement& iframe = *toHTMLIFrameElement(document().getElementById("frame"));
    OwnPtr<FrameLoaderClient> frameLoaderClient = adoptPtr(new FrameLoaderClientWithParent(document().frame()));
    RefPtrWillBePersistent<LocalFrame> subframe = LocalFrame::create(frameLoaderClient.get(), document().frame()->host(), &iframe);
    subframe->setView(FrameView::create(subframe.get(), IntSize(500, 500)));
    subframe->init();
    static_cast<SingleChildFrameLoaderClient*>(document().frame()->client())->setChild(subframe.get());
    document().frame()->host()->incrementSubframeCount();

    Document& frameDocument = *iframe.contentDocument();
    frameDocument.setBaseURLOverride(KURL(ParsedURLString, "http://b.com/"));
    frameDocument.body()->setInnerHTML(absoluteBlockHtmlForLink(50, 60, 70, 80, "#fragment")
        + absoluteBlockHtmlForLink(150, 160, 170, 180, "http://www.google.com")
        + absoluteBlockHtmlForLink(250, 260, 270, 280, "http://www.google.com#fragment"),
        ASSERT_NO_EXCEPTION);

    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(2u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(250, 260, 170, 180, operations[0].rect);
    EXPECT_EQ(MockCanvas::DrawRect, operations[1].type);
    EXPECT_SKRECT_EQ(350, 360, 270, 280, operations[1].rect);

    subframe->detach(FrameDetachType::Remove);
    static_cast<SingleChildFrameLoaderClient*>(document().frame()->client())->setChild(nullptr);
    document().frame()->host()->decrementSubframeCount();
}

TEST_F(PrintContextFrameTest, WithScrolledSubframe)
{
    MockCanvas canvas;
    document().setBaseURLOverride(KURL(ParsedURLString, "http://a.com/"));
    setBodyInnerHTML("<style>::-webkit-scrollbar { display: none }</style>"
        "<iframe id='frame' src='http://b.com/' width='500' height='500'"
        " style='border-width: 5px; margin: 5px; position: absolute; top: 90px; left: 90px'></iframe>");

    HTMLIFrameElement& iframe = *toHTMLIFrameElement(document().getElementById("frame"));
    OwnPtr<FrameLoaderClient> frameLoaderClient = adoptPtr(new FrameLoaderClientWithParent(document().frame()));
    RefPtrWillBePersistent<LocalFrame> subframe = LocalFrame::create(frameLoaderClient.get(), document().frame()->host(), &iframe);
    subframe->setView(FrameView::create(subframe.get(), IntSize(500, 500)));
    subframe->init();
    static_cast<SingleChildFrameLoaderClient*>(document().frame()->client())->setChild(subframe.get());
    document().frame()->host()->incrementSubframeCount();

    Document& frameDocument = *iframe.contentDocument();
    frameDocument.setBaseURLOverride(KURL(ParsedURLString, "http://b.com/"));
    frameDocument.body()->setInnerHTML(
        absoluteBlockHtmlForLink(10, 10, 20, 20, "http://invisible.com")
        + absoluteBlockHtmlForLink(50, 60, 70, 80, "http://partly.visible.com")
        + absoluteBlockHtmlForLink(150, 160, 170, 180, "http://www.google.com")
        + absoluteBlockHtmlForLink(250, 260, 270, 280, "http://www.google.com#fragment")
        + absoluteBlockHtmlForLink(850, 860, 70, 80, "http://another.invisible.com"),
        ASSERT_NO_EXCEPTION);
    iframe.contentWindow()->scrollTo(100, 100);

    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(3u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[0].rect); // FIXME: the rect should be clipped.
    EXPECT_EQ(MockCanvas::DrawRect, operations[1].type);
    EXPECT_SKRECT_EQ(150, 160, 170, 180, operations[1].rect);
    EXPECT_EQ(MockCanvas::DrawRect, operations[2].type);
    EXPECT_SKRECT_EQ(250, 260, 270, 280, operations[2].rect);

    subframe->detach(FrameDetachType::Remove);
    static_cast<SingleChildFrameLoaderClient*>(document().frame()->client())->setChild(nullptr);
    document().frame()->host()->decrementSubframeCount();
}

} // namespace blink
