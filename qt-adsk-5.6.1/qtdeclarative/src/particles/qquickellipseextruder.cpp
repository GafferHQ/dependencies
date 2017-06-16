/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickellipseextruder_p.h"
#include <qmath.h>
#include <stdlib.h>

QT_BEGIN_NAMESPACE
/*!
    \qmltype EllipseShape
    \instantiates QQuickEllipseExtruder
    \inqmlmodule QtQuick.Particles
    \ingroup qtquick-particles
    \inherits Shape
    \brief Represents an ellipse to other particle system elements

    This shape can be used by Emitter subclasses and Affector subclasses to have
    them act upon an ellipse shaped area.
*/
QQuickEllipseExtruder::QQuickEllipseExtruder(QObject *parent) :
    QQuickParticleExtruder(parent)
  , m_fill(true)
{
}

/*!
    \qmlproperty bool QtQuick.Particles::EllipseShape::fill
    If fill is true the ellipse is filled; otherwise it is just a border.

    Default is true.
*/

QPointF QQuickEllipseExtruder::extrude(const QRectF & r)
{
    qreal theta = ((qreal)rand()/RAND_MAX) * 6.2831853071795862;
    qreal mag = m_fill ? ((qreal)rand()/RAND_MAX) : 1;
    return QPointF(r.x() + r.width()/2 + mag * (r.width()/2) * qCos(theta),
                   r.y() + r.height()/2 + mag * (r.height()/2) * qSin(theta));
}

bool QQuickEllipseExtruder::contains(const QRectF &bounds, const QPointF &point)
{
    if (!bounds.contains(point))
        return false;

    QPointF relPoint(bounds.center() - point);
    qreal xa = relPoint.x()/bounds.width();
    qreal yb = relPoint.y()/bounds.height();
    return  (xa * xa + yb * yb) < 0.25;
}

QT_END_NAMESPACE
