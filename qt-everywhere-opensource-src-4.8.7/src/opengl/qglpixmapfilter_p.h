/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtOpenGL module of the Qt Toolkit.
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

#ifndef QGLPIXMAPFILTER_P_H
#define QGLPIXMAPFILTER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qpixmapfilter_p.h>

#include <QtOpenGL/qgl.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

class QGLPixelBuffer;

QT_MODULE(OpenGL)

class QGLPixmapFilterBase
{
public:
    virtual ~QGLPixmapFilterBase() {}
protected:
    void bindTexture(const QPixmap &src) const;
    void drawImpl(QPainter *painter, const QPointF &pos, const QPixmap &src, const QRectF &srcRect = QRectF()) const;

    virtual bool processGL(QPainter *painter, const QPointF &pos, const QPixmap &src, const QRectF &srcRect) const = 0;
};

template <typename Filter>
class QGLPixmapFilter : public Filter, public QGLPixmapFilterBase
{
public:
    void draw(QPainter *painter, const QPointF &pos, const QPixmap &src, const QRectF &srcRect = QRectF())  const {
        const QRectF source = srcRect.isNull() ? QRectF(src.rect()) : srcRect;
        if (painter)
           drawImpl(painter, pos, src, source);
    }
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QGLPIXMAPFILTER_P_H
