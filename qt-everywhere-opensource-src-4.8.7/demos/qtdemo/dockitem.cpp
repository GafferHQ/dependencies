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

#include "dockitem.h"
#include "colors.h"

DockItem::DockItem(ORIENTATION orien, qreal x, qreal y, qreal width, qreal length, QGraphicsScene *scene, QGraphicsItem *parent)
    : DemoItem(scene, parent)
{
    this->orientation = orien;
    this->width = width;
    this->length = length;
    this->setPos(x, y);
    this->setZValue(40);
    this->setupPixmap();
}

void DockItem::setupPixmap()
{
    this->pixmap = new QPixmap(int(this->boundingRect().width()), int(this->boundingRect().height()));
    this->pixmap->fill(QColor(0, 0, 0, 0));
    QPainter painter(this->pixmap);
    // create brush:
    QColor background = Colors::sceneBg1;
    QLinearGradient brush(0, 0, 0, this->boundingRect().height());
    brush.setSpread(QGradient::PadSpread);

    if (this->orientation == DOWN){
        brush.setColorAt(0.0, background);
        brush.setColorAt(0.2, background);
        background.setAlpha(0);
        brush.setColorAt(1.0, background);
    }
    else
        if (this->orientation == UP){
        brush.setColorAt(1.0, background);
        brush.setColorAt(0.8, background);
        background.setAlpha(0);
        brush.setColorAt(0.0, background);
    }
    else
        qWarning("DockItem doesn't support the orientation given!");

    painter.fillRect(0, 0, int(this->boundingRect().width()), int(this->boundingRect().height()), brush);

}

DockItem::~DockItem()
{
    delete this->pixmap;
}

QRectF DockItem::boundingRect() const
{
    if (this->orientation == UP || this->orientation == DOWN)
        return QRectF(0, 0, this->length, this->width);
    else
        return QRectF(0, 0, this->width, this->length);
}

void DockItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->drawPixmap(0, 0, *this->pixmap);
}



