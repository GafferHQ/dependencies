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


#include "sensorfwcompass.h"

char const * const SensorfwCompass::id("sensorfw.compass");

SensorfwCompass::SensorfwCompass(QSensor *sensor)
    : SensorfwSensorBase(sensor)
    , m_initDone(false)
{
    init();
    setReading<QCompassReading>(&m_reading);
    sensor->setDataRate(50);//set a default rate
}

void SensorfwCompass::slotDataAvailable(const Compass& data)
{
    // The scale for level is [0,3], where 3 is the best
    // Qt: Measured as a value from 0 to 1 with higher values being better.
    m_reading.setCalibrationLevel(((float) data.level()) / 3.0);

    // The scale for degrees from sensord is [0,359]
    // Value can be directly used as azimuth
    m_reading.setAzimuth(data.degrees());

    m_reading.setTimestamp(data.data().timestamp_);
    newReadingAvailable();
}


bool SensorfwCompass::doConnect()
{
    Q_ASSERT(m_sensorInterface);
    return QObject::connect(m_sensorInterface, SIGNAL(dataAvailable(Compass)),
                            this, SLOT(slotDataAvailable(Compass)));
}

QString SensorfwCompass::sensorName() const
{
    return "compasssensor";
}

void SensorfwCompass::init()
{
    m_initDone = false;
    initSensor<CompassSensorChannelInterface>(m_initDone);
}

void SensorfwCompass::start()
{
    if (reinitIsNeeded)
        init();
    SensorfwSensorBase::start();
}
