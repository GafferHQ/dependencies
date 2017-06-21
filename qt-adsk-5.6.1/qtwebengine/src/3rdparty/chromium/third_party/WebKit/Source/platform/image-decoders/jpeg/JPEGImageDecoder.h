/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JPEGImageDecoder_h
#define JPEGImageDecoder_h

#include "platform/image-decoders/ImageDecoder.h"

#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"

namespace blink {

class JPEGImageReader;

// This class decodes the JPEG image format.
class PLATFORM_EXPORT JPEGImageDecoder : public ImageDecoder {
    WTF_MAKE_NONCOPYABLE(JPEGImageDecoder);
public:
    JPEGImageDecoder(ImageSource::AlphaOption, ImageSource::GammaAndColorProfileOption, size_t maxDecodedBytes);
    ~JPEGImageDecoder() override;

    // ImageDecoder:
    String filenameExtension() const override { return "jpg"; }
    bool hasColorProfile() const override { return m_hasColorProfile; }
    IntSize decodedSize() const override { return m_decodedSize; }
    IntSize decodedYUVSize(int component, SizeType) const override;
    bool setSize(unsigned width, unsigned height) override;
    bool canDecodeToYUV() override;
    bool decodeToYUV() override;
    void setImagePlanes(PassOwnPtr<ImagePlanes>) override;
    bool hasImagePlanes() const { return m_imagePlanes; }

    bool outputScanlines();
    unsigned desiredScaleNumerator() const;
    void complete();

    void setOrientation(ImageOrientation orientation) { m_orientation = orientation; }
    void setHasColorProfile(bool hasColorProfile) { m_hasColorProfile = hasColorProfile; }
    void setDecodedSize(unsigned width, unsigned height);

private:
    // ImageDecoder:
    void decodeSize() override { decode(true); }
    void decode(size_t) override { decode(false); }

    // Decodes the image.  If |onlySize| is true, stops decoding after
    // calculating the image size.  If decoding fails but there is no more
    // data coming, sets the "decode failure" flag.
    void decode(bool onlySize);

    OwnPtr<JPEGImageReader> m_reader;
    OwnPtr<ImagePlanes> m_imagePlanes;
    IntSize m_decodedSize;
    bool m_hasColorProfile;
};

} // namespace blink

#endif
