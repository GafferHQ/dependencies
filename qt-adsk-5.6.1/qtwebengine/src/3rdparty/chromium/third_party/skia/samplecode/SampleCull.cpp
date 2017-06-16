
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "SampleCode.h"
#include "SkView.h"
#include "SkCanvas.h"
#include "SkCornerPathEffect.h"
#include "SkCullPoints.h"
#include "SkGradientShader.h"
#include "SkPath.h"
#include "SkRegion.h"
#include "SkShader.h"
#include "SkUtils.h"
#include "SkRandom.h"

static void addbump(SkPath* path, const SkPoint pts[2], SkScalar bump) {
    SkVector    tang;

    tang.setLength(pts[1].fX - pts[0].fX, pts[1].fY - pts[0].fY, bump);

    path->lineTo(SkScalarHalf(pts[0].fX + pts[1].fX) - tang.fY,
                 SkScalarHalf(pts[0].fY + pts[1].fY) + tang.fX);
    path->lineTo(pts[1]);
}

static void subdivide(SkPath* path, SkScalar bump) {
    SkPath::Iter    iter(*path, false);
    SkPoint         pts[4];
    SkPath          tmp;

    for (;;)
        switch (iter.next(pts)) {
        case SkPath::kMove_Verb:
            tmp.moveTo(pts[0]);
            break;
        case SkPath::kLine_Verb:
            addbump(&tmp, pts, bump);
            bump = -bump;
            break;
        case SkPath::kDone_Verb:
            goto FINISH;
        default:
            break;
        }

FINISH:
    path->swap(tmp);
}

static SkIPoint* getpts(const SkPath& path, int* count) {
    SkPoint     pts[4];
    int         n = 1;
    SkIPoint*   array;

    {
        SkPath::Iter    iter(path, false);
        for (;;)
            switch (iter.next(pts)) {
            case SkPath::kLine_Verb:
                n += 1;
                break;
            case SkPath::kDone_Verb:
                goto FINISHED;
            default:
                break;
            }
    }

FINISHED:
    array = new SkIPoint[n];
    n = 0;

    {
        SkPath::Iter    iter(path, false);
        for (;;)
            switch (iter.next(pts)) {
            case SkPath::kMove_Verb:
                array[n++].set(SkScalarRoundToInt(pts[0].fX),
                               SkScalarRoundToInt(pts[0].fY));
                break;
            case SkPath::kLine_Verb:
                array[n++].set(SkScalarRoundToInt(pts[1].fX),
                               SkScalarRoundToInt(pts[1].fY));
                break;
            case SkPath::kDone_Verb:
                goto FINISHED2;
            default:
                break;
            }
    }

FINISHED2:
    *count = n;
    return array;
}

static SkScalar nextScalarRange(SkRandom& rand, SkScalar min, SkScalar max) {
    return min + SkScalarMul(rand.nextUScalar1(), max - min);
}

class CullView : public SampleView {
public:
    CullView() {
        fClip.set(0, 0, SkIntToScalar(160), SkIntToScalar(160));

        SkRandom    rand;

        for (int i = 0; i < 50; i++) {
            SkScalar x = nextScalarRange(rand, -fClip.width()*1, fClip.width()*2);
            SkScalar y = nextScalarRange(rand, -fClip.height()*1, fClip.height()*2);
            if (i == 0)
                fPath.moveTo(x, y);
            else
                fPath.lineTo(x, y);
        }

        SkScalar bump = fClip.width()/8;
        subdivide(&fPath, bump);
        subdivide(&fPath, bump);
        subdivide(&fPath, bump);
        fPoints = getpts(fPath, &fPtCount);

        this->setBGColor(0xFFDDDDDD);
    }

    virtual ~CullView() {
        delete[] fPoints;
    }

protected:
    // overrides from SkEventSink
    virtual bool onQuery(SkEvent* evt) {
        if (SampleCode::TitleQ(*evt)) {
            SampleCode::TitleR(evt, "Culling");
            return true;
        }
        return this->INHERITED::onQuery(evt);
    }

    virtual void onDrawContent(SkCanvas* canvas) {
        SkAutoCanvasRestore ar(canvas, true);

        canvas->translate(  SkScalarHalf(this->width() - fClip.width()),
                            SkScalarHalf(this->height() - fClip.height()));

   //     canvas->scale(SK_Scalar1*3, SK_Scalar1*3, 0, 0);

        SkPaint paint;

    //    paint.setAntiAliasOn(true);
        paint.setStyle(SkPaint::kStroke_Style);

        canvas->drawRect(fClip, paint);

#if 1
        paint.setColor(0xFF555555);
        paint.setStrokeWidth(SkIntToScalar(2));
//        paint.setPathEffect(new SkCornerPathEffect(SkIntToScalar(30)))->unref();
        canvas->drawPath(fPath, paint);
//        paint.setPathEffect(NULL);
#endif

        SkPath  tmp;
        SkIRect iclip;
        fClip.round(&iclip);

        SkCullPointsPath    cpp(iclip, &tmp);

        cpp.moveTo(fPoints[0].fX, fPoints[0].fY);
        for (int i = 0; i < fPtCount; i++)
            cpp.lineTo(fPoints[i].fX, fPoints[i].fY);

        paint.setColor(SK_ColorRED);
        paint.setStrokeWidth(SkIntToScalar(3));
        paint.setStrokeJoin(SkPaint::kRound_Join);
        canvas->drawPath(tmp, paint);

        this->inval(NULL);
    }

private:
    SkRect      fClip;
    SkIPoint*   fPoints;
    SkPath      fPath;
    int         fPtCount;

    typedef SampleView INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

static SkView* MyFactory() { return new CullView; }
static SkViewRegister reg(MyFactory);
