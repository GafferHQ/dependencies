/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkMasks.h"
#include "SkSwizzler.h"
#include "SkTypes.h"

/*
 *
 * Used to swizzle images whose pixel components are extracted by bit masks
 * Currently only used by bmp
 *
 */
class SkMaskSwizzler {
public:

    /*
     *
     * Create a new swizzler
     * @param masks Unowned pointer to helper class
     *
     */
    static SkMaskSwizzler* CreateMaskSwizzler(const SkImageInfo& imageInfo,
                                              void* dst, size_t dstRowBytes,
                                              SkMasks* masks,
                                              uint32_t bitsPerPixel);

    /*
     *
     * Swizzle the row with the specified y value
     *
     */
    SkSwizzler::ResultAlpha next(const uint8_t* SK_RESTRICT src, int y);

private:

    /*
     *
     * Row procedure used for swizzle
     *
     */
    typedef SkSwizzler::ResultAlpha (*RowProc)(
            void* dstRow, const uint8_t* srcRow, int width,
            SkMasks* masks);

    /*
     *
     * Constructor for mask swizzler
     *
     */
    SkMaskSwizzler(const SkImageInfo& info, void* dst, size_t dstRowBytes,
            SkMasks* masks, RowProc proc);

    // Fields
    const SkImageInfo& fDstInfo;
    void*              fDst;
    size_t             fDstRowBytes;
    SkMasks*           fMasks;       // unowned
    const RowProc      fRowProc;
};
