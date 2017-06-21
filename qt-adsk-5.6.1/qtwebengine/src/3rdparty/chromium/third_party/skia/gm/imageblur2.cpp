/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkBlurImageFilter.h"
#include "SkRandom.h"

// TODO deprecate imageblur

#define WIDTH 500
#define HEIGHT 500

static const float kBlurSigmas[] = {
        0.0, 0.3f, 0.5f, 2.0f, 32.0f, 80.0f };

const char* kTestStrings[] = {
        "The quick`~",
        "brown fox[]",
        "jumped over",
        "the lazy@#$",
        "dog.{}!%^&",
        "*()+=-\\'\"/",
};

namespace skiagm {

class BlurImageFilter : public GM {
public:
    BlurImageFilter() {
        this->setBGColor(0xFFFFFFFF);
        fName.printf("imageblur2");
    }

protected:

    SkString onShortName() override {
        return fName;
    }

    SkISize onISize() override {
        return SkISize::Make(WIDTH, HEIGHT);
    }

    void onDraw(SkCanvas* canvas) override {
        const int sigmaCount = SK_ARRAY_COUNT(kBlurSigmas);
        const int testStringCount = SK_ARRAY_COUNT(kTestStrings);
        SkScalar dx = WIDTH / sigmaCount;
        SkScalar dy = HEIGHT / sigmaCount;
        const SkScalar textSize = 12;

        for (int x = 0; x < sigmaCount; x++) {
            SkScalar sigmaX = kBlurSigmas[x];
            for (int y = 0; y < sigmaCount; y++) {
                SkScalar sigmaY = kBlurSigmas[y];

                SkPaint paint;
                paint.setImageFilter(SkBlurImageFilter::Create(sigmaX, sigmaY))->unref();
                canvas->saveLayer(NULL, &paint);

                SkRandom rand;
                SkPaint textPaint;
                textPaint.setAntiAlias(false);
                textPaint.setColor(rand.nextBits(24) | 0xFF000000);
                textPaint.setTextSize(textSize);

                for (int i = 0; i < testStringCount; i++) {
                    canvas->drawText(kTestStrings[i],
                                     strlen(kTestStrings[i]),
                                     SkIntToScalar(x * dx),
                                     SkIntToScalar(y * dy + textSize * i + textSize),
                                     textPaint);
                }
                canvas->restore();
            }
        }
    }

private:
    SkString fName;

    typedef GM INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

static GM* MyFactory(void*) { return new BlurImageFilter; }
static GMRegistry reg(MyFactory);

}
