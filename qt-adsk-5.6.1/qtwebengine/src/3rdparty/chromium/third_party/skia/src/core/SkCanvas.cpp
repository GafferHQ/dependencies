/*
 * Copyright 2008 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkCanvas.h"
#include "SkCanvasPriv.h"
#include "SkBitmapDevice.h"
#include "SkColorFilter.h"
#include "SkDraw.h"
#include "SkDrawable.h"
#include "SkDrawFilter.h"
#include "SkDrawLooper.h"
#include "SkErrorInternals.h"
#include "SkImage.h"
#include "SkMetaData.h"
#include "SkNinePatchIter.h"
#include "SkPathOps.h"
#include "SkPatchUtils.h"
#include "SkPicture.h"
#include "SkRasterClip.h"
#include "SkReadPixelsRec.h"
#include "SkRRect.h"
#include "SkSmallAllocator.h"
#include "SkSurface_Base.h"
#include "SkTemplates.h"
#include "SkTextBlob.h"
#include "SkTextFormatParams.h"
#include "SkTLazy.h"
#include "SkTraceEvent.h"
#include "SkUtils.h"

#if SK_SUPPORT_GPU
#include "GrRenderTarget.h"
#endif

static bool gIgnoreSaveLayerBounds;
void SkCanvas::Internal_Private_SetIgnoreSaveLayerBounds(bool ignore) {
    gIgnoreSaveLayerBounds = ignore;
}
bool SkCanvas::Internal_Private_GetIgnoreSaveLayerBounds() {
    return gIgnoreSaveLayerBounds;
}

static bool gTreatSpriteAsBitmap;
void SkCanvas::Internal_Private_SetTreatSpriteAsBitmap(bool spriteAsBitmap) {
    gTreatSpriteAsBitmap = spriteAsBitmap;
}
bool SkCanvas::Internal_Private_GetTreatSpriteAsBitmap() {
    return gTreatSpriteAsBitmap;
}

// experimental for faster tiled drawing...
//#define SK_ENABLE_CLIP_QUICKREJECT

//#define SK_TRACE_SAVERESTORE

#ifdef SK_TRACE_SAVERESTORE
    static int gLayerCounter;
    static void inc_layer() { ++gLayerCounter; printf("----- inc layer %d\n", gLayerCounter); }
    static void dec_layer() { --gLayerCounter; printf("----- dec layer %d\n", gLayerCounter); }

    static int gRecCounter;
    static void inc_rec() { ++gRecCounter; printf("----- inc rec %d\n", gRecCounter); }
    static void dec_rec() { --gRecCounter; printf("----- dec rec %d\n", gRecCounter); }

    static int gCanvasCounter;
    static void inc_canvas() { ++gCanvasCounter; printf("----- inc canvas %d\n", gCanvasCounter); }
    static void dec_canvas() { --gCanvasCounter; printf("----- dec canvas %d\n", gCanvasCounter); }
#else
    #define inc_layer()
    #define dec_layer()
    #define inc_rec()
    #define dec_rec()
    #define inc_canvas()
    #define dec_canvas()
#endif

typedef SkTLazy<SkPaint> SkLazyPaint;

void SkCanvas::predrawNotify() {
    if (fSurfaceBase) {
        fSurfaceBase->aboutToDraw(SkSurface::kRetain_ContentChangeMode);
    }
}

///////////////////////////////////////////////////////////////////////////////

static uint32_t filter_paint_flags(const SkSurfaceProps& props, uint32_t flags) {
    const uint32_t propFlags = props.flags();
    if (propFlags & SkSurfaceProps::kDisallowDither_Flag) {
        flags &= ~SkPaint::kDither_Flag;
    }
    if (propFlags & SkSurfaceProps::kDisallowAntiAlias_Flag) {
        flags &= ~SkPaint::kAntiAlias_Flag;
    }
    return flags;
}

///////////////////////////////////////////////////////////////////////////////

/*  This is the record we keep for each SkBaseDevice that the user installs.
    The clip/matrix/proc are fields that reflect the top of the save/restore
    stack. Whenever the canvas changes, it marks a dirty flag, and then before
    these are used (assuming we're not on a layer) we rebuild these cache
    values: they reflect the top of the save stack, but translated and clipped
    by the device's XY offset and bitmap-bounds.
*/
struct DeviceCM {
    DeviceCM*           fNext;
    SkBaseDevice*       fDevice;
    SkRasterClip        fClip;
    SkPaint*            fPaint; // may be null (in the future)
    const SkMatrix*     fMatrix;
    SkMatrix            fMatrixStorage;
    const bool          fDeviceIsBitmapDevice;

    DeviceCM(SkBaseDevice* device, const SkPaint* paint, SkCanvas* canvas,
             bool conservativeRasterClip, bool deviceIsBitmapDevice)
        : fNext(NULL)
        , fClip(conservativeRasterClip)
        , fDeviceIsBitmapDevice(deviceIsBitmapDevice)
    {
        if (NULL != device) {
            device->ref();
            device->onAttachToCanvas(canvas);
        }
        fDevice = device;
        fPaint = paint ? SkNEW_ARGS(SkPaint, (*paint)) : NULL;
    }

    ~DeviceCM() {
        if (fDevice) {
            fDevice->onDetachFromCanvas();
            fDevice->unref();
        }
        SkDELETE(fPaint);
    }

    void reset(const SkIRect& bounds) {
        SkASSERT(!fPaint);
        SkASSERT(!fNext);
        SkASSERT(fDevice);
        fClip.setRect(bounds);
    }

    void updateMC(const SkMatrix& totalMatrix, const SkRasterClip& totalClip,
                  const SkClipStack& clipStack, SkRasterClip* updateClip) {
        int x = fDevice->getOrigin().x();
        int y = fDevice->getOrigin().y();
        int width = fDevice->width();
        int height = fDevice->height();

        if ((x | y) == 0) {
            fMatrix = &totalMatrix;
            fClip = totalClip;
        } else {
            fMatrixStorage = totalMatrix;
            fMatrixStorage.postTranslate(SkIntToScalar(-x),
                                         SkIntToScalar(-y));
            fMatrix = &fMatrixStorage;

            totalClip.translate(-x, -y, &fClip);
        }

        fClip.op(SkIRect::MakeWH(width, height), SkRegion::kIntersect_Op);

        // intersect clip, but don't translate it (yet)

        if (updateClip) {
            updateClip->op(SkIRect::MakeXYWH(x, y, width, height),
                           SkRegion::kDifference_Op);
        }

        fDevice->setMatrixClip(*fMatrix, fClip.forceGetBW(), clipStack);

#ifdef SK_DEBUG
        if (!fClip.isEmpty()) {
            SkIRect deviceR;
            deviceR.set(0, 0, width, height);
            SkASSERT(deviceR.contains(fClip.getBounds()));
        }
#endif
    }
};

/*  This is the record we keep for each save/restore level in the stack.
    Since a level optionally copies the matrix and/or stack, we have pointers
    for these fields. If the value is copied for this level, the copy is
    stored in the ...Storage field, and the pointer points to that. If the
    value is not copied for this level, we ignore ...Storage, and just point
    at the corresponding value in the previous level in the stack.
*/
class SkCanvas::MCRec {
public:
    SkDrawFilter*   fFilter;    // the current filter (or null)
    DeviceCM*       fLayer;
    /*  If there are any layers in the stack, this points to the top-most
        one that is at or below this level in the stack (so we know what
        bitmap/device to draw into from this level. This value is NOT
        reference counted, since the real owner is either our fLayer field,
        or a previous one in a lower level.)
    */
    DeviceCM*       fTopLayer;
    SkRasterClip    fRasterClip;
    SkMatrix        fMatrix;
    int             fDeferredSaveCount;

    MCRec(bool conservativeRasterClip) : fRasterClip(conservativeRasterClip) {
        fFilter     = NULL;
        fLayer      = NULL;
        fTopLayer   = NULL;
        fMatrix.reset();
        fDeferredSaveCount = 0;

        // don't bother initializing fNext
        inc_rec();
    }
    MCRec(const MCRec& prev) : fRasterClip(prev.fRasterClip), fMatrix(prev.fMatrix) {
        fFilter = SkSafeRef(prev.fFilter);
        fLayer = NULL;
        fTopLayer = prev.fTopLayer;
        fDeferredSaveCount = 0;

        // don't bother initializing fNext
        inc_rec();
    }
    ~MCRec() {
        SkSafeUnref(fFilter);
        SkDELETE(fLayer);
        dec_rec();
    }

    void reset(const SkIRect& bounds) {
        SkASSERT(fLayer);
        SkASSERT(fDeferredSaveCount == 0);

        fMatrix.reset();
        fRasterClip.setRect(bounds);
        fLayer->reset(bounds);
    }
};

class SkDrawIter : public SkDraw {
public:
    SkDrawIter(SkCanvas* canvas, bool skipEmptyClips = true) {
        canvas = canvas->canvasForDrawIter();
        fCanvas = canvas;
        canvas->updateDeviceCMCache();

        fClipStack = canvas->fClipStack;
        fCurrLayer = canvas->fMCRec->fTopLayer;
        fSkipEmptyClips = skipEmptyClips;
    }

    bool next() {
        // skip over recs with empty clips
        if (fSkipEmptyClips) {
            while (fCurrLayer && fCurrLayer->fClip.isEmpty()) {
                fCurrLayer = fCurrLayer->fNext;
            }
        }

        const DeviceCM* rec = fCurrLayer;
        if (rec && rec->fDevice) {

            fMatrix = rec->fMatrix;
            fClip   = &((SkRasterClip*)&rec->fClip)->forceGetBW();
            fRC     = &rec->fClip;
            fDevice = rec->fDevice;
            if (!fDevice->accessPixels(&fDst)) {
                fDst.reset(fDevice->imageInfo(), NULL, 0);
            }
            fPaint  = rec->fPaint;
            SkDEBUGCODE(this->validate();)

            fCurrLayer = rec->fNext;
            // fCurrLayer may be NULL now

            return true;
        }
        return false;
    }

    SkBaseDevice* getDevice() const { return fDevice; }
    int getX() const { return fDevice->getOrigin().x(); }
    int getY() const { return fDevice->getOrigin().y(); }
    const SkMatrix& getMatrix() const { return *fMatrix; }
    const SkRegion& getClip() const { return *fClip; }
    const SkPaint* getPaint() const { return fPaint; }

private:
    SkCanvas*       fCanvas;
    const DeviceCM* fCurrLayer;
    const SkPaint*  fPaint;     // May be null.
    SkBool8         fSkipEmptyClips;

    typedef SkDraw INHERITED;
};

/////////////////////////////////////////////////////////////////////////////

static SkPaint* set_if_needed(SkLazyPaint* lazy, const SkPaint& orig) {
    return lazy->isValid() ? lazy->get() : lazy->set(orig);
}

/**
 *  If the paint has an imagefilter, but it can be simplified to just a colorfilter, return that
 *  colorfilter, else return NULL.
 */
static SkColorFilter* image_to_color_filter(const SkPaint& paint) {
    SkImageFilter* imgf = paint.getImageFilter();
    if (!imgf) {
        return NULL;
    }

    SkColorFilter* imgCF;
    if (!imgf->asAColorFilter(&imgCF)) {
        return NULL;
    }

    SkColorFilter* paintCF = paint.getColorFilter();
    if (NULL == paintCF) {
        // there is no existing paint colorfilter, so we can just return the imagefilter's
        return imgCF;
    }

    // The paint has both a colorfilter(paintCF) and an imagefilter-which-is-a-colorfilter(imgCF)
    // and we need to combine them into a single colorfilter.
    SkAutoTUnref<SkColorFilter> autoImgCF(imgCF);
    return SkColorFilter::CreateComposeFilter(imgCF, paintCF);
}

class AutoDrawLooper {
public:
    AutoDrawLooper(SkCanvas* canvas, const SkSurfaceProps& props, const SkPaint& paint,
                   bool skipLayerForImageFilter = false,
                   const SkRect* bounds = NULL) : fOrigPaint(paint) {
        fCanvas = canvas;
        fFilter = canvas->getDrawFilter();
        fPaint = &fOrigPaint;
        fSaveCount = canvas->getSaveCount();
        fTempLayerForImageFilter = false;
        fDone = false;

        SkColorFilter* simplifiedCF = image_to_color_filter(fOrigPaint);
        if (simplifiedCF) {
            SkPaint* paint = set_if_needed(&fLazyPaintInit, fOrigPaint);
            paint->setColorFilter(simplifiedCF)->unref();
            paint->setImageFilter(NULL);
            fPaint = paint;
        }

        if (!skipLayerForImageFilter && fPaint->getImageFilter()) {
            /**
             *  We implement ImageFilters for a given draw by creating a layer, then applying the
             *  imagefilter to the pixels of that layer (its backing surface/image), and then
             *  we call restore() to xfer that layer to the main canvas.
             *
             *  1. SaveLayer (with a paint containing the current imagefilter and xfermode)
             *  2. Generate the src pixels:
             *      Remove the imagefilter and the xfermode from the paint that we (AutoDrawLooper)
             *      return (fPaint). We then draw the primitive (using srcover) into a cleared
             *      buffer/surface.
             *  3. Restore the layer created in #1
             *      The imagefilter is passed the buffer/surface from the layer (now filled with the
             *      src pixels of the primitive). It returns a new "filtered" buffer, which we
             *      draw onto the previous layer using the xfermode from the original paint.
             */
            SkPaint tmp;
            tmp.setImageFilter(fPaint->getImageFilter());
            tmp.setXfermode(fPaint->getXfermode());
            (void)canvas->internalSaveLayer(bounds, &tmp, SkCanvas::kARGB_ClipLayer_SaveFlag,
                                            SkCanvas::kFullLayer_SaveLayerStrategy);
            fTempLayerForImageFilter = true;
            // we remove the imagefilter/xfermode inside doNext()
        }

        if (SkDrawLooper* looper = paint.getLooper()) {
            void* buffer = fLooperContextAllocator.reserveT<SkDrawLooper::Context>(
                    looper->contextSize());
            fLooperContext = looper->createContext(canvas, buffer);
            fIsSimple = false;
        } else {
            fLooperContext = NULL;
            // can we be marked as simple?
            fIsSimple = !fFilter && !fTempLayerForImageFilter;
        }

        uint32_t oldFlags = paint.getFlags();
        fNewPaintFlags = filter_paint_flags(props, oldFlags);
        if (fIsSimple && (fNewPaintFlags != oldFlags)) {
            SkPaint* paint = set_if_needed(&fLazyPaintInit, fOrigPaint);
            paint->setFlags(fNewPaintFlags);
            fPaint = paint;
            // if we're not simple, doNext() will take care of calling setFlags()
        }
    }

    ~AutoDrawLooper() {
        if (fTempLayerForImageFilter) {
            fCanvas->internalRestore();
        }
        SkASSERT(fCanvas->getSaveCount() == fSaveCount);
    }

    const SkPaint& paint() const {
        SkASSERT(fPaint);
        return *fPaint;
    }

    bool next(SkDrawFilter::Type drawType) {
        if (fDone) {
            return false;
        } else if (fIsSimple) {
            fDone = true;
            return !fPaint->nothingToDraw();
        } else {
            return this->doNext(drawType);
        }
    }

private:
    SkLazyPaint     fLazyPaintInit; // base paint storage in case we need to modify it
    SkLazyPaint     fLazyPaintPerLooper;  // per-draw-looper storage, so the looper can modify it
    SkCanvas*       fCanvas;
    const SkPaint&  fOrigPaint;
    SkDrawFilter*   fFilter;
    const SkPaint*  fPaint;
    int             fSaveCount;
    uint32_t        fNewPaintFlags;
    bool            fTempLayerForImageFilter;
    bool            fDone;
    bool            fIsSimple;
    SkDrawLooper::Context* fLooperContext;
    SkSmallAllocator<1, 32> fLooperContextAllocator;

    bool doNext(SkDrawFilter::Type drawType);
};

bool AutoDrawLooper::doNext(SkDrawFilter::Type drawType) {
    fPaint = NULL;
    SkASSERT(!fIsSimple);
    SkASSERT(fLooperContext || fFilter || fTempLayerForImageFilter);

    SkPaint* paint = fLazyPaintPerLooper.set(fLazyPaintInit.isValid() ?
                                             *fLazyPaintInit.get() : fOrigPaint);
    paint->setFlags(fNewPaintFlags);

    if (fTempLayerForImageFilter) {
        paint->setImageFilter(NULL);
        paint->setXfermode(NULL);
    }

    if (fLooperContext && !fLooperContext->next(fCanvas, paint)) {
        fDone = true;
        return false;
    }
    if (fFilter) {
        if (!fFilter->filter(paint, drawType)) {
            fDone = true;
            return false;
        }
        if (NULL == fLooperContext) {
            // no looper means we only draw once
            fDone = true;
        }
    }
    fPaint = paint;

    // if we only came in here for the imagefilter, mark us as done
    if (!fLooperContext && !fFilter) {
        fDone = true;
    }

    // call this after any possible paint modifiers
    if (fPaint->nothingToDraw()) {
        fPaint = NULL;
        return false;
    }
    return true;
}

////////// macros to place around the internal draw calls //////////////////

#define LOOPER_BEGIN_DRAWDEVICE(paint, type)                        \
    this->predrawNotify();                                          \
    AutoDrawLooper  looper(this, fProps, paint, true);              \
    while (looper.next(type)) {                                     \
        SkDrawIter          iter(this);

#define LOOPER_BEGIN(paint, type, bounds)                           \
    this->predrawNotify();                                          \
    AutoDrawLooper  looper(this, fProps, paint, false, bounds);     \
    while (looper.next(type)) {                                     \
        SkDrawIter          iter(this);

#define LOOPER_END    }

////////////////////////////////////////////////////////////////////////////

void SkCanvas::resetForNextPicture(const SkIRect& bounds) {
    this->restoreToCount(1);
    fCachedLocalClipBounds.setEmpty();
    fCachedLocalClipBoundsDirty = true;
    fClipStack->reset();
    fMCRec->reset(bounds);

    // We're peering through a lot of structs here.  Only at this scope do we
    // know that the device is an SkBitmapDevice (really an SkNoPixelsBitmapDevice).
    static_cast<SkBitmapDevice*>(fMCRec->fLayer->fDevice)->setNewSize(bounds.size());
}

SkBaseDevice* SkCanvas::init(SkBaseDevice* device, InitFlags flags) {
    fConservativeRasterClip = SkToBool(flags & kConservativeRasterClip_InitFlag);
    fCachedLocalClipBounds.setEmpty();
    fCachedLocalClipBoundsDirty = true;
    fAllowSoftClip = true;
    fAllowSimplifyClip = false;
    fDeviceCMDirty = true;
    fSaveCount = 1;
    fMetaData = NULL;

    fClipStack.reset(SkNEW(SkClipStack));

    fMCRec = (MCRec*)fMCStack.push_back();
    new (fMCRec) MCRec(fConservativeRasterClip);

    SkASSERT(sizeof(DeviceCM) <= sizeof(fDeviceCMStorage));
    fMCRec->fLayer = (DeviceCM*)fDeviceCMStorage;
    new (fDeviceCMStorage) DeviceCM(NULL, NULL, NULL, fConservativeRasterClip, false);

    fMCRec->fTopLayer = fMCRec->fLayer;

    fSurfaceBase = NULL;

    if (device) {
        // The root device and the canvas should always have the same pixel geometry
        SkASSERT(fProps.pixelGeometry() == device->surfaceProps().pixelGeometry());
        if (device->forceConservativeRasterClip()) {
            fConservativeRasterClip = true;
        }
        device->onAttachToCanvas(this);
        fMCRec->fLayer->fDevice = SkRef(device);
        fMCRec->fRasterClip.setRect(device->getGlobalBounds());
    }
    return device;
}

SkCanvas::SkCanvas()
    : fMCStack(sizeof(MCRec), fMCRecStorage, sizeof(fMCRecStorage))
    , fProps(SkSurfaceProps::kLegacyFontHost_InitType)
{
    inc_canvas();

    this->init(NULL, kDefault_InitFlags);
}

static SkBitmap make_nopixels(int width, int height) {
    SkBitmap bitmap;
    bitmap.setInfo(SkImageInfo::MakeUnknown(width, height));
    return bitmap;
}

class SkNoPixelsBitmapDevice : public SkBitmapDevice {
public:
    SkNoPixelsBitmapDevice(const SkIRect& bounds, const SkSurfaceProps& surfaceProps)
        : INHERITED(make_nopixels(bounds.width(), bounds.height()), surfaceProps)
    {
        this->setOrigin(bounds.x(), bounds.y());
    }

private:

    typedef SkBitmapDevice INHERITED;
};

SkCanvas::SkCanvas(int width, int height, const SkSurfaceProps* props)
    : fMCStack(sizeof(MCRec), fMCRecStorage, sizeof(fMCRecStorage))
    , fProps(SkSurfacePropsCopyOrDefault(props))
{
    inc_canvas();

    this->init(SkNEW_ARGS(SkNoPixelsBitmapDevice,
                          (SkIRect::MakeWH(width, height), fProps)), kDefault_InitFlags)->unref();
}

SkCanvas::SkCanvas(const SkIRect& bounds, InitFlags flags)
    : fMCStack(sizeof(MCRec), fMCRecStorage, sizeof(fMCRecStorage))
    , fProps(SkSurfaceProps::kLegacyFontHost_InitType)
{
    inc_canvas();

    this->init(SkNEW_ARGS(SkNoPixelsBitmapDevice, (bounds, fProps)), flags)->unref();
}

SkCanvas::SkCanvas(SkBaseDevice* device)
    : fMCStack(sizeof(MCRec), fMCRecStorage, sizeof(fMCRecStorage))
    , fProps(device->surfaceProps())
{
    inc_canvas();

    this->init(device, kDefault_InitFlags);
}

SkCanvas::SkCanvas(SkBaseDevice* device, InitFlags flags)
    : fMCStack(sizeof(MCRec), fMCRecStorage, sizeof(fMCRecStorage))
    , fProps(device->surfaceProps())
{
    inc_canvas();

    this->init(device, flags);
}

SkCanvas::SkCanvas(const SkBitmap& bitmap, const SkSurfaceProps& props)
    : fMCStack(sizeof(MCRec), fMCRecStorage, sizeof(fMCRecStorage))
    , fProps(props)
{
    inc_canvas();

    SkAutoTUnref<SkBaseDevice> device(SkNEW_ARGS(SkBitmapDevice, (bitmap, fProps)));
    this->init(device, kDefault_InitFlags);
}

SkCanvas::SkCanvas(const SkBitmap& bitmap)
    : fMCStack(sizeof(MCRec), fMCRecStorage, sizeof(fMCRecStorage))
    , fProps(SkSurfaceProps::kLegacyFontHost_InitType)
{
    inc_canvas();

    SkAutoTUnref<SkBaseDevice> device(SkNEW_ARGS(SkBitmapDevice, (bitmap, fProps)));
    this->init(device, kDefault_InitFlags);
}

SkCanvas::~SkCanvas() {
    // free up the contents of our deque
    this->restoreToCount(1);    // restore everything but the last

    this->internalRestore();    // restore the last, since we're going away

    SkDELETE(fMetaData);

    dec_canvas();
}

SkDrawFilter* SkCanvas::getDrawFilter() const {
    return fMCRec->fFilter;
}

SkDrawFilter* SkCanvas::setDrawFilter(SkDrawFilter* filter) {
    this->checkForDeferredSave();
    SkRefCnt_SafeAssign(fMCRec->fFilter, filter);
    return filter;
}

SkMetaData& SkCanvas::getMetaData() {
    // metadata users are rare, so we lazily allocate it. If that changes we
    // can decide to just make it a field in the device (rather than a ptr)
    if (NULL == fMetaData) {
        fMetaData = new SkMetaData;
    }
    return *fMetaData;
}

///////////////////////////////////////////////////////////////////////////////

void SkCanvas::flush() {
    SkBaseDevice* device = this->getDevice();
    if (device) {
        device->flush();
    }
}

SkISize SkCanvas::getTopLayerSize() const {
    SkBaseDevice* d = this->getTopDevice();
    return d ? SkISize::Make(d->width(), d->height()) : SkISize::Make(0, 0);
}

SkIPoint SkCanvas::getTopLayerOrigin() const {
    SkBaseDevice* d = this->getTopDevice();
    return d ? d->getOrigin() : SkIPoint::Make(0, 0);
}

SkISize SkCanvas::getBaseLayerSize() const {
    SkBaseDevice* d = this->getDevice();
    return d ? SkISize::Make(d->width(), d->height()) : SkISize::Make(0, 0);
}

SkBaseDevice* SkCanvas::getDevice() const {
    // return root device
    MCRec* rec = (MCRec*) fMCStack.front();
    SkASSERT(rec && rec->fLayer);
    return rec->fLayer->fDevice;
}

SkBaseDevice* SkCanvas::getTopDevice(bool updateMatrixClip) const {
    if (updateMatrixClip) {
        const_cast<SkCanvas*>(this)->updateDeviceCMCache();
    }
    return fMCRec->fTopLayer->fDevice;
}

bool SkCanvas::readPixels(SkBitmap* bitmap, int x, int y) {
    if (kUnknown_SkColorType == bitmap->colorType() || bitmap->getTexture()) {
        return false;
    }

    bool weAllocated = false;
    if (NULL == bitmap->pixelRef()) {
        if (!bitmap->tryAllocPixels()) {
            return false;
        }
        weAllocated = true;
    }

    SkAutoPixmapUnlock unlocker;
    if (bitmap->requestLock(&unlocker)) {
        const SkPixmap& pm = unlocker.pixmap();
        if (this->readPixels(pm.info(), pm.writable_addr(), pm.rowBytes(), x, y)) {
            return true;
        }
    }

    if (weAllocated) {
        bitmap->setPixelRef(NULL);
    }
    return false;
}

bool SkCanvas::readPixels(const SkIRect& srcRect, SkBitmap* bitmap) {
    SkIRect r = srcRect;
    const SkISize size = this->getBaseLayerSize();
    if (!r.intersect(0, 0, size.width(), size.height())) {
        bitmap->reset();
        return false;
    }

    if (!bitmap->tryAllocN32Pixels(r.width(), r.height())) {
        // bitmap will already be reset.
        return false;
    }
    if (!this->readPixels(bitmap->info(), bitmap->getPixels(), bitmap->rowBytes(), r.x(), r.y())) {
        bitmap->reset();
        return false;
    }
    return true;
}

bool SkCanvas::readPixels(const SkImageInfo& dstInfo, void* dstP, size_t rowBytes, int x, int y) {
    SkBaseDevice* device = this->getDevice();
    if (!device) {
        return false;
    }
    const SkISize size = this->getBaseLayerSize();

    SkReadPixelsRec rec(dstInfo, dstP, rowBytes, x, y);
    if (!rec.trim(size.width(), size.height())) {
        return false;
    }

    // The device can assert that the requested area is always contained in its bounds
    return device->readPixels(rec.fInfo, rec.fPixels, rec.fRowBytes, rec.fX, rec.fY);
}

bool SkCanvas::writePixels(const SkBitmap& bitmap, int x, int y) {
    if (bitmap.getTexture()) {
        return false;
    }

    SkAutoPixmapUnlock unlocker;
    if (bitmap.requestLock(&unlocker)) {
        const SkPixmap& pm = unlocker.pixmap();
        return this->writePixels(pm.info(), pm.addr(), pm.rowBytes(), x, y);
    }
    return false;
}

bool SkCanvas::writePixels(const SkImageInfo& origInfo, const void* pixels, size_t rowBytes,
                           int x, int y) {
    switch (origInfo.colorType()) {
        case kUnknown_SkColorType:
        case kIndex_8_SkColorType:
            return false;
        default:
            break;
    }
    if (NULL == pixels || rowBytes < origInfo.minRowBytes()) {
        return false;
    }

    const SkISize size = this->getBaseLayerSize();
    SkIRect target = SkIRect::MakeXYWH(x, y, origInfo.width(), origInfo.height());
    if (!target.intersect(0, 0, size.width(), size.height())) {
        return false;
    }

    SkBaseDevice* device = this->getDevice();
    if (!device) {
        return false;
    }

    // the intersect may have shrunk info's logical size
    const SkImageInfo info = origInfo.makeWH(target.width(), target.height());

    // if x or y are negative, then we have to adjust pixels
    if (x > 0) {
        x = 0;
    }
    if (y > 0) {
        y = 0;
    }
    // here x,y are either 0 or negative
    pixels = ((const char*)pixels - y * rowBytes - x * info.bytesPerPixel());

    // Tell our owning surface to bump its generation ID
    this->predrawNotify();

    // The device can assert that the requested area is always contained in its bounds
    return device->writePixels(info, pixels, rowBytes, target.x(), target.y());
}

SkCanvas* SkCanvas::canvasForDrawIter() {
    return this;
}

//////////////////////////////////////////////////////////////////////////////

void SkCanvas::updateDeviceCMCache() {
    if (fDeviceCMDirty) {
        const SkMatrix& totalMatrix = this->getTotalMatrix();
        const SkRasterClip& totalClip = fMCRec->fRasterClip;
        DeviceCM*       layer = fMCRec->fTopLayer;

        if (NULL == layer->fNext) {   // only one layer
            layer->updateMC(totalMatrix, totalClip, *fClipStack, NULL);
        } else {
            SkRasterClip clip(totalClip);
            do {
                layer->updateMC(totalMatrix, clip, *fClipStack, &clip);
            } while ((layer = layer->fNext) != NULL);
        }
        fDeviceCMDirty = false;
    }
}

///////////////////////////////////////////////////////////////////////////////

void SkCanvas::checkForDeferredSave() {
    if (fMCRec->fDeferredSaveCount > 0) {
        this->doSave();
    }
}

int SkCanvas::getSaveCount() const {
#ifdef SK_DEBUG
    int count = 0;
    SkDeque::Iter iter(fMCStack, SkDeque::Iter::kFront_IterStart);
    for (;;) {
        const MCRec* rec = (const MCRec*)iter.next();
        if (!rec) {
            break;
        }
        count += 1 + rec->fDeferredSaveCount;
    }
    SkASSERT(count == fSaveCount);
#endif
    return fSaveCount;
}

int SkCanvas::save() {
    fSaveCount += 1;
    fMCRec->fDeferredSaveCount += 1;
    return this->getSaveCount() - 1;  // return our prev value
}

void SkCanvas::doSave() {
    this->willSave();

    SkASSERT(fMCRec->fDeferredSaveCount > 0);
    fMCRec->fDeferredSaveCount -= 1;
    this->internalSave();
}

void SkCanvas::restore() {
    if (fMCRec->fDeferredSaveCount > 0) {
        SkASSERT(fSaveCount > 1);
        fSaveCount -= 1;
        fMCRec->fDeferredSaveCount -= 1;
    } else {
        // check for underflow
        if (fMCStack.count() > 1) {
            this->willRestore();
            SkASSERT(fSaveCount > 1);
            fSaveCount -= 1;
            this->internalRestore();
            this->didRestore();
        }
    }
}

void SkCanvas::restoreToCount(int count) {
    // sanity check
    if (count < 1) {
        count = 1;
    }

    int n = this->getSaveCount() - count;
    for (int i = 0; i < n; ++i) {
        this->restore();
    }
}

void SkCanvas::internalSave() {
    MCRec* newTop = (MCRec*)fMCStack.push_back();
    new (newTop) MCRec(*fMCRec);    // balanced in restore()
    fMCRec = newTop;

    fClipStack->save();
}

static bool bounds_affects_clip(SkCanvas::SaveFlags flags) {
#ifdef SK_SUPPORT_LEGACY_CLIPTOLAYERFLAG
    return (flags & SkCanvas::kClipToLayer_SaveFlag) != 0;
#else
    return true;
#endif
}

bool SkCanvas::clipRectBounds(const SkRect* bounds, SaveFlags flags,
                              SkIRect* intersection, const SkImageFilter* imageFilter) {
    SkIRect clipBounds;
    if (!this->getClipDeviceBounds(&clipBounds)) {
        return false;
    }

    const SkMatrix& ctm = fMCRec->fMatrix;  // this->getTotalMatrix()

    if (imageFilter) {
        imageFilter->filterBounds(clipBounds, ctm, &clipBounds);
    }
    SkIRect ir;
    if (bounds) {
        SkRect r;

        ctm.mapRect(&r, *bounds);
        r.roundOut(&ir);
        // early exit if the layer's bounds are clipped out
        if (!ir.intersect(clipBounds)) {
            if (bounds_affects_clip(flags)) {
                fCachedLocalClipBoundsDirty = true;
                fMCRec->fRasterClip.setEmpty();
            }
            return false;
        }
    } else {    // no user bounds, so just use the clip
        ir = clipBounds;
    }
    SkASSERT(!ir.isEmpty());

    if (bounds_affects_clip(flags)) {
        // Simplify the current clips since they will be applied properly during restore()
        fCachedLocalClipBoundsDirty = true;
        fClipStack->clipDevRect(ir, SkRegion::kReplace_Op);
        fMCRec->fRasterClip.setRect(ir);
    }

    if (intersection) {
        *intersection = ir;
    }
    return true;
}

int SkCanvas::saveLayer(const SkRect* bounds, const SkPaint* paint) {
    if (gIgnoreSaveLayerBounds) {
        bounds = NULL;
    }
    SaveLayerStrategy strategy = this->willSaveLayer(bounds, paint, kARGB_ClipLayer_SaveFlag);
    fSaveCount += 1;
    this->internalSaveLayer(bounds, paint, kARGB_ClipLayer_SaveFlag, strategy);
    return this->getSaveCount() - 1;
}

int SkCanvas::saveLayer(const SkRect* bounds, const SkPaint* paint, SaveFlags flags) {
    if (gIgnoreSaveLayerBounds) {
        bounds = NULL;
    }
    SaveLayerStrategy strategy = this->willSaveLayer(bounds, paint, flags);
    fSaveCount += 1;
    this->internalSaveLayer(bounds, paint, flags, strategy);
    return this->getSaveCount() - 1;
}

void SkCanvas::internalSaveLayer(const SkRect* bounds, const SkPaint* paint, SaveFlags flags,
                                 SaveLayerStrategy strategy) {
#ifndef SK_SUPPORT_LEGACY_CLIPTOLAYERFLAG
    flags |= kClipToLayer_SaveFlag;
#endif

    // do this before we create the layer. We don't call the public save() since
    // that would invoke a possibly overridden virtual
    this->internalSave();

    fDeviceCMDirty = true;

    SkIRect ir;
    if (!this->clipRectBounds(bounds, flags, &ir, paint ? paint->getImageFilter() : NULL)) {
        return;
    }

    // FIXME: do willSaveLayer() overriders returning kNoLayer_SaveLayerStrategy really care about
    // the clipRectBounds() call above?
    if (kNoLayer_SaveLayerStrategy == strategy) {
        return;
    }

    bool isOpaque = !SkToBool(flags & kHasAlphaLayer_SaveFlag);
    SkPixelGeometry geo = fProps.pixelGeometry();
    if (paint) {
        // TODO: perhaps add a query to filters so we might preserve opaqueness...
        if (paint->getImageFilter() || paint->getColorFilter()) {
            isOpaque = false;
            geo = kUnknown_SkPixelGeometry;
        }
    }
    SkImageInfo info = SkImageInfo::MakeN32(ir.width(), ir.height(),
                        isOpaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType);

    SkBaseDevice* device = this->getTopDevice();
    if (NULL == device) {
        SkDebugf("Unable to find device for layer.");
        return;
    }

    bool forceSpriteOnRestore = false;
    {
        const SkBaseDevice::TileUsage usage = SkBaseDevice::kNever_TileUsage;
        const SkBaseDevice::CreateInfo createInfo = SkBaseDevice::CreateInfo(info, usage, geo);
        SkBaseDevice* newDev = device->onCreateDevice(createInfo, paint);
        if (NULL == newDev) {
            // If onCreateDevice didn't succeed, try raster (e.g. PDF couldn't handle the paint)
            const SkSurfaceProps surfaceProps(fProps.flags(), createInfo.fPixelGeometry);
            newDev = SkBitmapDevice::Create(createInfo.fInfo, surfaceProps);
            if (NULL == newDev) {
                SkErrorInternals::SetError(kInternalError_SkError,
                                           "Unable to create device for layer.");
                return;
            }
            forceSpriteOnRestore = true;
        }
        device = newDev;
    }

    device->setOrigin(ir.fLeft, ir.fTop);
    DeviceCM* layer = SkNEW_ARGS(DeviceCM, (device, paint, this, fConservativeRasterClip,
                                            forceSpriteOnRestore));
    device->unref();

    layer->fNext = fMCRec->fTopLayer;
    fMCRec->fLayer = layer;
    fMCRec->fTopLayer = layer;    // this field is NOT an owner of layer
}

int SkCanvas::saveLayerAlpha(const SkRect* bounds, U8CPU alpha) {
    return this->saveLayerAlpha(bounds, alpha, kARGB_ClipLayer_SaveFlag);
}

int SkCanvas::saveLayerAlpha(const SkRect* bounds, U8CPU alpha,
                             SaveFlags flags) {
    if (0xFF == alpha) {
        return this->saveLayer(bounds, NULL, flags);
    } else {
        SkPaint tmpPaint;
        tmpPaint.setAlpha(alpha);
        return this->saveLayer(bounds, &tmpPaint, flags);
    }
}

void SkCanvas::internalRestore() {
    SkASSERT(fMCStack.count() != 0);

    fDeviceCMDirty = true;
    fCachedLocalClipBoundsDirty = true;

    fClipStack->restore();

    // reserve our layer (if any)
    DeviceCM* layer = fMCRec->fLayer;   // may be null
    // now detach it from fMCRec so we can pop(). Gets freed after its drawn
    fMCRec->fLayer = NULL;

    // now do the normal restore()
    fMCRec->~MCRec();       // balanced in save()
    fMCStack.pop_back();
    fMCRec = (MCRec*)fMCStack.back();

    /*  Time to draw the layer's offscreen. We can't call the public drawSprite,
        since if we're being recorded, we don't want to record this (the
        recorder will have already recorded the restore).
    */
    if (layer) {
        if (layer->fNext) {
            const SkIPoint& origin = layer->fDevice->getOrigin();
            this->internalDrawDevice(layer->fDevice, origin.x(), origin.y(),
                                     layer->fPaint, layer->fDeviceIsBitmapDevice);
            // reset this, since internalDrawDevice will have set it to true
            fDeviceCMDirty = true;
            SkDELETE(layer);
        } else {
            // we're at the root
            SkASSERT(layer == (void*)fDeviceCMStorage);
            layer->~DeviceCM();
        }
    }
}

SkSurface* SkCanvas::newSurface(const SkImageInfo& info, const SkSurfaceProps* props) {
    if (NULL == props) {
        props = &fProps;
    }
    return this->onNewSurface(info, *props);
}

SkSurface* SkCanvas::onNewSurface(const SkImageInfo& info, const SkSurfaceProps& props) {
    SkBaseDevice* dev = this->getDevice();
    return dev ? dev->newSurface(info, props) : NULL;
}

SkImageInfo SkCanvas::imageInfo() const {
    SkBaseDevice* dev = this->getDevice();
    if (dev) {
        return dev->imageInfo();
    } else {
        return SkImageInfo::MakeUnknown(0, 0);
    }
}

const void* SkCanvas::peekPixels(SkImageInfo* info, size_t* rowBytes) {
    SkPixmap pmap;
    if (!this->onPeekPixels(&pmap)) {
        return NULL;
    }
    if (info) {
        *info = pmap.info();
    }
    if (rowBytes) {
        *rowBytes = pmap.rowBytes();
    }
    return pmap.addr();
}

bool SkCanvas::onPeekPixels(SkPixmap* pmap) {
    SkBaseDevice* dev = this->getDevice();
    return dev && dev->peekPixels(pmap);
}

void* SkCanvas::accessTopLayerPixels(SkImageInfo* info, size_t* rowBytes, SkIPoint* origin) {
    SkPixmap pmap;
    if (!this->onAccessTopLayerPixels(&pmap)) {
        return NULL;
    }
    if (info) {
        *info = pmap.info();
    }
    if (rowBytes) {
        *rowBytes = pmap.rowBytes();
    }
    if (origin) {
        *origin = this->getTopDevice(false)->getOrigin();
    }
    return pmap.writable_addr();
}

bool SkCanvas::onAccessTopLayerPixels(SkPixmap* pmap) {
    SkBaseDevice* dev = this->getTopDevice();
    return dev && dev->accessPixels(pmap);
}

SkAutoROCanvasPixels::SkAutoROCanvasPixels(SkCanvas* canvas) {
    fAddr = canvas->peekPixels(&fInfo, &fRowBytes);
    if (NULL == fAddr) {
        fInfo = canvas->imageInfo();
        if (kUnknown_SkColorType == fInfo.colorType() || !fBitmap.tryAllocPixels(fInfo)) {
            return; // failure, fAddr is NULL
        }
        if (!canvas->readPixels(&fBitmap, 0, 0)) {
            return; // failure, fAddr is NULL
        }
        fAddr = fBitmap.getPixels();
        fRowBytes = fBitmap.rowBytes();
    }
    SkASSERT(fAddr);    // success
}

bool SkAutoROCanvasPixels::asROBitmap(SkBitmap* bitmap) const {
    if (fAddr) {
        return bitmap->installPixels(fInfo, const_cast<void*>(fAddr), fRowBytes);
    } else {
        bitmap->reset();
        return false;
    }
}

/////////////////////////////////////////////////////////////////////////////
void SkCanvas::internalDrawBitmap(const SkBitmap& bitmap,
                                const SkMatrix& matrix, const SkPaint* paint) {
    if (bitmap.drawsNothing()) {
        return;
    }

    SkLazyPaint lazy;
    if (NULL == paint) {
        paint = lazy.init();
    }

    SkDEBUGCODE(bitmap.validate();)

    SkRect storage;
    const SkRect* bounds = NULL;
    if (paint && paint->canComputeFastBounds()) {
        bitmap.getBounds(&storage);
        matrix.mapRect(&storage);
        bounds = &paint->computeFastBounds(storage, &storage);
    }

    LOOPER_BEGIN(*paint, SkDrawFilter::kBitmap_Type, bounds)

    while (iter.next()) {
        iter.fDevice->drawBitmap(iter, bitmap, matrix, looper.paint());
    }

    LOOPER_END
}

void SkCanvas::internalDrawDevice(SkBaseDevice* srcDev, int x, int y,
                                  const SkPaint* paint, bool deviceIsBitmapDevice) {
    SkPaint tmp;
    if (NULL == paint) {
        paint = &tmp;
    }

    LOOPER_BEGIN_DRAWDEVICE(*paint, SkDrawFilter::kBitmap_Type)
    while (iter.next()) {
        SkBaseDevice* dstDev = iter.fDevice;
        paint = &looper.paint();
        SkImageFilter* filter = paint->getImageFilter();
        SkIPoint pos = { x - iter.getX(), y - iter.getY() };
        if (filter && !dstDev->canHandleImageFilter(filter)) {
            SkImageFilter::Proxy proxy(dstDev);
            SkBitmap dst;
            SkIPoint offset = SkIPoint::Make(0, 0);
            const SkBitmap& src = srcDev->accessBitmap(false);
            SkMatrix matrix = *iter.fMatrix;
            matrix.postTranslate(SkIntToScalar(-pos.x()), SkIntToScalar(-pos.y()));
            SkIRect clipBounds = SkIRect::MakeWH(srcDev->width(), srcDev->height());
            SkAutoTUnref<SkImageFilter::Cache> cache(dstDev->getImageFilterCache());
            SkImageFilter::Context ctx(matrix, clipBounds, cache.get());
            if (filter->filterImage(&proxy, src, ctx, &dst, &offset)) {
                SkPaint tmpUnfiltered(*paint);
                tmpUnfiltered.setImageFilter(NULL);
                dstDev->drawSprite(iter, dst, pos.x() + offset.x(), pos.y() + offset.y(),
                                   tmpUnfiltered);
            }
        } else if (deviceIsBitmapDevice) {
            const SkBitmap& src = static_cast<SkBitmapDevice*>(srcDev)->fBitmap;
            dstDev->drawSprite(iter, src, pos.x(), pos.y(), *paint);
        } else {
            dstDev->drawDevice(iter, srcDev, pos.x(), pos.y(), *paint);
        }
    }
    LOOPER_END
}

void SkCanvas::onDrawSprite(const SkBitmap& bitmap, int x, int y, const SkPaint* paint) {
    if (gTreatSpriteAsBitmap) {
        this->save();
        this->resetMatrix();
        this->drawBitmap(bitmap, SkIntToScalar(x), SkIntToScalar(y), paint);
        this->restore();
        return;
    }

    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawSprite()");
    if (bitmap.drawsNothing()) {
        return;
    }
    SkDEBUGCODE(bitmap.validate();)

    SkPaint tmp;
    if (NULL == paint) {
        paint = &tmp;
    }

    LOOPER_BEGIN_DRAWDEVICE(*paint, SkDrawFilter::kBitmap_Type)

    while (iter.next()) {
        paint = &looper.paint();
        SkImageFilter* filter = paint->getImageFilter();
        SkIPoint pos = { x - iter.getX(), y - iter.getY() };
        if (filter && !iter.fDevice->canHandleImageFilter(filter)) {
            SkImageFilter::Proxy proxy(iter.fDevice);
            SkBitmap dst;
            SkIPoint offset = SkIPoint::Make(0, 0);
            SkMatrix matrix = *iter.fMatrix;
            matrix.postTranslate(SkIntToScalar(-pos.x()), SkIntToScalar(-pos.y()));
            const SkIRect clipBounds = bitmap.bounds();
            SkAutoTUnref<SkImageFilter::Cache> cache(iter.fDevice->getImageFilterCache());
            SkImageFilter::Context ctx(matrix, clipBounds, cache.get());
            if (filter->filterImage(&proxy, bitmap, ctx, &dst, &offset)) {
                SkPaint tmpUnfiltered(*paint);
                tmpUnfiltered.setImageFilter(NULL);
                iter.fDevice->drawSprite(iter, dst, pos.x() + offset.x(), pos.y() + offset.y(),
                                         tmpUnfiltered);
            }
        } else {
            iter.fDevice->drawSprite(iter, bitmap, pos.x(), pos.y(), *paint);
        }
    }
    LOOPER_END
}

/////////////////////////////////////////////////////////////////////////////
void SkCanvas::translate(SkScalar dx, SkScalar dy) {
    SkMatrix m;
    m.setTranslate(dx, dy);
    this->concat(m);
}

void SkCanvas::scale(SkScalar sx, SkScalar sy) {
    SkMatrix m;
    m.setScale(sx, sy);
    this->concat(m);
}

void SkCanvas::rotate(SkScalar degrees) {
    SkMatrix m;
    m.setRotate(degrees);
    this->concat(m);
}

void SkCanvas::skew(SkScalar sx, SkScalar sy) {
    SkMatrix m;
    m.setSkew(sx, sy);
    this->concat(m);
}

void SkCanvas::concat(const SkMatrix& matrix) {
    if (matrix.isIdentity()) {
        return;
    }

    this->checkForDeferredSave();
    fDeviceCMDirty = true;
    fCachedLocalClipBoundsDirty = true;
    fMCRec->fMatrix.preConcat(matrix);

    this->didConcat(matrix);
}

void SkCanvas::setMatrix(const SkMatrix& matrix) {
    this->checkForDeferredSave();
    fDeviceCMDirty = true;
    fCachedLocalClipBoundsDirty = true;
    fMCRec->fMatrix = matrix;
    this->didSetMatrix(matrix);
}

void SkCanvas::resetMatrix() {
    SkMatrix matrix;

    matrix.reset();
    this->setMatrix(matrix);
}

//////////////////////////////////////////////////////////////////////////////

void SkCanvas::clipRect(const SkRect& rect, SkRegion::Op op, bool doAA) {
    this->checkForDeferredSave();
    ClipEdgeStyle edgeStyle = doAA ? kSoft_ClipEdgeStyle : kHard_ClipEdgeStyle;
    this->onClipRect(rect, op, edgeStyle);
}

void SkCanvas::onClipRect(const SkRect& rect, SkRegion::Op op, ClipEdgeStyle edgeStyle) {
#ifdef SK_ENABLE_CLIP_QUICKREJECT
    if (SkRegion::kIntersect_Op == op) {
        if (fMCRec->fRasterClip.isEmpty()) {
            return false;
        }

        if (this->quickReject(rect)) {
            fDeviceCMDirty = true;
            fCachedLocalClipBoundsDirty = true;

            fClipStack->clipEmpty();
            return fMCRec->fRasterClip.setEmpty();
        }
    }
#endif

    AutoValidateClip avc(this);

    fDeviceCMDirty = true;
    fCachedLocalClipBoundsDirty = true;
    if (!fAllowSoftClip) {
        edgeStyle = kHard_ClipEdgeStyle;
    }

    if (fMCRec->fMatrix.rectStaysRect()) {
        // for these simpler matrices, we can stay a rect even after applying
        // the matrix. This means we don't have to a) make a path, and b) tell
        // the region code to scan-convert the path, only to discover that it
        // is really just a rect.
        SkRect      r;

        fMCRec->fMatrix.mapRect(&r, rect);
        fClipStack->clipDevRect(r, op, kSoft_ClipEdgeStyle == edgeStyle);
        fMCRec->fRasterClip.op(r, this->getBaseLayerSize(), op, kSoft_ClipEdgeStyle == edgeStyle);
    } else {
        // since we're rotated or some such thing, we convert the rect to a path
        // and clip against that, since it can handle any matrix. However, to
        // avoid recursion in the case where we are subclassed (e.g. Pictures)
        // we explicitly call "our" version of clipPath.
        SkPath  path;

        path.addRect(rect);
        this->SkCanvas::onClipPath(path, op, edgeStyle);
    }
}

static void rasterclip_path(SkRasterClip* rc, const SkCanvas* canvas, const SkPath& devPath,
                            SkRegion::Op op, bool doAA) {
    rc->op(devPath, canvas->getBaseLayerSize(), op, doAA);
}

void SkCanvas::clipRRect(const SkRRect& rrect, SkRegion::Op op, bool doAA) {
    this->checkForDeferredSave();
    ClipEdgeStyle edgeStyle = doAA ? kSoft_ClipEdgeStyle : kHard_ClipEdgeStyle;
    if (rrect.isRect()) {
        this->onClipRect(rrect.getBounds(), op, edgeStyle);
    } else {
        this->onClipRRect(rrect, op, edgeStyle);
    }
}

void SkCanvas::onClipRRect(const SkRRect& rrect, SkRegion::Op op, ClipEdgeStyle edgeStyle) {
    SkRRect transformedRRect;
    if (rrect.transform(fMCRec->fMatrix, &transformedRRect)) {
        AutoValidateClip avc(this);

        fDeviceCMDirty = true;
        fCachedLocalClipBoundsDirty = true;
        if (!fAllowSoftClip) {
            edgeStyle = kHard_ClipEdgeStyle;
        }

        fClipStack->clipDevRRect(transformedRRect, op, kSoft_ClipEdgeStyle == edgeStyle);

        SkPath devPath;
        devPath.addRRect(transformedRRect);

        rasterclip_path(&fMCRec->fRasterClip, this, devPath, op, kSoft_ClipEdgeStyle == edgeStyle);
        return;
    }

    SkPath path;
    path.addRRect(rrect);
    // call the non-virtual version
    this->SkCanvas::onClipPath(path, op, edgeStyle);
}

void SkCanvas::clipPath(const SkPath& path, SkRegion::Op op, bool doAA) {
    this->checkForDeferredSave();
    ClipEdgeStyle edgeStyle = doAA ? kSoft_ClipEdgeStyle : kHard_ClipEdgeStyle;
    SkRect r;
    if (!path.isInverseFillType() && path.isRect(&r)) {
        this->onClipRect(r, op, edgeStyle);
    } else {
        this->onClipPath(path, op, edgeStyle);
    }
}

void SkCanvas::onClipPath(const SkPath& path, SkRegion::Op op, ClipEdgeStyle edgeStyle) {
#ifdef SK_ENABLE_CLIP_QUICKREJECT
    if (SkRegion::kIntersect_Op == op && !path.isInverseFillType()) {
        if (fMCRec->fRasterClip.isEmpty()) {
            return false;
        }

        if (this->quickReject(path.getBounds())) {
            fDeviceCMDirty = true;
            fCachedLocalClipBoundsDirty = true;

            fClipStack->clipEmpty();
            return fMCRec->fRasterClip.setEmpty();
        }
    }
#endif

    AutoValidateClip avc(this);

    fDeviceCMDirty = true;
    fCachedLocalClipBoundsDirty = true;
    if (!fAllowSoftClip) {
        edgeStyle = kHard_ClipEdgeStyle;
    }

    SkPath devPath;
    path.transform(fMCRec->fMatrix, &devPath);

    // Check if the transfomation, or the original path itself
    // made us empty. Note this can also happen if we contained NaN
    // values. computing the bounds detects this, and will set our
    // bounds to empty if that is the case. (see SkRect::set(pts, count))
    if (devPath.getBounds().isEmpty()) {
        // resetting the path will remove any NaN or other wanky values
        // that might upset our scan converter.
        devPath.reset();
    }

    // if we called path.swap() we could avoid a deep copy of this path
    fClipStack->clipDevPath(devPath, op, kSoft_ClipEdgeStyle == edgeStyle);

    if (fAllowSimplifyClip) {
        bool clipIsAA = getClipStack()->asPath(&devPath);
        if (clipIsAA) {
            edgeStyle = kSoft_ClipEdgeStyle;
        }

        op = SkRegion::kReplace_Op;
    }

    rasterclip_path(&fMCRec->fRasterClip, this, devPath, op, edgeStyle);
}

void SkCanvas::clipRegion(const SkRegion& rgn, SkRegion::Op op) {
    this->checkForDeferredSave();
    this->onClipRegion(rgn, op);
}

void SkCanvas::onClipRegion(const SkRegion& rgn, SkRegion::Op op) {
    AutoValidateClip avc(this);

    fDeviceCMDirty = true;
    fCachedLocalClipBoundsDirty = true;

    // todo: signal fClipStack that we have a region, and therefore (I guess)
    // we have to ignore it, and use the region directly?
    fClipStack->clipDevRect(rgn.getBounds(), op);

    fMCRec->fRasterClip.op(rgn, op);
}

#ifdef SK_DEBUG
void SkCanvas::validateClip() const {
    // construct clipRgn from the clipstack
    const SkBaseDevice* device = this->getDevice();
    if (!device) {
        SkASSERT(this->isClipEmpty());
        return;
    }

    SkIRect ir;
    ir.set(0, 0, device->width(), device->height());
    SkRasterClip tmpClip(ir, fConservativeRasterClip);

    SkClipStack::B2TIter                iter(*fClipStack);
    const SkClipStack::Element* element;
    while ((element = iter.next()) != NULL) {
        switch (element->getType()) {
            case SkClipStack::Element::kRect_Type:
                element->getRect().round(&ir);
                tmpClip.op(ir, element->getOp());
                break;
            case SkClipStack::Element::kEmpty_Type:
                tmpClip.setEmpty();
                break;
            default: {
                SkPath path;
                element->asPath(&path);
                rasterclip_path(&tmpClip, this, path, element->getOp(), element->isAA());
                break;
            }
        }
    }
}
#endif

void SkCanvas::replayClips(ClipVisitor* visitor) const {
    SkClipStack::B2TIter                iter(*fClipStack);
    const SkClipStack::Element*         element;

    while ((element = iter.next()) != NULL) {
        element->replay(visitor);
    }
}

///////////////////////////////////////////////////////////////////////////////

bool SkCanvas::isClipEmpty() const {
    return fMCRec->fRasterClip.isEmpty();
}

bool SkCanvas::isClipRect() const {
    return fMCRec->fRasterClip.isRect();
}

bool SkCanvas::quickReject(const SkRect& rect) const {
    if (!rect.isFinite())
        return true;

    if (fMCRec->fRasterClip.isEmpty()) {
        return true;
    }

    if (fMCRec->fMatrix.hasPerspective()) {
        SkRect dst;
        fMCRec->fMatrix.mapRect(&dst, rect);
        return !SkIRect::Intersects(dst.roundOut(), fMCRec->fRasterClip.getBounds());
    } else {
        const SkRect& clipR = this->getLocalClipBounds();

        // for speed, do the most likely reject compares first
        // TODO: should we use | instead, or compare all 4 at once?
        if (rect.fTop >= clipR.fBottom || rect.fBottom <= clipR.fTop) {
            return true;
        }
        if (rect.fLeft >= clipR.fRight || rect.fRight <= clipR.fLeft) {
            return true;
        }
        return false;
    }
}

bool SkCanvas::quickReject(const SkPath& path) const {
    return path.isEmpty() || this->quickReject(path.getBounds());
}

bool SkCanvas::getClipBounds(SkRect* bounds) const {
    SkIRect ibounds;
    if (!this->getClipDeviceBounds(&ibounds)) {
        return false;
    }

    SkMatrix inverse;
    // if we can't invert the CTM, we can't return local clip bounds
    if (!fMCRec->fMatrix.invert(&inverse)) {
        if (bounds) {
            bounds->setEmpty();
        }
        return false;
    }

    if (bounds) {
        SkRect r;
        // adjust it outwards in case we are antialiasing
        const int inset = 1;

        r.iset(ibounds.fLeft - inset, ibounds.fTop - inset,
               ibounds.fRight + inset, ibounds.fBottom + inset);
        inverse.mapRect(bounds, r);
    }
    return true;
}

bool SkCanvas::getClipDeviceBounds(SkIRect* bounds) const {
    const SkRasterClip& clip = fMCRec->fRasterClip;
    if (clip.isEmpty()) {
        if (bounds) {
            bounds->setEmpty();
        }
        return false;
    }

    if (bounds) {
        *bounds = clip.getBounds();
    }
    return true;
}

const SkMatrix& SkCanvas::getTotalMatrix() const {
    return fMCRec->fMatrix;
}

const SkRegion& SkCanvas::internal_private_getTotalClip() const {
    return fMCRec->fRasterClip.forceGetBW();
}

GrRenderTarget* SkCanvas::internal_private_accessTopLayerRenderTarget() {
    SkBaseDevice* dev = this->getTopDevice();
    return dev ? dev->accessRenderTarget() : NULL;
}

GrContext* SkCanvas::getGrContext() {
#if SK_SUPPORT_GPU
    SkBaseDevice* device = this->getTopDevice();
    if (device) {
        GrRenderTarget* renderTarget = device->accessRenderTarget();
        if (renderTarget) {
            return renderTarget->getContext();
        }
    }
#endif

    return NULL;

}

void SkCanvas::drawDRRect(const SkRRect& outer, const SkRRect& inner,
                          const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawDRRect()");
    if (outer.isEmpty()) {
        return;
    }
    if (inner.isEmpty()) {
        this->drawRRect(outer, paint);
        return;
    }

    // We don't have this method (yet), but technically this is what we should
    // be able to assert...
    // SkASSERT(outer.contains(inner));
    //
    // For now at least check for containment of bounds
    SkASSERT(outer.getBounds().contains(inner.getBounds()));

    this->onDrawDRRect(outer, inner, paint);
}

// These need to stop being virtual -- clients need to override the onDraw... versions

void SkCanvas::drawPaint(const SkPaint& paint) {
    this->onDrawPaint(paint);
}

void SkCanvas::drawRect(const SkRect& r, const SkPaint& paint) {
    this->onDrawRect(r, paint);
}

void SkCanvas::drawOval(const SkRect& r, const SkPaint& paint) {
    this->onDrawOval(r, paint);
}

void SkCanvas::drawRRect(const SkRRect& rrect, const SkPaint& paint) {
    this->onDrawRRect(rrect, paint);
}

void SkCanvas::drawPoints(PointMode mode, size_t count, const SkPoint pts[], const SkPaint& paint) {
    this->onDrawPoints(mode, count, pts, paint);
}

void SkCanvas::drawVertices(VertexMode vmode, int vertexCount, const SkPoint vertices[],
                            const SkPoint texs[], const SkColor colors[], SkXfermode* xmode,
                            const uint16_t indices[], int indexCount, const SkPaint& paint) {
    this->onDrawVertices(vmode, vertexCount, vertices, texs, colors, xmode,
                         indices, indexCount, paint);
}

void SkCanvas::drawPath(const SkPath& path, const SkPaint& paint) {
    this->onDrawPath(path, paint);
}

void SkCanvas::drawImage(const SkImage* image, SkScalar x, SkScalar y, const SkPaint* paint) {
    this->onDrawImage(image, x, y, paint);
}

void SkCanvas::drawImageRect(const SkImage* image, const SkRect* src, const SkRect& dst,
                             const SkPaint* paint) {
    if (dst.isEmpty()) {
        return;
    }
    this->onDrawImageRect(image, src, dst, paint);
}

void SkCanvas::drawImageNine(const SkImage* image, const SkIRect& center, const SkRect& dst,
                             const SkPaint* paint) {
    if (dst.isEmpty()) {
        return;
    }
    if (!SkNinePatchIter::Valid(image->width(), image->height(), center)) {
        this->drawImageRect(image, NULL, dst, paint);
    }
    this->onDrawImageNine(image, center, dst, paint);
}

void SkCanvas::drawBitmap(const SkBitmap& bitmap, SkScalar dx, SkScalar dy, const SkPaint* paint) {
    if (bitmap.drawsNothing()) {
        return;
    }
    this->onDrawBitmap(bitmap, dx, dy, paint);
}

void SkCanvas::drawBitmapRectToRect(const SkBitmap& bitmap, const SkRect* src, const SkRect& dst,
                                    const SkPaint* paint, DrawBitmapRectFlags flags) {
    if (bitmap.drawsNothing() || dst.isEmpty()) {
        return;
    }
    this->onDrawBitmapRect(bitmap, src, dst, paint, flags);
}

void SkCanvas::drawBitmapNine(const SkBitmap& bitmap, const SkIRect& center, const SkRect& dst,
                              const SkPaint* paint) {
    if (bitmap.drawsNothing() || dst.isEmpty()) {
        return;
    }
    if (!SkNinePatchIter::Valid(bitmap.width(), bitmap.height(), center)) {
        this->drawBitmapRectToRect(bitmap, NULL, dst, paint);
    }
    this->onDrawBitmapNine(bitmap, center, dst, paint);
}

void SkCanvas::drawSprite(const SkBitmap& bitmap, int left, int top, const SkPaint* paint) {
    if (bitmap.drawsNothing()) {
        return;
    }
    this->onDrawSprite(bitmap, left, top, paint);
}

void SkCanvas::drawAtlas(const SkImage* atlas, const SkRSXform xform[], const SkRect tex[],
                         const SkColor colors[], int count, SkXfermode::Mode mode,
                         const SkRect* cull, const SkPaint* paint) {
    if (count <= 0) {
        return;
    }
    SkASSERT(atlas);
    SkASSERT(xform);
    SkASSERT(tex);
    this->onDrawAtlas(atlas, xform, tex, colors, count, mode, cull, paint);
}

//////////////////////////////////////////////////////////////////////////////
//  These are the virtual drawing methods
//////////////////////////////////////////////////////////////////////////////

void SkCanvas::onDiscard() {
    if (fSurfaceBase) {
        fSurfaceBase->aboutToDraw(SkSurface::kDiscard_ContentChangeMode);
    }
}

void SkCanvas::onDrawPaint(const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawPaint()");
    this->internalDrawPaint(paint);
}

void SkCanvas::internalDrawPaint(const SkPaint& paint) {
    LOOPER_BEGIN(paint, SkDrawFilter::kPaint_Type, NULL)

    while (iter.next()) {
        iter.fDevice->drawPaint(iter, looper.paint());
    }

    LOOPER_END
}

void SkCanvas::onDrawPoints(PointMode mode, size_t count, const SkPoint pts[],
                            const SkPaint& paint) {
    TRACE_EVENT1("disabled-by-default-skia", "SkCanvas::drawPoints()", "count", static_cast<uint64_t>(count));
    if ((long)count <= 0) {
        return;
    }

    SkRect r, storage;
    const SkRect* bounds = NULL;
    if (paint.canComputeFastBounds()) {
        // special-case 2 points (common for drawing a single line)
        if (2 == count) {
            r.set(pts[0], pts[1]);
        } else {
            r.set(pts, SkToInt(count));
        }
        bounds = &paint.computeFastStrokeBounds(r, &storage);
        if (this->quickReject(*bounds)) {
            return;
        }
    }

    SkASSERT(pts != NULL);

    LOOPER_BEGIN(paint, SkDrawFilter::kPoint_Type, bounds)

    while (iter.next()) {
        iter.fDevice->drawPoints(iter, mode, count, pts, looper.paint());
    }

    LOOPER_END
}

void SkCanvas::onDrawRect(const SkRect& r, const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawRect()");
    SkRect storage;
    const SkRect* bounds = NULL;
    if (paint.canComputeFastBounds()) {
        // Skia will draw an inverted rect, because it explicitly "sorts" it downstream.
        // To prevent accidental rejecting at this stage, we have to sort it before we check.
        SkRect tmp(r);
        tmp.sort();

        bounds = &paint.computeFastBounds(tmp, &storage);
        if (this->quickReject(*bounds)) {
            return;
        }
    }

    LOOPER_BEGIN(paint, SkDrawFilter::kRect_Type, bounds)

    while (iter.next()) {
        iter.fDevice->drawRect(iter, r, looper.paint());
    }

    LOOPER_END
}

void SkCanvas::onDrawOval(const SkRect& oval, const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawOval()");
    SkRect storage;
    const SkRect* bounds = NULL;
    if (paint.canComputeFastBounds()) {
        bounds = &paint.computeFastBounds(oval, &storage);
        if (this->quickReject(*bounds)) {
            return;
        }
    }

    LOOPER_BEGIN(paint, SkDrawFilter::kOval_Type, bounds)

    while (iter.next()) {
        iter.fDevice->drawOval(iter, oval, looper.paint());
    }

    LOOPER_END
}

void SkCanvas::onDrawRRect(const SkRRect& rrect, const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawRRect()");
    SkRect storage;
    const SkRect* bounds = NULL;
    if (paint.canComputeFastBounds()) {
        bounds = &paint.computeFastBounds(rrect.getBounds(), &storage);
        if (this->quickReject(*bounds)) {
            return;
        }
    }

    if (rrect.isRect()) {
        // call the non-virtual version
        this->SkCanvas::drawRect(rrect.getBounds(), paint);
        return;
    } else if (rrect.isOval()) {
        // call the non-virtual version
        this->SkCanvas::drawOval(rrect.getBounds(), paint);
        return;
    }

    LOOPER_BEGIN(paint, SkDrawFilter::kRRect_Type, bounds)

    while (iter.next()) {
        iter.fDevice->drawRRect(iter, rrect, looper.paint());
    }

    LOOPER_END
}

void SkCanvas::onDrawDRRect(const SkRRect& outer, const SkRRect& inner,
                            const SkPaint& paint) {
    SkRect storage;
    const SkRect* bounds = NULL;
    if (paint.canComputeFastBounds()) {
        bounds = &paint.computeFastBounds(outer.getBounds(), &storage);
        if (this->quickReject(*bounds)) {
            return;
        }
    }

    LOOPER_BEGIN(paint, SkDrawFilter::kRRect_Type, bounds)

    while (iter.next()) {
        iter.fDevice->drawDRRect(iter, outer, inner, looper.paint());
    }

    LOOPER_END
}

void SkCanvas::onDrawPath(const SkPath& path, const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawPath()");
    if (!path.isFinite()) {
        return;
    }

    SkRect storage;
    const SkRect* bounds = NULL;
    if (!path.isInverseFillType() && paint.canComputeFastBounds()) {
        const SkRect& pathBounds = path.getBounds();
        bounds = &paint.computeFastBounds(pathBounds, &storage);
        if (this->quickReject(*bounds)) {
            return;
        }
    }

    const SkRect& r = path.getBounds();
    if (r.width() <= 0 && r.height() <= 0) {
        if (path.isInverseFillType()) {
            this->internalDrawPaint(paint);
        }
        return;
    }

    LOOPER_BEGIN(paint, SkDrawFilter::kPath_Type, bounds)

    while (iter.next()) {
        iter.fDevice->drawPath(iter, path, looper.paint());
    }

    LOOPER_END
}

void SkCanvas::onDrawImage(const SkImage* image, SkScalar x, SkScalar y, const SkPaint* paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawImage()");
    SkRect bounds = SkRect::MakeXYWH(x, y,
                                     SkIntToScalar(image->width()), SkIntToScalar(image->height()));
    if (NULL == paint || paint->canComputeFastBounds()) {
        if (paint) {
            paint->computeFastBounds(bounds, &bounds);
        }
        if (this->quickReject(bounds)) {
            return;
        }
    }
    
    SkLazyPaint lazy;
    if (NULL == paint) {
        paint = lazy.init();
    }
    
    LOOPER_BEGIN(*paint, SkDrawFilter::kBitmap_Type, &bounds)
    
    while (iter.next()) {
        iter.fDevice->drawImage(iter, image, x, y, looper.paint());
    }
    
    LOOPER_END
}

void SkCanvas::onDrawImageRect(const SkImage* image, const SkRect* src, const SkRect& dst,
                               const SkPaint* paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawImageRect()");
    SkRect storage;
    const SkRect* bounds = &dst;
    if (NULL == paint || paint->canComputeFastBounds()) {
        if (paint) {
            bounds = &paint->computeFastBounds(dst, &storage);
        }
        if (this->quickReject(*bounds)) {
            return;
        }
    }
    SkLazyPaint lazy;
    if (NULL == paint) {
        paint = lazy.init();
    }
    
    LOOPER_BEGIN(*paint, SkDrawFilter::kBitmap_Type, bounds)
    
    while (iter.next()) {
        iter.fDevice->drawImageRect(iter, image, src, dst, looper.paint());
    }
    
    LOOPER_END
}

void SkCanvas::onDrawBitmap(const SkBitmap& bitmap, SkScalar x, SkScalar y, const SkPaint* paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawBitmap()");
    SkDEBUGCODE(bitmap.validate();)

    if (NULL == paint || paint->canComputeFastBounds()) {
        SkRect bounds = {
            x, y,
            x + SkIntToScalar(bitmap.width()),
            y + SkIntToScalar(bitmap.height())
        };
        if (paint) {
            (void)paint->computeFastBounds(bounds, &bounds);
        }
        if (this->quickReject(bounds)) {
            return;
        }
    }

    SkMatrix matrix;
    matrix.setTranslate(x, y);
    this->internalDrawBitmap(bitmap, matrix, paint);
}

// this one is non-virtual, so it can be called safely by other canvas apis
void SkCanvas::internalDrawBitmapRect(const SkBitmap& bitmap, const SkRect* src,
                                      const SkRect& dst, const SkPaint* paint,
                                      DrawBitmapRectFlags flags) {
    if (bitmap.drawsNothing() || dst.isEmpty()) {
        return;
    }

    SkRect storage;
    const SkRect* bounds = &dst;
    if (NULL == paint || paint->canComputeFastBounds()) {
        if (paint) {
            bounds = &paint->computeFastBounds(dst, &storage);
        }
        if (this->quickReject(*bounds)) {
            return;
        }
    }

    SkLazyPaint lazy;
    if (NULL == paint) {
        paint = lazy.init();
    }

    LOOPER_BEGIN(*paint, SkDrawFilter::kBitmap_Type, bounds)

    while (iter.next()) {
        iter.fDevice->drawBitmapRect(iter, bitmap, src, dst, looper.paint(), flags);
    }

    LOOPER_END
}

void SkCanvas::onDrawBitmapRect(const SkBitmap& bitmap, const SkRect* src, const SkRect& dst,
                                const SkPaint* paint, DrawBitmapRectFlags flags) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawBitmapRectToRect()");
    SkDEBUGCODE(bitmap.validate();)
    this->internalDrawBitmapRect(bitmap, src, dst, paint, flags);
}

void SkCanvas::onDrawImageNine(const SkImage* image, const SkIRect& center, const SkRect& dst,
                               const SkPaint* paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawImageNine()");
    
    SkRect storage;
    const SkRect* bounds = &dst;
    if (NULL == paint || paint->canComputeFastBounds()) {
        if (paint) {
            bounds = &paint->computeFastBounds(dst, &storage);
        }
        if (this->quickReject(*bounds)) {
            return;
        }
    }
    
    SkLazyPaint lazy;
    if (NULL == paint) {
        paint = lazy.init();
    }
    
    LOOPER_BEGIN(*paint, SkDrawFilter::kBitmap_Type, bounds)
    
    while (iter.next()) {
        iter.fDevice->drawImageNine(iter, image, center, dst, looper.paint());
    }
    
    LOOPER_END
}

void SkCanvas::onDrawBitmapNine(const SkBitmap& bitmap, const SkIRect& center, const SkRect& dst,
                                const SkPaint* paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawBitmapNine()");
    SkDEBUGCODE(bitmap.validate();)

    SkRect storage;
    const SkRect* bounds = &dst;
    if (NULL == paint || paint->canComputeFastBounds()) {
        if (paint) {
            bounds = &paint->computeFastBounds(dst, &storage);
        }
        if (this->quickReject(*bounds)) {
            return;
        }
    }
    
    SkLazyPaint lazy;
    if (NULL == paint) {
        paint = lazy.init();
    }
    
    LOOPER_BEGIN(*paint, SkDrawFilter::kBitmap_Type, bounds)
    
    while (iter.next()) {
        iter.fDevice->drawBitmapNine(iter, bitmap, center, dst, looper.paint());
    }
    
    LOOPER_END
}

class SkDeviceFilteredPaint {
public:
    SkDeviceFilteredPaint(SkBaseDevice* device, const SkPaint& paint) {
        uint32_t filteredFlags = device->filterTextFlags(paint);
        if (filteredFlags != paint.getFlags()) {
            SkPaint* newPaint = fLazy.set(paint);
            newPaint->setFlags(filteredFlags);
            fPaint = newPaint;
        } else {
            fPaint = &paint;
        }
    }

    const SkPaint& paint() const { return *fPaint; }

private:
    const SkPaint*  fPaint;
    SkLazyPaint     fLazy;
};

void SkCanvas::DrawRect(const SkDraw& draw, const SkPaint& paint,
                        const SkRect& r, SkScalar textSize) {
    if (paint.getStyle() == SkPaint::kFill_Style) {
        draw.fDevice->drawRect(draw, r, paint);
    } else {
        SkPaint p(paint);
        p.setStrokeWidth(SkScalarMul(textSize, paint.getStrokeWidth()));
        draw.fDevice->drawRect(draw, r, p);
    }
}

void SkCanvas::DrawTextDecorations(const SkDraw& draw, const SkPaint& paint,
                                   const char text[], size_t byteLength,
                                   SkScalar x, SkScalar y) {
    SkASSERT(byteLength == 0 || text != NULL);

    // nothing to draw
    if (text == NULL || byteLength == 0 ||
        draw.fClip->isEmpty() ||
        (paint.getAlpha() == 0 && paint.getXfermode() == NULL)) {
        return;
    }

    SkScalar    width = 0;
    SkPoint     start;

    start.set(0, 0);    // to avoid warning
    if (paint.getFlags() & (SkPaint::kUnderlineText_Flag |
                            SkPaint::kStrikeThruText_Flag)) {
        width = paint.measureText(text, byteLength);

        SkScalar offsetX = 0;
        if (paint.getTextAlign() == SkPaint::kCenter_Align) {
            offsetX = SkScalarHalf(width);
        } else if (paint.getTextAlign() == SkPaint::kRight_Align) {
            offsetX = width;
        }
        start.set(x - offsetX, y);
    }

    if (0 == width) {
        return;
    }

    uint32_t flags = paint.getFlags();

    if (flags & (SkPaint::kUnderlineText_Flag |
                 SkPaint::kStrikeThruText_Flag)) {
        SkScalar textSize = paint.getTextSize();
        SkScalar height = SkScalarMul(textSize, kStdUnderline_Thickness);
        SkRect   r;

        r.fLeft = start.fX;
        r.fRight = start.fX + width;

        if (flags & SkPaint::kUnderlineText_Flag) {
            SkScalar offset = SkScalarMulAdd(textSize, kStdUnderline_Offset,
                                             start.fY);
            r.fTop = offset;
            r.fBottom = offset + height;
            DrawRect(draw, paint, r, textSize);
        }
        if (flags & SkPaint::kStrikeThruText_Flag) {
            SkScalar offset = SkScalarMulAdd(textSize, kStdStrikeThru_Offset,
                                             start.fY);
            r.fTop = offset;
            r.fBottom = offset + height;
            DrawRect(draw, paint, r, textSize);
        }
    }
}

void SkCanvas::onDrawText(const void* text, size_t byteLength, SkScalar x, SkScalar y,
                          const SkPaint& paint) {
    LOOPER_BEGIN(paint, SkDrawFilter::kText_Type, NULL)

    while (iter.next()) {
        SkDeviceFilteredPaint dfp(iter.fDevice, looper.paint());
        iter.fDevice->drawText(iter, text, byteLength, x, y, dfp.paint());
        DrawTextDecorations(iter, dfp.paint(),
                            static_cast<const char*>(text), byteLength, x, y);
    }

    LOOPER_END
}

void SkCanvas::onDrawPosText(const void* text, size_t byteLength, const SkPoint pos[],
                             const SkPaint& paint) {
    SkPoint textOffset = SkPoint::Make(0, 0);

    LOOPER_BEGIN(paint, SkDrawFilter::kText_Type, NULL)

    while (iter.next()) {
        SkDeviceFilteredPaint dfp(iter.fDevice, looper.paint());
        iter.fDevice->drawPosText(iter, text, byteLength, &pos->fX, 2, textOffset,
                                  dfp.paint());
    }

    LOOPER_END
}

void SkCanvas::onDrawPosTextH(const void* text, size_t byteLength, const SkScalar xpos[],
                              SkScalar constY, const SkPaint& paint) {

    SkPoint textOffset = SkPoint::Make(0, constY);

    LOOPER_BEGIN(paint, SkDrawFilter::kText_Type, NULL)

    while (iter.next()) {
        SkDeviceFilteredPaint dfp(iter.fDevice, looper.paint());
        iter.fDevice->drawPosText(iter, text, byteLength, xpos, 1, textOffset,
                                  dfp.paint());
    }

    LOOPER_END
}

void SkCanvas::onDrawTextOnPath(const void* text, size_t byteLength, const SkPath& path,
                                const SkMatrix* matrix, const SkPaint& paint) {
    LOOPER_BEGIN(paint, SkDrawFilter::kText_Type, NULL)

    while (iter.next()) {
        iter.fDevice->drawTextOnPath(iter, text, byteLength, path,
                                     matrix, looper.paint());
    }

    LOOPER_END
}

void SkCanvas::onDrawTextBlob(const SkTextBlob* blob, SkScalar x, SkScalar y,
                              const SkPaint& paint) {

    SkRect storage;
    const SkRect* bounds = NULL;
    if (paint.canComputeFastBounds()) {
        storage = blob->bounds().makeOffset(x, y);
        bounds = &paint.computeFastBounds(storage, &storage);

        if (this->quickReject(*bounds)) {
            return;
        }
    }

    // We cannot filter in the looper as we normally do, because the paint is
    // incomplete at this point (text-related attributes are embedded within blob run paints).
    SkDrawFilter* drawFilter = fMCRec->fFilter;
    fMCRec->fFilter = NULL;

    LOOPER_BEGIN(paint, SkDrawFilter::kText_Type, bounds)

    while (iter.next()) {
        SkDeviceFilteredPaint dfp(iter.fDevice, looper.paint());
        iter.fDevice->drawTextBlob(iter, blob, x, y, dfp.paint(), drawFilter);
    }

    LOOPER_END

    fMCRec->fFilter = drawFilter;
}

// These will become non-virtual, so they always call the (virtual) onDraw... method
void SkCanvas::drawText(const void* text, size_t byteLength, SkScalar x, SkScalar y,
                        const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawText()");
    this->onDrawText(text, byteLength, x, y, paint);
}
void SkCanvas::drawPosText(const void* text, size_t byteLength, const SkPoint pos[],
                           const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawPosText()");
    this->onDrawPosText(text, byteLength, pos, paint);
}
void SkCanvas::drawPosTextH(const void* text, size_t byteLength, const SkScalar xpos[],
                            SkScalar constY, const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawPosTextH()");
    this->onDrawPosTextH(text, byteLength, xpos, constY, paint);
}
void SkCanvas::drawTextOnPath(const void* text, size_t byteLength, const SkPath& path,
                              const SkMatrix* matrix, const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawTextOnPath()");
    this->onDrawTextOnPath(text, byteLength, path, matrix, paint);
}
void SkCanvas::drawTextBlob(const SkTextBlob* blob, SkScalar x, SkScalar y,
                            const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawTextBlob()");
    if (blob) {
        this->onDrawTextBlob(blob, x, y, paint);
    }
}

void SkCanvas::onDrawVertices(VertexMode vmode, int vertexCount,
                              const SkPoint verts[], const SkPoint texs[],
                              const SkColor colors[], SkXfermode* xmode,
                              const uint16_t indices[], int indexCount,
                              const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawVertices()");
    LOOPER_BEGIN(paint, SkDrawFilter::kPath_Type, NULL)

    while (iter.next()) {
        iter.fDevice->drawVertices(iter, vmode, vertexCount, verts, texs,
                                   colors, xmode, indices, indexCount,
                                   looper.paint());
    }

    LOOPER_END
}

void SkCanvas::drawPatch(const SkPoint cubics[12], const SkColor colors[4],
                         const SkPoint texCoords[4], SkXfermode* xmode, const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawPatch()");
    if (NULL == cubics) {
        return;
    }

    // Since a patch is always within the convex hull of the control points, we discard it when its
    // bounding rectangle is completely outside the current clip.
    SkRect bounds;
    bounds.set(cubics, SkPatchUtils::kNumCtrlPts);
    if (this->quickReject(bounds)) {
        return;
    }

    this->onDrawPatch(cubics, colors, texCoords, xmode, paint);
}

void SkCanvas::onDrawPatch(const SkPoint cubics[12], const SkColor colors[4],
                           const SkPoint texCoords[4], SkXfermode* xmode, const SkPaint& paint) {

    LOOPER_BEGIN(paint, SkDrawFilter::kPath_Type, NULL)

    while (iter.next()) {
        iter.fDevice->drawPatch(iter, cubics, colors, texCoords, xmode, paint);
    }

    LOOPER_END
}

void SkCanvas::drawDrawable(SkDrawable* dr, SkScalar x, SkScalar y) {
    if (dr) {
        if (x || y) {
            SkMatrix matrix = SkMatrix::MakeTrans(x, y);
            this->onDrawDrawable(dr, &matrix);
        } else {
            this->onDrawDrawable(dr, NULL);
        }
    }
}

void SkCanvas::drawDrawable(SkDrawable* dr, const SkMatrix* matrix) {
    if (dr) {
        if (matrix && matrix->isIdentity()) {
            matrix = NULL;
        }
        this->onDrawDrawable(dr, matrix);
    }
}

void SkCanvas::onDrawDrawable(SkDrawable* dr, const SkMatrix* matrix) {
    SkRect bounds = dr->getBounds();
    if (matrix) {
        matrix->mapRect(&bounds);
    }
    if (this->quickReject(bounds)) {
        return;
    }
    dr->draw(this, matrix);
}

void SkCanvas::onDrawAtlas(const SkImage* atlas, const SkRSXform xform[], const SkRect tex[],
                           const SkColor colors[], int count, SkXfermode::Mode mode,
                           const SkRect* cull, const SkPaint* paint) {
    if (cull && this->quickReject(*cull)) {
        return;
    }

    SkPaint pnt;
    if (paint) {
        pnt = *paint;
    }
    
    LOOPER_BEGIN(pnt, SkDrawFilter::kPath_Type, NULL)
    while (iter.next()) {
        iter.fDevice->drawAtlas(iter, atlas, xform, tex, colors, count, mode, pnt);
    }
    LOOPER_END
}

//////////////////////////////////////////////////////////////////////////////
// These methods are NOT virtual, and therefore must call back into virtual
// methods, rather than actually drawing themselves.
//////////////////////////////////////////////////////////////////////////////

void SkCanvas::drawARGB(U8CPU a, U8CPU r, U8CPU g, U8CPU b,
                        SkXfermode::Mode mode) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawARGB()");
    SkPaint paint;

    paint.setARGB(a, r, g, b);
    if (SkXfermode::kSrcOver_Mode != mode) {
        paint.setXfermodeMode(mode);
    }
    this->drawPaint(paint);
}

void SkCanvas::drawColor(SkColor c, SkXfermode::Mode mode) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawColor()");
    SkPaint paint;

    paint.setColor(c);
    if (SkXfermode::kSrcOver_Mode != mode) {
        paint.setXfermodeMode(mode);
    }
    this->drawPaint(paint);
}

void SkCanvas::drawPoint(SkScalar x, SkScalar y, const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawPoint(SkPaint)");
    SkPoint pt;

    pt.set(x, y);
    this->drawPoints(kPoints_PointMode, 1, &pt, paint);
}

void SkCanvas::drawPoint(SkScalar x, SkScalar y, SkColor color) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawPoint(SkColor)");
    SkPoint pt;
    SkPaint paint;

    pt.set(x, y);
    paint.setColor(color);
    this->drawPoints(kPoints_PointMode, 1, &pt, paint);
}

void SkCanvas::drawLine(SkScalar x0, SkScalar y0, SkScalar x1, SkScalar y1,
                        const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawLine()");
    SkPoint pts[2];

    pts[0].set(x0, y0);
    pts[1].set(x1, y1);
    this->drawPoints(kLines_PointMode, 2, pts, paint);
}

void SkCanvas::drawRectCoords(SkScalar left, SkScalar top,
                              SkScalar right, SkScalar bottom,
                              const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawRectCoords()");
    SkRect  r;

    r.set(left, top, right, bottom);
    this->drawRect(r, paint);
}

void SkCanvas::drawCircle(SkScalar cx, SkScalar cy, SkScalar radius,
                          const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawCircle()");
    if (radius < 0) {
        radius = 0;
    }

    SkRect  r;
    r.set(cx - radius, cy - radius, cx + radius, cy + radius);
    this->drawOval(r, paint);
}

void SkCanvas::drawRoundRect(const SkRect& r, SkScalar rx, SkScalar ry,
                             const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawRoundRect()");
    if (rx > 0 && ry > 0) {
        if (paint.canComputeFastBounds()) {
            SkRect storage;
            if (this->quickReject(paint.computeFastBounds(r, &storage))) {
                return;
            }
        }
        SkRRect rrect;
        rrect.setRectXY(r, rx, ry);
        this->drawRRect(rrect, paint);
    } else {
        this->drawRect(r, paint);
    }
}

void SkCanvas::drawArc(const SkRect& oval, SkScalar startAngle,
                       SkScalar sweepAngle, bool useCenter,
                       const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawArc()");
    if (SkScalarAbs(sweepAngle) >= SkIntToScalar(360)) {
        this->drawOval(oval, paint);
    } else {
        SkPath  path;
        if (useCenter) {
            path.moveTo(oval.centerX(), oval.centerY());
        }
        path.arcTo(oval, startAngle, sweepAngle, !useCenter);
        if (useCenter) {
            path.close();
        }
        this->drawPath(path, paint);
    }
}

void SkCanvas::drawTextOnPathHV(const void* text, size_t byteLength,
                                const SkPath& path, SkScalar hOffset,
                                SkScalar vOffset, const SkPaint& paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawTextOnPathHV()");
    SkMatrix    matrix;

    matrix.setTranslate(hOffset, vOffset);
    this->drawTextOnPath(text, byteLength, path, &matrix, paint);
}

///////////////////////////////////////////////////////////////////////////////

/**
 *  This constant is trying to balance the speed of ref'ing a subpicture into a parent picture,
 *  against the playback cost of recursing into the subpicture to get at its actual ops.
 *
 *  For now we pick a conservatively small value, though measurement (and other heuristics like
 *  the type of ops contained) may justify changing this value.
 */
#define kMaxPictureOpsToUnrollInsteadOfRef  1

void SkCanvas::drawPicture(const SkPicture* picture, const SkMatrix* matrix, const SkPaint* paint) {
    TRACE_EVENT0("disabled-by-default-skia", "SkCanvas::drawPicture()");
    if (picture) {
        if (matrix && matrix->isIdentity()) {
            matrix = NULL;
        }
        if (picture->approximateOpCount() <= kMaxPictureOpsToUnrollInsteadOfRef) {
            SkAutoCanvasMatrixPaint acmp(this, matrix, paint, picture->cullRect());
            picture->playback(this);
        } else {
            this->onDrawPicture(picture, matrix, paint);
        }
    }
}

void SkCanvas::onDrawPicture(const SkPicture* picture, const SkMatrix* matrix,
                             const SkPaint* paint) {
    SkBaseDevice* device = this->getTopDevice();
    if (device) {
        // Canvas has to first give the device the opportunity to render
        // the picture itself.
        if (device->EXPERIMENTAL_drawPicture(this, picture, matrix, paint)) {
            return; // the device has rendered the entire picture
        }
    }

    SkAutoCanvasMatrixPaint acmp(this, matrix, paint, picture->cullRect());
    picture->playback(this);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

SkCanvas::LayerIter::LayerIter(SkCanvas* canvas, bool skipEmptyClips) {
    SK_COMPILE_ASSERT(sizeof(fStorage) >= sizeof(SkDrawIter), fStorage_too_small);

    SkASSERT(canvas);

    fImpl = new (fStorage) SkDrawIter(canvas, skipEmptyClips);
    fDone = !fImpl->next();
}

SkCanvas::LayerIter::~LayerIter() {
    fImpl->~SkDrawIter();
}

void SkCanvas::LayerIter::next() {
    fDone = !fImpl->next();
}

SkBaseDevice* SkCanvas::LayerIter::device() const {
    return fImpl->getDevice();
}

const SkMatrix& SkCanvas::LayerIter::matrix() const {
    return fImpl->getMatrix();
}

const SkPaint& SkCanvas::LayerIter::paint() const {
    const SkPaint* paint = fImpl->getPaint();
    if (NULL == paint) {
        paint = &fDefaultPaint;
    }
    return *paint;
}

const SkRegion& SkCanvas::LayerIter::clip() const { return fImpl->getClip(); }
int SkCanvas::LayerIter::x() const { return fImpl->getX(); }
int SkCanvas::LayerIter::y() const { return fImpl->getY(); }

///////////////////////////////////////////////////////////////////////////////

SkCanvasClipVisitor::~SkCanvasClipVisitor() { }

///////////////////////////////////////////////////////////////////////////////

static bool supported_for_raster_canvas(const SkImageInfo& info) {
    switch (info.alphaType()) {
        case kPremul_SkAlphaType:
        case kOpaque_SkAlphaType:
            break;
        default:
            return false;
    }

    switch (info.colorType()) {
        case kAlpha_8_SkColorType:
        case kRGB_565_SkColorType:
        case kN32_SkColorType:
            break;
        default:
            return false;
    }

    return true;
}

SkCanvas* SkCanvas::NewRasterDirect(const SkImageInfo& info, void* pixels, size_t rowBytes) {
    if (!supported_for_raster_canvas(info)) {
        return NULL;
    }

    SkBitmap bitmap;
    if (!bitmap.installPixels(info, pixels, rowBytes)) {
        return NULL;
    }
    return SkNEW_ARGS(SkCanvas, (bitmap));
}

///////////////////////////////////////////////////////////////////////////////

SkAutoCanvasMatrixPaint::SkAutoCanvasMatrixPaint(SkCanvas* canvas, const SkMatrix* matrix,
                                                 const SkPaint* paint, const SkRect& bounds)
    : fCanvas(canvas)
    , fSaveCount(canvas->getSaveCount())
{
    if (paint) {
        SkRect newBounds = bounds;
        if (matrix) {
            matrix->mapRect(&newBounds);
        }
        canvas->saveLayer(&newBounds, paint);
    } else if (matrix) {
        canvas->save();
    }

    if (matrix) {
        canvas->concat(*matrix);
    }
}

SkAutoCanvasMatrixPaint::~SkAutoCanvasMatrixPaint() {
    fCanvas->restoreToCount(fSaveCount);
}
