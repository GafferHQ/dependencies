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

#include "guidecircle.h"

static float PI2 = 2*3.1415f;

GuideCircle::GuideCircle(const QRectF &rect, float startAngle, float span, DIRECTION dir, Guide *follows) : Guide(follows)
{
    this->radiusX = rect.width() / 2.0;
    this->radiusY = rect.height() / 2.0;
    this->posX = rect.topLeft().x();
    this->posY = rect.topLeft().y();
    this->spanRad = span * PI2 / -360.0;
    if (dir == CCW){
        this->startAngleRad = startAngle * PI2 / -360.0;
        this->endAngleRad = startAngleRad + spanRad;
        this->stepAngleRad = this->spanRad / this->length();
    }
    else{
        this->startAngleRad = spanRad + (startAngle * PI2 / -360.0);
        this->endAngleRad = startAngle * PI2 / -360.0;
        this->stepAngleRad = -this->spanRad / this->length();
    }
}

float GuideCircle::length()
{
    return qAbs(this->radiusX * spanRad);
}

QPointF GuideCircle::startPos()
{
    return QPointF((posX + radiusX + radiusX * cos(startAngleRad)) * scaleX,
                   (posY + radiusY + radiusY * sin(startAngleRad)) * scaleY);
}

QPointF GuideCircle::endPos()
{
    return QPointF((posX + radiusX + radiusX * cos(endAngleRad)) * scaleX,
                   (posY + radiusY + radiusY * sin(endAngleRad)) * scaleY);
}

void GuideCircle::guide(DemoItem *item, float moveSpeed)
{
    float frame = item->guideFrame - this->startLength;
    QPointF end((posX + radiusX + radiusX * cos(startAngleRad + (frame * stepAngleRad))) * scaleX,
                (posY + radiusY + radiusY * sin(startAngleRad + (frame * stepAngleRad))) * scaleY);
    this->move(item, end, moveSpeed);
}
