/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkWebpCodec.h"
#include "SkTemplates.h"

// A WebP decoder on top of (subset of) libwebp
// For more information on WebP image format, and libwebp library, see:
//   https://code.google.com/speed/webp/
//   http://www.webmproject.org/code/#libwebp-webp-image-library
//   https://chromium.googlesource.com/webm/libwebp

// If moving libwebp out of skia source tree, path for webp headers must be
// updated accordingly. Here, we enforce using local copy in webp sub-directory.
#include "webp/decode.h"
#include "webp/encode.h"

bool SkWebpCodec::IsWebp(SkStream* stream) {
    // WEBP starts with the following:
    // RIFFXXXXWEBPVP
    // Where XXXX is unspecified.
    const char LENGTH = 14;
    char bytes[LENGTH];
    if (stream->read(&bytes, LENGTH) != LENGTH) {
        return false;
    }
    return !memcmp(bytes, "RIFF", 4) && !memcmp(&bytes[8], "WEBPVP", 6);
}

static const size_t WEBP_VP8_HEADER_SIZE = 30;

// Parse headers of RIFF container, and check for valid Webp (VP8) content.
// NOTE: This calls peek instead of read, since onGetPixels will need these
// bytes again.
static bool webp_parse_header(SkStream* stream, SkImageInfo* info) {
    unsigned char buffer[WEBP_VP8_HEADER_SIZE];
    if (!stream->peek(buffer, WEBP_VP8_HEADER_SIZE)) {
        return false;
    }

    WebPBitstreamFeatures features;
    VP8StatusCode status = WebPGetFeatures(buffer, WEBP_VP8_HEADER_SIZE, &features);
    if (VP8_STATUS_OK != status) {
        return false; // Invalid WebP file.
    }

    // sanity check for image size that's about to be decoded.
    {
        const int64_t size = sk_64_mul(features.width, features.height);
        if (!sk_64_isS32(size)) {
            return false;
        }
        // now check that if we are 4-bytes per pixel, we also don't overflow
        if (sk_64_asS32(size) > (0x7FFFFFFF >> 2)) {
            return false;
        }
    }

    if (info) {
        // FIXME: Is N32 the right type?
        // Is unpremul the right type? Clients of SkCodec may assume it's the
        // best type, when Skia currently cannot draw unpremul (and raster is faster
        // with premul).
        *info = SkImageInfo::Make(features.width, features.height, kN32_SkColorType,
                                  SkToBool(features.has_alpha) ? kUnpremul_SkAlphaType
                                                              : kOpaque_SkAlphaType);
    }
    return true;
}

SkCodec* SkWebpCodec::NewFromStream(SkStream* stream) {
    SkAutoTDelete<SkStream> streamDeleter(stream);
    SkImageInfo info;
    if (webp_parse_header(stream, &info)) {
        return SkNEW_ARGS(SkWebpCodec, (info, streamDeleter.detach()));
    }
    return NULL;
}

static bool conversion_possible(const SkImageInfo& dst, const SkImageInfo& src) {
    switch (dst.colorType()) {
        // Both byte orders are supported.
        case kBGRA_8888_SkColorType:
        case kRGBA_8888_SkColorType:
            break;
        default:
            return false;
    }
    if (dst.profileType() != src.profileType()) {
        return false;
    }
    if (dst.alphaType() == src.alphaType()) {
        return true;
    }
    return kPremul_SkAlphaType == dst.alphaType() &&
            kUnpremul_SkAlphaType == src.alphaType();
}

SkISize SkWebpCodec::onGetScaledDimensions(float desiredScale) const {
    SkISize dim = this->getInfo().dimensions();
    // SkCodec treats zero dimensional images as errors, so the minimum size
    // that we will recommend is 1x1.
    dim.fWidth = SkTMax(1, SkScalarRoundToInt(desiredScale * dim.fWidth));
    dim.fHeight = SkTMax(1, SkScalarRoundToInt(desiredScale * dim.fHeight));
    return dim;
}

static WEBP_CSP_MODE webp_decode_mode(SkColorType ct, bool premultiply) {
    switch (ct) {
        case kBGRA_8888_SkColorType:
            return premultiply ? MODE_bgrA : MODE_BGRA;
        case kRGBA_8888_SkColorType:
            return premultiply ? MODE_rgbA : MODE_RGBA;
        default:
            return MODE_LAST;
    }
}

// The WebP decoding API allows us to incrementally pass chunks of bytes as we receive them to the
// decoder with WebPIAppend. In order to do so, we need to read chunks from the SkStream. This size
// is arbitrary.
static const size_t BUFFER_SIZE = 4096;

SkCodec::Result SkWebpCodec::onGetPixels(const SkImageInfo& dstInfo, void* dst, size_t rowBytes,
                                         const Options&, SkPMColor*, int*) {
    switch (this->rewindIfNeeded()) {
        case kCouldNotRewind_RewindState:
            return kCouldNotRewind;
        case kRewound_RewindState:
            // Rewound to the beginning. Since creation only does a peek, the stream is at the
            // correct position.
            break;
        case kNoRewindNecessary_RewindState:
            // Already at the right spot for decoding.
            break;
    }

    if (!conversion_possible(dstInfo, this->getInfo())) {
        return kInvalidConversion;
    }

    WebPDecoderConfig config;
    if (0 == WebPInitDecoderConfig(&config)) {
        // ABI mismatch.
        // FIXME: New enum for this?
        return kInvalidInput;
    }

    // Free any memory associated with the buffer. Must be called last, so we declare it first.
    SkAutoTCallVProc<WebPDecBuffer, WebPFreeDecBuffer> autoFree(&(config.output));

    SkISize dimensions = dstInfo.dimensions();
    if (this->getInfo().dimensions() != dimensions) {
        // Caller is requesting scaling.
        config.options.use_scaling = 1;
        config.options.scaled_width = dimensions.width();
        config.options.scaled_height = dimensions.height();
    }

    config.output.colorspace = webp_decode_mode(dstInfo.colorType(),
            dstInfo.alphaType() == kPremul_SkAlphaType);
    config.output.u.RGBA.rgba = (uint8_t*) dst;
    config.output.u.RGBA.stride = (int) rowBytes;
    config.output.u.RGBA.size = dstInfo.getSafeSize(rowBytes);
    config.output.is_external_memory = 1;

    SkAutoTCallVProc<WebPIDecoder, WebPIDelete> idec(WebPIDecode(NULL, 0, &config));
    if (!idec) {
        return kInvalidInput;
    }

    SkAutoMalloc storage(BUFFER_SIZE);
    uint8_t* buffer = static_cast<uint8_t*>(storage.get());
    while (true) {
        const size_t bytesRead = stream()->read(buffer, BUFFER_SIZE);
        if (0 == bytesRead) {
            // FIXME: Maybe this is an incomplete image? How to decide? Based
            // on the number of rows decoded? We can know the number of rows
            // decoded using WebPIDecGetRGB.
            return kInvalidInput;
        }

        switch (WebPIAppend(idec, buffer, bytesRead)) {
            case VP8_STATUS_OK:
                return kSuccess;
            case VP8_STATUS_SUSPENDED:
                // Break out of the switch statement. Continue the loop.
                break;
            default:
                return kInvalidInput;
        }
    }
}

SkWebpCodec::SkWebpCodec(const SkImageInfo& info, SkStream* stream)
    : INHERITED(info, stream) {}
