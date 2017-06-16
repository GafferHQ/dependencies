/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CompositingRequirementsUpdater_h
#define CompositingRequirementsUpdater_h

#include "platform/geometry/IntRect.h"
#include "platform/graphics/CompositingReasons.h"
#include "wtf/Vector.h"

namespace blink {

class CompositingReasonFinder;
class DeprecatedPaintLayer;
class LayoutView;

class CompositingRequirementsUpdater {
public:
    CompositingRequirementsUpdater(LayoutView&, CompositingReasonFinder&);
    ~CompositingRequirementsUpdater();

    //  Recurse through the layers in z-index and overflow order (which is equivalent to painting order)
    //  For the z-order children of a compositing layer:
    //      If a child layers has a compositing layer, then all subsequent layers must
    //      be compositing in order to render above that layer.
    //
    //      If a child in the negative z-order list is compositing, then the layer itself
    //      must be compositing so that its contents render over that child.
    //      This implies that its positive z-index children must also be compositing.
    //
    void update(DeprecatedPaintLayer* root);

private:
    class OverlapMap;
    class RecursionData;

    void updateRecursive(DeprecatedPaintLayer* ancestorLayer, DeprecatedPaintLayer* currentLayer, OverlapMap&, RecursionData&, bool& descendantHas3DTransform, Vector<DeprecatedPaintLayer*>& unclippedDescendants, IntRect& absoluteDecendantBoundingBox);

    LayoutView& m_layoutView;
    CompositingReasonFinder& m_compositingReasonFinder;
};

} // namespace blink

#endif // CompositingRequirementsUpdater_h
