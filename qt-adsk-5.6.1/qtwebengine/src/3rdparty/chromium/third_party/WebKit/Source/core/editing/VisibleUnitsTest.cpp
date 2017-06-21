// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/VisibleUnits.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/VisiblePosition.h"

namespace blink {

namespace {

PositionWithAffinity positionWithAffinityInDOMTree(Node& anchor, int offset, EAffinity affinity = DOWNSTREAM)
{
    return PositionWithAffinity(canonicalPositionOf(Position(&anchor, offset)), affinity);
}

PositionInComposedTreeWithAffinity positionWithAffinityInComposedTree(Node& anchor, int offset, EAffinity affinity = DOWNSTREAM)
{
    return PositionInComposedTreeWithAffinity(canonicalPositionOf(PositionInComposedTree(&anchor, offset)), affinity);
}

} // namespace

class VisibleUnitsTest : public EditingTestBase {
};

TEST_F(VisibleUnitsTest, inSameLine)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<div><span id='s4'>44</span><content select=#two></content><br><span id='s5'>55</span><br><content select=#one></content><span id='s6'>66</span></div>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> body = document().body();
    RefPtrWillBeRawPtr<Element> one = body->querySelector("#one", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> two = body->querySelector("#two", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> four = shadowRoot->querySelector("#s4", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> five = shadowRoot->querySelector("#s5", ASSERT_NO_EXCEPTION);

    EXPECT_TRUE(inSameLine(positionWithAffinityInDOMTree(*one, 0), positionWithAffinityInDOMTree(*two, 0)));
    EXPECT_TRUE(inSameLine(positionWithAffinityInDOMTree(*one->firstChild(), 0), positionWithAffinityInDOMTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInDOMTree(*one->firstChild(), 0), positionWithAffinityInDOMTree(*five->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInDOMTree(*two->firstChild(), 0), positionWithAffinityInDOMTree(*four->firstChild(), 0)));

    EXPECT_FALSE(inSameLine(positionWithAffinityInComposedTree(*one, 0), positionWithAffinityInComposedTree(*two, 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInComposedTree(*one->firstChild(), 0), positionWithAffinityInComposedTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInComposedTree(*one->firstChild(), 0), positionWithAffinityInComposedTree(*five->firstChild(), 0)));
    EXPECT_TRUE(inSameLine(positionWithAffinityInComposedTree(*two->firstChild(), 0), positionWithAffinityInComposedTree(*four->firstChild(), 0)));
}

} // namespace blink
