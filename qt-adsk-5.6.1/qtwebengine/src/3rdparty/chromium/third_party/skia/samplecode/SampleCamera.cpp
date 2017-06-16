/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SampleCode.h"
#include "SkAnimTimer.h"
#include "SkView.h"
#include "SkCanvas.h"
#include "SkCamera.h"
#include "SkEmbossMaskFilter.h"
#include "SkGradientShader.h"
#include "SkPath.h"
#include "SkRegion.h"
#include "SkShader.h"
#include "SkUtils.h"
#include "SkRandom.h"
#include "SkImageDecoder.h"

class CameraView : public SampleView {
    SkTDArray<SkShader*> fShaders;
    int     fShaderIndex;
    bool    fFrontFace;
public:
    CameraView() {
        fRX = fRY = fRZ = 0;
        fShaderIndex = 0;
        fFrontFace = false;

        for (int i = 0;; i++) {
            SkString str;
            str.printf("/skimages/elephant%d.jpeg", i);
            SkBitmap bm;
            if (SkImageDecoder::DecodeFile(str.c_str(), &bm)) {
                SkRect src = { 0, 0, SkIntToScalar(bm.width()), SkIntToScalar(bm.height()) };
                SkRect dst = { -150, -150, 150, 150 };
                SkMatrix matrix;
                matrix.setRectToRect(src, dst, SkMatrix::kFill_ScaleToFit);

                SkShader* s = SkShader::CreateBitmapShader(bm,
                                                           SkShader::kClamp_TileMode,
                                                           SkShader::kClamp_TileMode,
                                                           &matrix);
                *fShaders.append() = s;
            } else {
                break;
            }
        }
        this->setBGColor(0xFFDDDDDD);
    }

    virtual ~CameraView() {
        fShaders.unrefAll();
    }

protected:
    // overrides from SkEventSink
    virtual bool onQuery(SkEvent* evt) {
        if (SampleCode::TitleQ(*evt)) {
            SampleCode::TitleR(evt, "Camera");
            return true;
        }
        return this->INHERITED::onQuery(evt);
    }

    virtual void onDrawContent(SkCanvas* canvas) {
        canvas->translate(this->width()/2, this->height()/2);

        Sk3DView    view;
        view.rotateX(fRX);
        view.rotateY(fRY);
        view.applyToCanvas(canvas);

        SkPaint paint;
        if (fShaders.count() > 0) {
            bool frontFace = view.dotWithNormal(0, 0, SK_Scalar1) < 0;
            if (frontFace != fFrontFace) {
                fFrontFace = frontFace;
                fShaderIndex = (fShaderIndex + 1) % fShaders.count();
            }

            paint.setAntiAlias(true);
            paint.setShader(fShaders[fShaderIndex]);
            paint.setFilterQuality(kLow_SkFilterQuality);
            SkRect r = { -150, -150, 150, 150 };
            canvas->drawRoundRect(r, 30, 30, paint);
        }
    }

    bool onAnimate(const SkAnimTimer& timer) override {
        if (timer.isStopped()) {
            fRY = 0;
        } else {
            fRY = timer.scaled(90, 360);
        }
        return true;
    }

private:
    SkScalar fRX, fRY, fRZ;
    typedef SampleView INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

static SkView* MyFactory() { return new CameraView; }
static SkViewRegister reg(MyFactory);
