/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdirectfbscreen.h"
#include "qdirectfbpaintdevice.h"
#include "qdirectfbpaintengine.h"

#ifndef QT_NO_QWS_DIRECTFB

QT_BEGIN_NAMESPACE

QDirectFBPaintDevice::QDirectFBPaintDevice(QDirectFBScreen *scr)
    : QCustomRasterPaintDevice(0), dfbSurface(0), screen(scr),
      bpl(-1), lockFlgs(DFBSurfaceLockFlags(0)), mem(0), engine(0), imageFormat(QImage::Format_Invalid)
{
#ifdef QT_DIRECTFB_SUBSURFACE
    subSurface = 0;
    syncPending = false;
#endif
}

QDirectFBPaintDevice::~QDirectFBPaintDevice()
{
    if (QDirectFBScreen::instance()) {
        unlockSurface();
#ifdef QT_DIRECTFB_SUBSURFACE
        releaseSubSurface();
#endif
        if (dfbSurface) {
            screen->releaseDFBSurface(dfbSurface);
        }
    }
    delete engine;
}

IDirectFBSurface *QDirectFBPaintDevice::directFBSurface() const
{
    return dfbSurface;
}

bool QDirectFBPaintDevice::lockSurface(DFBSurfaceLockFlags lockFlags)
{
    if (lockFlgs && (lockFlags & ~lockFlgs))
        unlockSurface();
    if (!mem) {
        Q_ASSERT(dfbSurface);
#ifdef QT_DIRECTFB_SUBSURFACE
        if (!subSurface) {
            DFBResult result;
            subSurface = screen->getSubSurface(dfbSurface, QRect(), QDirectFBScreen::TrackSurface, &result);
            if (result != DFB_OK || !subSurface) {
                DirectFBError("Couldn't create sub surface", result);
                return false;
            }
        }
        IDirectFBSurface *surface = subSurface;
#else
        IDirectFBSurface *surface = dfbSurface;
#endif
        Q_ASSERT(surface);
        mem = QDirectFBScreen::lockSurface(surface, lockFlags, &bpl);
        lockFlgs = lockFlags;
        Q_ASSERT(mem);
        Q_ASSERT(bpl > 0);
        const QSize s = size();
        lockedImage = QImage(mem, s.width(), s.height(), bpl,
                             QDirectFBScreen::getImageFormat(dfbSurface));
        return true;
    }
#ifdef QT_DIRECTFB_SUBSURFACE
    if (syncPending) {
        syncPending = false;
        screen->waitIdle();
    }
#endif
    return false;
}

void QDirectFBPaintDevice::unlockSurface()
{
    if (QDirectFBScreen::instance() && lockFlgs) {
#ifdef QT_DIRECTFB_SUBSURFACE
        IDirectFBSurface *surface = subSurface;
#else
        IDirectFBSurface *surface = dfbSurface;
#endif
        if (surface) {
            surface->Unlock(surface);
            lockFlgs = static_cast<DFBSurfaceLockFlags>(0);
            mem = 0;
        }
    }
}

void *QDirectFBPaintDevice::memory() const
{
    return mem;
}

QImage::Format QDirectFBPaintDevice::format() const
{
    return imageFormat;
}

int QDirectFBPaintDevice::bytesPerLine() const
{
    Q_ASSERT(!mem || bpl != -1);
    return bpl;
}

QSize QDirectFBPaintDevice::size() const
{
    int w, h;
    dfbSurface->GetSize(dfbSurface, &w, &h);
    return QSize(w, h);
}

int QDirectFBPaintDevice::metric(QPaintDevice::PaintDeviceMetric metric) const
{
    if (!dfbSurface)
        return 0;

    switch (metric) {
    case QPaintDevice::PdmWidth:
    case QPaintDevice::PdmHeight:
        return (metric == PdmWidth ? size().width() : size().height());
    case QPaintDevice::PdmWidthMM:
        return (size().width() * 1000) / dotsPerMeterX();
    case QPaintDevice::PdmHeightMM:
        return (size().height() * 1000) / dotsPerMeterY();
    case QPaintDevice::PdmPhysicalDpiX:
    case QPaintDevice::PdmDpiX:
        return (dotsPerMeterX() * 254) / 10000; // 0.0254 meters-per-inch
    case QPaintDevice::PdmPhysicalDpiY:
    case QPaintDevice::PdmDpiY:
        return (dotsPerMeterY() * 254) / 10000; // 0.0254 meters-per-inch
    case QPaintDevice::PdmDepth:
        return QDirectFBScreen::depth(imageFormat);
    case QPaintDevice::PdmNumColors: {
        if (!lockedImage.isNull())
            return lockedImage.colorCount();

        DFBResult result;
        IDirectFBPalette *palette = 0;
        unsigned int numColors = 0;

        result = dfbSurface->GetPalette(dfbSurface, &palette);
        if ((result != DFB_OK) || !palette)
            return 0;

        result = palette->GetSize(palette, &numColors);
        palette->Release(palette);
        if (result != DFB_OK)
            return 0;

        return numColors;
    }
    default:
        qCritical("QDirectFBPaintDevice::metric(): Unhandled metric!");
        return 0;
    }
}

QPaintEngine *QDirectFBPaintDevice::paintEngine() const
{
    return engine;
}

#ifdef QT_DIRECTFB_SUBSURFACE
void QDirectFBPaintDevice::releaseSubSurface()
{
    Q_ASSERT(QDirectFBScreen::instance());
    if (subSurface) {
        unlockSurface();
        screen->releaseDFBSurface(subSurface);
        subSurface = 0;
    }
}
#endif

QT_END_NAMESPACE

#endif // QT_NO_QWS_DIRECTFB
