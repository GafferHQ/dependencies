/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkCanvas.h"
#include "SkDashPathEffect.h"

static void test_nulldev(SkCanvas* canvas) {
    SkBitmap bm;
    bm.setInfo(SkImageInfo::MakeN32Premul(30, 30));
    // notice: no pixels mom! be sure we don't crash
    // https://code.google.com/p/chromium/issues/detail?id=352616
    SkCanvas c(bm);

    SkBitmap src;
    src.allocN32Pixels(10, 10);
    src.eraseColor(SK_ColorRED);

    // ensure we don't crash
    c.writePixels(src, 0, 0);
}

static void draw_text_stroked(SkCanvas* canvas, const SkPaint& paint, SkScalar strokeWidth) {
    SkPaint p(paint);
    SkPoint loc = { 20, 435 };

    if (strokeWidth > 0) {
        p.setStyle(SkPaint::kFill_Style);
        canvas->drawText("P", 1, loc.fX, loc.fY - 225, p);
        canvas->drawPosText("P", 1, &loc, p);
    }

    p.setColor(SK_ColorRED);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(strokeWidth);

    canvas->drawText("P", 1, loc.fX, loc.fY - 225, p);
    canvas->drawPosText("P", 1, &loc, p);
}

static void draw_text_set(SkCanvas* canvas, const SkPaint& paint) {
    SkAutoCanvasRestore acr(canvas, true);

    draw_text_stroked(canvas, paint, 10);

    canvas->translate(200, 0);
    draw_text_stroked(canvas, paint, 0);

    const SkScalar intervals[] = { 20, 10, 5, 10 };
    const SkScalar phase = 0;

    canvas->translate(200, 0);
    SkPaint p(paint);
    p.setPathEffect(SkDashPathEffect::Create(intervals, SK_ARRAY_COUNT(intervals), phase))->unref();
    draw_text_stroked(canvas, p, 10);
}

class StrokeTextGM : public skiagm::GM {
    // Skia has a threshold above which it draws text via paths instead of using scalercontext
    // and caching the glyph. This GM wants to ensure that we draw stroking correctly on both
    // sides of this threshold.
    enum {
        kBelowThreshold_TextSize = 255,
        kAboveThreshold_TextSize = 257
    };
public:
    StrokeTextGM() {}

protected:

    SkString onShortName() override {
        return SkString("stroketext");
    }

    SkISize onISize() override {
        return SkISize::Make(1200, 480);
    }

    void onDraw(SkCanvas* canvas) override {
        if (true) { test_nulldev(canvas); }
        SkPaint paint;
        paint.setAntiAlias(true);
        sk_tool_utils::set_portable_typeface(&paint);

        paint.setTextSize(kBelowThreshold_TextSize);
        draw_text_set(canvas, paint);

        canvas->translate(600, 0);
        paint.setTextSize(kAboveThreshold_TextSize);
        draw_text_set(canvas, paint);
    }

private:
    typedef skiagm::GM INHERITED;
};

DEF_GM( return SkNEW(StrokeTextGM); )
