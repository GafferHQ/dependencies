
/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkGradientShader.h"
#include "SkPatchUtils.h"

static SkShader* make_shader() {
    const SkColor colors[] = {
        SK_ColorRED, SK_ColorCYAN, SK_ColorGREEN, SK_ColorWHITE, SK_ColorMAGENTA, SK_ColorBLUE,
        SK_ColorYELLOW,
    };
    const SkPoint pts[] = { { 100.f / 4.f, 0.f }, { 3.f * 100.f / 4.f, 100.f } };
    
    return SkGradientShader::CreateLinear(pts, colors, NULL, SK_ARRAY_COUNT(colors),
                                          SkShader::kMirror_TileMode);
}

static void draw_control_points(SkCanvas* canvas, const SkPoint cubics[12]) {
    //draw control points
    SkPaint paint;
    SkPoint bottom[SkPatchUtils::kNumPtsCubic];
    SkPatchUtils::getBottomCubic(cubics, bottom);
    SkPoint top[SkPatchUtils::kNumPtsCubic];
    SkPatchUtils::getTopCubic(cubics, top);
    SkPoint left[SkPatchUtils::kNumPtsCubic];
    SkPatchUtils::getLeftCubic(cubics, left);
    SkPoint right[SkPatchUtils::kNumPtsCubic];
    SkPatchUtils::getRightCubic(cubics, right);

    paint.setColor(SK_ColorBLACK);
    paint.setStrokeWidth(0.5f);
    SkPoint corners[4] = { bottom[0], bottom[3], top[0], top[3] };
    canvas->drawPoints(SkCanvas::kLines_PointMode, 4, bottom, paint);
    canvas->drawPoints(SkCanvas::kLines_PointMode, 2, bottom + 1, paint);
    canvas->drawPoints(SkCanvas::kLines_PointMode, 4, top, paint);
    canvas->drawPoints(SkCanvas::kLines_PointMode, 4, left, paint);
    canvas->drawPoints(SkCanvas::kLines_PointMode, 4, right, paint);

    canvas->drawPoints(SkCanvas::kLines_PointMode, 2, top + 1, paint);
    canvas->drawPoints(SkCanvas::kLines_PointMode, 2, left + 1, paint);
    canvas->drawPoints(SkCanvas::kLines_PointMode, 2, right + 1, paint);

    paint.setStrokeWidth(2);

    paint.setColor(SK_ColorRED);
    canvas->drawPoints(SkCanvas::kPoints_PointMode, 4, corners, paint);

    paint.setColor(SK_ColorBLUE);
    canvas->drawPoints(SkCanvas::kPoints_PointMode, 2, bottom + 1, paint);

    paint.setColor(SK_ColorCYAN);
    canvas->drawPoints(SkCanvas::kPoints_PointMode, 2, top + 1, paint);

    paint.setColor(SK_ColorYELLOW);
    canvas->drawPoints(SkCanvas::kPoints_PointMode, 2, left + 1, paint);

    paint.setColor(SK_ColorGREEN);
    canvas->drawPoints(SkCanvas::kPoints_PointMode, 2, right + 1, paint);
}

namespace skiagm {
/**
 * This GM draws a cubics coons patch using the specialized call SkCanvas::drawPatch.
 */
class SkPatchGM : public GM {
    
public:
    SkPatchGM() {
        this->setBGColor(0xFFFFFFFF);
    }

protected:
    SkString onShortName() override {
        return SkString("patch_primitive");
    }

    SkISize onISize() override {
        return SkISize::Make(800, 800);
    }

    void onDraw(SkCanvas* canvas) override {

        SkPaint paint;
        
        // The order of the colors and points is clockwise starting at upper-left corner.
        const SkPoint cubics[SkPatchUtils::kNumCtrlPts] = {
            //top points
            {100,100},{150,50},{250,150}, {300,100},
            //right points
            {250, 150},{350,250},
            //bottom points
            {300,300},{250,250},{150,350},{100,300},
            //left points
            {50,250},{150,150}
        };
        
        const SkColor colors[SkPatchUtils::kNumCorners] = {
            SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE, SK_ColorCYAN
        };
        const SkPoint texCoords[SkPatchUtils::kNumCorners] = {
            {0.0f, 0.0f}, {100.0f, 0.0f}, {100.0f,100.0f}, {0.0f, 100.0f}}
        ;
        
        const SkXfermode::Mode modes[] = {
            SkXfermode::kSrc_Mode,
            SkXfermode::kDst_Mode,
            SkXfermode::kModulate_Mode,
        };
        
        SkAutoTUnref<SkShader> shader(make_shader());
        
        canvas->save();
        for (int y = 0; y < 3; y++) {
            SkAutoTUnref<SkXfermode> xfer(SkXfermode::Create(modes[y]));

            for (int x = 0; x < 4; x++) {
                canvas->save();
                canvas->translate(x * 350.0f, y * 350.0f);
                switch (x) {
                    case 0:
                        canvas->drawPatch(cubics, NULL, NULL, xfer, paint);
                        break;
                    case 1:
                        canvas->drawPatch(cubics, colors, NULL, xfer, paint);
                        break;
                    case 2:
                        paint.setShader(shader);
                        canvas->drawPatch(cubics, NULL, texCoords, xfer, paint);
                        paint.setShader(NULL);
                        break;
                    case 3:
                        paint.setShader(shader);
                        canvas->drawPatch(cubics, colors, texCoords, xfer, paint);
                        paint.setShader(NULL);
                        break;
                    default:
                        break;
                }
                
                draw_control_points(canvas, cubics);
                canvas->restore();
            }
        }
        canvas->restore();
    }

private:
    typedef GM INHERITED;
};

DEF_GM(return SkNEW(SkPatchGM); )

}
