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

#ifndef QDIRECTFBBLITTER_H
#define QDIRECTFBBLITTER_H

#include "qdirectfbconvenience.h"

#include <private/qblittable_p.h>

#include <directfb.h>

QT_BEGIN_NAMESPACE

class QDirectFbBlitter : public QBlittable
{
public:
    QDirectFbBlitter(const QSize &size, IDirectFBSurface *surface);
    QDirectFbBlitter(const QSize &size, bool alpha);
    virtual ~QDirectFbBlitter();

    virtual void fillRect(const QRectF &rect, const QColor &color);
    virtual void drawPixmap(const QRectF &rect, const QPixmap &pixmap, const QRectF &subrect);
    void alphaFillRect(const QRectF &rect, const QColor &color, QPainter::CompositionMode cmode);
    void drawPixmapOpacity(const QRectF &rect, const QPixmap &pixmap, const QRectF &subrect, QPainter::CompositionMode cmode, qreal opacity);

    IDirectFBSurface *dfbSurface() const;

    static DFBSurfacePixelFormat alphaPixmapFormat();
    static DFBSurfacePixelFormat pixmapFormat();
    static DFBSurfacePixelFormat selectPixmapFormat(bool withAlpha);

protected:
    virtual QImage *doLock();
    virtual void doUnlock();

    QDirectFBPointer<IDirectFBSurface> m_surface;
    QImage m_image;

    friend class QDirectFbConvenience;

private:
    bool m_premult;
};

class QDirectFbBlitterPlatformPixmap : public QBlittablePixmapData
{
public:
    QBlittable *createBlittable(const QSize &size, bool alpha) const;

    QDirectFbBlitter *dfbBlitter() const;

    virtual bool fromFile(const QString &filename, const char *format,
                          Qt::ImageConversionFlags flags);

private:
    bool fromDataBufferDescription(const DFBDataBufferDescription &);
};

inline QBlittable *QDirectFbBlitterPlatformPixmap::createBlittable(const QSize& size, bool alpha) const
{
    return new QDirectFbBlitter(size, alpha);
}

inline QDirectFbBlitter *QDirectFbBlitterPlatformPixmap::dfbBlitter() const
{
    return static_cast<QDirectFbBlitter*>(blittable());
}

inline IDirectFBSurface *QDirectFbBlitter::dfbSurface() const
{
    return m_surface.data();
}

QT_END_NAMESPACE

#endif // QDIRECTFBBLITTER_H
