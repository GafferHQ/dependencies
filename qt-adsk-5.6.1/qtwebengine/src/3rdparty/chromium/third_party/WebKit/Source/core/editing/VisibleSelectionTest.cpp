// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/VisibleSelection.h"

#include "core/dom/Range.h"
#include "core/editing/EditingTestBase.h"

#define LOREM_IPSUM \
    "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor " \
    "incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud " \
    "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure " \
    "dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur." \
    "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt " \
    "mollit anim id est laborum."

namespace blink {

class VisibleSelectionTest : public EditingTestBase {
protected:
    // Helper function to set the VisibleSelection base/extent.
    void setSelection(VisibleSelection& selection, int base) { setSelection(selection, base, base); }

    // Helper function to set the VisibleSelection base/extent.
    void setSelection(VisibleSelection& selection, int base, int extend)
    {
        Node* node = document().body()->firstChild();
        selection.setBase(Position(node, base));
        selection.setExtent(Position(node, extend));
    }

    static bool equalPositions(const Position&, const PositionInComposedTree&);
    static void testComposedTreePositionsToEqualToDOMTreePositions(const VisibleSelection&);
};

bool VisibleSelectionTest::equalPositions(const Position& positionInDOMTree, const PositionInComposedTree& positionInComposedTree)
{
    // Since DOM tree positions can't be map to composed tree version, e.g.
    // shadow root, not distributed node, we map a position in composed tree
    // to DOM tree position.
    return positionInDOMTree == toPositionInDOMTree(positionInComposedTree);
}

void VisibleSelectionTest::testComposedTreePositionsToEqualToDOMTreePositions(const VisibleSelection& selection)
{
    EXPECT_TRUE(equalPositions(selection.start(), VisibleSelection::InComposedTree::selectionStart(selection)));
    EXPECT_TRUE(equalPositions(selection.end(), VisibleSelection::InComposedTree::selectionEnd(selection)));
    EXPECT_TRUE(equalPositions(selection.base(), VisibleSelection::InComposedTree::selectionBase(selection)));
    EXPECT_TRUE(equalPositions(selection.extent(), VisibleSelection::InComposedTree::selectionExtent(selection)));
}

TEST_F(VisibleSelectionTest, Initialisation)
{
    setBodyContent(LOREM_IPSUM);

    VisibleSelection selection;
    setSelection(selection, 0);

    EXPECT_FALSE(selection.isNone());
    EXPECT_TRUE(selection.isCaret());

    RefPtrWillBeRawPtr<Range> range = selection.firstRange();
    EXPECT_EQ(0, range->startOffset());
    EXPECT_EQ(0, range->endOffset());
    EXPECT_EQ("", range->text());
    testComposedTreePositionsToEqualToDOMTreePositions(selection);
}

TEST_F(VisibleSelectionTest, ShadowCrossing)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<a><span id='s4'>44</span><content select=#two></content><span id='s5'>55</span><content select=#one></content><span id='s6'>66</span></a>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);

    RefPtrWillBeRawPtr<Element> body = document().body();
    RefPtrWillBeRawPtr<Element> host = body->querySelector("#host", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> one = body->querySelector("#one", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> six = shadowRoot->querySelector("#s6", ASSERT_NO_EXCEPTION);

    VisibleSelection selection(Position::firstPositionInNode(one.get()), Position::lastPositionInNode(shadowRoot.get()));

    EXPECT_EQ(Position(host.get(), PositionAnchorType::BeforeAnchor), selection.start());
    EXPECT_EQ(Position(one->firstChild(), 0), selection.end());
    EXPECT_EQ(PositionInComposedTree(one->firstChild(), 0), selection.startInComposedTree());
    EXPECT_EQ(PositionInComposedTree(six->firstChild(), 2), selection.endInComposedTree());
}

TEST_F(VisibleSelectionTest, ShadowDistributedNodes)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<a><span id='s4'>44</span><content select=#two></content><span id='s5'>55</span><content select=#one></content><span id='s6'>66</span></a>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);

    RefPtrWillBeRawPtr<Element> body = document().body();
    RefPtrWillBeRawPtr<Element> one = body->querySelector("#one", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> two = body->querySelector("#two", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> five = shadowRoot->querySelector("#s5", ASSERT_NO_EXCEPTION);

    VisibleSelection selection(Position::firstPositionInNode(one.get()), Position::lastPositionInNode(two.get()));

    EXPECT_EQ(Position(one->firstChild(), 0), selection.start());
    EXPECT_EQ(Position(two->firstChild(), 2), selection.end());
    EXPECT_EQ(PositionInComposedTree(five->firstChild(), 0), selection.startInComposedTree());
    EXPECT_EQ(PositionInComposedTree(five->firstChild(), 2), selection.endInComposedTree());
}

TEST_F(VisibleSelectionTest, ShadowNested)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<a><span id='s4'>44</span><content select=#two></content><span id='s5'>55</span><content select=#one></content><span id='s6'>66</span></a>";
    const char* shadowContent2 = "<span id='s7'>77</span><content></content><span id='s8'>88</span>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot2 = createShadowRootForElementWithIDAndSetInnerHTML(*shadowRoot, "s5", shadowContent2);

    RefPtrWillBeRawPtr<Element> body = document().body();
    RefPtrWillBeRawPtr<Element> host = body->querySelector("#host", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> one = body->querySelector("#one", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> eight = shadowRoot2->querySelector("#s8", ASSERT_NO_EXCEPTION);

    VisibleSelection selection(Position::firstPositionInNode(one.get()), Position::lastPositionInNode(shadowRoot2.get()));

    EXPECT_EQ(Position(host.get(), PositionAnchorType::BeforeAnchor), selection.start());
    EXPECT_EQ(Position(one->firstChild(), 0), selection.end());
    EXPECT_EQ(PositionInComposedTree(eight->firstChild(), 2), selection.startInComposedTree());
    EXPECT_EQ(PositionInComposedTree(one->firstChild(), 0), selection.endInComposedTree());
}

TEST_F(VisibleSelectionTest, WordGranularity)
{
    setBodyContent(LOREM_IPSUM);

    VisibleSelection selection;

    // Beginning of a word.
    {
        setSelection(selection, 0);
        selection.expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection.firstRange();
        EXPECT_EQ(0, range->startOffset());
        EXPECT_EQ(5, range->endOffset());
        EXPECT_EQ("Lorem", range->text());
        testComposedTreePositionsToEqualToDOMTreePositions(selection);

    }

    // Middle of a word.
    {
        setSelection(selection, 8);
        selection.expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection.firstRange();
        EXPECT_EQ(6, range->startOffset());
        EXPECT_EQ(11, range->endOffset());
        EXPECT_EQ("ipsum", range->text());
        testComposedTreePositionsToEqualToDOMTreePositions(selection);

    }

    // End of a word.
    // FIXME: that sounds buggy, we might want to select the word _before_ instead
    // of the space...
    {
        setSelection(selection, 5);
        selection.expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection.firstRange();
        EXPECT_EQ(5, range->startOffset());
        EXPECT_EQ(6, range->endOffset());
        EXPECT_EQ(" ", range->text());
        testComposedTreePositionsToEqualToDOMTreePositions(selection);
    }

    // Before comma.
    // FIXME: that sounds buggy, we might want to select the word _before_ instead
    // of the comma.
    {
        setSelection(selection, 26);
        selection.expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection.firstRange();
        EXPECT_EQ(26, range->startOffset());
        EXPECT_EQ(27, range->endOffset());
        EXPECT_EQ(",", range->text());
        testComposedTreePositionsToEqualToDOMTreePositions(selection);
    }

    // After comma.
    {
        setSelection(selection, 27);
        selection.expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection.firstRange();
        EXPECT_EQ(27, range->startOffset());
        EXPECT_EQ(28, range->endOffset());
        EXPECT_EQ(" ", range->text());
        testComposedTreePositionsToEqualToDOMTreePositions(selection);
    }

    // When selecting part of a word.
    {
        setSelection(selection, 0, 1);
        selection.expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection.firstRange();
        EXPECT_EQ(0, range->startOffset());
        EXPECT_EQ(5, range->endOffset());
        EXPECT_EQ("Lorem", range->text());
        testComposedTreePositionsToEqualToDOMTreePositions(selection);
    }

    // When selecting part of two words.
    {
        setSelection(selection, 2, 8);
        selection.expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection.firstRange();
        EXPECT_EQ(0, range->startOffset());
        EXPECT_EQ(11, range->endOffset());
        EXPECT_EQ("Lorem ipsum", range->text());
        testComposedTreePositionsToEqualToDOMTreePositions(selection);
    }
}

} // namespace blink
