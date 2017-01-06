/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qwindowsurface_mac_p.h"

#include <private/qt_mac_p.h>
#include <private/qt_cocoa_helpers_mac_p.h>
#include <QtGui/qwidget.h>

QT_BEGIN_NAMESPACE

struct QMacWindowSurfacePrivate
{
    QWidget *widget;
    QPixmap device;
};

QMacWindowSurface::QMacWindowSurface(QWidget *widget)
    : QWindowSurface(widget), d_ptr(new QMacWindowSurfacePrivate)
{
    d_ptr->widget = widget;
}

QMacWindowSurface::~QMacWindowSurface()
{
    delete d_ptr;
}

QPaintDevice *QMacWindowSurface::paintDevice()
{
    return &d_ptr->device;
}

void QMacWindowSurface::flush(QWidget *widget, const QRegion &rgn, const QPoint &offset)
{
    Q_UNUSED(offset);

    // Get a context for the widget.
#ifndef QT_MAC_USE_COCOA
    CGContextRef context;
    CGrafPtr port = GetWindowPort(qt_mac_window_for(widget));
    QDBeginCGContext(port, &context);
#else
    extern CGContextRef qt_mac_graphicsContextFor(QWidget *);
    CGContextRef context = qt_mac_graphicsContextFor(widget);
#endif
    CGContextRetain(context);
    CGContextSaveGState(context);

    // Flip context.
    CGContextTranslateCTM(context, 0, widget->height());
    CGContextScaleCTM(context, 1, -1);

    // Clip to region.
    const QVector<QRect> &rects = rgn.rects();
    for (int i = 0; i < rects.size(); ++i) {
        const QRect &rect = rects.at(i);
        CGContextAddRect(context, CGRectMake(rect.x(), rect.y(), rect.width(), rect.height()));
    }
    CGContextClip(context);

    // Draw the image onto the window.
    const CGRect dest = CGRectMake(0, 0, widget->width(), widget->height());
    const CGImageRef image = d_ptr->device.toMacCGImageRef();
    qt_mac_drawCGImage(context, &dest, image);
    CFRelease(image);

    // Restore context.
    CGContextRestoreGState(context);
#ifndef QT_MAC_USE_COCOA
    QDEndCGContext(port, &context);
#else
    CGContextFlush(context);
#endif
    CGContextRelease(context);
}

void QMacWindowSurface::setGeometry(const QRect &rect)
{
    QWindowSurface::setGeometry(rect);
    const QSize size = rect.size();
    if (d_ptr->device.size() != size)
        d_ptr->device = QPixmap(size);
}

bool QMacWindowSurface::scroll(const QRegion &area, int dx, int dy)
{
    if (d_ptr->device.size().isNull())
        return false;

    QCFType<CGImageRef> image = d_ptr->device.toMacCGImageRef();
    const QRect rect(area.boundingRect());
    const CGRect dest = CGRectMake(rect.x(), rect.y(), rect.width(), rect.height());
    QCFType<CGImageRef> subimage = CGImageCreateWithImageInRect(image, dest);
    QCFType<CGContextRef> context = qt_mac_cg_context(&d_ptr->device);
    CGContextTranslateCTM(context, dx, dy);
    qt_mac_drawCGImage(context, &dest, subimage);
    return true;
}

QT_END_NAMESPACE
