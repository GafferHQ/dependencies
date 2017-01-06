/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

#ifndef DEMO_TEXT_ITEM_H
#define DEMO_TEXT_ITEM_H

#include <QtGui>
#include "demoitem.h"

class DemoTextItem : public DemoItem
{
public:
    enum TYPE {STATIC_TEXT, DYNAMIC_TEXT};

    DemoTextItem(const QString &text, const QFont &font, const QColor &textColor,
        float textWidth, QGraphicsScene *scene = 0, QGraphicsItem *parent = 0, TYPE type = STATIC_TEXT, const QColor &bgColor = QColor());
    void setText(const QString &text);
    QRectF boundingRect() const; // overridden
    void animationStarted(int id = 0);
    void animationStopped(int id = 0);

protected:
    virtual QImage *createImage(const QMatrix &matrix) const; // overridden
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option = 0, QWidget *widget = 0); // overridden

private:
    float textWidth;
    QString text;
    QFont font;
    QColor textColor;
    QColor bgColor;
    TYPE type;
};

#endif // DEMO_TEXT_ITEM_H

