/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include "sensorfwrotationsensor.h"

char const * const SensorfwRotationSensor::id("sensorfw.rotationsensor");

SensorfwRotationSensor::SensorfwRotationSensor(QSensor *sensor)
    : SensorfwSensorBase(sensor)
    , m_initDone(false)
{
    init();
    setReading<QRotationReading>(&m_reading);
    QRotationSensor *const rotationSensor = qobject_cast<QRotationSensor *>(sensor);
    if (rotationSensor)
        rotationSensor->setHasZ(true);
    sensor->setDataRate(20);//set a default rate
}

void SensorfwRotationSensor::slotDataAvailable(const XYZ& data)
{
    m_reading.setFromEuler(data.x(),data.y(),data.z());
    m_reading.setTimestamp(data.XYZData().timestamp_);
    newReadingAvailable();
}

void SensorfwRotationSensor::slotFrameAvailable(const QVector<XYZ>&  frame)
{
    for (int i=0, l=frame.size(); i<l; i++) {
        slotDataAvailable(frame.at(i));
    }
}

bool SensorfwRotationSensor::doConnect()
{
    Q_ASSERT(m_sensorInterface);
    if (m_bufferSize==1)
       return QObject::connect(m_sensorInterface, SIGNAL(dataAvailable(XYZ)), this, SLOT(slotDataAvailable(XYZ)));
    return QObject::connect(m_sensorInterface, SIGNAL(frameAvailable(QVector<XYZ>)),this, SLOT(slotFrameAvailable(QVector<XYZ>)));
}

QString SensorfwRotationSensor::sensorName() const
{
    return "rotationsensor";
}

void SensorfwRotationSensor::init()
{
    m_initDone = false;
    initSensor<RotationSensorChannelInterface>(m_initDone);
}

void SensorfwRotationSensor::start()
{
    if (reinitIsNeeded)
        init();
    SensorfwSensorBase::start();
}
