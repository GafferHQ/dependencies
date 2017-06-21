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

#include "config.h"
#include "core/layout/compositing/CompositingRequirementsUpdater.h"

#include "core/layout/LayoutView.h"
#include "core/layout/compositing/DeprecatedPaintLayerCompositor.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "core/paint/DeprecatedPaintLayerStackingNode.h"
#include "core/paint/DeprecatedPaintLayerStackingNodeIterator.h"
#include "platform/TraceEvent.h"

namespace blink {

class OverlapMapContainer {
public:
    void add(const IntRect& bounds)
    {
        m_layerRects.append(bounds);
        m_boundingBox.unite(bounds);
    }

    bool overlapsLayers(const IntRect& bounds) const
    {
        // Checking with the bounding box will quickly reject cases when
        // layers are created for lists of items going in one direction and
        // never overlap with each other.
        if (!bounds.intersects(m_boundingBox))
            return false;
        for (unsigned i = 0; i < m_layerRects.size(); i++) {
            if (m_layerRects[i].intersects(bounds))
                return true;
        }
        return false;
    }

    void unite(const OverlapMapContainer& otherContainer)
    {
        m_layerRects.appendVector(otherContainer.m_layerRects);
        m_boundingBox.unite(otherContainer.m_boundingBox);
    }
private:
    Vector<IntRect, 64> m_layerRects;
    IntRect m_boundingBox;
};

class CompositingRequirementsUpdater::OverlapMap {
    WTF_MAKE_NONCOPYABLE(OverlapMap);
public:
    OverlapMap()
    {
        // Begin by assuming the root layer will be composited so that there
        // is something on the stack. The root layer should also never get a
        // finishCurrentOverlapTestingContext() call.
        beginNewOverlapTestingContext();
    }

    void add(DeprecatedPaintLayer* layer, const IntRect& bounds)
    {
        ASSERT(!layer->isRootLayer());
        if (bounds.isEmpty())
            return;

        // Layers do not contribute to overlap immediately--instead, they will
        // contribute to overlap as soon as they have been recursively processed
        // and popped off the stack.
        ASSERT(m_overlapStack.size() >= 2);
        m_overlapStack[m_overlapStack.size() - 2].add(bounds);
    }

    bool overlapsLayers(const IntRect& bounds) const
    {
        return m_overlapStack.last().overlapsLayers(bounds);
    }

    void beginNewOverlapTestingContext()
    {
        // This effectively creates a new "clean slate" for overlap state.
        // This is used when we know that a subtree or remaining set of
        // siblings does not need to check overlap with things behind it.
        m_overlapStack.append(OverlapMapContainer());
    }

    void finishCurrentOverlapTestingContext()
    {
        // The overlap information on the top of the stack is still necessary
        // for checking overlap of any layers outside this context that may
        // overlap things from inside this context. Therefore, we must merge
        // the information from the top of the stack before popping the stack.
        //
        // FIXME: we may be able to avoid this deep copy by rearranging how
        //        overlapMap state is managed.
        m_overlapStack[m_overlapStack.size() - 2].unite(m_overlapStack.last());
        m_overlapStack.removeLast();
    }

private:
    Vector<OverlapMapContainer> m_overlapStack;
};

class CompositingRequirementsUpdater::RecursionData {
public:
    explicit RecursionData(DeprecatedPaintLayer* compositingAncestor)
        : m_compositingAncestor(compositingAncestor)
        , m_subtreeIsCompositing(false)
        , m_hasUnisolatedCompositedBlendingDescendant(false)
        , m_testingOverlap(true)
        , m_hasCompositedScrollingAncestor(false)
    {
    }

    DeprecatedPaintLayer* m_compositingAncestor;
    bool m_subtreeIsCompositing;
    bool m_hasUnisolatedCompositedBlendingDescendant;
    bool m_testingOverlap;
    bool m_hasCompositedScrollingAncestor;
};

static bool requiresCompositingOrSquashing(CompositingReasons reasons)
{
#if ENABLE(ASSERT)
    bool fastAnswer = reasons != CompositingReasonNone;
    bool slowAnswer = requiresCompositing(reasons) || requiresSquashing(reasons);
    ASSERT(fastAnswer == slowAnswer);
#endif
    return reasons != CompositingReasonNone;
}

static CompositingReasons subtreeReasonsForCompositing(DeprecatedPaintLayer* layer, bool hasCompositedDescendants, bool has3DTransformedDescendants)
{
    CompositingReasons subtreeReasons = CompositingReasonNone;

    // When a layer has composited descendants, some effects, like 2d transforms, filters, masks etc must be implemented
    // via compositing so that they also apply to those composited descdendants.
    if (hasCompositedDescendants) {
        subtreeReasons |= layer->potentialCompositingReasonsFromStyle() & CompositingReasonComboCompositedDescendants;

        if (layer->shouldIsolateCompositedDescendants()) {
            ASSERT(layer->stackingNode()->isStackingContext());
            subtreeReasons |= CompositingReasonIsolateCompositedDescendants;
        }

        // FIXME: This should move into CompositingReasonFinder::potentialCompositingReasonsFromStyle, but
        // theres a poor interaction with LayoutTextControlSingleLine, which sets this hasOverflowClip directly.
        if (layer->layoutObject()->hasClipOrOverflowClip())
            subtreeReasons |= CompositingReasonClipsCompositingDescendants;
    }

    // A layer with preserve-3d or perspective only needs to be composited if there are descendant layers that
    // will be affected by the preserve-3d or perspective.
    if (has3DTransformedDescendants)
        subtreeReasons |= layer->potentialCompositingReasonsFromStyle() & CompositingReasonCombo3DDescendants;

    return subtreeReasons;
}

CompositingRequirementsUpdater::CompositingRequirementsUpdater(LayoutView& layoutView, CompositingReasonFinder& compositingReasonFinder)
    : m_layoutView(layoutView)
    , m_compositingReasonFinder(compositingReasonFinder)
{
}

CompositingRequirementsUpdater::~CompositingRequirementsUpdater()
{
}

void CompositingRequirementsUpdater::update(DeprecatedPaintLayer* root)
{
    TRACE_EVENT0("blink", "CompositingRequirementsUpdater::updateRecursive");

    // Go through the layers in presentation order, so that we can compute which Layers need compositing layers.
    // FIXME: we could maybe do this and the hierarchy update in one pass, but the parenting logic would be more complex.
    RecursionData recursionData(root);
    OverlapMap overlapTestRequestMap;
    bool saw3DTransform = false;

    // FIXME: Passing these unclippedDescendants down and keeping track
    // of them dynamically, we are requiring a full tree walk. This
    // should be removed as soon as proper overlap testing based on
    // scrolling and animation bounds is implemented (crbug.com/252472).
    Vector<DeprecatedPaintLayer*> unclippedDescendants;
    IntRect absoluteDecendantBoundingBox;
    updateRecursive(0, root, overlapTestRequestMap, recursionData, saw3DTransform, unclippedDescendants, absoluteDecendantBoundingBox);
}

void CompositingRequirementsUpdater::updateRecursive(DeprecatedPaintLayer* ancestorLayer, DeprecatedPaintLayer* layer, OverlapMap& overlapMap, RecursionData& currentRecursionData, bool& descendantHas3DTransform, Vector<DeprecatedPaintLayer*>& unclippedDescendants, IntRect& absoluteDecendantBoundingBox)
{
    DeprecatedPaintLayerCompositor* compositor = m_layoutView.compositor();

    layer->stackingNode()->updateLayerListsIfNeeded();

    CompositingReasons reasonsToComposite = CompositingReasonNone;
    CompositingReasons directReasons = m_compositingReasonFinder.directReasons(layer);

    // Video is special. It's the only DeprecatedPaintLayer type that can both have
    // DeprecatedPaintLayer children and whose children can't use its backing to render
    // into. These children (the controls) always need to be promoted into their
    // own layers to draw on top of the accelerated video.
    if (currentRecursionData.m_compositingAncestor && currentRecursionData.m_compositingAncestor->layoutObject()->isVideo())
        directReasons |= CompositingReasonVideoOverlay;

    if (currentRecursionData.m_hasCompositedScrollingAncestor && layer->layoutObject()->styleRef().hasViewportConstrainedPosition())
        directReasons |= CompositingReasonPositionFixed;

    if (compositor->canBeComposited(layer)) {
        reasonsToComposite |= directReasons;

        if (layer->isRootLayer() && compositor->rootShouldAlwaysComposite())
            reasonsToComposite |= CompositingReasonRoot;

        if (reasonsToComposite && layer->scrollsOverflow() && !layer->needsCompositedScrolling()) {
            // We will only set needsCompositedScrolling if we don't care about
            // the LCD text hit, we may be able to switch to the compositor
            // driven path if we're alread composited for other reasons and are
            // therefore using grayscale AA.
            //
            // FIXME: it should also be possible to promote if the layer can
            // still use LCD text when promoted, but detecting when the
            // compositor can do this is tricky. Currently, the layer must be
            // both opaque and may only have an integer translation as its
            // transform. Both opacity and screen space transform are inherited
            // properties, so this cannot be determined from local information.
            layer->scrollableArea()->updateNeedsCompositedScrolling(DeprecatedPaintLayerScrollableArea::IgnoreLCDText);
            if (layer->needsCompositedScrolling())
                reasonsToComposite |= CompositingReasonOverflowScrollingTouch;
        }
    }

    if (reasonsToComposite & CompositingReasonOverflowScrollingTouch)
        currentRecursionData.m_hasCompositedScrollingAncestor = true;

    // Next, accumulate reasons related to overlap.
    // If overlap testing is used, this reason will be overridden. If overlap testing is not
    // used, we must assume we overlap if there is anything composited behind us in paint-order.
    CompositingReasons overlapCompositingReason = currentRecursionData.m_subtreeIsCompositing ? CompositingReasonAssumedOverlap : CompositingReasonNone;

    if (currentRecursionData.m_hasCompositedScrollingAncestor) {
        Vector<size_t> unclippedDescendantsToRemove;
        for (size_t i = 0; i < unclippedDescendants.size(); i++) {
            DeprecatedPaintLayer* unclippedDescendant = unclippedDescendants.at(i);
            // If we've reached the containing block of one of the unclipped
            // descendants, that element is no longer relevant to whether or not we
            // should opt in. Unfortunately we can't easily remove from the list
            // while we're iterating, so we have to store it for later removal.
            if (unclippedDescendant->layoutObject()->containingBlock() == layer->layoutObject()) {
                unclippedDescendantsToRemove.append(i);
                continue;
            }
            if (layer->scrollsWithRespectTo(unclippedDescendant))
                reasonsToComposite |= CompositingReasonAssumedOverlap;
        }

        // Remove irrelevant unclipped descendants in reverse order so our stored
        // indices remain valid.
        for (size_t i = 0; i < unclippedDescendantsToRemove.size(); i++)
            unclippedDescendants.remove(unclippedDescendantsToRemove.at(unclippedDescendantsToRemove.size() - i - 1));

        if (reasonsToComposite & CompositingReasonOutOfFlowClipping)
            unclippedDescendants.append(layer);
    }

    const IntRect& absBounds = layer->clippedAbsoluteBoundingBox();
    absoluteDecendantBoundingBox = absBounds;

    if (currentRecursionData.m_testingOverlap && !requiresCompositingOrSquashing(directReasons))
        overlapCompositingReason = overlapMap.overlapsLayers(absBounds) ? CompositingReasonOverlap : CompositingReasonNone;

    reasonsToComposite |= overlapCompositingReason;

    // The children of this layer don't need to composite, unless there is
    // a compositing layer among them, so start by inheriting the compositing
    // ancestor with m_subtreeIsCompositing set to false.
    RecursionData childRecursionData = currentRecursionData;
    childRecursionData.m_subtreeIsCompositing = false;

    bool willBeCompositedOrSquashed = compositor->canBeComposited(layer) && requiresCompositingOrSquashing(reasonsToComposite);
    if (willBeCompositedOrSquashed) {
        // Tell the parent it has compositing descendants.
        currentRecursionData.m_subtreeIsCompositing = true;
        // This layer now acts as the ancestor for kids.
        childRecursionData.m_compositingAncestor = layer;

        // Here we know that all children and the layer's own contents can blindly paint into
        // this layer's backing, until a descendant is composited. So, we don't need to check
        // for overlap with anything behind this layer.
        overlapMap.beginNewOverlapTestingContext();
        // This layer is going to be composited, so children can safely ignore the fact that there's an
        // animation running behind this layer, meaning they can rely on the overlap map testing again.
        childRecursionData.m_testingOverlap = true;
    }

#if ENABLE(ASSERT)
    LayerListMutationDetector mutationChecker(layer->stackingNode());
#endif

    bool anyDescendantHas3DTransform = false;
    bool willHaveForegroundLayer = false;

    if (layer->stackingNode()->isStackingContext()) {
        DeprecatedPaintLayerStackingNodeIterator iterator(*layer->stackingNode(), NegativeZOrderChildren);
        while (DeprecatedPaintLayerStackingNode* curNode = iterator.next()) {
            IntRect absoluteChildDecendantBoundingBox;
            updateRecursive(layer, curNode->layer(), overlapMap, childRecursionData, anyDescendantHas3DTransform, unclippedDescendants, absoluteChildDecendantBoundingBox);
            absoluteDecendantBoundingBox.unite(absoluteChildDecendantBoundingBox);

            // If we have to make a layer for this child, make one now so we can have a contents layer
            // (since we need to ensure that the -ve z-order child renders underneath our contents).
            if (childRecursionData.m_subtreeIsCompositing) {
                reasonsToComposite |= CompositingReasonNegativeZIndexChildren;

                if (!willBeCompositedOrSquashed) {
                    // make layer compositing
                    childRecursionData.m_compositingAncestor = layer;
                    overlapMap.beginNewOverlapTestingContext();
                    willBeCompositedOrSquashed = true;
                    willHaveForegroundLayer = true;

                    // FIXME: temporary solution for the first negative z-index composited child:
                    //        re-compute the absBounds for the child so that we can add the
                    //        negative z-index child's bounds to the new overlap context.
                    overlapMap.beginNewOverlapTestingContext();
                    overlapMap.add(curNode->layer(), curNode->layer()->clippedAbsoluteBoundingBox());
                    overlapMap.finishCurrentOverlapTestingContext();
                }
            }
        }
    }

    if (willHaveForegroundLayer) {
        ASSERT(willBeCompositedOrSquashed);
        // A foreground layer effectively is a new backing for all subsequent children, so
        // we don't need to test for overlap with anything behind this. So, we can finish
        // the previous context that was accumulating rects for the negative z-index
        // children, and start with a fresh new empty context.
        overlapMap.finishCurrentOverlapTestingContext();
        overlapMap.beginNewOverlapTestingContext();
        // This layer is going to be composited, so children can safely ignore the fact that there's an
        // animation running behind this layer, meaning they can rely on the overlap map testing again
        childRecursionData.m_testingOverlap = true;
    }

    DeprecatedPaintLayerStackingNodeIterator iterator(*layer->stackingNode(), NormalFlowChildren | PositiveZOrderChildren);
    while (DeprecatedPaintLayerStackingNode* curNode = iterator.next()) {
        IntRect absoluteChildDecendantBoundingBox;
        updateRecursive(layer, curNode->layer(), overlapMap, childRecursionData, anyDescendantHas3DTransform, unclippedDescendants, absoluteChildDecendantBoundingBox);
        absoluteDecendantBoundingBox.unite(absoluteChildDecendantBoundingBox);
    }

    // Now that the subtree has been traversed, we can check for compositing reasons that depended on the state of the subtree.

    if (layer->stackingNode()->isStackingContext()) {
        layer->setShouldIsolateCompositedDescendants(childRecursionData.m_hasUnisolatedCompositedBlendingDescendant);
    } else {
        layer->setShouldIsolateCompositedDescendants(false);
        currentRecursionData.m_hasUnisolatedCompositedBlendingDescendant = childRecursionData.m_hasUnisolatedCompositedBlendingDescendant;
    }

    // Subsequent layers in the parent's stacking context may also need to composite.
    if (childRecursionData.m_subtreeIsCompositing)
        currentRecursionData.m_subtreeIsCompositing = true;

    // Set the flag to say that this SC has compositing children.
    layer->setHasCompositingDescendant(childRecursionData.m_subtreeIsCompositing);

    if (layer->isRootLayer()) {
        // The root layer needs to be composited if anything else in the tree is composited.
        // Otherwise, we can disable compositing entirely.
        if (childRecursionData.m_subtreeIsCompositing || requiresCompositingOrSquashing(reasonsToComposite) || compositor->rootShouldAlwaysComposite()) {
            reasonsToComposite |= CompositingReasonRoot;
        } else {
            compositor->setCompositingModeEnabled(false);
            reasonsToComposite = CompositingReasonNone;
        }
    } else {
        // All layers (even ones that aren't being composited) need to get added to
        // the overlap map. Layers that are not separately composited will paint into their
        // compositing ancestor's backing, and so are still considered for overlap.
        if (childRecursionData.m_compositingAncestor && !childRecursionData.m_compositingAncestor->isRootLayer())
            overlapMap.add(layer, absBounds);

        // Now check for reasons to become composited that depend on the state of descendant layers.
        CompositingReasons subtreeCompositingReasons = subtreeReasonsForCompositing(layer, childRecursionData.m_subtreeIsCompositing, anyDescendantHas3DTransform);
        reasonsToComposite |= subtreeCompositingReasons;
        if (!willBeCompositedOrSquashed && compositor->canBeComposited(layer) && requiresCompositingOrSquashing(subtreeCompositingReasons)) {
            childRecursionData.m_compositingAncestor = layer;
            // FIXME: this context push is effectively a no-op but needs to exist for
            // now, because the code is designed to push overlap information to the
            // second-from-top context of the stack.
            overlapMap.beginNewOverlapTestingContext();
            overlapMap.add(layer, absoluteDecendantBoundingBox);
            willBeCompositedOrSquashed = true;
        }

        if (willBeCompositedOrSquashed)
            reasonsToComposite |= layer->potentialCompositingReasonsFromStyle() & CompositingReasonInlineTransform;

        // If the original layer is composited, the reflection needs to be, too.
        if (layer->reflectionInfo()) {
            // FIXME: Shouldn't we call computeCompositingRequirements to handle a reflection overlapping with another layoutObject?
            DeprecatedPaintLayer* reflectionLayer = layer->reflectionInfo()->reflectionLayer();
            CompositingReasons reflectionCompositingReason = willBeCompositedOrSquashed ? CompositingReasonReflectionOfCompositedParent : CompositingReasonNone;
            reflectionLayer->setCompositingReasons(reflectionCompositingReason, CompositingReasonReflectionOfCompositedParent);
        }

        if (willBeCompositedOrSquashed && layer->layoutObject()->style()->hasBlendMode())
            currentRecursionData.m_hasUnisolatedCompositedBlendingDescendant = true;

        // Turn overlap testing off for later layers if it's already off, or if we have an animating transform.
        // Note that if the layer clips its descendants, there's no reason to propagate the child animation to the parent layers. That's because
        // we know for sure the animation is contained inside the clipping rectangle, which is already added to the overlap map.
        bool isCompositedClippingLayer = compositor->canBeComposited(layer) && (reasonsToComposite & CompositingReasonClipsCompositingDescendants);
        bool isCompositedWithInlineTransform = reasonsToComposite & CompositingReasonInlineTransform;
        if ((!childRecursionData.m_testingOverlap && !isCompositedClippingLayer) || layer->layoutObject()->style()->hasCurrentTransformAnimation() || isCompositedWithInlineTransform)
            currentRecursionData.m_testingOverlap = false;

        if (childRecursionData.m_compositingAncestor == layer)
            overlapMap.finishCurrentOverlapTestingContext();

        descendantHas3DTransform |= anyDescendantHas3DTransform || layer->has3DTransform();
    }

    // At this point we have finished collecting all reasons to composite this layer.
    layer->setCompositingReasons(reasonsToComposite);

}

} // namespace blink
