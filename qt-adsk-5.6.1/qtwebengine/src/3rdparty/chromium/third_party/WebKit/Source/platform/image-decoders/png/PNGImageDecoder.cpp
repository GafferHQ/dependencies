/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * Portions are Copyright (C) 2001 mozilla.org
 *
 * Other contributors:
 *   Stuart Parmenter <stuart@mozilla.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "platform/image-decoders/png/PNGImageDecoder.h"

#include "wtf/PassOwnPtr.h"

#include "png.h"
#if !defined(PNG_LIBPNG_VER_MAJOR) || !defined(PNG_LIBPNG_VER_MINOR)
#error version error: compile against a versioned libpng.
#endif
#if USE(QCMSLIB)
#include "qcms.h"
#endif

#if PNG_LIBPNG_VER_MAJOR > 1 || (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR >= 4)
#define JMPBUF(png_ptr) png_jmpbuf(png_ptr)
#else
#define JMPBUF(png_ptr) png_ptr->jmpbuf
#endif

namespace {

inline blink::PNGImageDecoder* imageDecoder(png_structp png)
{
    return static_cast<blink::PNGImageDecoder*>(png_get_progressive_ptr(png));
}

void PNGAPI pngHeaderAvailable(png_structp png, png_infop)
{
    imageDecoder(png)->headerAvailable();
}

void PNGAPI pngRowAvailable(png_structp png, png_bytep row, png_uint_32 rowIndex, int state)
{
    imageDecoder(png)->rowAvailable(row, rowIndex, state);
}

void PNGAPI pngComplete(png_structp png, png_infop)
{
    imageDecoder(png)->complete();
}

void PNGAPI pngFailed(png_structp png, png_const_charp)
{
    longjmp(JMPBUF(png), 1);
}

} // anonymous

namespace blink {

class PNGImageReader {
    WTF_MAKE_FAST_ALLOCATED(PNGImageReader);
public:
    PNGImageReader(PNGImageDecoder* decoder)
        : m_decoder(decoder)
        , m_readOffset(0)
        , m_currentBufferSize(0)
        , m_decodingSizeOnly(false)
        , m_hasAlpha(false)
#if USE(QCMSLIB)
        , m_transform(0)
        , m_rowBuffer()
#endif
    {
        m_png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, pngFailed, 0);
        m_info = png_create_info_struct(m_png);
        png_set_progressive_read_fn(m_png, m_decoder, pngHeaderAvailable, pngRowAvailable, pngComplete);
    }

    ~PNGImageReader()
    {
        close();
    }

    void close()
    {
        if (m_png && m_info)
            // This will zero the pointers.
            png_destroy_read_struct(&m_png, &m_info, 0);
#if USE(QCMSLIB)
        clearColorTransform();
#endif
        m_readOffset = 0;
    }

    bool decode(const SharedBuffer& data, bool sizeOnly)
    {
        m_decodingSizeOnly = sizeOnly;

        // We need to do the setjmp here. Otherwise bad things will happen.
        if (setjmp(JMPBUF(m_png)))
            return m_decoder->setFailed();

        const char* segment;
        while (unsigned segmentLength = data.getSomeData(segment, m_readOffset)) {
            m_readOffset += segmentLength;
            m_currentBufferSize = m_readOffset;
            png_process_data(m_png, m_info, reinterpret_cast<png_bytep>(const_cast<char*>(segment)), segmentLength);
            if (sizeOnly ? m_decoder->isDecodedSizeAvailable() : m_decoder->frameIsCompleteAtIndex(0))
                return true;
        }

        return false;
    }

    png_structp pngPtr() const { return m_png; }
    png_infop infoPtr() const { return m_info; }

    void setReadOffset(unsigned offset) { m_readOffset = offset; }
    unsigned currentBufferSize() const { return m_currentBufferSize; }
    bool decodingSizeOnly() const { return m_decodingSizeOnly; }
    void setHasAlpha(bool hasAlpha) { m_hasAlpha = hasAlpha; }
    bool hasAlpha() const { return m_hasAlpha; }

    png_bytep interlaceBuffer() const { return m_interlaceBuffer.get(); }
    void createInterlaceBuffer(int size) { m_interlaceBuffer = adoptArrayPtr(new png_byte[size]); }
#if USE(QCMSLIB)
    png_bytep rowBuffer() const { return m_rowBuffer.get(); }
    void createRowBuffer(int size) { m_rowBuffer = adoptArrayPtr(new png_byte[size]); }
    qcms_transform* colorTransform() const { return m_transform; }

    void clearColorTransform()
    {
        if (m_transform)
            qcms_transform_release(m_transform);
        m_transform = 0;
    }

    void createColorTransform(const ColorProfile& colorProfile, bool hasAlpha, bool sRGB)
    {
        clearColorTransform();

        if (colorProfile.isEmpty() && !sRGB)
            return;
        qcms_profile* deviceProfile = ImageDecoder::qcmsOutputDeviceProfile();
        if (!deviceProfile)
            return;
        qcms_profile* inputProfile = 0;
        if (!colorProfile.isEmpty())
            inputProfile = qcms_profile_from_memory(colorProfile.data(), colorProfile.size());
        else
            inputProfile = qcms_profile_sRGB();
        if (!inputProfile)
            return;
        // We currently only support color profiles for RGB and RGBA images.
        ASSERT(rgbData == qcms_profile_get_color_space(inputProfile));
        qcms_data_type dataFormat = hasAlpha ? QCMS_DATA_RGBA_8 : QCMS_DATA_RGB_8;
        // FIXME: Don't force perceptual intent if the image profile contains an intent.
        m_transform = qcms_transform_create(inputProfile, dataFormat, deviceProfile, dataFormat, QCMS_INTENT_PERCEPTUAL);
        qcms_profile_release(inputProfile);
    }
#endif

private:
    png_structp m_png;
    png_infop m_info;
    PNGImageDecoder* m_decoder;
    unsigned m_readOffset;
    unsigned m_currentBufferSize;
    bool m_decodingSizeOnly;
    bool m_hasAlpha;
    OwnPtr<png_byte[]> m_interlaceBuffer;
#if USE(QCMSLIB)
    qcms_transform* m_transform;
    OwnPtr<png_byte[]> m_rowBuffer;
#endif
};

PNGImageDecoder::PNGImageDecoder(ImageSource::AlphaOption alphaOption, ImageSource::GammaAndColorProfileOption colorOptions, size_t maxDecodedBytes)
    : ImageDecoder(alphaOption, colorOptions, maxDecodedBytes)
    , m_hasColorProfile(false)
{
}

PNGImageDecoder::~PNGImageDecoder()
{
}

#if USE(QCMSLIB)
static void getColorProfile(png_structp png, png_infop info, ColorProfile& colorProfile, bool& sRGB)
{
#ifdef PNG_iCCP_SUPPORTED
    ASSERT(colorProfile.isEmpty());
    if (png_get_valid(png, info, PNG_INFO_sRGB)) {
        sRGB = true;
        return;
    }

    char* profileName;
    int compressionType;
#if (PNG_LIBPNG_VER < 10500)
    png_charp profile;
#else
    png_bytep profile;
#endif
    png_uint_32 profileLength;
    if (!png_get_iCCP(png, info, &profileName, &compressionType, &profile, &profileLength))
        return;

    // Only accept RGB color profiles from input class devices.
    bool ignoreProfile = false;
    char* profileData = reinterpret_cast<char*>(profile);
    if (profileLength < ImageDecoder::iccColorProfileHeaderLength)
        ignoreProfile = true;
    else if (!ImageDecoder::rgbColorProfile(profileData, profileLength))
        ignoreProfile = true;
    else if (!ImageDecoder::inputDeviceColorProfile(profileData, profileLength))
        ignoreProfile = true;

    if (!ignoreProfile)
        colorProfile.append(profileData, profileLength);
#endif
}
#endif

void PNGImageDecoder::headerAvailable()
{
    png_structp png = m_reader->pngPtr();
    png_infop info = m_reader->infoPtr();
    png_uint_32 width = png_get_image_width(png, info);
    png_uint_32 height = png_get_image_height(png, info);

    // Protect against large PNGs. See http://bugzil.la/251381 for more details.
    const unsigned long maxPNGSize = 1000000UL;
    if (width > maxPNGSize || height > maxPNGSize) {
        longjmp(JMPBUF(png), 1);
        return;
    }

    // Set the image size now that the image header is available.
    if (!setSize(width, height)) {
        longjmp(JMPBUF(png), 1);
        return;
    }

    int bitDepth, colorType, interlaceType, compressionType, filterType, channels;
    png_get_IHDR(png, info, &width, &height, &bitDepth, &colorType, &interlaceType, &compressionType, &filterType);

    // The options we set here match what Mozilla does.

    // Expand to ensure we use 24-bit for RGB and 32-bit for RGBA.
    if (colorType == PNG_COLOR_TYPE_PALETTE || (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8))
        png_set_expand(png);

    png_bytep trns = 0;
    int trnsCount = 0;
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_get_tRNS(png, info, &trns, &trnsCount, 0);
        png_set_expand(png);
    }

    if (bitDepth == 16)
        png_set_strip_16(png);

    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

#if USE(QCMSLIB)
    if ((colorType & PNG_COLOR_MASK_COLOR) && !m_ignoreGammaAndColorProfile) {
        // We only support color profiles for color PALETTE and RGB[A] PNG. Supporting
        // color profiles for gray-scale images is slightly tricky, at least using the
        // CoreGraphics ICC library, because we expand gray-scale images to RGB but we
        // do not similarly transform the color profile. We'd either need to transform
        // the color profile or we'd need to decode into a gray-scale image buffer and
        // hand that to CoreGraphics.
        bool sRGB = false;
        ColorProfile colorProfile;
        getColorProfile(png, info, colorProfile, sRGB);
        bool imageHasAlpha = (colorType & PNG_COLOR_MASK_ALPHA) || trnsCount;
        m_reader->createColorTransform(colorProfile, imageHasAlpha, sRGB);
        m_hasColorProfile = !!m_reader->colorTransform();
    }
#endif

    if (!m_hasColorProfile) {
        // Deal with gamma and keep it under our control.
        const double inverseGamma = 0.45455;
        const double defaultGamma = 2.2;
        double gamma;
        if (!m_ignoreGammaAndColorProfile && png_get_gAMA(png, info, &gamma)) {
            const double maxGamma = 21474.83;
            if ((gamma <= 0.0) || (gamma > maxGamma)) {
                gamma = inverseGamma;
                png_set_gAMA(png, info, gamma);
            }
            png_set_gamma(png, defaultGamma, gamma);
        } else {
            png_set_gamma(png, defaultGamma, inverseGamma);
        }
    }

    // Tell libpng to send us rows for interlaced pngs.
    if (interlaceType == PNG_INTERLACE_ADAM7)
        png_set_interlace_handling(png);

    // Update our info now.
    png_read_update_info(png, info);
    channels = png_get_channels(png, info);
    ASSERT(channels == 3 || channels == 4);

    m_reader->setHasAlpha(channels == 4);

    if (m_reader->decodingSizeOnly()) {
        // If we only needed the size, halt the reader.
#if PNG_LIBPNG_VER_MAJOR > 1 || (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR >= 5)
        // '0' argument to png_process_data_pause means: Do not cache unprocessed data.
        m_reader->setReadOffset(m_reader->currentBufferSize() - png_process_data_pause(png, 0));
#else
        m_reader->setReadOffset(m_reader->currentBufferSize() - png->buffer_size);
        png->buffer_size = 0;
#endif
    }
}

void PNGImageDecoder::rowAvailable(unsigned char* rowBuffer, unsigned rowIndex, int)
{
    if (m_frameBufferCache.isEmpty())
        return;

    // Initialize the framebuffer if needed.
    ImageFrame& buffer = m_frameBufferCache[0];
    if (buffer.status() == ImageFrame::FrameEmpty) {
        png_structp png = m_reader->pngPtr();
        if (!buffer.setSize(size().width(), size().height())) {
            longjmp(JMPBUF(png), 1);
            return;
        }

        unsigned colorChannels = m_reader->hasAlpha() ? 4 : 3;
        if (PNG_INTERLACE_ADAM7 == png_get_interlace_type(png, m_reader->infoPtr())) {
            m_reader->createInterlaceBuffer(colorChannels * size().width() * size().height());
            if (!m_reader->interlaceBuffer()) {
                longjmp(JMPBUF(png), 1);
                return;
            }
        }

#if USE(QCMSLIB)
        if (m_reader->colorTransform()) {
            m_reader->createRowBuffer(colorChannels * size().width());
            if (!m_reader->rowBuffer()) {
                longjmp(JMPBUF(png), 1);
                return;
            }
        }
#endif
        buffer.setStatus(ImageFrame::FramePartial);
        buffer.setHasAlpha(false);

        // For PNGs, the frame always fills the entire image.
        buffer.setOriginalFrameRect(IntRect(IntPoint(), size()));
    }

    /* libpng comments (here to explain what follows).
     *
     * this function is called for every row in the image. If the
     * image is interlacing, and you turned on the interlace handler,
     * this function will be called for every row in every pass.
     * Some of these rows will not be changed from the previous pass.
     * When the row is not changed, the new_row variable will be NULL.
     * The rows and passes are called in order, so you don't really
     * need the row_num and pass, but I'm supplying them because it
     * may make your life easier.
     */

    // Nothing to do if the row is unchanged, or the row is outside
    // the image bounds: libpng may send extra rows, ignore them to
    // make our lives easier.
    if (!rowBuffer)
        return;
    int y = rowIndex;
    if (y < 0 || y >= size().height())
        return;

    /* libpng comments (continued).
     *
     * For the non-NULL rows of interlaced images, you must call
     * png_progressive_combine_row() passing in the row and the
     * old row.  You can call this function for NULL rows (it will
     * just return) and for non-interlaced images (it just does the
     * memcpy for you) if it will make the code easier. Thus, you
     * can just do this for all cases:
     *
     *    png_progressive_combine_row(png_ptr, old_row, new_row);
     *
     * where old_row is what was displayed for previous rows. Note
     * that the first pass (pass == 0 really) will completely cover
     * the old row, so the rows do not have to be initialized. After
     * the first pass (and only for interlaced images), you will have
     * to pass the current row, and the function will combine the
     * old row and the new row.
     */

    bool hasAlpha = m_reader->hasAlpha();
    png_bytep row = rowBuffer;

    if (png_bytep interlaceBuffer = m_reader->interlaceBuffer()) {
        unsigned colorChannels = hasAlpha ? 4 : 3;
        row = interlaceBuffer + (rowIndex * colorChannels * size().width());
        png_progressive_combine_row(m_reader->pngPtr(), row, rowBuffer);
    }

#if USE(QCMSLIB)
    if (qcms_transform* transform = m_reader->colorTransform()) {
        qcms_transform_data(transform, row, m_reader->rowBuffer(), size().width());
        row = m_reader->rowBuffer();
    }
#endif

    // Write the decoded row pixels to the frame buffer. The repetitive
    // form of the row write loops is for speed.
    ImageFrame::PixelData* address = buffer.getAddr(0, y);
    unsigned alphaMask = 255;
    int width = size().width();

    png_bytep pixel = row;
    if (hasAlpha) {
        if (buffer.premultiplyAlpha()) {
            for (int x = 0; x < width; ++x, pixel += 4) {
                buffer.setRGBAPremultiply(address++, pixel[0], pixel[1], pixel[2], pixel[3]);
                alphaMask &= pixel[3];
            }
        } else {
            for (int x = 0; x < width; ++x, pixel += 4) {
                buffer.setRGBARaw(address++, pixel[0], pixel[1], pixel[2], pixel[3]);
                alphaMask &= pixel[3];
            }
        }
    } else {
        for (int x = 0; x < width; ++x, pixel += 3) {
            buffer.setRGBARaw(address++, pixel[0], pixel[1], pixel[2], 255);
        }
    }

    if (alphaMask != 255 && !buffer.hasAlpha())
        buffer.setHasAlpha(true);

    buffer.setPixelsChanged(true);
}

void PNGImageDecoder::complete()
{
    if (m_frameBufferCache.isEmpty())
        return;

    m_frameBufferCache[0].setStatus(ImageFrame::FrameComplete);
}

inline bool isComplete(const PNGImageDecoder* decoder)
{
    return decoder->frameIsCompleteAtIndex(0);
}

void PNGImageDecoder::decode(bool onlySize)
{
    if (failed())
        return;

    if (!m_reader)
        m_reader = adoptPtr(new PNGImageReader(this));

    // If we couldn't decode the image but have received all the data, decoding
    // has failed.
    if (!m_reader->decode(*m_data, onlySize) && isAllDataReceived())
        setFailed();

    // If decoding is done or failed, we don't need the PNGImageReader anymore.
    if (isComplete(this) || failed())
        m_reader.clear();
}

} // namespace blink
