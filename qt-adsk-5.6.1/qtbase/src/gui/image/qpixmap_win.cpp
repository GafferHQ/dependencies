/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qbitmap.h"
#include "qpixmap.h"
#include <qpa/qplatformpixmap.h>
#include "qpixmap_raster_p.h"

#include <qglobal.h>
#include <QScopedArrayPointer>
#include <qt_windows.h>

QT_BEGIN_NAMESPACE

#ifdef Q_OS_WINCE
#define GetDIBits(a,b,c,d,e,f,g) qt_wince_GetDIBits(a,b,c,d,e,f,g)
int qt_wince_GetDIBits(HDC /*hdc*/ , HBITMAP hSourceBitmap, uint, uint, LPVOID lpvBits, LPBITMAPINFO, uint)
{
    if (!lpvBits) {
        qWarning("::GetDIBits(), lpvBits NULL");
        return 0;
    }
    BITMAP bm;
    GetObject(hSourceBitmap, sizeof(BITMAP), &bm);
    bm.bmHeight = qAbs(bm.bmHeight);

    HBITMAP hTargetBitmap;
    void *pixels;

    BITMAPINFO dibInfo;
    memset(&dibInfo, 0, sizeof(dibInfo));
    dibInfo.bmiHeader.biBitCount = 32;
    dibInfo.bmiHeader.biClrImportant = 0;
    dibInfo.bmiHeader.biClrUsed = 0;
    dibInfo.bmiHeader.biCompression = BI_RGB;;
    dibInfo.bmiHeader.biHeight = -bm.bmHeight;
    dibInfo.bmiHeader.biWidth = bm.bmWidth;
    dibInfo.bmiHeader.biPlanes = 1;
    dibInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    dibInfo.bmiHeader.biSizeImage = bm.bmWidth * bm.bmHeight * 4;

    HDC displayDC = GetDC(NULL);
    if (!displayDC) {
        qWarning("::GetDIBits(), failed to GetDC");
        return 0;
    }

    int ret = bm.bmHeight;

    hTargetBitmap = CreateDIBSection(displayDC, (const BITMAPINFO*) &dibInfo, DIB_RGB_COLORS,
                                    (void**)&pixels, NULL, 0);
    if (!hTargetBitmap) {
        qWarning("::GetDIBits(), failed to CreateDIBSection");
        return 0;
    }

    HDC hdcSrc = CreateCompatibleDC(displayDC);
    HDC hdcDst = CreateCompatibleDC(displayDC);

    if (!(hdcDst && hdcSrc)) {
        qWarning("::GetDIBits(), failed to CreateCompatibleDC");
        ret = 0;
    }

    HBITMAP hOldBitmap1 = (HBITMAP) SelectObject(hdcSrc, hSourceBitmap);
    HBITMAP hOldBitmap2 = (HBITMAP) SelectObject(hdcDst, hTargetBitmap);

    if (!(hOldBitmap1 && hOldBitmap2)) {
        qWarning("::GetDIBits(), failed to SelectObject for bitmaps");
        ret = 0;
    }

    if (!BitBlt(hdcDst, 0, 0, bm.bmWidth, bm.bmHeight, hdcSrc, 0, 0, SRCCOPY)) {
        qWarning("::GetDIBits(), BitBlt failed");
        ret = 0;
    }

    SelectObject(hdcSrc, hOldBitmap1);
    SelectObject(hdcDst, hOldBitmap2);

    DeleteDC(hdcSrc);
    DeleteDC(hdcDst);

    ReleaseDC(NULL, displayDC);

    memcpy(lpvBits, pixels, dibInfo.bmiHeader.biSizeImage);

    DeleteObject(hTargetBitmap);
    return ret;
}
#endif

static inline void initBitMapInfoHeader(int width, int height, bool topToBottom, BITMAPINFOHEADER *bih)
{
    memset(bih, 0, sizeof(BITMAPINFOHEADER));
    bih->biSize        = sizeof(BITMAPINFOHEADER);
    bih->biWidth       = width;
    bih->biHeight      = topToBottom ? -height : height;
    bih->biPlanes      = 1;
    bih->biBitCount    = 32;
    bih->biCompression = BI_RGB;
    bih->biSizeImage   = width * height * 4;
}

static inline void initBitMapInfo(int width, int height, bool topToBottom, BITMAPINFO *bmi)
{
    initBitMapInfoHeader(width, height, topToBottom, &bmi->bmiHeader);
    memset(bmi->bmiColors, 0, sizeof(RGBQUAD));
}

static inline uchar *getDiBits(HDC hdc, HBITMAP bitmap, int width, int height, bool topToBottom = true)
{
    BITMAPINFO bmi;
    initBitMapInfo(width, height, topToBottom, &bmi);
    uchar *result = new uchar[bmi.bmiHeader.biSizeImage];
    if (!GetDIBits(hdc, bitmap, 0, height, result, &bmi, DIB_RGB_COLORS)) {
        delete [] result;
        qErrnoWarning("%s: GetDIBits() failed to get bitmap bits.", __FUNCTION__);
        return 0;
    }
    return result;
}

static inline void copyImageDataCreateAlpha(const uchar *data, QImage *target)
{
    const uint mask = target->format() == QImage::Format_RGB32 ? 0xff000000 : 0;
    const int height = target->height();
    const int width = target->width();
    const int bytesPerLine = width * int(sizeof(QRgb));
    for (int y = 0; y < height; ++y) {
        QRgb *dest = reinterpret_cast<QRgb *>(target->scanLine(y));
        const QRgb *src = reinterpret_cast<const QRgb *>(data + y * bytesPerLine);
        for (int x = 0; x < width; ++x) {
            const uint pixel = src[x];
            if ((pixel & 0xff000000) == 0 && (pixel & 0x00ffffff) != 0)
                dest[x] = pixel | 0xff000000;
            else
                dest[x] = pixel | mask;
        }
    }
}

static inline void copyImageData(const uchar *data, QImage *target)
{
    const int height = target->height();
    const int bytesPerLine = target->bytesPerLine();
    for (int y = 0; y < height; ++y) {
        void *dest = static_cast<void *>(target->scanLine(y));
        const void *src = data + y * bytesPerLine;
        memcpy(dest, src, bytesPerLine);
    }

}

enum HBitmapFormat
{
    HBitmapNoAlpha,
    HBitmapPremultipliedAlpha,
    HBitmapAlpha
};

Q_GUI_EXPORT HBITMAP qt_createIconMask(const QBitmap &bitmap)
{
    QImage bm = bitmap.toImage().convertToFormat(QImage::Format_Mono);
    const int w = bm.width();
    const int h = bm.height();
    const int bpl = ((w+15)/16)*2; // bpl, 16 bit alignment
    QScopedArrayPointer<uchar> bits(new uchar[bpl * h]);
    bm.invertPixels();
    for (int y = 0; y < h; ++y)
        memcpy(bits.data() + y * bpl, bm.constScanLine(y), bpl);
    HBITMAP hbm = CreateBitmap(w, h, 1, 1, bits.data());
    return hbm;
}

Q_GUI_EXPORT HBITMAP qt_pixmapToWinHBITMAP(const QPixmap &p, int hbitmapFormat = 0)
{
    if (p.isNull())
        return 0;

    HBITMAP bitmap = 0;
    if (p.handle()->classId() != QPlatformPixmap::RasterClass) {
        QRasterPlatformPixmap *data = new QRasterPlatformPixmap(p.depth() == 1 ?
            QRasterPlatformPixmap::BitmapType : QRasterPlatformPixmap::PixmapType);
        data->fromImage(p.toImage(), Qt::AutoColor);
        return qt_pixmapToWinHBITMAP(QPixmap(data), hbitmapFormat);
    }

    QRasterPlatformPixmap *d = static_cast<QRasterPlatformPixmap*>(p.handle());
    const QImage *rasterImage = d->buffer();
    const int w = rasterImage->width();
    const int h = rasterImage->height();

    HDC display_dc = GetDC(0);

    // Define the header
    BITMAPINFO bmi;
    initBitMapInfo(w, h, true, &bmi);

    // Create the pixmap
    uchar *pixels = 0;
    bitmap = CreateDIBSection(display_dc, &bmi, DIB_RGB_COLORS, (void **) &pixels, 0, 0);
    ReleaseDC(0, display_dc);
    if (!bitmap) {
        qErrnoWarning("%s, failed to create dibsection", __FUNCTION__);
        return 0;
    }
    if (!pixels) {
        qErrnoWarning("%s, did not allocate pixel data", __FUNCTION__);
        return 0;
    }

    // Copy over the data
    QImage::Format imageFormat = QImage::Format_RGB32;
    if (hbitmapFormat == HBitmapAlpha)
        imageFormat = QImage::Format_ARGB32;
    else if (hbitmapFormat == HBitmapPremultipliedAlpha)
        imageFormat = QImage::Format_ARGB32_Premultiplied;
    const QImage image = rasterImage->convertToFormat(imageFormat);
    const int bytes_per_line = w * 4;
    for (int y=0; y < h; ++y)
        memcpy(pixels + y * bytes_per_line, image.scanLine(y), bytes_per_line);

    return bitmap;
}


Q_GUI_EXPORT QPixmap qt_pixmapFromWinHBITMAP(HBITMAP bitmap, int hbitmapFormat = 0)
{
    // Verify size
    BITMAP bitmap_info;
    memset(&bitmap_info, 0, sizeof(BITMAP));

    const int res = GetObject(bitmap, sizeof(BITMAP), &bitmap_info);
    if (!res) {
        qErrnoWarning("QPixmap::fromWinHBITMAP(), failed to get bitmap info");
        return QPixmap();
    }
    const int w = bitmap_info.bmWidth;
    const int h = bitmap_info.bmHeight;

    // Get bitmap bits
    HDC display_dc = GetDC(0);
    QScopedArrayPointer<uchar> data(getDiBits(display_dc, bitmap, w, h, true));
    if (data.isNull()) {
        ReleaseDC(0, display_dc);
        return QPixmap();
    }

    const QImage::Format imageFormat = hbitmapFormat == HBitmapNoAlpha ?
        QImage::Format_RGB32 : QImage::Format_ARGB32_Premultiplied;

    // Create image and copy data into image.
    QImage image(w, h, imageFormat);
    if (image.isNull()) { // failed to alloc?
        ReleaseDC(0, display_dc);
        qWarning("%s, failed create image of %dx%d", __FUNCTION__, w, h);
        return QPixmap();
    }
    copyImageDataCreateAlpha(data.data(), &image);
    ReleaseDC(0, display_dc);
    return QPixmap::fromImage(image);
}


Q_GUI_EXPORT HICON qt_pixmapToWinHICON(const QPixmap &p)
{
    if (p.isNull())
        return 0;

    QBitmap maskBitmap = p.mask();
    if (maskBitmap.isNull()) {
        maskBitmap = QBitmap(p.size());
        maskBitmap.fill(Qt::color1);
    }

    ICONINFO ii;
    ii.fIcon    = true;
    ii.hbmMask  = qt_createIconMask(maskBitmap);
    ii.hbmColor = qt_pixmapToWinHBITMAP(p, HBitmapAlpha);
    ii.xHotspot = 0;
    ii.yHotspot = 0;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(ii.hbmColor);
    DeleteObject(ii.hbmMask);

    return hIcon;
}

#ifndef Q_OS_WINCE

Q_GUI_EXPORT QImage qt_imageFromWinHBITMAP(HDC hdc, HBITMAP bitmap, int w, int h)
{
    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        return image;
    QScopedArrayPointer<uchar> data(getDiBits(hdc, bitmap, w, h, true));
    if (data.isNull())
        return QImage();
    copyImageDataCreateAlpha(data.data(), &image);
    return image;
}

static QImage qt_imageFromWinIconHBITMAP(HDC hdc, HBITMAP bitmap, int w, int h)
{
    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        return image;
    QScopedArrayPointer<uchar> data(getDiBits(hdc, bitmap, w, h, true));
    if (data.isNull())
        return QImage();
    copyImageData(data.data(), &image);
    return image;
}

static inline bool hasAlpha(const QImage &image)
{
    const int w = image.width();
    const int h = image.height();
    for (int y = 0; y < h; ++y) {
        const QRgb *scanLine = reinterpret_cast<const QRgb *>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (qAlpha(scanLine[x]) != 0)
                return true;
        }
    }
    return false;
}

Q_GUI_EXPORT QPixmap qt_pixmapFromWinHICON(HICON icon)
{
    HDC screenDevice = GetDC(0);
    HDC hdc = CreateCompatibleDC(screenDevice);
    ReleaseDC(0, screenDevice);

    ICONINFO iconinfo;
    const bool result = GetIconInfo(icon, &iconinfo); //x and y Hotspot describes the icon center
    if (!result) {
        qErrnoWarning("QPixmap::fromWinHICON(), failed to GetIconInfo()");
        DeleteDC(hdc);
        return QPixmap();
    }

    const int w = iconinfo.xHotspot * 2;
    const int h = iconinfo.yHotspot * 2;

    BITMAPINFOHEADER bitmapInfo;
    initBitMapInfoHeader(w, h, false, &bitmapInfo);
    DWORD* bits;

    HBITMAP winBitmap = CreateDIBSection(hdc, (BITMAPINFO*)&bitmapInfo, DIB_RGB_COLORS, (VOID**)&bits, NULL, 0);
    HGDIOBJ oldhdc = (HBITMAP)SelectObject(hdc, winBitmap);
    DrawIconEx( hdc, 0, 0, icon, iconinfo.xHotspot * 2, iconinfo.yHotspot * 2, 0, 0, DI_NORMAL);
    QImage image = qt_imageFromWinIconHBITMAP(hdc, winBitmap, w, h);

    if (!image.isNull() && !hasAlpha(image)) { //If no alpha was found, we use the mask to set alpha values
        DrawIconEx( hdc, 0, 0, icon, w, h, 0, 0, DI_MASK);
        const QImage mask = qt_imageFromWinIconHBITMAP(hdc, winBitmap, w, h);

        for (int y = 0 ; y < h ; y++){
            QRgb *scanlineImage = reinterpret_cast<QRgb *>(image.scanLine(y));
            const QRgb *scanlineMask = mask.isNull() ? 0 : reinterpret_cast<const QRgb *>(mask.scanLine(y));
            for (int x = 0; x < w ; x++){
                if (scanlineMask && qRed(scanlineMask[x]) != 0)
                    scanlineImage[x] = 0; //mask out this pixel
                else
                    scanlineImage[x] |= 0xff000000; // set the alpha channel to 255
            }
        }
    }
    //dispose resources created by iconinfo call
    DeleteObject(iconinfo.hbmMask);
    DeleteObject(iconinfo.hbmColor);

    SelectObject(hdc, oldhdc); //restore state
    DeleteObject(winBitmap);
    DeleteDC(hdc);
    return QPixmap::fromImage(image);
}
#else //ifndef Q_OS_WINCE
Q_GUI_EXPORT QPixmap qt_pixmapFromWinHICON(HICON icon)
{
    HDC screenDevice = GetDC(0);
    HDC hdc = CreateCompatibleDC(screenDevice);
    ReleaseDC(0, screenDevice);

    ICONINFO iconinfo;
    bool result = GetIconInfo(icon, &iconinfo);
    if (!result)
        qWarning("QPixmap::fromWinHICON(), failed to GetIconInfo()");

    int w = 0;
    int h = 0;
    if (!iconinfo.xHotspot || !iconinfo.yHotspot) {
        // We could not retrieve the icon size via GetIconInfo,
        // so we try again using the icon bitmap.
        BITMAP bm;
        int result = GetObject(iconinfo.hbmColor, sizeof(BITMAP), &bm);
        if (!result) result = GetObject(iconinfo.hbmMask, sizeof(BITMAP), &bm);
        if (!result) {
            qWarning("QPixmap::fromWinHICON(), failed to retrieve icon size");
            return QPixmap();
        }
        w = bm.bmWidth;
        h = bm.bmHeight;
    } else {
        // x and y Hotspot describes the icon center
        w = iconinfo.xHotspot * 2;
        h = iconinfo.yHotspot * 2;
    }
    const DWORD dwImageSize = w * h * 4;

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFO);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage   = dwImageSize;

    uchar* bits;

    HBITMAP winBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**) &bits, 0, 0);
    if (winBitmap )
        memset(bits, 0xff, dwImageSize);
    if (!winBitmap) {
        qWarning("QPixmap::fromWinHICON(), failed to CreateDIBSection()");
        return QPixmap();
    }

    HGDIOBJ oldhdc = (HBITMAP)SelectObject(hdc, winBitmap);
    if (!DrawIconEx( hdc, 0, 0, icon, w, h, 0, 0, DI_NORMAL))
        qWarning("QPixmap::fromWinHICON(), failed to DrawIcon()");

    uint mask = 0xff000000;
    // Create image and copy data into image.
    QImage image(w, h, QImage::Format_ARGB32);

    if (!image.isNull()) { // failed to alloc?
        int bytes_per_line = w * sizeof(QRgb);
        for (int y=0; y < h; ++y) {
            QRgb *dest = (QRgb *) image.scanLine(y);
            const QRgb *src = (const QRgb *) (bits + y * bytes_per_line);
            for (int x=0; x < w; ++x) {
                dest[x] = src[x];
            }
        }
    }
    if (!DrawIconEx( hdc, 0, 0, icon, w, h, 0, 0, DI_MASK))
        qWarning("QPixmap::fromWinHICON(), failed to DrawIcon()");
    if (!image.isNull()) { // failed to alloc?
        int bytes_per_line = w * sizeof(QRgb);
        for (int y=0; y < h; ++y) {
            QRgb *dest = (QRgb *) image.scanLine(y);
            const QRgb *src = (const QRgb *) (bits + y * bytes_per_line);
            for (int x=0; x < w; ++x) {
                if (!src[x])
                    dest[x] = dest[x] | mask;
            }
        }
    }
    SelectObject(hdc, oldhdc); //restore state
    DeleteObject(winBitmap);
    DeleteDC(hdc);
    return QPixmap::fromImage(image);
}
#endif //ifndef Q_OS_WINCE

QT_END_NAMESPACE
