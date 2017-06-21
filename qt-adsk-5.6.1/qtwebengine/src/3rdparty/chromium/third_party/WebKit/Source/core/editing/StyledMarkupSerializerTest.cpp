// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/StyledMarkupSerializer.h"

#include "core/dom/Text.h"
#include "core/editing/EditingTestBase.h"

namespace blink {

// This is smoke test of |StyledMarkupSerializer|. Full testing will be done
// in layout tests.
class StyledMarkupSerializerTest : public EditingTestBase {
protected:
    template <typename Tree>
    std::string serialize(EAnnotateForInterchange = DoNotAnnotateForInterchange);

    template <typename Tree>
    std::string serializePart(const typename Tree::PositionType& start, const typename Tree::PositionType& end, EAnnotateForInterchange = DoNotAnnotateForInterchange);
};

template <typename Tree>
std::string StyledMarkupSerializerTest::serialize(EAnnotateForInterchange shouldAnnotate)
{
    using PositionType = typename Tree::PositionType;
    PositionType start = PositionType(document().body(), PositionAnchorType::BeforeChildren);
    PositionType end = PositionType(document().body(), PositionAnchorType::AfterChildren);
    return createMarkup(start, end, shouldAnnotate).utf8().data();
}

template <typename Tree>
std::string StyledMarkupSerializerTest::serializePart(const typename Tree::PositionType& start, const typename Tree::PositionType& end, EAnnotateForInterchange shouldAnnotate)
{
    return createMarkup(start, end, shouldAnnotate).utf8().data();
}

TEST_F(StyledMarkupSerializerTest, TextOnly)
{
    const char* bodyContent = "Hello world!";
    setBodyContent(bodyContent);
    const char* expectedResult = "<span style=\"display: inline !important; float: none;\">Hello world!</span>";
    EXPECT_EQ(expectedResult, serialize<EditingStrategy>());
    EXPECT_EQ(expectedResult, serialize<EditingInComposedTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, BlockFormatting)
{
    const char* bodyContent = "<div>Hello world!</div>";
    setBodyContent(bodyContent);
    EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
    EXPECT_EQ(bodyContent, serialize<EditingInComposedTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlInput)
{
    const char* bodyContent = "<input value='foo'>";
    setBodyContent(bodyContent);
    const char* expectedResult = "<input value=\"foo\">";
    EXPECT_EQ(expectedResult, serialize<EditingStrategy>());
    EXPECT_EQ(expectedResult, serialize<EditingInComposedTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlInputRange)
{
    const char* bodyContent = "<input type=range>";
    setBodyContent(bodyContent);
    const char* expectedResult = "<input type=\"range\">";
    EXPECT_EQ(expectedResult, serialize<EditingStrategy>());
    EXPECT_EQ(expectedResult, serialize<EditingInComposedTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlSelect)
{
    const char* bodyContent = "<select><option value=\"1\">one</option><option value=\"2\">two</option></select>";
    setBodyContent(bodyContent);
    EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
    EXPECT_EQ(bodyContent, serialize<EditingInComposedTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlTextArea)
{
    const char* bodyContent = "<textarea>foo bar</textarea>";
    setBodyContent(bodyContent);
    const char* expectedResult = "<textarea></textarea>";
    EXPECT_EQ(expectedResult, serialize<EditingStrategy>())
        << "contents of TEXTAREA element should not be appeared.";
    EXPECT_EQ(expectedResult, serialize<EditingInComposedTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, HeadingFormatting)
{
    const char* bodyContent = "<h4>Hello world!</h4>";
    setBodyContent(bodyContent);
    EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
    EXPECT_EQ(bodyContent, serialize<EditingInComposedTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, InlineFormatting)
{
    const char* bodyContent = "<b>Hello world!</b>";
    setBodyContent(bodyContent);
    EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
    EXPECT_EQ(bodyContent, serialize<EditingInComposedTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, Mixed)
{
    const char* bodyContent = "<i>foo<b>bar</b>baz</i>";
    setBodyContent(bodyContent);
    EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
    EXPECT_EQ(bodyContent, serialize<EditingInComposedTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeDistributeOrder)
{
    const char* bodyContent = "<p id=\"host\">00<b id=\"one\">11</b><b id=\"two\">22</b>33</p>";
    const char* shadowContent = "<a><content select=#two></content><content select=#one></content></a>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent);
    EXPECT_EQ("<p id=\"host\"><b id=\"one\">11</b><b id=\"two\">22</b></p>", serialize<EditingStrategy>())
        << "00 and 33 aren't appeared since they aren't distributed.";
    EXPECT_EQ("<p id=\"host\"><a><b id=\"two\">22</b><b id=\"one\">11</b></a></p>", serialize<EditingInComposedTreeStrategy>())
        << "00 and 33 aren't appeared since they aren't distributed.";
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeInput)
{
    const char* bodyContent = "<p id=\"host\">00<b id=\"one\">11</b><b id=\"two\"><input value=\"22\"></b>33</p>";
    const char* shadowContent = "<a><content select=#two></content><content select=#one></content></a>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent);
    EXPECT_EQ("<p id=\"host\"><b id=\"one\">11</b><b id=\"two\"><input value=\"22\"></b></p>", serialize<EditingStrategy>())
        << "00 and 33 aren't appeared since they aren't distributed.";
    EXPECT_EQ("<p id=\"host\"><a><b id=\"two\"><input value=\"22\"></b><b id=\"one\">11</b></a></p>", serialize<EditingInComposedTreeStrategy>())
        << "00 and 33 aren't appeared since they aren't distributed.";
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeNested)
{
    const char* bodyContent = "<p id=\"host\">00<b id=\"one\">11</b><b id=\"two\">22</b>33</p>";
    const char* shadowContent1 = "<a><content select=#two></content><b id=host2></b><content select=#one></content></a>";
    const char* shadowContent2 = "NESTED";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot1 = setShadowContent(shadowContent1);
    createShadowRootForElementWithIDAndSetInnerHTML(*shadowRoot1, "host2", shadowContent2);

    EXPECT_EQ("<p id=\"host\"><b id=\"one\">11</b><b id=\"two\">22</b></p>", serialize<EditingStrategy>())
        << "00 and 33 aren't appeared since they aren't distributed.";
    EXPECT_EQ("<p id=\"host\"><a><b id=\"two\">22</b><b id=\"host2\">NESTED</b><b id=\"one\">11</b></a></p>", serialize<EditingInComposedTreeStrategy>())
        << "00 and 33 aren't appeared since they aren't distributed.";
}

TEST_F(StyledMarkupSerializerTest, StyleDisplayNone)
{
    const char* bodyContent = "<b>00<i style='display:none'>11</i>22</b>";
    setBodyContent(bodyContent);
    const char* expectedResult = "<b>0022</b>";
    EXPECT_EQ(expectedResult, serialize<EditingStrategy>());
    EXPECT_EQ(expectedResult, serialize<EditingInComposedTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, StyleDisplayNoneAndNewLines)
{
    const char* bodyContent = "<div style='display:none'>11</div>\n\n";
    setBodyContent(bodyContent);
    EXPECT_EQ("", serialize<EditingStrategy>());
    EXPECT_EQ("", serialize<EditingInComposedTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeStyle)
{
    const char* bodyContent = "<p id='host' style='color: red'><span style='font-weight: bold;'><span id='one'>11</span></span></p>\n";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<Element> one = document().getElementById("one");
    RefPtrWillBeRawPtr<Text> text = toText(one->firstChild());
    Position startDOM(text, 0);
    Position endDOM(text, 2);
    const std::string& serializedDOM = serializePart<EditingStrategy>(startDOM, endDOM, AnnotateForInterchange);

    bodyContent = "<p id='host' style='color: red'>00<span id='one'>11</span>22</p>\n";
    const char* shadowContent = "<span style='font-weight: bold'><content select=#one></content></span>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent);
    one = document().getElementById("one");
    text = toText(one->firstChild());
    PositionInComposedTree startICT(text, 0);
    PositionInComposedTree endICT(text, 2);
    const std::string& serializedICT = serializePart<EditingInComposedTreeStrategy>(startICT, endICT, AnnotateForInterchange);

    EXPECT_EQ(serializedDOM, serializedICT);
}

TEST_F(StyledMarkupSerializerTest, AcrossShadow)
{
    const char* bodyContent = "<p id='host1'>[<span id='one'>11</span>]</p><p id='host2'>[<span id='two'>22</span>]</p>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<Element> one = document().getElementById("one");
    RefPtrWillBeRawPtr<Element> two = document().getElementById("two");
    Position startDOM(toText(one->firstChild()), 0);
    Position endDOM(toText(two->firstChild()), 2);
    const std::string& serializedDOM = serializePart<EditingStrategy>(startDOM, endDOM, AnnotateForInterchange);

    bodyContent = "<p id='host1'><span id='one'>11</span></p><p id='host2'><span id='two'>22</span></p>";
    const char* shadowContent1 = "[<content select=#one></content>]";
    const char* shadowContent2 = "[<content select=#two></content>]";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent1, "host1");
    setShadowContent(shadowContent2, "host2");
    one = document().getElementById("one");
    two = document().getElementById("two");
    PositionInComposedTree startICT(toText(one->firstChild()), 0);
    PositionInComposedTree endICT(toText(two->firstChild()), 2);
    const std::string& serializedICT = serializePart<EditingInComposedTreeStrategy>(startICT, endICT, AnnotateForInterchange);

    EXPECT_EQ(serializedDOM, serializedICT);
}

TEST_F(StyledMarkupSerializerTest, AcrossInvisibleElements)
{
    const char* bodyContent = "<span id='span1' style='display: none'>11</span><span id='span2' style='display: none'>22</span>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<Element> span1 = document().getElementById("span1");
    RefPtrWillBeRawPtr<Element> span2 = document().getElementById("span2");
    Position startDOM = Position::firstPositionInNode(span1.get());
    Position endDOM = Position::lastPositionInNode(span2.get());
    EXPECT_EQ("", serializePart<EditingStrategy>(startDOM, endDOM));
    PositionInComposedTree startICT = PositionInComposedTree::firstPositionInNode(span1.get());
    PositionInComposedTree endICT = PositionInComposedTree::lastPositionInNode(span2.get());
    EXPECT_EQ("", serializePart<EditingInComposedTreeStrategy>(startICT, endICT));
}

} // namespace blink
