/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkImageFilter_DEFINED
#define SkImageFilter_DEFINED

#include "SkFilterQuality.h"
#include "SkFlattenable.h"
#include "SkMatrix.h"
#include "SkRect.h"
#include "SkSurfaceProps.h"
#include "SkTemplates.h"

class GrFragmentProcessor;
class GrProcessorDataManager;
class GrTexture;
class SkBaseDevice;
class SkBitmap;
class SkColorFilter;
struct SkIPoint;

/**
 *  Base class for image filters. If one is installed in the paint, then
 *  all drawing occurs as usual, but it is as if the drawing happened into an
 *  offscreen (before the xfermode is applied). This offscreen bitmap will
 *  then be handed to the imagefilter, who in turn creates a new bitmap which
 *  is what will finally be drawn to the device (using the original xfermode).
 */
class SK_API SkImageFilter : public SkFlattenable {
public:
    class CropRect {
    public:
        enum CropEdge {
            kHasLeft_CropEdge   = 0x01,
            kHasTop_CropEdge    = 0x02,
            kHasRight_CropEdge  = 0x04,
            kHasBottom_CropEdge = 0x08,
            kHasAll_CropEdge    = 0x0F,
        };
        CropRect() {}
        explicit CropRect(const SkRect& rect, uint32_t flags = kHasAll_CropEdge) : fRect(rect), fFlags(flags) {}
        uint32_t flags() const { return fFlags; }
        const SkRect& rect() const { return fRect; }
    private:
        SkRect fRect;
        uint32_t fFlags;
    };

    // This cache maps from (filter's unique ID + CTM + clipBounds + src bitmap generation ID) to
    // (result, offset).
    class Cache : public SkRefCnt {
    public:
        struct Key;
        virtual ~Cache() {}
        static Cache* Create(size_t maxBytes);
        static Cache* Get();
        virtual bool get(const Key& key, SkBitmap* result, SkIPoint* offset) const = 0;
        virtual void set(const Key& key, const SkBitmap& result, const SkIPoint& offset) = 0;
        virtual void purge() {}
    };

    class Context {
    public:
        Context(const SkMatrix& ctm, const SkIRect& clipBounds, Cache* cache) :
            fCTM(ctm), fClipBounds(clipBounds), fCache(cache) {
        }
        const SkMatrix& ctm() const { return fCTM; }
        const SkIRect& clipBounds() const { return fClipBounds; }
        Cache* cache() const { return fCache; }
    private:
        SkMatrix fCTM;
        SkIRect  fClipBounds;
        Cache*   fCache;
    };

    class Proxy {
    public:
        Proxy(SkBaseDevice* device) : fDevice(device) { }

        SkBaseDevice* createDevice(int width, int height);

        // Returns true if the proxy handled the filter itself. If this returns
        // false then the filter's code will be called.
        bool filterImage(const SkImageFilter*, const SkBitmap& src, const SkImageFilter::Context&,
                         SkBitmap* result, SkIPoint* offset);

    private:
        SkBaseDevice* fDevice;
    };


    /**
     *  Request a new (result) image to be created from the src image.
     *  If the src has no pixels (isNull()) then the request just wants to
     *  receive the config and width/height of the result.
     *
     *  The matrix is the current matrix on the canvas.
     *
     *  Offset is the amount to translate the resulting image relative to the
     *  src when it is drawn. This is an out-param.
     *
     *  If the result image cannot be created, return false, in which case both
     *  the result and offset parameters will be ignored by the caller.
     */
    bool filterImage(Proxy*, const SkBitmap& src, const Context&,
                     SkBitmap* result, SkIPoint* offset) const;

    /**
     *  Given the src bounds of an image, this returns the bounds of the result
     *  image after the filter has been applied.
     */
    bool filterBounds(const SkIRect& src, const SkMatrix& ctm, SkIRect* dst) const;

    /**
     *  Returns true if the filter can be processed on the GPU.  This is most
     *  often used for multi-pass effects, where intermediate results must be
     *  rendered to textures.  For single-pass effects, use asFragmentProcessor().
     *  The default implementation returns asFragmentProcessor(NULL, NULL, SkMatrix::I(),
     *  SkIRect()).
     */
    virtual bool canFilterImageGPU() const;

    /**
     *  Process this image filter on the GPU.  This is most often used for
     *  multi-pass effects, where intermediate results must be rendered to
     *  textures.  For single-pass effects, use asFragmentProcessor().  src is the
     *  source image for processing, as a texture-backed bitmap.  result is
     *  the destination bitmap, which should contain a texture-backed pixelref
     *  on success.  offset is the amount to translate the resulting image
     *  relative to the src when it is drawn. The default implementation does
     *  single-pass processing using asFragmentProcessor().
     */
    virtual bool filterImageGPU(Proxy*, const SkBitmap& src, const Context&,
                                SkBitmap* result, SkIPoint* offset) const;

    /**
     *  Returns whether this image filter is a color filter and puts the color filter into the
     *  "filterPtr" parameter if it can. Does nothing otherwise.
     *  If this returns false, then the filterPtr is unchanged.
     *  If this returns true, then if filterPtr is not null, it must be set to a ref'd colorfitler
     *  (i.e. it may not be set to NULL).
     */
    bool isColorFilterNode(SkColorFilter** filterPtr) const {
        return this->onIsColorFilterNode(filterPtr);
    }

    // DEPRECATED : use isColorFilterNode() instead
    bool asColorFilter(SkColorFilter** filterPtr) const {
        return this->isColorFilterNode(filterPtr);
    }

    /**
     *  Returns true (and optionally returns a ref'd filter) if this imagefilter can be completely
     *  replaced by the returned colorfilter. i.e. the two effects will affect drawing in the
     *  same way.
     */
    bool asAColorFilter(SkColorFilter** filterPtr) const {
        return this->countInputs() > 0 &&
               NULL == this->getInput(0) &&
               this->isColorFilterNode(filterPtr);
    }

    /**
     *  Returns the number of inputs this filter will accept (some inputs can
     *  be NULL).
     */
    int countInputs() const { return fInputCount; }

    /**
     *  Returns the input filter at a given index, or NULL if no input is
     *  connected.  The indices used are filter-specific.
     */
    SkImageFilter* getInput(int i) const {
        SkASSERT(i < fInputCount);
        return fInputs[i];
    }

    /**
     *  Returns whether any edges of the crop rect have been set. The crop
     *  rect is set at construction time, and determines which pixels from the
     *  input image will be processed. The size of the crop rect should be
     *  used as the size of the destination image. The origin of this rect
     *  should be used to offset access to the input images, and should also
     *  be added to the "offset" parameter in onFilterImage and
     *  filterImageGPU(). (The latter ensures that the resulting buffer is
     *  drawn in the correct location.)
     */
    bool cropRectIsSet() const { return fCropRect.flags() != 0x0; }

    CropRect getCropRect() const { return fCropRect; }

    // Default impl returns union of all input bounds.
    virtual void computeFastBounds(const SkRect&, SkRect*) const;

    /**
     * Create an SkMatrixImageFilter, which transforms its input by the given matrix.
     */
    static SkImageFilter* CreateMatrixFilter(const SkMatrix& matrix,
                                             SkFilterQuality,
                                             SkImageFilter* input = NULL);

#if SK_SUPPORT_GPU
    /**
     * Wrap the given texture in a texture-backed SkBitmap.
     */
    static void WrapTexture(GrTexture* texture, int width, int height, SkBitmap* result);

    /**
     * Recursively evaluate this filter on the GPU. If the filter has no GPU
     * implementation, it will be processed in software and uploaded to the GPU.
     */
    bool getInputResultGPU(SkImageFilter::Proxy* proxy, const SkBitmap& src, const Context&,
                           SkBitmap* result, SkIPoint* offset) const;
#endif

    SK_TO_STRING_PUREVIRT()
    SK_DEFINE_FLATTENABLE_TYPE(SkImageFilter)

protected:
    class Common {
    public:
        Common() {}
        ~Common();

        /**
         *  Attempt to unflatten the cropRect and the expected number of input filters.
         *  If any number of input filters is valid, pass -1.
         *  If this fails (i.e. corrupt buffer or contents) then return false and common will
         *  be left uninitialized.
         *  If this returns true, then inputCount() is the number of found input filters, each
         *  of which may be NULL or a valid imagefilter.
         */
        bool unflatten(SkReadBuffer&, int expectedInputs);

        const CropRect& cropRect() const { return fCropRect; }
        int             inputCount() const { return fInputs.count(); }
        SkImageFilter** inputs() const { return fInputs.get(); }

        SkImageFilter*  getInput(int index) const { return fInputs[index]; }

        // If the caller wants a copy of the inputs, call this and it will transfer ownership
        // of the unflattened input filters to the caller. This is just a short-cut for copying
        // the inputs, calling ref() on each, and then waiting for Common's destructor to call
        // unref() on each.
        void detachInputs(SkImageFilter** inputs);

    private:
        CropRect fCropRect;
        // most filters accept at most 2 input-filters
        SkAutoSTArray<2, SkImageFilter*> fInputs;

        void allocInputs(int count);
    };

    SkImageFilter(int inputCount, SkImageFilter** inputs, const CropRect* cropRect = NULL);

    virtual ~SkImageFilter();

    /**
     *  Constructs a new SkImageFilter read from an SkReadBuffer object.
     *
     *  @param inputCount    The exact number of inputs expected for this SkImageFilter object.
     *                       -1 can be used if the filter accepts any number of inputs.
     *  @param rb            SkReadBuffer object from which the SkImageFilter is read.
     */
    explicit SkImageFilter(int inputCount, SkReadBuffer& rb);

    void flatten(SkWriteBuffer&) const override;

    /**
     *  This is the virtual which should be overridden by the derived class
     *  to perform image filtering.
     *
     *  src is the original primitive bitmap. If the filter has a connected
     *  input, it should recurse on that input and use that in place of src.
     *
     *  The matrix is the current matrix on the canvas.
     *
     *  Offset is the amount to translate the resulting image relative to the
     *  src when it is drawn. This is an out-param.
     *
     *  If the result image cannot be created, this should false, in which
     *  case both the result and offset parameters will be ignored by the
     *  caller.
     */
    virtual bool onFilterImage(Proxy*, const SkBitmap& src, const Context&,
                               SkBitmap* result, SkIPoint* offset) const;
    // Given the bounds of the destination rect to be filled in device
    // coordinates (first parameter), and the CTM, compute (conservatively)
    // which rect of the source image would be required (third parameter).
    // Used for clipping and temp-buffer allocations, so the result need not
    // be exact, but should never be smaller than the real answer. The default
    // implementation recursively unions all input bounds, or returns false if
    // no inputs.
    virtual bool onFilterBounds(const SkIRect&, const SkMatrix&, SkIRect*) const;

    /**
     *  Return true (and return a ref'd colorfilter) if this node in the DAG is just a
     *  colorfilter w/o CropRect constraints.
     */
    virtual bool onIsColorFilterNode(SkColorFilter** /*filterPtr*/) const {
        return false;
    }

    /** Computes source bounds as the src bitmap bounds offset by srcOffset.
     *  Apply the transformed crop rect to the bounds if any of the
     *  corresponding edge flags are set. Intersects the result against the
     *  context's clipBounds, and returns the result in "bounds". If there is
     *  no intersection, returns false and leaves "bounds" unchanged.
     */
    bool applyCropRect(const Context&, const SkBitmap& src, const SkIPoint& srcOffset,
                       SkIRect* bounds) const;

    /** Same as the above call, except that if the resulting crop rect is not
     *  entirely contained by the source bitmap's bounds, it creates a new
     *  bitmap in "result" and pads the edges with transparent black. In that
     *  case, the srcOffset is modified to be the same as the bounds, since no
     *  further adjustment is needed by the caller. This version should only
     *  be used by filters which are not capable of processing a smaller
     *  source bitmap into a larger destination.
     */
    bool applyCropRect(const Context&, Proxy* proxy, const SkBitmap& src, SkIPoint* srcOffset,
                       SkIRect* bounds, SkBitmap* result) const;

    /**
     *  Returns true if the filter can be expressed a single-pass
     *  GrProcessor, used to process this filter on the GPU, or false if
     *  not.
     *
     *  If effect is non-NULL, a new GrProcessor instance is stored
     *  in it.  The caller assumes ownership of the stage, and it is up to the
     *  caller to unref it.
     *
     *  The effect can assume its vertexCoords space maps 1-to-1 with texels
     *  in the texture.  "matrix" is a transformation to apply to filter
     *  parameters before they are used in the effect. Note that this function
     *  will be called with (NULL, NULL, SkMatrix::I()) to query for support,
     *  so returning "true" indicates support for all possible matrices.
     */
    virtual bool asFragmentProcessor(GrFragmentProcessor**, GrProcessorDataManager*, GrTexture*,
                                     const SkMatrix&, const SkIRect& bounds) const;

private:
    friend class SkGraphics;
    static void PurgeCache();

    bool usesSrcInput() const { return fUsesSrcInput; }

    typedef SkFlattenable INHERITED;
    int fInputCount;
    SkImageFilter** fInputs;
    bool fUsesSrcInput;
    CropRect fCropRect;
    uint32_t fUniqueID; // Globally unique
};

/**
 *  Helper to unflatten the common data, and return NULL if we fail.
 */
#define SK_IMAGEFILTER_UNFLATTEN_COMMON(localVar, expectedCount)    \
    Common localVar;                                                \
    do {                                                            \
        if (!localVar.unflatten(buffer, expectedCount)) {           \
            return NULL;                                            \
        }                                                           \
    } while (0)

#endif
