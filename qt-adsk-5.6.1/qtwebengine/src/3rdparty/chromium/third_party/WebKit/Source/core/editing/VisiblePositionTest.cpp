// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/VisiblePosition.h"

#include "core/editing/EditingTestBase.h"

namespace blink {

class VisiblePositionTest : public EditingTestBase {
};

TEST_F(VisiblePositionTest, ShadowDistributedNodes)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<a><span id='s4'>44</span><content select=#two></content><span id='s5'>55</span><content select=#one></content><span id='s6'>66</span></a>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);

    RefPtrWillBeRawPtr<Element> body = document().body();
    RefPtrWillBeRawPtr<Element> one = body->querySelector("#one", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> two = body->querySelector("#two", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> four = shadowRoot->querySelector("#s4", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> five = shadowRoot->querySelector("#s5", ASSERT_NO_EXCEPTION);

    EXPECT_EQ(positionInDOMTree(*one->firstChild(), 0), canonicalPositionOf(positionInDOMTree(*one, 0)));
    EXPECT_EQ(positionInDOMTree(*one->firstChild(), 0), VisiblePosition(positionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(positionInDOMTree(*one->firstChild(), 2), canonicalPositionOf(positionInDOMTree(*two, 0)));
    EXPECT_EQ(positionInDOMTree(*one->firstChild(), 2), VisiblePosition(positionInDOMTree(*two, 0)).deepEquivalent());

    EXPECT_EQ(positionInComposedTree(*five->firstChild(), 2), canonicalPositionOf(positionInComposedTree(*one, 0)));
    EXPECT_EQ(positionInDOMTree(*five->firstChild(), 2), VisiblePosition(positionInComposedTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(positionInComposedTree(*four->firstChild(), 2), canonicalPositionOf(positionInComposedTree(*two, 0)));
    EXPECT_EQ(positionInDOMTree(*four->firstChild(), 2), VisiblePosition(positionInComposedTree(*two, 0)).deepEquivalent());
}

} // namespace blink
