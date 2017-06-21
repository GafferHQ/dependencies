/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qwindowsdirect2dcontext.h"
#include "qwindowsdirect2dpaintdevice.h"
#include "qwindowsdirect2dplatformpixmap.h"
#include "qwindowsdirect2dbitmap.h"
#include "qwindowsdirect2dhelpers.h"

#include <QtGui/QPainter>
#include <QtGui/QImage>
#include <QtGui/QPaintDevice>
#include <QtGui/QPaintEngine>

QT_BEGIN_NAMESPACE

class QWindowsDirect2DPlatformPixmapPrivate
{
public:
    QWindowsDirect2DPlatformPixmapPrivate()
        : owns_bitmap(true)
        , bitmap(new QWindowsDirect2DBitmap)
        , device(new QWindowsDirect2DPaintDevice(bitmap, QInternal::Pixmap))
        , devicePixelRatio(1.0)
    {}

    QWindowsDirect2DPlatformPixmapPrivate(QWindowsDirect2DBitmap *bitmap,
                                          QWindowsDirect2DPaintEngine::Flags flags)
        : owns_bitmap(false)
        , bitmap(bitmap)
        , device(new QWindowsDirect2DPaintDevice(bitmap, QInternal::Pixmap, flags))
        , devicePixelRatio(1.0)
    {}

    ~QWindowsDirect2DPlatformPixmapPrivate()
    {
        if (owns_bitmap)
            delete bitmap;
    }

    bool owns_bitmap;
    QWindowsDirect2DBitmap *bitmap;
    QScopedPointer<QWindowsDirect2DPaintDevice> device;
    qreal devicePixelRatio;
};

static int qt_d2dpixmap_serno = 0;

QWindowsDirect2DPlatformPixmap::QWindowsDirect2DPlatformPixmap(PixelType pixelType)
    : QPlatformPixmap(pixelType, Direct2DClass)
    , d_ptr(new QWindowsDirect2DPlatformPixmapPrivate)
{
    setSerialNumber(qt_d2dpixmap_serno++);
}

QWindowsDirect2DPlatformPixmap::QWindowsDirect2DPlatformPixmap(QPlatformPixmap::PixelType pixelType,
                                                               QWindowsDirect2DPaintEngine::Flags flags,
                                                               QWindowsDirect2DBitmap *bitmap)
    : QPlatformPixmap(pixelType, Direct2DClass)
    , d_ptr(new QWindowsDirect2DPlatformPixmapPrivate(bitmap, flags))
{
    setSerialNumber(qt_d2dpixmap_serno++);

    is_null = false;
    w = bitmap->size().width();
    h = bitmap->size().height();
    this->d = 32;
}

QWindowsDirect2DPlatformPixmap::~QWindowsDirect2DPlatformPixmap()
{

}

void QWindowsDirect2DPlatformPixmap::resize(int width, int height)
{
    Q_D(QWindowsDirect2DPlatformPixmap);

    if (!d->bitmap->resize(width, height)) {
        qWarning("%s: Could not resize bitmap", __FUNCTION__);
        return;
    }

    is_null = false;
    w = width;
    h = height;
    this->d = 32;
}

void QWindowsDirect2DPlatformPixmap::fromImage(const QImage &image,
                                               Qt::ImageConversionFlags flags)
{
    Q_D(QWindowsDirect2DPlatformPixmap);

    if (!d->bitmap->fromImage(image, flags)) {
        qWarning("%s: Could not init from image", __FUNCTION__);
        return;
    }

    is_null = false;
    w = image.width();
    h = image.height();
    this->d = 32;
}

int QWindowsDirect2DPlatformPixmap::metric(QPaintDevice::PaintDeviceMetric metric) const
{
    Q_D(const QWindowsDirect2DPlatformPixmap);

    Q_GUI_EXPORT int qt_paint_device_metric(const QPaintDevice *device, QPaintDevice::PaintDeviceMetric metric);
    return qt_paint_device_metric(d->device.data(), metric);
}

void QWindowsDirect2DPlatformPixmap::fill(const QColor &color)
{
    Q_D(QWindowsDirect2DPlatformPixmap);
    d->bitmap->fill(color);
}

bool QWindowsDirect2DPlatformPixmap::hasAlphaChannel() const
{
    return true;
}

QImage QWindowsDirect2DPlatformPixmap::toImage() const
{
    return toImage(QRect());
}

QImage QWindowsDirect2DPlatformPixmap::toImage(const QRect &rect) const
{
    Q_D(const QWindowsDirect2DPlatformPixmap);

    QWindowsDirect2DPaintEngineSuspender suspender(static_cast<QWindowsDirect2DPaintEngine *>(d->device->paintEngine()));
    return d->bitmap->toImage(rect);
}

QPaintEngine* QWindowsDirect2DPlatformPixmap::paintEngine() const
{
    Q_D(const QWindowsDirect2DPlatformPixmap);
    return d->device->paintEngine();
}

qreal QWindowsDirect2DPlatformPixmap::devicePixelRatio() const
{
    Q_D(const QWindowsDirect2DPlatformPixmap);
    return d->devicePixelRatio;
}

void QWindowsDirect2DPlatformPixmap::setDevicePixelRatio(qreal scaleFactor)
{
    Q_D(QWindowsDirect2DPlatformPixmap);
    d->devicePixelRatio = scaleFactor;
}

QWindowsDirect2DBitmap *QWindowsDirect2DPlatformPixmap::bitmap() const
{
    Q_D(const QWindowsDirect2DPlatformPixmap);
    return d->bitmap;
}

QT_END_NAMESPACE
