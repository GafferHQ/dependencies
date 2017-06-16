/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkGradientShader.h"

typedef SkShader* (*MakeShaderProc)(const SkColor[], int count, const SkSize&);

static SkShader* shader_linear(const SkColor colors[], int count, const SkSize& size) {
    SkPoint pts[] = { { 0, 0 }, { size.width(), size.height() } };
    return SkGradientShader::CreateLinear(pts, colors, NULL, count,
                                          SkShader::kClamp_TileMode);
}

static SkShader* shader_radial(const SkColor colors[], int count, const SkSize& size) {
    SkPoint center = { size.width()/2, size.height()/2 };
    return SkGradientShader::CreateRadial(center, size.width()/2, colors, NULL, count,
                                          SkShader::kClamp_TileMode);
}

static SkShader* shader_conical(const SkColor colors[], int count, const SkSize& size) {
    SkPoint center = { size.width()/2, size.height()/2 };
    return SkGradientShader::CreateTwoPointConical(center, size.width()/64,
                                                   center, size.width()/2,
                                                   colors, NULL, count,
                                                   SkShader::kClamp_TileMode);
}

static SkShader* shader_sweep(const SkColor colors[], int count, const SkSize& size) {
    return SkGradientShader::CreateSweep(size.width()/2, size.height()/2,
                                         colors, NULL, count);
}

class ShallowGradientGM : public skiagm::GM {
public:
    ShallowGradientGM(MakeShaderProc proc, const char name[]) : fProc(proc) {
        fName.printf("shallow_gradient_%s", name);
    }

protected:

    SkString onShortName() override {
        return fName;
    }

    SkISize onISize() override {
        return SkISize::Make(800, 800);
    }

    void onDraw(SkCanvas* canvas) override {
        const SkColor colors[] = { 0xFF555555, 0xFF444444 };
        const int colorCount = SK_ARRAY_COUNT(colors);

        SkRect r = { 0, 0, this->width(), this->height() };
        SkSize size = SkSize::Make(r.width(), r.height());

        SkPaint paint;
        paint.setShader(fProc(colors, colorCount, size))->unref();
        canvas->drawRect(r, paint);
    }

private:
    MakeShaderProc fProc;
    SkString fName;

    typedef skiagm::GM INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

DEF_GM( return new ShallowGradientGM(shader_linear, "linear"); )
DEF_GM( return new ShallowGradientGM(shader_radial, "radial"); )
DEF_GM( return new ShallowGradientGM(shader_conical, "conical"); )
DEF_GM( return new ShallowGradientGM(shader_sweep, "sweep"); )
