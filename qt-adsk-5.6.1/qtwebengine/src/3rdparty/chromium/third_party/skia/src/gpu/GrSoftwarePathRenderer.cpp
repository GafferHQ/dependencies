
/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrSoftwarePathRenderer.h"
#include "GrContext.h"
#include "GrSWMaskHelper.h"
#include "GrVertexBuffer.h"

////////////////////////////////////////////////////////////////////////////////
bool GrSoftwarePathRenderer::canDrawPath(const GrDrawTarget*,
                                         const GrPipelineBuilder*,
                                         const SkMatrix& viewMatrix,
                                         const SkPath&,
                                         const GrStrokeInfo& stroke,
                                         bool antiAlias) const {
    if (NULL == fContext) {
        return false;
    }
    if (stroke.isDashed()) {
        return false;
    }
    return true;
}

GrPathRenderer::StencilSupport
GrSoftwarePathRenderer::onGetStencilSupport(const GrDrawTarget*,
                                            const GrPipelineBuilder*,
                                            const SkPath&,
                                            const GrStrokeInfo&) const {
    return GrPathRenderer::kNoSupport_StencilSupport;
}

namespace {

////////////////////////////////////////////////////////////////////////////////
// gets device coord bounds of path (not considering the fill) and clip. The
// path bounds will be a subset of the clip bounds. returns false if
// path bounds would be empty.
bool get_path_and_clip_bounds(const GrDrawTarget* target,
                              const GrPipelineBuilder* pipelineBuilder,
                              const SkPath& path,
                              const SkMatrix& matrix,
                              SkIRect* devPathBounds,
                              SkIRect* devClipBounds) {
    // compute bounds as intersection of rt size, clip, and path
    const GrRenderTarget* rt = pipelineBuilder->getRenderTarget();
    if (NULL == rt) {
        return false;
    }

    pipelineBuilder->clip().getConservativeBounds(rt, devClipBounds);

    if (devClipBounds->isEmpty()) {
        *devPathBounds = SkIRect::MakeWH(rt->width(), rt->height());
        return false;
    }

    if (!path.getBounds().isEmpty()) {
        SkRect pathSBounds;
        matrix.mapRect(&pathSBounds, path.getBounds());
        SkIRect pathIBounds;
        pathSBounds.roundOut(&pathIBounds);
        *devPathBounds = *devClipBounds;
        if (!devPathBounds->intersect(pathIBounds)) {
            // set the correct path bounds, as this would be used later.
            *devPathBounds = pathIBounds;
            return false;
        }
    } else {
        *devPathBounds = SkIRect::EmptyIRect();
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
void draw_around_inv_path(GrDrawTarget* target,
                          GrPipelineBuilder* pipelineBuilder,
                          GrColor color,
                          const SkMatrix& viewMatrix,
                          const SkIRect& devClipBounds,
                          const SkIRect& devPathBounds) {
    SkMatrix invert;
    if (!viewMatrix.invert(&invert)) {
        return;
    }

    SkRect rect;
    if (devClipBounds.fTop < devPathBounds.fTop) {
        rect.iset(devClipBounds.fLeft, devClipBounds.fTop,
                  devClipBounds.fRight, devPathBounds.fTop);
        target->drawBWRect(pipelineBuilder, color, SkMatrix::I(), rect, NULL, &invert);
    }
    if (devClipBounds.fLeft < devPathBounds.fLeft) {
        rect.iset(devClipBounds.fLeft, devPathBounds.fTop,
                  devPathBounds.fLeft, devPathBounds.fBottom);
        target->drawBWRect(pipelineBuilder, color, SkMatrix::I(), rect, NULL, &invert);
    }
    if (devClipBounds.fRight > devPathBounds.fRight) {
        rect.iset(devPathBounds.fRight, devPathBounds.fTop,
                  devClipBounds.fRight, devPathBounds.fBottom);
        target->drawBWRect(pipelineBuilder, color, SkMatrix::I(), rect, NULL, &invert);
    }
    if (devClipBounds.fBottom > devPathBounds.fBottom) {
        rect.iset(devClipBounds.fLeft, devPathBounds.fBottom,
                  devClipBounds.fRight, devClipBounds.fBottom);
        target->drawBWRect(pipelineBuilder, color, SkMatrix::I(), rect, NULL, &invert);
    }
}

}

////////////////////////////////////////////////////////////////////////////////
// return true on success; false on failure
bool GrSoftwarePathRenderer::onDrawPath(GrDrawTarget* target,
                                        GrPipelineBuilder* pipelineBuilder,
                                        GrColor color,
                                        const SkMatrix& viewMatrix,
                                        const SkPath& path,
                                        const GrStrokeInfo& stroke,
                                        bool antiAlias) {
    if (NULL == fContext) {
        return false;
    }

    SkIRect devPathBounds, devClipBounds;
    if (!get_path_and_clip_bounds(target, pipelineBuilder, path, viewMatrix, &devPathBounds,
                                  &devClipBounds)) {
        if (path.isInverseFillType()) {
            draw_around_inv_path(target, pipelineBuilder, color, viewMatrix, devClipBounds,
                                 devPathBounds);
        }
        return true;
    }

    SkAutoTUnref<GrTexture> texture(
            GrSWMaskHelper::DrawPathMaskToTexture(fContext, path, stroke,
                                                  devPathBounds,
                                                  antiAlias, &viewMatrix));
    if (NULL == texture) {
        return false;
    }

    GrSWMaskHelper::DrawToTargetWithPathMask(texture, target, pipelineBuilder, color, viewMatrix,
                                             devPathBounds);

    if (path.isInverseFillType()) {
        draw_around_inv_path(target, pipelineBuilder, color, viewMatrix, devClipBounds,
                             devPathBounds);
    }

    return true;
}
