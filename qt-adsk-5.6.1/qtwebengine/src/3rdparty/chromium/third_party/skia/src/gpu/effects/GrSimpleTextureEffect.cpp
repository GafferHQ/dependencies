/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrSimpleTextureEffect.h"
#include "GrInvariantOutput.h"
#include "GrTexture.h"
#include "gl/GrGLCaps.h"
#include "gl/GrGLProcessor.h"
#include "gl/GrGLTexture.h"
#include "gl/builders/GrGLProgramBuilder.h"

class GrGLSimpleTextureEffect : public GrGLFragmentProcessor {
public:
    GrGLSimpleTextureEffect(const GrProcessor&) {}

    virtual void emitCode(GrGLFPBuilder* builder,
                          const GrFragmentProcessor& fp,
                          const char* outputColor,
                          const char* inputColor,
                          const TransformedCoordsArray& coords,
                          const TextureSamplerArray& samplers) override {
        GrGLFragmentBuilder* fsBuilder = builder->getFragmentShaderBuilder();
        fsBuilder->codeAppendf("\t%s = ", outputColor);
        fsBuilder->appendTextureLookupAndModulate(inputColor,
                                                  samplers[0],
                                                  coords[0].c_str(),
                                                  coords[0].getType());
        fsBuilder->codeAppend(";\n");
    }

private:
    typedef GrGLFragmentProcessor INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

void GrSimpleTextureEffect::onComputeInvariantOutput(GrInvariantOutput* inout) const {
    this->updateInvariantOutputForModulation(inout);
}

void GrSimpleTextureEffect::getGLProcessorKey(const GrGLSLCaps& caps,
                                              GrProcessorKeyBuilder* b) const {
    GrGLSimpleTextureEffect::GenKey(*this, caps, b);
}

GrGLFragmentProcessor* GrSimpleTextureEffect::createGLInstance() const  {
    return SkNEW_ARGS(GrGLSimpleTextureEffect, (*this));
}

///////////////////////////////////////////////////////////////////////////////

GR_DEFINE_FRAGMENT_PROCESSOR_TEST(GrSimpleTextureEffect);

GrFragmentProcessor* GrSimpleTextureEffect::TestCreate(GrProcessorTestData* d) {
    int texIdx = d->fRandom->nextBool() ? GrProcessorUnitTest::kSkiaPMTextureIdx :
                                          GrProcessorUnitTest::kAlphaTextureIdx;
    static const SkShader::TileMode kTileModes[] = {
        SkShader::kClamp_TileMode,
        SkShader::kRepeat_TileMode,
        SkShader::kMirror_TileMode,
    };
    SkShader::TileMode tileModes[] = {
        kTileModes[d->fRandom->nextULessThan(SK_ARRAY_COUNT(kTileModes))],
        kTileModes[d->fRandom->nextULessThan(SK_ARRAY_COUNT(kTileModes))],
    };
    GrTextureParams params(tileModes, d->fRandom->nextBool() ? GrTextureParams::kBilerp_FilterMode :
                                                               GrTextureParams::kNone_FilterMode);

    static const GrCoordSet kCoordSets[] = {
        kLocal_GrCoordSet,
        kDevice_GrCoordSet
    };
    GrCoordSet coordSet = kCoordSets[d->fRandom->nextULessThan(SK_ARRAY_COUNT(kCoordSets))];

    const SkMatrix& matrix = GrTest::TestMatrix(d->fRandom);
    return GrSimpleTextureEffect::Create(d->fProcDataManager, d->fTextures[texIdx], matrix,
                                         coordSet);
}
