/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrDefaultPathRenderer.h"

#include "GrBatch.h"
#include "GrBatchTarget.h"
#include "GrBatchTest.h"
#include "GrContext.h"
#include "GrDefaultGeoProcFactory.h"
#include "GrPathUtils.h"
#include "GrPipelineBuilder.h"
#include "GrVertices.h"
#include "SkGeometry.h"
#include "SkString.h"
#include "SkStrokeRec.h"
#include "SkTLazy.h"
#include "SkTraceEvent.h"

GrDefaultPathRenderer::GrDefaultPathRenderer(bool separateStencilSupport,
                                             bool stencilWrapOpsSupport)
    : fSeparateStencil(separateStencilSupport)
    , fStencilWrapOps(stencilWrapOpsSupport) {
}


////////////////////////////////////////////////////////////////////////////////
// Stencil rules for paths

////// Even/Odd

GR_STATIC_CONST_SAME_STENCIL(gEOStencilPass,
    kInvert_StencilOp,
    kKeep_StencilOp,
    kAlwaysIfInClip_StencilFunc,
    0xffff,
    0xffff,
    0xffff);

// ok not to check clip b/c stencil pass only wrote inside clip
GR_STATIC_CONST_SAME_STENCIL(gEOColorPass,
    kZero_StencilOp,
    kZero_StencilOp,
    kNotEqual_StencilFunc,
    0xffff,
    0x0000,
    0xffff);

// have to check clip b/c outside clip will always be zero.
GR_STATIC_CONST_SAME_STENCIL(gInvEOColorPass,
    kZero_StencilOp,
    kZero_StencilOp,
    kEqualIfInClip_StencilFunc,
    0xffff,
    0x0000,
    0xffff);

////// Winding

// when we have separate stencil we increment front faces / decrement back faces
// when we don't have wrap incr and decr we use the stencil test to simulate
// them.

GR_STATIC_CONST_STENCIL(gWindStencilSeparateWithWrap,
    kIncWrap_StencilOp,             kDecWrap_StencilOp,
    kKeep_StencilOp,                kKeep_StencilOp,
    kAlwaysIfInClip_StencilFunc,    kAlwaysIfInClip_StencilFunc,
    0xffff,                         0xffff,
    0xffff,                         0xffff,
    0xffff,                         0xffff);

// if inc'ing the max value, invert to make 0
// if dec'ing zero invert to make all ones.
// we can't avoid touching the stencil on both passing and
// failing, so we can't resctrict ourselves to the clip.
GR_STATIC_CONST_STENCIL(gWindStencilSeparateNoWrap,
    kInvert_StencilOp,              kInvert_StencilOp,
    kIncClamp_StencilOp,            kDecClamp_StencilOp,
    kEqual_StencilFunc,             kEqual_StencilFunc,
    0xffff,                         0xffff,
    0xffff,                         0x0000,
    0xffff,                         0xffff);

// When there are no separate faces we do two passes to setup the winding rule
// stencil. First we draw the front faces and inc, then we draw the back faces
// and dec. These are same as the above two split into the incrementing and
// decrementing passes.
GR_STATIC_CONST_SAME_STENCIL(gWindSingleStencilWithWrapInc,
    kIncWrap_StencilOp,
    kKeep_StencilOp,
    kAlwaysIfInClip_StencilFunc,
    0xffff,
    0xffff,
    0xffff);

GR_STATIC_CONST_SAME_STENCIL(gWindSingleStencilWithWrapDec,
    kDecWrap_StencilOp,
    kKeep_StencilOp,
    kAlwaysIfInClip_StencilFunc,
    0xffff,
    0xffff,
    0xffff);

GR_STATIC_CONST_SAME_STENCIL(gWindSingleStencilNoWrapInc,
    kInvert_StencilOp,
    kIncClamp_StencilOp,
    kEqual_StencilFunc,
    0xffff,
    0xffff,
    0xffff);

GR_STATIC_CONST_SAME_STENCIL(gWindSingleStencilNoWrapDec,
    kInvert_StencilOp,
    kDecClamp_StencilOp,
    kEqual_StencilFunc,
    0xffff,
    0x0000,
    0xffff);

// Color passes are the same whether we use the two-sided stencil or two passes

GR_STATIC_CONST_SAME_STENCIL(gWindColorPass,
    kZero_StencilOp,
    kZero_StencilOp,
    kNonZeroIfInClip_StencilFunc,
    0xffff,
    0x0000,
    0xffff);

GR_STATIC_CONST_SAME_STENCIL(gInvWindColorPass,
    kZero_StencilOp,
    kZero_StencilOp,
    kEqualIfInClip_StencilFunc,
    0xffff,
    0x0000,
    0xffff);

////// Normal render to stencil

// Sometimes the default path renderer can draw a path directly to the stencil
// buffer without having to first resolve the interior / exterior.
GR_STATIC_CONST_SAME_STENCIL(gDirectToStencil,
    kZero_StencilOp,
    kIncClamp_StencilOp,
    kAlwaysIfInClip_StencilFunc,
    0xffff,
    0x0000,
    0xffff);

////////////////////////////////////////////////////////////////////////////////
// Helpers for drawPath

#define STENCIL_OFF     0   // Always disable stencil (even when needed)

static inline bool single_pass_path(const SkPath& path, const SkStrokeRec& stroke) {
#if STENCIL_OFF
    return true;
#else
    if (!stroke.isHairlineStyle() && !path.isInverseFillType()) {
        return path.isConvex();
    }
    return false;
#endif
}

GrPathRenderer::StencilSupport
GrDefaultPathRenderer::onGetStencilSupport(const GrDrawTarget*,
                                           const GrPipelineBuilder*,
                                           const SkPath& path,
                                           const GrStrokeInfo& stroke) const {
    if (single_pass_path(path, stroke)) {
        return GrPathRenderer::kNoRestriction_StencilSupport;
    } else {
        return GrPathRenderer::kStencilOnly_StencilSupport;
    }
}

static inline void append_countour_edge_indices(bool hairLine,
                                                uint16_t fanCenterIdx,
                                                uint16_t edgeV0Idx,
                                                uint16_t** indices) {
    // when drawing lines we're appending line segments along
    // the contour. When applying the other fill rules we're
    // drawing triangle fans around fanCenterIdx.
    if (!hairLine) {
        *((*indices)++) = fanCenterIdx;
    }
    *((*indices)++) = edgeV0Idx;
    *((*indices)++) = edgeV0Idx + 1;
}

static inline void add_quad(SkPoint** vert, const SkPoint* base, const SkPoint pts[],
                            SkScalar srcSpaceTolSqd, SkScalar srcSpaceTol, bool indexed,
                            bool isHairline, uint16_t subpathIdxStart, int offset, uint16_t** idx) {
    // first pt of quad is the pt we ended on in previous step
    uint16_t firstQPtIdx = (uint16_t)(*vert - base) - 1 + offset;
    uint16_t numPts =  (uint16_t)
        GrPathUtils::generateQuadraticPoints(
            pts[0], pts[1], pts[2],
            srcSpaceTolSqd, vert,
            GrPathUtils::quadraticPointCount(pts, srcSpaceTol));
    if (indexed) {
        for (uint16_t i = 0; i < numPts; ++i) {
            append_countour_edge_indices(isHairline, subpathIdxStart,
                                         firstQPtIdx + i, idx);
        }
    }
}

class DefaultPathBatch : public GrBatch {
public:
    struct Geometry {
        GrColor fColor;
        SkPath fPath;
        SkScalar fTolerance;
    };

    static GrBatch* Create(const Geometry& geometry, uint8_t coverage, const SkMatrix& viewMatrix,
                           bool isHairline, const SkRect& devBounds) {
        return SkNEW_ARGS(DefaultPathBatch, (geometry, coverage, viewMatrix, isHairline,
                                             devBounds));
    }

    const char* name() const override { return "DefaultPathBatch"; }

    void getInvariantOutputColor(GrInitInvariantOutput* out) const override {
        // When this is called on a batch, there is only one geometry bundle
        out->setKnownFourComponents(fGeoData[0].fColor);
    }
    void getInvariantOutputCoverage(GrInitInvariantOutput* out) const override {
        out->setKnownSingleComponent(this->coverage());
    }

    void initBatchTracker(const GrPipelineInfo& init) override {
        // Handle any color overrides
        if (!init.readsColor()) {
            fGeoData[0].fColor = GrColor_ILLEGAL;
        }
        init.getOverrideColorIfSet(&fGeoData[0].fColor);

        // setup batch properties
        fBatch.fColorIgnored = !init.readsColor();
        fBatch.fColor = fGeoData[0].fColor;
        fBatch.fUsesLocalCoords = init.readsLocalCoords();
        fBatch.fCoverageIgnored = !init.readsCoverage();
    }

    void generateGeometry(GrBatchTarget* batchTarget, const GrPipeline* pipeline) override {
        SkAutoTUnref<const GrGeometryProcessor> gp(
                GrDefaultGeoProcFactory::Create(GrDefaultGeoProcFactory::kPosition_GPType,
                                                this->color(),
                                                this->usesLocalCoords(),
                                                this->coverageIgnored(),
                                                this->viewMatrix(),
                                                SkMatrix::I(),
                                                this->coverage()));

        size_t vertexStride = gp->getVertexStride();
        SkASSERT(vertexStride == sizeof(SkPoint));

        batchTarget->initDraw(gp, pipeline);

        int instanceCount = fGeoData.count();

        // compute number of vertices
        int maxVertices = 0;

        // We will use index buffers if we have multiple paths or one path with multiple contours
        bool isIndexed = instanceCount > 1;
        for (int i = 0; i < instanceCount; i++) {
            Geometry& args = fGeoData[i];

            int contourCount;
            maxVertices += GrPathUtils::worstCasePointCount(args.fPath, &contourCount,
                                                            args.fTolerance);

            isIndexed = isIndexed || contourCount > 1;
        }

        if (maxVertices == 0 || maxVertices > ((int)SK_MaxU16 + 1)) {
            SkDebugf("Cannot render path (%d)\n", maxVertices);
            return;
        }

        // determine primitiveType
        int maxIndices = 0;
        GrPrimitiveType primitiveType;
        if (this->isHairline()) {
            if (isIndexed) {
                maxIndices = 2 * maxVertices;
                primitiveType = kLines_GrPrimitiveType;
            } else {
                primitiveType = kLineStrip_GrPrimitiveType;
            }
        } else {
            if (isIndexed) {
                maxIndices = 3 * maxVertices;
                primitiveType = kTriangles_GrPrimitiveType;
            } else {
                primitiveType = kTriangleFan_GrPrimitiveType;
            }
        }

        // allocate vertex / index buffers
        const GrVertexBuffer* vertexBuffer;
        int firstVertex;

        void* verts = batchTarget->makeVertSpace(vertexStride, maxVertices,
                                                 &vertexBuffer, &firstVertex);

        if (!verts) {
            SkDebugf("Could not allocate vertices\n");
            return;
        }

        const GrIndexBuffer* indexBuffer = NULL;
        int firstIndex = 0;

        void* indices = NULL;
        if (isIndexed) {
            indices = batchTarget->makeIndexSpace(maxIndices, &indexBuffer, &firstIndex);

            if (!indices) {
                SkDebugf("Could not allocate indices\n");
                return;
            }
        }

        // fill buffers
        int vertexOffset = 0;
        int indexOffset = 0;
        for (int i = 0; i < instanceCount; i++) {
            Geometry& args = fGeoData[i];

            int vertexCnt = 0;
            int indexCnt = 0;
            if (!this->createGeom(verts,
                                  vertexOffset,
                                  indices,
                                  indexOffset,
                                  &vertexCnt,
                                  &indexCnt,
                                  args.fPath,
                                  args.fTolerance,
                                  isIndexed)) {
                return;
            }

            vertexOffset += vertexCnt;
            indexOffset += indexCnt;
            SkASSERT(vertexOffset <= maxVertices && indexOffset <= maxIndices);
        }

        GrVertices vertices;
        if (isIndexed) {
            vertices.initIndexed(primitiveType, vertexBuffer, indexBuffer, firstVertex, firstIndex,
                                 vertexOffset, indexOffset);
        } else {
            vertices.init(primitiveType, vertexBuffer, firstVertex, vertexOffset);
        }
        batchTarget->draw(vertices);

        // put back reserves
        batchTarget->putBackIndices((size_t)(maxIndices - indexOffset));
        batchTarget->putBackVertices((size_t)(maxVertices - vertexOffset), (size_t)vertexStride);
    }

    SkSTArray<1, Geometry, true>* geoData() { return &fGeoData; }

private:
    DefaultPathBatch(const Geometry& geometry, uint8_t coverage, const SkMatrix& viewMatrix,
                     bool isHairline, const SkRect& devBounds) {
        this->initClassID<DefaultPathBatch>();
        fBatch.fCoverage = coverage;
        fBatch.fIsHairline = isHairline;
        fBatch.fViewMatrix = viewMatrix;
        fGeoData.push_back(geometry);

        this->setBounds(devBounds);
    }

    bool onCombineIfPossible(GrBatch* t) override {
        DefaultPathBatch* that = t->cast<DefaultPathBatch>();

        if (this->color() != that->color()) {
            return false;
        }

        if (this->coverage() != that->coverage()) {
            return false;
        }

        if (!this->viewMatrix().cheapEqualTo(that->viewMatrix())) {
            return false;
        }

        if (this->isHairline() != that->isHairline()) {
            return false;
        }

        fGeoData.push_back_n(that->geoData()->count(), that->geoData()->begin());
        this->joinBounds(that->bounds());
        return true;
    }

    bool createGeom(void* vertices,
                    size_t vertexOffset,
                    void* indices,
                    size_t indexOffset,
                    int* vertexCnt,
                    int* indexCnt,
                    const SkPath& path,
                    SkScalar srcSpaceTol,
                    bool isIndexed)  {
        {
            SkScalar srcSpaceTolSqd = SkScalarMul(srcSpaceTol, srcSpaceTol);

            uint16_t indexOffsetU16 = (uint16_t)indexOffset;
            uint16_t vertexOffsetU16 = (uint16_t)vertexOffset;

            uint16_t* idxBase = reinterpret_cast<uint16_t*>(indices) + indexOffsetU16;
            uint16_t* idx = idxBase;
            uint16_t subpathIdxStart = vertexOffsetU16;

            SkPoint* base = reinterpret_cast<SkPoint*>(vertices) + vertexOffset;
            SkPoint* vert = base;

            SkPoint pts[4];

            bool first = true;
            int subpath = 0;

            SkPath::Iter iter(path, false);

            bool done = false;
            while (!done) {
                SkPath::Verb verb = iter.next(pts);
                switch (verb) {
                    case SkPath::kMove_Verb:
                        if (!first) {
                            uint16_t currIdx = (uint16_t) (vert - base) + vertexOffsetU16;
                            subpathIdxStart = currIdx;
                            ++subpath;
                        }
                        *vert = pts[0];
                        vert++;
                        break;
                    case SkPath::kLine_Verb:
                        if (isIndexed) {
                            uint16_t prevIdx = (uint16_t)(vert - base) - 1 + vertexOffsetU16;
                            append_countour_edge_indices(this->isHairline(), subpathIdxStart,
                                                         prevIdx, &idx);
                        }
                        *(vert++) = pts[1];
                        break;
                    case SkPath::kConic_Verb: {
                        SkScalar weight = iter.conicWeight();
                        SkAutoConicToQuads converter;
                        // Converting in src-space, hance the finer tolerance (0.25)
                        // TODO: find a way to do this in dev-space so the tolerance means something
                        const SkPoint* quadPts = converter.computeQuads(pts, weight, 0.25f);
                        for (int i = 0; i < converter.countQuads(); ++i) {
                            add_quad(&vert, base, quadPts + i*2, srcSpaceTolSqd, srcSpaceTol,
                                     isIndexed, this->isHairline(), subpathIdxStart,
                                     (int)vertexOffset, &idx);
                        }
                        break;
                    }
                    case SkPath::kQuad_Verb:
                        add_quad(&vert, base, pts, srcSpaceTolSqd, srcSpaceTol, isIndexed,
                                 this->isHairline(), subpathIdxStart, (int)vertexOffset, &idx);
                        break;
                    case SkPath::kCubic_Verb: {
                        // first pt of cubic is the pt we ended on in previous step
                        uint16_t firstCPtIdx = (uint16_t)(vert - base) - 1 + vertexOffsetU16;
                        uint16_t numPts = (uint16_t) GrPathUtils::generateCubicPoints(
                                        pts[0], pts[1], pts[2], pts[3],
                                        srcSpaceTolSqd, &vert,
                                        GrPathUtils::cubicPointCount(pts, srcSpaceTol));
                        if (isIndexed) {
                            for (uint16_t i = 0; i < numPts; ++i) {
                                append_countour_edge_indices(this->isHairline(), subpathIdxStart,
                                                             firstCPtIdx + i, &idx);
                            }
                        }
                        break;
                    }
                    case SkPath::kClose_Verb:
                        break;
                    case SkPath::kDone_Verb:
                        done = true;
                }
                first = false;
            }

            *vertexCnt = static_cast<int>(vert - base);
            *indexCnt = static_cast<int>(idx - idxBase);

        }
        return true;
    }

    GrColor color() const { return fBatch.fColor; }
    uint8_t coverage() const { return fBatch.fCoverage; }
    bool usesLocalCoords() const { return fBatch.fUsesLocalCoords; }
    const SkMatrix& viewMatrix() const { return fBatch.fViewMatrix; }
    bool isHairline() const { return fBatch.fIsHairline; }
    bool coverageIgnored() const { return fBatch.fCoverageIgnored; }

    struct BatchTracker {
        GrColor fColor;
        uint8_t fCoverage;
        SkMatrix fViewMatrix;
        bool fUsesLocalCoords;
        bool fColorIgnored;
        bool fCoverageIgnored;
        bool fIsHairline;
    };

    BatchTracker fBatch;
    SkSTArray<1, Geometry, true> fGeoData;
};

bool GrDefaultPathRenderer::internalDrawPath(GrDrawTarget* target,
                                             GrPipelineBuilder* pipelineBuilder,
                                             GrColor color,
                                             const SkMatrix& viewMatrix,
                                             const SkPath& path,
                                             const GrStrokeInfo& origStroke,
                                             bool stencilOnly) {
    SkTCopyOnFirstWrite<GrStrokeInfo> stroke(origStroke);

    SkScalar hairlineCoverage;
    uint8_t newCoverage = 0xff;
    if (IsStrokeHairlineOrEquivalent(*stroke, viewMatrix, &hairlineCoverage)) {
        newCoverage = SkScalarRoundToInt(hairlineCoverage * 0xff);

        if (!stroke->isHairlineStyle()) {
            stroke.writable()->setHairlineStyle();
        }
    }

    const bool isHairline = stroke->isHairlineStyle();

    // Save the current xp on the draw state so we can reset it if needed
    SkAutoTUnref<const GrXPFactory> backupXPFactory(SkRef(pipelineBuilder->getXPFactory()));
    // face culling doesn't make sense here
    SkASSERT(GrPipelineBuilder::kBoth_DrawFace == pipelineBuilder->getDrawFace());

    int                         passCount = 0;
    const GrStencilSettings*    passes[3];
    GrPipelineBuilder::DrawFace drawFace[3];
    bool                        reverse = false;
    bool                        lastPassIsBounds;

    if (isHairline) {
        passCount = 1;
        if (stencilOnly) {
            passes[0] = &gDirectToStencil;
        } else {
            passes[0] = NULL;
        }
        lastPassIsBounds = false;
        drawFace[0] = GrPipelineBuilder::kBoth_DrawFace;
    } else {
        if (single_pass_path(path, *stroke)) {
            passCount = 1;
            if (stencilOnly) {
                passes[0] = &gDirectToStencil;
            } else {
                passes[0] = NULL;
            }
            drawFace[0] = GrPipelineBuilder::kBoth_DrawFace;
            lastPassIsBounds = false;
        } else {
            switch (path.getFillType()) {
                case SkPath::kInverseEvenOdd_FillType:
                    reverse = true;
                    // fallthrough
                case SkPath::kEvenOdd_FillType:
                    passes[0] = &gEOStencilPass;
                    if (stencilOnly) {
                        passCount = 1;
                        lastPassIsBounds = false;
                    } else {
                        passCount = 2;
                        lastPassIsBounds = true;
                        if (reverse) {
                            passes[1] = &gInvEOColorPass;
                        } else {
                            passes[1] = &gEOColorPass;
                        }
                    }
                    drawFace[0] = drawFace[1] = GrPipelineBuilder::kBoth_DrawFace;
                    break;

                case SkPath::kInverseWinding_FillType:
                    reverse = true;
                    // fallthrough
                case SkPath::kWinding_FillType:
                    if (fSeparateStencil) {
                        if (fStencilWrapOps) {
                            passes[0] = &gWindStencilSeparateWithWrap;
                        } else {
                            passes[0] = &gWindStencilSeparateNoWrap;
                        }
                        passCount = 2;
                        drawFace[0] = GrPipelineBuilder::kBoth_DrawFace;
                    } else {
                        if (fStencilWrapOps) {
                            passes[0] = &gWindSingleStencilWithWrapInc;
                            passes[1] = &gWindSingleStencilWithWrapDec;
                        } else {
                            passes[0] = &gWindSingleStencilNoWrapInc;
                            passes[1] = &gWindSingleStencilNoWrapDec;
                        }
                        // which is cw and which is ccw is arbitrary.
                        drawFace[0] = GrPipelineBuilder::kCW_DrawFace;
                        drawFace[1] = GrPipelineBuilder::kCCW_DrawFace;
                        passCount = 3;
                    }
                    if (stencilOnly) {
                        lastPassIsBounds = false;
                        --passCount;
                    } else {
                        lastPassIsBounds = true;
                        drawFace[passCount-1] = GrPipelineBuilder::kBoth_DrawFace;
                        if (reverse) {
                            passes[passCount-1] = &gInvWindColorPass;
                        } else {
                            passes[passCount-1] = &gWindColorPass;
                        }
                    }
                    break;
                default:
                    SkDEBUGFAIL("Unknown path fFill!");
                    return false;
            }
        }
    }

    SkScalar tol = GrPathUtils::kDefaultTolerance;
    SkScalar srcSpaceTol = GrPathUtils::scaleToleranceToSrc(tol, viewMatrix, path.getBounds());

    SkRect devBounds;
    GetPathDevBounds(path, pipelineBuilder->getRenderTarget(), viewMatrix, &devBounds);

    for (int p = 0; p < passCount; ++p) {
        pipelineBuilder->setDrawFace(drawFace[p]);
        if (passes[p]) {
            *pipelineBuilder->stencil() = *passes[p];
        }

        if (lastPassIsBounds && (p == passCount-1)) {
            // Reset the XP Factory on pipelineBuilder
            pipelineBuilder->setXPFactory(backupXPFactory);
            SkRect bounds;
            SkMatrix localMatrix = SkMatrix::I();
            if (reverse) {
                SkASSERT(pipelineBuilder->getRenderTarget());
                // draw over the dev bounds (which will be the whole dst surface for inv fill).
                bounds = devBounds;
                SkMatrix vmi;
                // mapRect through persp matrix may not be correct
                if (!viewMatrix.hasPerspective() && viewMatrix.invert(&vmi)) {
                    vmi.mapRect(&bounds);
                } else {
                    if (!viewMatrix.invert(&localMatrix)) {
                        return false;
                    }
                }
            } else {
                bounds = path.getBounds();
            }
            const SkMatrix& viewM = (reverse && viewMatrix.hasPerspective()) ? SkMatrix::I() :
                                                                               viewMatrix;
            target->drawBWRect(pipelineBuilder, color, viewM, bounds, NULL, &localMatrix);
        } else {
            if (passCount > 1) {
                pipelineBuilder->setDisableColorXPFactory();
            }

            DefaultPathBatch::Geometry geometry;
            geometry.fColor = color;
            geometry.fPath = path;
            geometry.fTolerance = srcSpaceTol;

            SkAutoTUnref<GrBatch> batch(DefaultPathBatch::Create(geometry, newCoverage, viewMatrix,
                                                                 isHairline, devBounds));

            target->drawBatch(pipelineBuilder, batch);
        }
    }
    return true;
}

bool GrDefaultPathRenderer::canDrawPath(const GrDrawTarget* target,
                                        const GrPipelineBuilder* pipelineBuilder,
                                        const SkMatrix& viewMatrix,
                                        const SkPath& path,
                                        const GrStrokeInfo& stroke,
                                        bool antiAlias) const {
    // this class can draw any path with any fill but doesn't do any anti-aliasing.
    return !antiAlias && (stroke.isFillStyle() || IsStrokeHairlineOrEquivalent(stroke,
                                                                               viewMatrix,
                                                                               NULL));
}

bool GrDefaultPathRenderer::onDrawPath(GrDrawTarget* target,
                                       GrPipelineBuilder* pipelineBuilder,
                                       GrColor color,
                                       const SkMatrix& viewMatrix,
                                       const SkPath& path,
                                       const GrStrokeInfo& stroke,
                                       bool antiAlias) {
    return this->internalDrawPath(target,
                                  pipelineBuilder,
                                  color,
                                  viewMatrix,
                                  path,
                                  stroke,
                                  false);
}

void GrDefaultPathRenderer::onStencilPath(GrDrawTarget* target,
                                          GrPipelineBuilder* pipelineBuilder,
                                          const SkMatrix& viewMatrix,
                                          const SkPath& path,
                                          const GrStrokeInfo& stroke) {
    SkASSERT(SkPath::kInverseEvenOdd_FillType != path.getFillType());
    SkASSERT(SkPath::kInverseWinding_FillType != path.getFillType());
    this->internalDrawPath(target, pipelineBuilder, GrColor_WHITE, viewMatrix, path, stroke, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef GR_TEST_UTILS

BATCH_TEST_DEFINE(DefaultPathBatch) {
    GrColor color = GrRandomColor(random);
    SkMatrix viewMatrix = GrTest::TestMatrix(random);

    // For now just hairlines because the other types of draws require two batches.
    // TODO we should figure out a way to combine the stencil and cover steps into one batch
    GrStrokeInfo stroke(SkStrokeRec::kHairline_InitStyle);
    SkPath path = GrTest::TestPath(random);

    // Compute srcSpaceTol
    SkRect bounds = path.getBounds();
    SkScalar tol = GrPathUtils::kDefaultTolerance;
    SkScalar srcSpaceTol = GrPathUtils::scaleToleranceToSrc(tol, viewMatrix, bounds);

    DefaultPathBatch::Geometry geometry;
    geometry.fColor = color;
    geometry.fPath = path;
    geometry.fTolerance = srcSpaceTol;

    viewMatrix.mapRect(&bounds);
    uint8_t coverage = GrRandomCoverage(random);
    return DefaultPathBatch::Create(geometry, coverage, viewMatrix, true, bounds);
}

#endif
