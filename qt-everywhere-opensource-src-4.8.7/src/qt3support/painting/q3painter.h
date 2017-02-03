/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3Support module of the Qt Toolkit.
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

#ifndef Q3PAINTER_H
#define Q3PAINTER_H

#include <QtGui/qpainter.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Qt3SupportLight)

class Q_COMPAT_EXPORT Q3Painter : public QPainter
{
public:
    Q3Painter() : QPainter() { }
    Q3Painter(QPaintDevice *pdev) : QPainter(pdev) { }

    inline void drawRect(const QRect &rect);
    inline void drawRect(int x1, int y1, int w, int h)
    { drawRect(QRect(x1, y1, w, h)); }

    inline void drawRoundRect(const QRect &r, int xround = 25, int yround = 25);
    inline void drawRoundRect(int x, int y, int w, int h, int xround = 25, int yround = 25)
    { drawRoundRect(QRect(x, y, w, h), xround, yround); }

    inline void drawEllipse(const QRect &r);
    inline void drawEllipse(int x, int y, int w, int h)
    { drawEllipse(QRect(x, y, w, h)); }

    inline void drawArc(const QRect &r, int a, int alen);
    inline void drawArc(int x, int y, int w, int h, int a, int alen)
    { drawArc(QRect(x, y, w, h), a, alen); }

    inline void drawPie(const QRect &r, int a, int alen);
    inline void drawPie(int x, int y, int w, int h, int a, int alen)
    { drawPie(QRect(x, y, w, h), a, alen); }

    inline void drawChord(const QRect &r, int a, int alen);
    inline void drawChord(int x, int y, int w, int h, int a, int alen)
    { drawChord(QRect(x, y, w, h), a, alen); }

private:
    QRect adjustedRectangle(const QRect &r);
    
    Q_DISABLE_COPY(Q3Painter)
};

void inline Q3Painter::drawRect(const QRect &r)
{
    QPainter::drawRect(adjustedRectangle(r));
}

void inline Q3Painter::drawEllipse(const QRect &r)
{
    QPainter::drawEllipse(adjustedRectangle(r));
}

void inline Q3Painter::drawRoundRect(const QRect &r, int xrnd, int yrnd)
{
    QPainter::drawRoundRect(adjustedRectangle(r), xrnd, yrnd);
}

void inline Q3Painter::drawArc(const QRect &r, int angle, int arcLength)
{
    QPainter::drawArc(adjustedRectangle(r), angle, arcLength);
}

void inline Q3Painter::drawPie(const QRect &r, int angle, int arcLength)
{
    QPainter::drawPie(adjustedRectangle(r), angle, arcLength);
}

void inline Q3Painter::drawChord(const QRect &r, int angle, int arcLength)
{
    QPainter::drawChord(adjustedRectangle(r), angle, arcLength);
}

QT_END_NAMESPACE

QT_END_HEADER

#endif // Q3PAINTER_H
