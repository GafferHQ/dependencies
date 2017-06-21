/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkData.h"
#include "SkDecodingImageGenerator.h"
#include "SkImageDecoder.h"
#include "SkImageInfo.h"
#include "SkImageGenerator.h"
#include "SkImagePriv.h"
#include "SkStream.h"
#include "SkUtils.h"

namespace {
bool equal_modulo_alpha(const SkImageInfo& a, const SkImageInfo& b) {
    return a.width() == b.width() && a.height() == b.height() &&
           a.colorType() == b.colorType();
}

class DecodingImageGenerator : public SkImageGenerator {
public:
    virtual ~DecodingImageGenerator();

    SkData*                             fData;
    SkAutoTDelete<SkStreamRewindable>   fStream;
    const SkImageInfo                   fInfo;
    const int                           fSampleSize;
    const bool                          fDitherImage;

    DecodingImageGenerator(SkData* data,
                           SkStreamRewindable* stream,
                           const SkImageInfo& info,
                           int sampleSize,
                           bool ditherImage);

protected:
    SkData* onRefEncodedData() override;
#ifdef SK_LEGACY_IMAGE_GENERATOR_ENUMS_AND_OPTIONS
    Result onGetPixels(const SkImageInfo& info,
                       void* pixels, size_t rowBytes, const Options&,
                       SkPMColor ctable[], int* ctableCount) override;
#else
    bool onGetPixels(const SkImageInfo& info, void* pixels, size_t rowBytes,
                     SkPMColor ctable[], int* ctableCount) override;
#endif
    bool onGetYUV8Planes(SkISize sizes[3], void* planes[3], size_t rowBytes[3],
                         SkYUVColorSpace* colorSpace) override;

private:
    typedef SkImageGenerator INHERITED;
};

/**
 *  Special allocator used by getPixels(). Uses preallocated memory
 *  provided if possible, else fall-back on the default allocator
 */
class TargetAllocator : public SkBitmap::Allocator {
public:
    TargetAllocator(const SkImageInfo& info,
                    void* target,
                    size_t rowBytes)
        : fInfo(info)
        , fTarget(target)
        , fRowBytes(rowBytes)
    {}

    bool isReady() { return (fTarget != NULL); }

    virtual bool allocPixelRef(SkBitmap* bm, SkColorTable* ct) {
        if (NULL == fTarget || !equal_modulo_alpha(fInfo, bm->info())) {
            // Call default allocator.
            return bm->tryAllocPixels(NULL, ct);
        }

        // TODO(halcanary): verify that all callers of this function
        // will respect new RowBytes.  Will be moot once rowbytes belongs
        // to PixelRef.
        bm->installPixels(fInfo, fTarget, fRowBytes, ct, NULL, NULL);

        fTarget = NULL;  // never alloc same pixels twice!
        return true;
    }

private:
    const SkImageInfo fInfo;
    void* fTarget;  // Block of memory to be supplied as pixel memory
                    // in allocPixelRef.  Must be large enough to hold
                    // a bitmap described by fInfo and fRowBytes
    const size_t fRowBytes;  // rowbytes for the destination bitmap

    typedef SkBitmap::Allocator INHERITED;
};

// TODO(halcanary): Give this macro a better name and move it into SkTypes.h
#ifdef SK_DEBUG
    #define SkCheckResult(expr, value)  SkASSERT((value) == (expr))
#else
    #define SkCheckResult(expr, value)  (void)(expr)
#endif

#ifdef SK_DEBUG
inline bool check_alpha(SkAlphaType reported, SkAlphaType actual) {
    return ((reported == actual)
            || ((reported == kPremul_SkAlphaType)
                && (actual == kOpaque_SkAlphaType)));
}
#endif  // SK_DEBUG

////////////////////////////////////////////////////////////////////////////////

DecodingImageGenerator::DecodingImageGenerator(
        SkData* data,
        SkStreamRewindable* stream,
        const SkImageInfo& info,
        int sampleSize,
        bool ditherImage)
    : INHERITED(info)
    , fData(data)
    , fStream(stream)
    , fInfo(info)
    , fSampleSize(sampleSize)
    , fDitherImage(ditherImage)
{
    SkASSERT(stream != NULL);
    SkSafeRef(fData);  // may be NULL.
}

DecodingImageGenerator::~DecodingImageGenerator() {
    SkSafeUnref(fData);
}

SkData* DecodingImageGenerator::onRefEncodedData() {
    // This functionality is used in `gm --serialize`
    // Does not encode options.
    if (NULL == fData) {
        // TODO(halcanary): SkStreamRewindable needs a refData() function
        // which returns a cheap copy of the underlying data.
        if (!fStream->rewind()) {
            return NULL;
        }
        size_t length = fStream->getLength();
        if (length) {
            fData = SkData::NewFromStream(fStream, length);
        }
    }
    return SkSafeRef(fData);
}

#ifdef SK_LEGACY_IMAGE_GENERATOR_ENUMS_AND_OPTIONS
SkImageGenerator::Result DecodingImageGenerator::onGetPixels(const SkImageInfo& info,
        void* pixels, size_t rowBytes, const Options& options, SkPMColor ctableEntries[],
        int* ctableCount) {
#else
bool DecodingImageGenerator::onGetPixels(const SkImageInfo& info, void* pixels, size_t rowBytes,
                                         SkPMColor ctableEntries[], int* ctableCount) {
#endif
    if (fInfo != info) {
        // The caller has specified a different info.  This is an
        // error for this kind of SkImageGenerator.  Use the Options
        // to change the settings.
#ifdef SK_LEGACY_IMAGE_GENERATOR_ENUMS_AND_OPTIONS
        if (info.dimensions() != fInfo.dimensions()) {
            return kInvalidScale;
        }
        return kInvalidConversion;
#else
        return false;
#endif
    }

    SkAssertResult(fStream->rewind());
    SkAutoTDelete<SkImageDecoder> decoder(SkImageDecoder::Factory(fStream));
    if (NULL == decoder.get()) {
#ifdef SK_LEGACY_IMAGE_GENERATOR_ENUMS_AND_OPTIONS
        return kInvalidInput;
#else
        return false;
#endif
    }
    decoder->setDitherImage(fDitherImage);
    decoder->setSampleSize(fSampleSize);
    decoder->setRequireUnpremultipliedColors(info.alphaType() == kUnpremul_SkAlphaType);

    SkBitmap bitmap;
    TargetAllocator allocator(fInfo, pixels, rowBytes);
    decoder->setAllocator(&allocator);
    const SkImageDecoder::Result decodeResult = decoder->decode(fStream, &bitmap, info.colorType(),
                                                                SkImageDecoder::kDecodePixels_Mode);
    decoder->setAllocator(NULL);
    if (SkImageDecoder::kFailure == decodeResult) {
#ifdef SK_LEGACY_IMAGE_GENERATOR_ENUMS_AND_OPTIONS
        return kInvalidInput;
#else
        return false;
#endif
    }
    if (allocator.isReady()) {  // Did not use pixels!
        SkBitmap bm;
        SkASSERT(bitmap.canCopyTo(info.colorType()));
        bool copySuccess = bitmap.copyTo(&bm, info.colorType(), &allocator);
        if (!copySuccess || allocator.isReady()) {
            SkDEBUGFAIL("bitmap.copyTo(requestedConfig) failed.");
            // Earlier we checked canCopyto(); we expect consistency.
#ifdef SK_LEGACY_IMAGE_GENERATOR_ENUMS_AND_OPTIONS
            return kInvalidConversion;
#else
            return false;
#endif
        }
        SkASSERT(check_alpha(info.alphaType(), bm.alphaType()));
    } else {
        SkASSERT(check_alpha(info.alphaType(), bitmap.alphaType()));
    }

    if (kIndex_8_SkColorType == info.colorType()) {
        if (kIndex_8_SkColorType != bitmap.colorType()) {
            // they asked for Index8, but we didn't receive that from decoder
#ifdef SK_LEGACY_IMAGE_GENERATOR_ENUMS_AND_OPTIONS
            return kInvalidConversion;
#else
            return false;
#endif
        }
        SkColorTable* ctable = bitmap.getColorTable();
        if (NULL == ctable) {
#ifdef SK_LEGACY_IMAGE_GENERATOR_ENUMS_AND_OPTIONS
            return kInvalidConversion;
#else
            return false;
#endif
        }
        const int count = ctable->count();
        memcpy(ctableEntries, ctable->readColors(), count * sizeof(SkPMColor));
        *ctableCount = count;
    }
#ifdef SK_LEGACY_IMAGE_GENERATOR_ENUMS_AND_OPTIONS
    if (SkImageDecoder::kPartialSuccess == decodeResult) {
        return kIncompleteInput;
    }
    return kSuccess;
#else
    return true;
#endif
}

bool DecodingImageGenerator::onGetYUV8Planes(SkISize sizes[3], void* planes[3],
                                             size_t rowBytes[3], SkYUVColorSpace* colorSpace) {
    if (!fStream->rewind()) {
        return false;
    }

    SkAutoTDelete<SkImageDecoder> decoder(SkImageDecoder::Factory(fStream));
    if (NULL == decoder.get()) {
        return false;
    }

    return decoder->decodeYUV8Planes(fStream, sizes, planes, rowBytes, colorSpace);
}

// A contructor-type function that returns NULL on failure.  This
// prevents the returned SkImageGenerator from ever being in a bad
// state.  Called by both Create() functions
SkImageGenerator* CreateDecodingImageGenerator(
        SkData* data,
        SkStreamRewindable* stream,
        const SkDecodingImageGenerator::Options& opts) {
    SkASSERT(stream);
    SkAutoTDelete<SkStreamRewindable> autoStream(stream);  // always delete this
    SkAssertResult(autoStream->rewind());
    SkAutoTDelete<SkImageDecoder> decoder(SkImageDecoder::Factory(autoStream));
    if (NULL == decoder.get()) {
        return NULL;
    }
    SkBitmap bitmap;
    decoder->setSampleSize(opts.fSampleSize);
    decoder->setRequireUnpremultipliedColors(opts.fRequireUnpremul);
    if (!decoder->decode(stream, &bitmap, SkImageDecoder::kDecodeBounds_Mode)) {
        return NULL;
    }
    if (kUnknown_SkColorType == bitmap.colorType()) {
        return NULL;
    }

    SkImageInfo info = bitmap.info();

    if (opts.fUseRequestedColorType && (opts.fRequestedColorType != info.colorType())) {
        if (!bitmap.canCopyTo(opts.fRequestedColorType)) {
            SkASSERT(bitmap.colorType() != opts.fRequestedColorType);
            return NULL;  // Can not translate to needed config.
        }
        info = info.makeColorType(opts.fRequestedColorType);
    }

    if (opts.fRequireUnpremul && info.alphaType() != kOpaque_SkAlphaType) {
        info = info.makeAlphaType(kUnpremul_SkAlphaType);
    }

    SkAlphaType newAlphaType = info.alphaType();
    if (!SkColorTypeValidateAlphaType(info.colorType(), info.alphaType(), &newAlphaType)) {
        return NULL;
    }

    return SkNEW_ARGS(DecodingImageGenerator,
                      (data, autoStream.detach(), info.makeAlphaType(newAlphaType),
                       opts.fSampleSize, opts.fDitherImage));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

SkImageGenerator* SkDecodingImageGenerator::Create(
        SkData* data,
        const SkDecodingImageGenerator::Options& opts) {
    SkASSERT(data != NULL);
    if (NULL == data) {
        return NULL;
    }
    SkStreamRewindable* stream = SkNEW_ARGS(SkMemoryStream, (data));
    SkASSERT(stream != NULL);
    return CreateDecodingImageGenerator(data, stream, opts);
}

SkImageGenerator* SkDecodingImageGenerator::Create(
        SkStreamRewindable* stream,
        const SkDecodingImageGenerator::Options& opts) {
    SkASSERT(stream != NULL);
    if (stream == NULL) {
        return NULL;
    }
    return CreateDecodingImageGenerator(NULL, stream, opts);
}
