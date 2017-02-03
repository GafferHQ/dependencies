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

#ifndef QPAINTENGINE_DIRECTFB_P_H
#define QPAINTENGINE_DIRECTFB_P_H

#include <QtGui/qpaintengine.h>
#include <private/qpaintengine_raster_p.h>

#ifndef QT_NO_QWS_DIRECTFB

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

class QDirectFBPaintEnginePrivate;

class QDirectFBPaintEngine : public QRasterPaintEngine
{
    Q_DECLARE_PRIVATE(QDirectFBPaintEngine)
public:
    QDirectFBPaintEngine(QPaintDevice *device);
    virtual ~QDirectFBPaintEngine();

    virtual bool begin(QPaintDevice *device);
    virtual bool end();

    virtual void drawRects(const QRect  *rects, int rectCount);
    virtual void drawRects(const QRectF *rects, int rectCount);

    virtual void fillRect(const QRectF &r, const QBrush &brush);
    virtual void fillRect(const QRectF &r, const QColor &color);

    virtual void drawLines(const QLine *line, int lineCount);
    virtual void drawLines(const QLineF *line, int lineCount);

    virtual void drawImage(const QPointF &p, const QImage &img);
    virtual void drawImage(const QRectF &r, const QImage &pm, const QRectF &sr,
                           Qt::ImageConversionFlags falgs = Qt::AutoColor);

    virtual void drawPixmap(const QPointF &p, const QPixmap &pm);
    virtual void drawPixmap(const QRectF &r, const QPixmap &pixmap, const QRectF &sr);
    virtual void drawTiledPixmap(const QRectF &r, const QPixmap &pm, const QPointF &sr);

    virtual void drawBufferSpan(const uint *buffer, int bufsize,
                                int x, int y, int length, uint const_alpha);

    virtual void stroke(const QVectorPath &path, const QPen &pen);
    virtual void drawPath(const QPainterPath &path);
    virtual void drawPoints(const QPointF *points, int pointCount);
    virtual void drawPoints(const QPoint *points, int pointCount);
    virtual void drawEllipse(const QRectF &rect);
    virtual void drawPolygon(const QPointF *points, int pointCount, PolygonDrawMode mode);
    virtual void drawPolygon(const QPoint *points, int pointCount, PolygonDrawMode mode);
    virtual void drawTextItem(const QPointF &p, const QTextItem &textItem);
    virtual void fill(const QVectorPath &path, const QBrush &brush);
    virtual void drawRoundedRect(const QRectF &rect, qreal xrad, qreal yrad, Qt::SizeMode mode);

    virtual void clipEnabledChanged();
    virtual void brushChanged();
    virtual void penChanged();
    virtual void opacityChanged();
    virtual void compositionModeChanged();
    virtual void renderHintsChanged();
    virtual void transformChanged();

    virtual void setState(QPainterState *state);

    virtual void clip(const QVectorPath &path, Qt::ClipOperation op);
    virtual void clip(const QRegion &region, Qt::ClipOperation op);
    virtual void clip(const QRect &rect, Qt::ClipOperation op);

    virtual void drawStaticTextItem(QStaticTextItem *item);

    static void initImageCache(int size);
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QT_NO_QWS_DIRECTFB

#endif // QPAINTENGINE_DIRECTFB_P_H
