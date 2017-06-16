/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SampleCode.h"
#include "SkAnimTimer.h"
#include "SkView.h"
#include "SkCanvas.h"
#include "SkDrawable.h"
#include "SkPath.h"
#include "SkRandom.h"
#include "SkRSXform.h"
#include "SkSurface.h"

static SkImage* make_atlas(int atlasSize, int cellSize) {
    SkImageInfo info = SkImageInfo::MakeN32Premul(atlasSize, atlasSize);
    SkAutoTUnref<SkSurface> surface(SkSurface::NewRaster(info));
    SkCanvas* canvas = surface->getCanvas();

    SkPaint paint;
    paint.setAntiAlias(true);
    SkRandom rand;

    const SkScalar half = cellSize * SK_ScalarHalf;
    const char* s = "01234567890!@#$%^&*=+<>?abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    paint.setTextSize(28);
    paint.setTextAlign(SkPaint::kCenter_Align);
    int i = 0;
    for (int y = 0; y < atlasSize; y += cellSize) {
        for (int x = 0; x < atlasSize; x += cellSize) {
            paint.setColor(rand.nextU());
            paint.setAlpha(0xFF);
            int index = i % strlen(s);
            canvas->drawText(&s[index], 1, x + half, y + half + half/2, paint);
            i += 1;
        }
    }
    return surface->newImageSnapshot();
}

class DrawAtlasDrawable : public SkDrawable {
    enum {
        kMaxScale = 2,
        kCellSize = 32,
        kAtlasSize = 512,
    };

    struct Rec {
        SkPoint     fCenter;
        SkVector    fVelocity;
        SkScalar    fScale;
        SkScalar    fDScale;
        SkScalar    fRadian;
        SkScalar    fDRadian;
        SkScalar    fAlpha;
        SkScalar    fDAlpha;

        void advance(const SkRect& bounds) {
            fCenter += fVelocity;
            if (fCenter.fX > bounds.right()) {
                SkASSERT(fVelocity.fX > 0);
                fVelocity.fX = -fVelocity.fX;
            } else if (fCenter.fX < bounds.left()) {
                SkASSERT(fVelocity.fX < 0);
                fVelocity.fX = -fVelocity.fX;
            }
            if (fCenter.fY > bounds.bottom()) {
                if (fVelocity.fY > 0) {
                    fVelocity.fY = -fVelocity.fY;
                }
            } else if (fCenter.fY < bounds.top()) {
                if (fVelocity.fY < 0) {
                    fVelocity.fY = -fVelocity.fY;
                }
            }

            fScale += fDScale;
            if (fScale > 2 || fScale < SK_Scalar1/2) {
                fDScale = -fDScale;
            }

            fRadian += fDRadian;
            fRadian = SkScalarMod(fRadian, 2 * SK_ScalarPI);

            fAlpha += fDAlpha;
            if (fAlpha > 1) {
                fAlpha = 1;
                fDAlpha = -fDAlpha;
            } else if (fAlpha < 0) {
                fAlpha = 0;
                fDAlpha = -fDAlpha;
            }
        }
        
        SkRSXform asRSXform() const {
            SkMatrix m;
            m.setTranslate(-8, -8);
            m.postScale(fScale, fScale);
            m.postRotate(SkRadiansToDegrees(fRadian));
            m.postTranslate(fCenter.fX, fCenter.fY);

            SkRSXform x;
            x.fSCos = m.getScaleX();
            x.fSSin = m.getSkewY();
            x.fTx = m.getTranslateX();
            x.fTy = m.getTranslateY();
            return x;
        }
    };

    enum {
        N = 256,
    };

    SkAutoTUnref<SkImage> fAtlas;
    Rec         fRec[N];
    SkRect      fTex[N];
    SkRect      fBounds;
    bool        fUseColors;

public:
    DrawAtlasDrawable(const SkRect& r) : fBounds(r), fUseColors(false) {
        SkRandom rand;
        fAtlas.reset(make_atlas(kAtlasSize, kCellSize));
        const SkScalar kMaxSpeed = 5;
        const SkScalar cell = SkIntToScalar(kCellSize);
        int i = 0;
        for (int y = 0; y < kAtlasSize; y += kCellSize) {
            for (int x = 0; x < kAtlasSize; x += kCellSize) {
                const SkScalar sx = SkIntToScalar(x);
                const SkScalar sy = SkIntToScalar(y);
                fTex[i].setXYWH(sx, sy, cell, cell);
                
                fRec[i].fCenter.set(sx + cell/2, sy + 3*cell/4);
                fRec[i].fVelocity.fX = rand.nextSScalar1() * kMaxSpeed;
                fRec[i].fVelocity.fY = rand.nextSScalar1() * kMaxSpeed;
                fRec[i].fScale = 1;
                fRec[i].fDScale = rand.nextSScalar1() / 4;
                fRec[i].fRadian = 0;
                fRec[i].fDRadian = rand.nextSScalar1() / 8;
                fRec[i].fAlpha = rand.nextUScalar1();
                fRec[i].fDAlpha = rand.nextSScalar1() / 10;
                i += 1;
            }
        }
    }

    void toggleUseColors() {
        fUseColors = !fUseColors;
    }

protected:
    void onDraw(SkCanvas* canvas) override {
        SkRSXform xform[N];
        SkColor colors[N];

        for (int i = 0; i < N; ++i) {
            fRec[i].advance(fBounds);
            xform[i] = fRec[i].asRSXform();
            if (fUseColors) {
                colors[i] = SkColorSetARGB((int)(fRec[i].fAlpha * 0xFF), 0xFF, 0xFF, 0xFF);
            }
        }
        SkPaint paint;
        paint.setFilterQuality(kLow_SkFilterQuality);

        const SkRect cull = this->getBounds();
        const SkColor* colorsPtr = fUseColors ? colors : NULL;
        canvas->drawAtlas(fAtlas, xform, fTex, colorsPtr, N, SkXfermode::kModulate_Mode,
                          &cull, &paint);
    }
    
    SkRect onGetBounds() override {
        const SkScalar border = kMaxScale * kCellSize;
        SkRect r = fBounds;
        r.outset(border, border);
        return r;
    }

private:
    typedef SkDrawable INHERITED;
};

class DrawAtlasView : public SampleView {
    DrawAtlasDrawable* fDrawable;

public:
    DrawAtlasView() {
        fDrawable = new DrawAtlasDrawable(SkRect::MakeWH(640, 480));
    }

    ~DrawAtlasView() override {
        fDrawable->unref();
    }

protected:
    bool onQuery(SkEvent* evt) override {
        if (SampleCode::TitleQ(*evt)) {
            SampleCode::TitleR(evt, "DrawAtlas");
            return true;
        }
        SkUnichar uni;
        if (SampleCode::CharQ(*evt, &uni)) {
            switch (uni) {
                case 'C': fDrawable->toggleUseColors(); this->inval(NULL); return true;
                default: break;
            }
        }
        return this->INHERITED::onQuery(evt);
    }

    void onDrawContent(SkCanvas* canvas) override {
        canvas->drawDrawable(fDrawable);
        this->inval(NULL);
    }

#if 0
    // TODO: switch over to use this for our animation
    bool onAnimate(const SkAnimTimer& timer) override {
        SkScalar angle = SkDoubleToScalar(fmod(timer.secs() * 360 / 24, 360));
        fAnimatingDrawable->setSweep(angle);
        return true;
    }
#endif

private:
    typedef SampleView INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

static SkView* MyFactory() { return new DrawAtlasView; }
static SkViewRegister reg(MyFactory);
