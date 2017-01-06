/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtOpenVG module of the Qt Toolkit.
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

#include "qwindowsurface_vg_p.h"
#include "qwindowsurface_vgegl_p.h"
#include "qpaintengine_vg_p.h"
#include "qpixmapdata_vg_p.h"
#include "qvg_p.h"

#if !defined(QT_NO_EGL)

#include <QtGui/private/qeglcontext_p.h>
#include <QtGui/private/qwidget_p.h>

QT_BEGIN_NAMESPACE

QVGWindowSurface::QVGWindowSurface(QWidget *window)
    : QWindowSurface(window)
{
    // Create the default type of EGL window surface for windows.
    d_ptr = new QVGEGLWindowSurfaceDirect(this);
}

QVGWindowSurface::QVGWindowSurface
        (QWidget *window, QVGEGLWindowSurfacePrivate *d)
    : QWindowSurface(window), d_ptr(d)
{
}

QVGWindowSurface::~QVGWindowSurface()
{
    delete d_ptr;
}

QPaintDevice *QVGWindowSurface::paintDevice()
{
    return this;
}

void QVGWindowSurface::flush(QWidget *widget, const QRegion &region, const QPoint &offset)
{
    Q_UNUSED(offset);

#ifdef Q_OS_SYMBIAN
    if (window() != widget) {
        // For performance reasons we don't support
        // flushing native child widgets on Symbian.
        // It breaks overlapping native child widget
        // rendering in some cases but we prefer performance.
        return;
    }
#endif

    QWidget *parent = widget->internalWinId() ? widget : widget->nativeParentWidget();
    d_ptr->endPaint(parent, region);
}

#if !defined(Q_WS_QPA)
void QVGWindowSurface::setGeometry(const QRect &rect)
{
    QWindowSurface::setGeometry(rect);
}
#else
void QVGWindowSurface::resize(const QSize &size)
{
            QWindowSurface::resize(size);
}
#endif //!Q_WS_QPA

bool QVGWindowSurface::scroll(const QRegion &area, int dx, int dy)
{
    if (!d_ptr->scroll(window(), area, dx, dy))
        return QWindowSurface::scroll(area, dx, dy);
    return true;
}

void QVGWindowSurface::beginPaint(const QRegion &region)
{
    d_ptr->beginPaint(window());

    // If the window is not opaque, then fill the region we are about
    // to paint with the transparent color.
    if (!qt_widget_private(window())->isOpaque &&
            window()->testAttribute(Qt::WA_TranslucentBackground)) {
        QVGPaintEngine *engine = static_cast<QVGPaintEngine *>
            (d_ptr->paintEngine());
        engine->fillRegion(region, Qt::transparent, d_ptr->surfaceSize());
    }
}

void QVGWindowSurface::endPaint(const QRegion &region)
{
    // Nothing to do here.
    Q_UNUSED(region);
}

QPaintEngine *QVGWindowSurface::paintEngine() const
{
    return d_ptr->paintEngine();
}

QWindowSurface::WindowSurfaceFeatures QVGWindowSurface::features() const
{
    WindowSurfaceFeatures features = PartialUpdates | PreservedContents;
    if (d_ptr->supportsStaticContents())
        features |= StaticContents;
    return features;
}

int QVGWindowSurface::metric(PaintDeviceMetric met) const
{
    return qt_paint_device_metric(window(), met);
}

QT_END_NAMESPACE

#endif
