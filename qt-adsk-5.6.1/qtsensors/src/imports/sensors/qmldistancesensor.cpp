/****************************************************************************
**
** Copyright (C) 2014 BlackBerry Limited. All rights reserved.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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
#include "qmldistancesensor.h"
#include <QDistanceSensor>

/*!
    \qmltype DistanceSensor
    \instantiates QmlDistanceSensor
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.4
    \inherits Sensor
    \brief The DistanceSensor element reports the distance in cm from an object to the device.

    The DistanceSensor element reports the distance in cm from an object to the device.

    This element wraps the QDistanceSensor class. Please see the documentation for
    QDistanceSensor for details.

    \sa DistanceReading
*/

QmlDistanceSensor::QmlDistanceSensor(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QDistanceSensor(this))
{
}

QmlDistanceSensor::~QmlDistanceSensor()
{
}

QmlSensorReading *QmlDistanceSensor::createReading() const
{
    return new QmlDistanceReading(m_sensor);
}

QSensor *QmlDistanceSensor::sensor() const
{
    return m_sensor;
}

/*!
    \qmltype DistanceReading
    \instantiates QmlDistanceReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.4
    \inherits SensorReading
    \brief The DistanceReading element holds the most recent DistanceSensor reading.

    The DistanceReading element holds the most recent DistanceSensor reading.

    This element wraps the QDistanceReading class. Please see the documentation for
    QDistanceReading for details.

    This element cannot be directly created.
*/

QmlDistanceReading::QmlDistanceReading(QDistanceSensor *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
    , m_distance(0.0)
{
}

QmlDistanceReading::~QmlDistanceReading()
{
}

/*!
    \qmlproperty qreal DistanceReading::distance
    This property holds the distance measurement

    Please see QDistanceReading::distance for information about this property.
*/

qreal QmlDistanceReading::distance() const
{
    return m_distance;
}

QSensorReading *QmlDistanceReading::reading() const
{
    return m_sensor->reading();
}

void QmlDistanceReading::readingUpdate()
{
    qreal distance = m_sensor->reading()->distance();
    if (m_distance != distance) {
        m_distance = distance;
        Q_EMIT distanceChanged();
    }
}
