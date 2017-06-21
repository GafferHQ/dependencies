/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrConfigConversionEffect.h"
#include "GrContext.h"
#include "GrDrawContext.h"
#include "GrInvariantOutput.h"
#include "GrSimpleTextureEffect.h"
#include "SkMatrix.h"
#include "gl/GrGLProcessor.h"
#include "gl/builders/GrGLProgramBuilder.h"

class GrGLConfigConversionEffect : public GrGLFragmentProcessor {
public:
    GrGLConfigConversionEffect(const GrProcessor& processor) {
        const GrConfigConversionEffect& configConversionEffect =
                processor.cast<GrConfigConversionEffect>();
        fSwapRedAndBlue = configConversionEffect.swapsRedAndBlue();
        fPMConversion = configConversionEffect.pmConversion();
    }

    virtual void emitCode(GrGLFPBuilder* builder,
                          const GrFragmentProcessor&,
                          const char* outputColor,
                          const char* inputColor,
                          const TransformedCoordsArray& coords,
                          const TextureSamplerArray& samplers) override {
        // Using highp for GLES here in order to avoid some precision issues on specific GPUs.
        GrGLShaderVar tmpVar("tmpColor", kVec4f_GrSLType, 0, kHigh_GrSLPrecision);
        SkString tmpDecl;
        tmpVar.appendDecl(builder->ctxInfo(), &tmpDecl);

        GrGLFragmentBuilder* fsBuilder = builder->getFragmentShaderBuilder();

        fsBuilder->codeAppendf("%s;", tmpDecl.c_str());

        fsBuilder->codeAppendf("%s = ", tmpVar.c_str());
        fsBuilder->appendTextureLookup(samplers[0], coords[0].c_str(), coords[0].getType());
        fsBuilder->codeAppend(";");

        if (GrConfigConversionEffect::kNone_PMConversion == fPMConversion) {
            SkASSERT(fSwapRedAndBlue);
            fsBuilder->codeAppendf("%s = %s.bgra;", outputColor, tmpVar.c_str());
        } else {
            const char* swiz = fSwapRedAndBlue ? "bgr" : "rgb";
            switch (fPMConversion) {
                case GrConfigConversionEffect::kMulByAlpha_RoundUp_PMConversion:
                    fsBuilder->codeAppendf(
                        "%s = vec4(ceil(%s.%s * %s.a * 255.0) / 255.0, %s.a);",
                        tmpVar.c_str(), tmpVar.c_str(), swiz, tmpVar.c_str(), tmpVar.c_str());
                    break;
                case GrConfigConversionEffect::kMulByAlpha_RoundDown_PMConversion:
                    // Add a compensation(0.001) here to avoid the side effect of the floor operation.
                    // In Intel GPUs, the integer value converted from floor(%s.r * 255.0) / 255.0
                    // is less than the integer value converted from  %s.r by 1 when the %s.r is
                    // converted from the integer value 2^n, such as 1, 2, 4, 8, etc.
                    fsBuilder->codeAppendf(
                        "%s = vec4(floor(%s.%s * %s.a * 255.0 + 0.001) / 255.0, %s.a);",
                        tmpVar.c_str(), tmpVar.c_str(), swiz, tmpVar.c_str(), tmpVar.c_str());
                    break;
                case GrConfigConversionEffect::kDivByAlpha_RoundUp_PMConversion:
                    fsBuilder->codeAppendf(
                        "%s = %s.a <= 0.0 ? vec4(0,0,0,0) : vec4(ceil(%s.%s / %s.a * 255.0) / 255.0, %s.a);",
                        tmpVar.c_str(), tmpVar.c_str(), tmpVar.c_str(), swiz, tmpVar.c_str(), tmpVar.c_str());
                    break;
                case GrConfigConversionEffect::kDivByAlpha_RoundDown_PMConversion:
                    fsBuilder->codeAppendf(
                        "%s = %s.a <= 0.0 ? vec4(0,0,0,0) : vec4(floor(%s.%s / %s.a * 255.0) / 255.0, %s.a);",
                        tmpVar.c_str(), tmpVar.c_str(), tmpVar.c_str(), swiz, tmpVar.c_str(), tmpVar.c_str());
                    break;
                default:
                    SkFAIL("Unknown conversion op.");
                    break;
            }
            fsBuilder->codeAppendf("%s = %s;", outputColor, tmpVar.c_str());
        }
        SkString modulate;
        GrGLSLMulVarBy4f(&modulate, outputColor, inputColor);
        fsBuilder->codeAppend(modulate.c_str());
    }

    static inline void GenKey(const GrProcessor& processor, const GrGLSLCaps&,
                              GrProcessorKeyBuilder* b) {
        const GrConfigConversionEffect& conv = processor.cast<GrConfigConversionEffect>();
        uint32_t key = (conv.swapsRedAndBlue() ? 0 : 1) | (conv.pmConversion() << 1);
        b->add32(key);
    }

private:
    bool                                    fSwapRedAndBlue;
    GrConfigConversionEffect::PMConversion  fPMConversion;

    typedef GrGLFragmentProcessor INHERITED;

};

///////////////////////////////////////////////////////////////////////////////

GrConfigConversionEffect::GrConfigConversionEffect(GrProcessorDataManager* procDataManager,
                                                   GrTexture* texture,
                                                   bool swapRedAndBlue,
                                                   PMConversion pmConversion,
                                                   const SkMatrix& matrix)
    : INHERITED(procDataManager, texture, matrix)
    , fSwapRedAndBlue(swapRedAndBlue)
    , fPMConversion(pmConversion) {
    this->initClassID<GrConfigConversionEffect>();
    SkASSERT(kRGBA_8888_GrPixelConfig == texture->config() ||
             kBGRA_8888_GrPixelConfig == texture->config());
    // Why did we pollute our texture cache instead of using a GrSingleTextureEffect?
    SkASSERT(swapRedAndBlue || kNone_PMConversion != pmConversion);
}

bool GrConfigConversionEffect::onIsEqual(const GrFragmentProcessor& s) const {
    const GrConfigConversionEffect& other = s.cast<GrConfigConversionEffect>();
    return other.fSwapRedAndBlue == fSwapRedAndBlue &&
           other.fPMConversion == fPMConversion;
}

void GrConfigConversionEffect::onComputeInvariantOutput(GrInvariantOutput* inout) const {
    this->updateInvariantOutputForModulation(inout);
}

///////////////////////////////////////////////////////////////////////////////

GR_DEFINE_FRAGMENT_PROCESSOR_TEST(GrConfigConversionEffect);

GrFragmentProcessor* GrConfigConversionEffect::TestCreate(GrProcessorTestData* d) {
    PMConversion pmConv = static_cast<PMConversion>(d->fRandom->nextULessThan(kPMConversionCnt));
    bool swapRB;
    if (kNone_PMConversion == pmConv) {
        swapRB = true;
    } else {
        swapRB = d->fRandom->nextBool();
    }
    return SkNEW_ARGS(GrConfigConversionEffect,
                                      (d->fProcDataManager,
                                       d->fTextures[GrProcessorUnitTest::kSkiaPMTextureIdx],
                                       swapRB,
                                       pmConv,
                                       GrTest::TestMatrix(d->fRandom)));
}

///////////////////////////////////////////////////////////////////////////////

void GrConfigConversionEffect::getGLProcessorKey(const GrGLSLCaps& caps,
                                                 GrProcessorKeyBuilder* b) const {
    GrGLConfigConversionEffect::GenKey(*this, caps, b);
}

GrGLFragmentProcessor* GrConfigConversionEffect::createGLInstance() const {
    return SkNEW_ARGS(GrGLConfigConversionEffect, (*this));
}



void GrConfigConversionEffect::TestForPreservingPMConversions(GrContext* context,
                                                              PMConversion* pmToUPMRule,
                                                              PMConversion* upmToPMRule) {
    *pmToUPMRule = kNone_PMConversion;
    *upmToPMRule = kNone_PMConversion;
    SkAutoTMalloc<uint32_t> data(256 * 256 * 3);
    uint32_t* srcData = data.get();
    uint32_t* firstRead = data.get() + 256 * 256;
    uint32_t* secondRead = data.get() + 2 * 256 * 256;

    // Fill with every possible premultiplied A, color channel value. There will be 256-y duplicate
    // values in row y. We set r,g, and b to the same value since they are handled identically.
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            uint8_t* color = reinterpret_cast<uint8_t*>(&srcData[256*y + x]);
            color[3] = y;
            color[2] = SkTMin(x, y);
            color[1] = SkTMin(x, y);
            color[0] = SkTMin(x, y);
        }
    }

    GrSurfaceDesc desc;
    desc.fFlags = kRenderTarget_GrSurfaceFlag;
    desc.fWidth = 256;
    desc.fHeight = 256;
    desc.fConfig = kRGBA_8888_GrPixelConfig;

    SkAutoTUnref<GrTexture> readTex(context->textureProvider()->createTexture(desc, true, NULL, 0));
    if (!readTex.get()) {
        return;
    }
    SkAutoTUnref<GrTexture> tempTex(context->textureProvider()->createTexture(desc, true, NULL, 0));
    if (!tempTex.get()) {
        return;
    }
    desc.fFlags = kNone_GrSurfaceFlags;
    SkAutoTUnref<GrTexture> dataTex(context->textureProvider()->createTexture(desc, true, data, 0));
    if (!dataTex.get()) {
        return;
    }

    static const PMConversion kConversionRules[][2] = {
        {kDivByAlpha_RoundDown_PMConversion, kMulByAlpha_RoundUp_PMConversion},
        {kDivByAlpha_RoundUp_PMConversion, kMulByAlpha_RoundDown_PMConversion},
    };

    bool failed = true;

    GrDrawContext* drawContext = context->drawContext();
    if (!drawContext) {
        return;
    }

    for (size_t i = 0; i < SK_ARRAY_COUNT(kConversionRules) && failed; ++i) {
        *pmToUPMRule = kConversionRules[i][0];
        *upmToPMRule = kConversionRules[i][1];

        static const SkRect kDstRect = SkRect::MakeWH(SkIntToScalar(256), SkIntToScalar(256));
        static const SkRect kSrcRect = SkRect::MakeWH(SK_Scalar1, SK_Scalar1);
        // We do a PM->UPM draw from dataTex to readTex and read the data. Then we do a UPM->PM draw
        // from readTex to tempTex followed by a PM->UPM draw to readTex and finally read the data.
        // We then verify that two reads produced the same values.

        GrPaint paint1;
        GrPaint paint2;
        GrPaint paint3;
        SkAutoTUnref<GrFragmentProcessor> pmToUPM1(
                SkNEW_ARGS(GrConfigConversionEffect,
                           (paint1.getProcessorDataManager(), dataTex, false, *pmToUPMRule,
                            SkMatrix::I())));
        SkAutoTUnref<GrFragmentProcessor> upmToPM(
                SkNEW_ARGS(GrConfigConversionEffect,
                           (paint2.getProcessorDataManager(), readTex, false, *upmToPMRule,
                            SkMatrix::I())));
        SkAutoTUnref<GrFragmentProcessor> pmToUPM2(
                SkNEW_ARGS(GrConfigConversionEffect,
                           (paint3.getProcessorDataManager(), tempTex, false, *pmToUPMRule,
                            SkMatrix::I())));

        paint1.addColorProcessor(pmToUPM1);
        drawContext->drawNonAARectToRect(readTex->asRenderTarget(),
                                         GrClip::WideOpen(),
                                         paint1,
                                         SkMatrix::I(),
                                         kDstRect,
                                         kSrcRect);

        readTex->readPixels(0, 0, 256, 256, kRGBA_8888_GrPixelConfig, firstRead);

        paint2.addColorProcessor(upmToPM);
        drawContext->drawNonAARectToRect(tempTex->asRenderTarget(),
                                         GrClip::WideOpen(),
                                         paint2,
                                         SkMatrix::I(),
                                         kDstRect,
                                         kSrcRect);

        paint3.addColorProcessor(pmToUPM2);
        drawContext->drawNonAARectToRect(readTex->asRenderTarget(),
                                         GrClip::WideOpen(),
                                         paint3,
                                         SkMatrix::I(),
                                         kDstRect,
                                         kSrcRect);

        readTex->readPixels(0, 0, 256, 256, kRGBA_8888_GrPixelConfig, secondRead);

        failed = false;
        for (int y = 0; y < 256 && !failed; ++y) {
            for (int x = 0; x <= y; ++x) {
                if (firstRead[256 * y + x] != secondRead[256 * y + x]) {
                    failed = true;
                    break;
                }
            }
        }
    }
    if (failed) {
        *pmToUPMRule = kNone_PMConversion;
        *upmToPMRule = kNone_PMConversion;
    }
}

const GrFragmentProcessor* GrConfigConversionEffect::Create(GrProcessorDataManager* procDataManager,
                                                            GrTexture* texture,
                                                            bool swapRedAndBlue,
                                                            PMConversion pmConversion,
                                                            const SkMatrix& matrix) {
    if (!swapRedAndBlue && kNone_PMConversion == pmConversion) {
        // If we returned a GrConfigConversionEffect that was equivalent to a GrSimpleTextureEffect
        // then we may pollute our texture cache with redundant shaders. So in the case that no
        // conversions were requested we instead return a GrSimpleTextureEffect.
        return GrSimpleTextureEffect::Create(procDataManager, texture, matrix);
    } else {
        if (kRGBA_8888_GrPixelConfig != texture->config() &&
            kBGRA_8888_GrPixelConfig != texture->config() &&
            kNone_PMConversion != pmConversion) {
            // The PM conversions assume colors are 0..255
            return NULL;
        }
        return SkNEW_ARGS(GrConfigConversionEffect, (procDataManager,
                                                     texture,
                                                     swapRedAndBlue,
                                                     pmConversion,
                                                     matrix));
    }
}
