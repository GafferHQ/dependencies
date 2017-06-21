
/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "GrStencilAndCoverPathRenderer.h"
#include "GrCaps.h"
#include "GrContext.h"
#include "GrGpu.h"
#include "GrPath.h"
#include "GrRenderTarget.h"
#include "GrResourceProvider.h"
#include "GrStrokeInfo.h"

/*
 * For now paths only natively support winding and even odd fill types
 */
static GrPathRendering::FillType convert_skpath_filltype(SkPath::FillType fill) {
    switch (fill) {
        default:
            SkFAIL("Incomplete Switch\n");
        case SkPath::kWinding_FillType:
        case SkPath::kInverseWinding_FillType:
            return GrPathRendering::kWinding_FillType;
        case SkPath::kEvenOdd_FillType:
        case SkPath::kInverseEvenOdd_FillType:
            return GrPathRendering::kEvenOdd_FillType;
    }
}

GrPathRenderer* GrStencilAndCoverPathRenderer::Create(GrResourceProvider* resourceProvider,
                                                      const GrCaps& caps) {
    if (caps.shaderCaps()->pathRenderingSupport()) {
        return SkNEW_ARGS(GrStencilAndCoverPathRenderer, (resourceProvider));
    } else {
        return NULL;
    }
}

GrStencilAndCoverPathRenderer::GrStencilAndCoverPathRenderer(GrResourceProvider* resourceProvider)
    : fResourceProvider(resourceProvider) {    
}

bool GrStencilAndCoverPathRenderer::canDrawPath(const GrDrawTarget* target,
                                                const GrPipelineBuilder* pipelineBuilder,
                                                const SkMatrix& viewMatrix,
                                                const SkPath& path,
                                                const GrStrokeInfo& stroke,
                                                bool antiAlias) const {
    if (stroke.isHairlineStyle()) {
        return false;
    }
    if (!pipelineBuilder->getStencil().isDisabled()) {
        return false;
    }
    if (antiAlias) {
        return pipelineBuilder->getRenderTarget()->isStencilBufferMultisampled();
    } else {
        return true; // doesn't do per-path AA, relies on the target having MSAA
    }
}

GrPathRenderer::StencilSupport
GrStencilAndCoverPathRenderer::onGetStencilSupport(const GrDrawTarget*,
                                                   const GrPipelineBuilder*,
                                                   const SkPath&,
                                                   const GrStrokeInfo&) const {
    return GrPathRenderer::kStencilOnly_StencilSupport;
}

static GrPath* get_gr_path(GrResourceProvider* resourceProvider, const SkPath& skPath,
                           const GrStrokeInfo& stroke) {
    GrUniqueKey key;
    bool isVolatile;
    GrPath::ComputeKey(skPath, stroke, &key, &isVolatile);
    SkAutoTUnref<GrPath> path(
        static_cast<GrPath*>(resourceProvider->findAndRefResourceByUniqueKey(key)));
    if (!path) {
        path.reset(resourceProvider->createPath(skPath, stroke));
        if (!isVolatile) {
            resourceProvider->assignUniqueKeyToResource(key, path);
        }
    } else {
        SkASSERT(path->isEqualTo(skPath, stroke));
    }
    return path.detach();
}

void GrStencilAndCoverPathRenderer::onStencilPath(GrDrawTarget* target,
                                                  GrPipelineBuilder* pipelineBuilder,
                                                  const SkMatrix& viewMatrix,
                                                  const SkPath& path,
                                                  const GrStrokeInfo& stroke) {
    SkASSERT(!path.isInverseFillType());
    SkAutoTUnref<GrPathProcessor> pp(GrPathProcessor::Create(GrColor_WHITE, viewMatrix));
    SkAutoTUnref<GrPath> p(get_gr_path(fResourceProvider, path, stroke));
    target->stencilPath(pipelineBuilder, pp, p, convert_skpath_filltype(path.getFillType()));
}

bool GrStencilAndCoverPathRenderer::onDrawPath(GrDrawTarget* target,
                                               GrPipelineBuilder* pipelineBuilder,
                                               GrColor color,
                                               const SkMatrix& viewMatrix,
                                               const SkPath& path,
                                               const GrStrokeInfo& stroke,
                                               bool antiAlias) {
    SkASSERT(!stroke.isHairlineStyle());
    SkASSERT(pipelineBuilder->getStencil().isDisabled());

    if (antiAlias) {
        SkASSERT(pipelineBuilder->getRenderTarget()->isStencilBufferMultisampled());
        pipelineBuilder->enableState(GrPipelineBuilder::kHWAntialias_Flag);
    }

    SkAutoTUnref<GrPath> p(get_gr_path(fResourceProvider, path, stroke));

    if (path.isInverseFillType()) {
        GR_STATIC_CONST_SAME_STENCIL(kInvertedStencilPass,
            kZero_StencilOp,
            kZero_StencilOp,
            // We know our rect will hit pixels outside the clip and the user bits will be 0
            // outside the clip. So we can't just fill where the user bits are 0. We also need to
            // check that the clip bit is set.
            kEqualIfInClip_StencilFunc,
            0xffff,
            0x0000,
            0xffff);

        pipelineBuilder->setStencil(kInvertedStencilPass);

        // fake inverse with a stencil and cover
        SkAutoTUnref<GrPathProcessor> pp(GrPathProcessor::Create(GrColor_WHITE, viewMatrix));
        target->stencilPath(pipelineBuilder, pp, p, convert_skpath_filltype(path.getFillType()));

        SkMatrix invert = SkMatrix::I();
        SkRect bounds =
            SkRect::MakeLTRB(0, 0, SkIntToScalar(pipelineBuilder->getRenderTarget()->width()),
                             SkIntToScalar(pipelineBuilder->getRenderTarget()->height()));
        SkMatrix vmi;
        // mapRect through persp matrix may not be correct
        if (!viewMatrix.hasPerspective() && viewMatrix.invert(&vmi)) {
            vmi.mapRect(&bounds);
            // theoretically could set bloat = 0, instead leave it because of matrix inversion
            // precision.
            SkScalar bloat = viewMatrix.getMaxScale() * SK_ScalarHalf;
            bounds.outset(bloat, bloat);
        } else {
            if (!viewMatrix.invert(&invert)) {
                return false;
            }
        }
        const SkMatrix& viewM = viewMatrix.hasPerspective() ? SkMatrix::I() : viewMatrix;
        target->drawBWRect(pipelineBuilder, color, viewM, bounds, NULL, &invert);
    } else {
        GR_STATIC_CONST_SAME_STENCIL(kStencilPass,
            kZero_StencilOp,
            kZero_StencilOp,
            kNotEqual_StencilFunc,
            0xffff,
            0x0000,
            0xffff);

        pipelineBuilder->setStencil(kStencilPass);
        SkAutoTUnref<GrPathProcessor> pp(GrPathProcessor::Create(color, viewMatrix));
        target->drawPath(pipelineBuilder, pp, p, convert_skpath_filltype(path.getFillType()));
    }

    pipelineBuilder->stencil()->setDisabled();
    return true;
}
