/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SampleCode.h"
#include "SkView.h"
#include "SkBlurMask.h"
#include "SkCanvas.h"
#include "SkCornerPathEffect.h"
#include "SkGradientShader.h"
#include "SkGraphics.h"
#include "SkImageDecoder.h"
#include "SkPath.h"
#include "SkRandom.h"
#include "SkRegion.h"
#include "SkShader.h"
#include "SkUtils.h"
#include "SkColorPriv.h"
#include "SkColorFilter.h"
#include "SkTime.h"
#include "SkTypeface.h"
#include "SkXfermode.h"

#include "SkStream.h"
#include "SkXMLParser.h"
#include "SkColorPriv.h"
#include "SkImageDecoder.h"
#include "SkBlurMaskFilter.h"

static void setNamedTypeface(SkPaint* paint, const char name[]) {
    SkTypeface* face = SkTypeface::CreateFromName(name, SkTypeface::kNormal);
    paint->setTypeface(face);
    SkSafeUnref(face);
}

static uint16_t gBG[] = { 0xFFFF, 0xCCCF, 0xCCCF, 0xFFFF };

class XfermodesBlurView : public SampleView {
    SkBitmap    fBG;
    SkBitmap    fSrcB, fDstB;

    void draw_mode(SkCanvas* canvas, SkXfermode* mode, int alpha,
                   SkScalar x, SkScalar y) {
        SkPaint p;
        SkMaskFilter* mf = SkBlurMaskFilter::Create(kNormal_SkBlurStyle,
                                       SkBlurMask::ConvertRadiusToSigma(SkIntToScalar(5)),
                                       SkBlurMaskFilter::kNone_BlurFlag);
        p.setMaskFilter(mf)->unref();

        SkScalar ww = SkIntToScalar(W);
        SkScalar hh = SkIntToScalar(H);

        // draw a circle covering the upper
        // left three quarters of the canvas
        p.setColor(0xFFCC44FF);
        SkRect r;
        r.set(0, 0, ww*3/4, hh*3/4);
        r.offset(x, y);
        canvas->drawOval(r, p);

        p.setXfermode(mode);

        // draw a square overlapping the circle
        // in the lower right of the canvas
        p.setColor(0x00AA6633 | alpha << 24);
        r.set(ww/3, hh/3, ww*19/20, hh*19/20);
        r.offset(x, y);
        canvas->drawRect(r, p);
    }

public:
    const static int W = 64;
    const static int H = 64;
    XfermodesBlurView() {
        fBG.installPixels(SkImageInfo::Make(2, 2, kARGB_4444_SkColorType, kPremul_SkAlphaType),
                          gBG, 4);
    }

protected:
    // overrides from SkEventSink
    virtual bool onQuery(SkEvent* evt) {
        if (SampleCode::TitleQ(*evt)) {
            SampleCode::TitleR(evt, "XfermodesBlur");
            return true;
        }
        return this->INHERITED::onQuery(evt);
    }

    virtual void onDrawContent(SkCanvas* canvas) {
        canvas->translate(SkIntToScalar(10), SkIntToScalar(20));

        if (false) {
            SkPaint paint;
            paint.setAntiAlias(true);
            paint.setTextSize(50);
            paint.setTypeface(SkTypeface::CreateFromName("Arial Unicode MS", SkTypeface::kNormal));
            SkSafeUnref(paint.getTypeface());
            char buffer[10];
            size_t len = SkUTF8_FromUnichar(0x8500, buffer);
            canvas->drawText(buffer, len, 40, 40, paint);
            return;
        }
        if (false) {
            SkPaint paint;
            paint.setAntiAlias(true);

            SkRect r0 = { 0, 0, 10.5f, 20 };
            SkRect r1 = { 10.5f, 10, 20, 30 };
            paint.setColor(SK_ColorRED);
            canvas->drawRect(r0, paint);
            paint.setColor(SK_ColorBLUE);
            canvas->drawRect(r1, paint);
            return;
        }

        const struct {
            SkXfermode::Mode  fMode;
            const char*         fLabel;
        } gModes[] = {
            { SkXfermode::kClear_Mode,    "Clear"     },
            { SkXfermode::kSrc_Mode,      "Src"       },
            { SkXfermode::kDst_Mode,      "Dst"       },
            { SkXfermode::kSrcOver_Mode,  "SrcOver"   },
            { SkXfermode::kDstOver_Mode,  "DstOver"   },
            { SkXfermode::kSrcIn_Mode,    "SrcIn"     },
            { SkXfermode::kDstIn_Mode,    "DstIn"     },
            { SkXfermode::kSrcOut_Mode,   "SrcOut"    },
            { SkXfermode::kDstOut_Mode,   "DstOut"    },
            { SkXfermode::kSrcATop_Mode,  "SrcATop"   },
            { SkXfermode::kDstATop_Mode,  "DstATop"   },
            { SkXfermode::kXor_Mode,      "Xor"       },

            { SkXfermode::kPlus_Mode,         "Plus"          },
            /*{ SkXfermode::kModulate_Mode,     "Modulate"      },
            { SkXfermode::kScreen_Mode,       "Screen"        },
            { SkXfermode::kOverlay_Mode,      "Overlay"       },
            { SkXfermode::kDarken_Mode,       "Darken"        },
            { SkXfermode::kLighten_Mode,      "Lighten"       },
            { SkXfermode::kColorDodge_Mode,   "ColorDodge"    },
            { SkXfermode::kColorBurn_Mode,    "ColorBurn"     },
            { SkXfermode::kHardLight_Mode,    "HardLight"     },
            { SkXfermode::kSoftLight_Mode,    "SoftLight"     },
            { SkXfermode::kDifference_Mode,   "Difference"    },
            { SkXfermode::kExclusion_Mode,    "Exclusion"     },*/
        };

        const SkScalar w = SkIntToScalar(W);
        const SkScalar h = SkIntToScalar(H);
        SkMatrix m;
        m.setScale(SkIntToScalar(6), SkIntToScalar(6));
        SkShader* s = SkShader::CreateBitmapShader(fBG,
                                                   SkShader::kRepeat_TileMode,
                                                   SkShader::kRepeat_TileMode,
                                                   &m);

        SkPaint labelP;
        labelP.setAntiAlias(true);
        labelP.setLCDRenderText(true);
        labelP.setTextAlign(SkPaint::kCenter_Align);
        setNamedTypeface(&labelP, "Menlo Regular");

        const int W = 5;

        SkScalar x0 = 0;
        for (int twice = 0; twice < 2; twice++) {
            SkScalar x = x0, y = 0;
            for (size_t i = 0; i < SK_ARRAY_COUNT(gModes); i++) {
                SkXfermode* mode = SkXfermode::Create(gModes[i].fMode);
                SkAutoUnref aur(mode);
                SkRect r;
                r.set(x, y, x+w, y+h);

                SkPaint p;
                p.setStyle(SkPaint::kFill_Style);
                p.setShader(s);
                canvas->drawRect(r, p);

                canvas->saveLayer(&r, NULL);
                draw_mode(canvas, mode, twice ? 0x88 : 0xFF, r.fLeft, r.fTop);
                canvas->restore();

                r.inset(-SK_ScalarHalf, -SK_ScalarHalf);
                p.setStyle(SkPaint::kStroke_Style);
                p.setShader(NULL);
                canvas->drawRect(r, p);

                canvas->drawText(gModes[i].fLabel, strlen(gModes[i].fLabel),
                                 x + w/2, y - labelP.getTextSize()/2, labelP);
                x += w + SkIntToScalar(10);
                if ((i % W) == W - 1) {
                    x = x0;
                    y += h + SkIntToScalar(30);
                }
            }
            x0 += SkIntToScalar(400);
        }
        s->unref();
    }

private:
    typedef SampleView INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

static SkView* MyFactory() { return new XfermodesBlurView; }
static SkViewRegister reg(MyFactory);
