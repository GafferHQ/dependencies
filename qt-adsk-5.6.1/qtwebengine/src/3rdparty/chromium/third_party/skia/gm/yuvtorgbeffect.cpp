
/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This test only works with the GPU backend.

#include "gm.h"

#if SK_SUPPORT_GPU

#include "GrContext.h"
#include "GrTest.h"
#include "effects/GrYUVtoRGBEffect.h"
#include "SkBitmap.h"
#include "SkGr.h"
#include "SkGradientShader.h"

#define YSIZE 8
#define USIZE 4
#define VSIZE 4

namespace skiagm {
/**
 * This GM directly exercises GrYUVtoRGBEffect.
 */
class YUVtoRGBEffect : public GM {
public:
    YUVtoRGBEffect() {
        this->setBGColor(0xFFFFFFFF);
    }

protected:
    SkString onShortName() override {
        return SkString("yuv_to_rgb_effect");
    }

    SkISize onISize() override {
        return SkISize::Make(238, 84);
    }

    void onOnceBeforeDraw() override {
        SkImageInfo yinfo = SkImageInfo::MakeA8(YSIZE, YSIZE);
        fBmp[0].allocPixels(yinfo);
        SkImageInfo uinfo = SkImageInfo::MakeA8(USIZE, USIZE);
        fBmp[1].allocPixels(uinfo);
        SkImageInfo vinfo = SkImageInfo::MakeA8(VSIZE, VSIZE);
        fBmp[2].allocPixels(vinfo);
        unsigned char* pixels[3];
        for (int i = 0; i < 3; ++i) {
            pixels[i] = (unsigned char*)fBmp[i].getPixels();
        }
        int color[] = {0, 85, 170};
        const int limit[] = {255, 0, 255};
        const int invl[]  = {0, 255, 0};
        const int inc[]   = {1, -1, 1};
        for (int i = 0; i < 3; ++i) {
            const size_t nbBytes = fBmp[i].rowBytes() * fBmp[i].height();
            for (size_t j = 0; j < nbBytes; ++j) {
                pixels[i][j] = (unsigned char)color[i];
                color[i] = (color[i] == limit[i]) ? invl[i] : color[i] + inc[i];
            }
        }
    }

    void onDraw(SkCanvas* canvas) override {
        GrRenderTarget* rt = canvas->internal_private_accessTopLayerRenderTarget();
        if (NULL == rt) {
            return;
        }
        GrContext* context = rt->getContext();
        if (NULL == context) {
            this->drawGpuOnlyMessage(canvas);
            return;
        }

        GrTestTarget tt;
        context->getTestTarget(&tt);
        if (NULL == tt.target()) {
            SkDEBUGFAIL("Couldn't get Gr test target.");
            return;
        }

        SkAutoTUnref<GrTexture> texture[3];
        texture[0].reset(GrRefCachedBitmapTexture(context, fBmp[0], NULL));
        texture[1].reset(GrRefCachedBitmapTexture(context, fBmp[1], NULL));
        texture[2].reset(GrRefCachedBitmapTexture(context, fBmp[2], NULL));

        if (!texture[0] || !texture[1] || !texture[2]) {
            return;
        }

        static const SkScalar kDrawPad = 10.f;
        static const SkScalar kTestPad = 10.f;
        static const SkScalar kColorSpaceOffset = 36.f;
        SkISize sizes[3] = {{YSIZE, YSIZE}, {USIZE, USIZE}, {VSIZE, VSIZE}};

        for (int space = kJPEG_SkYUVColorSpace; space <= kLastEnum_SkYUVColorSpace;
             ++space) {
            SkRect renderRect = SkRect::MakeWH(SkIntToScalar(fBmp[0].width()),
                                               SkIntToScalar(fBmp[0].height()));
            renderRect.outset(kDrawPad, kDrawPad);

            SkScalar y = kDrawPad + kTestPad + space * kColorSpaceOffset;
            SkScalar x = kDrawPad + kTestPad;

            const int indices[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                                       {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};

            for (int i = 0; i < 6; ++i) {
                GrPipelineBuilder pipelineBuilder;
                SkAutoTUnref<GrFragmentProcessor> fp(
                            GrYUVtoRGBEffect::Create(pipelineBuilder.getProcessorDataManager(),
                                                     texture[indices[i][0]],
                                                     texture[indices[i][1]],
                                                     texture[indices[i][2]],
                                                     sizes,
                                                     static_cast<SkYUVColorSpace>(space)));
                if (fp) {
                    SkMatrix viewMatrix;
                    viewMatrix.setTranslate(x, y);
                    pipelineBuilder.setRenderTarget(rt);
                    pipelineBuilder.addColorProcessor(fp);
                    tt.target()->drawSimpleRect(&pipelineBuilder,
                                                GrColor_WHITE,
                                                viewMatrix,
                                                renderRect);
                }
                x += renderRect.width() + kTestPad;
            }
        }
     }

private:
    SkBitmap fBmp[3];

    typedef GM INHERITED;
};

DEF_GM( return SkNEW(YUVtoRGBEffect); )
}

#endif
