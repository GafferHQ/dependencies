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

#include "iosmotionmanager.h"
#include "iosmagnetometer.h"

QT_BEGIN_NAMESPACE

char const * const IOSMagnetometer::id("ios.magnetometer");

IOSMagnetometer::IOSMagnetometer(QSensor *sensor)
    : QSensorBackend(sensor)
    , m_motionManager([QIOSMotionManager sharedManager])
    , m_timer(0)
    , m_returnGeoValues(false)
{
    setReading<QMagnetometerReading>(&m_reading);
    // Technical information about data rate is not found, but
    // seems to be ~70Hz after testing on iPad4:
    addDataRate(1, 70);
    // Output range is +/- 2 gauss (0.0002 tesla) and can sense magnetic fields less than
    // 100 microgauss (1e-08 tesla) Ref: "iOS Sensor Programming", Alasdair, 2012.
    addOutputRange(-0.0002, 0.0002, 1e-08);
}

void IOSMagnetometer::start()
{
    int hz = sensor()->dataRate();
    m_timer = startTimer(1000 / (hz == 0 ? 60 : hz));
    m_returnGeoValues = static_cast<QMagnetometer *>(sensor())->returnGeoValues();

    if (m_returnGeoValues)
        [m_motionManager startDeviceMotionUpdates];
    else
        [m_motionManager startMagnetometerUpdates];
}

void IOSMagnetometer::stop()
{
    if (m_returnGeoValues)
        [m_motionManager stopDeviceMotionUpdates];
    else
        [m_motionManager stopMagnetometerUpdates];
    killTimer(m_timer);
    m_timer = 0;
}

void IOSMagnetometer::timerEvent(QTimerEvent *)
{
    CMMagneticField field;

    if (m_returnGeoValues) {
        CMDeviceMotion *deviceMotion = m_motionManager.deviceMotion;
        CMCalibratedMagneticField calibratedField = deviceMotion.magneticField;
        field = calibratedField.field;
        // skip update if NaN
        if (field.x != field.x || field.y != field.y || field.z != field.z)
            return;
        m_reading.setTimestamp(quint64(deviceMotion.timestamp * 1e6));

        switch (calibratedField.accuracy) {
        case CMMagneticFieldCalibrationAccuracyUncalibrated:
            m_reading.setCalibrationLevel(0.0);
            break;
        case CMMagneticFieldCalibrationAccuracyLow:
            m_reading.setCalibrationLevel(0.3);
            break;
        case CMMagneticFieldCalibrationAccuracyMedium:
            m_reading.setCalibrationLevel(0.6);
            break;
        case CMMagneticFieldCalibrationAccuracyHigh:
            m_reading.setCalibrationLevel(1.0);
            break;
        }
    } else {
        CMMagnetometerData *data = m_motionManager.magnetometerData;
        field = data.magneticField;
        // skip update if NaN
        if (field.x != field.x || field.y != field.y || field.z != field.z)
            return;
        m_reading.setTimestamp(quint64(data.timestamp * 1e6));
        m_reading.setCalibrationLevel(1.0);
    }

    // Convert NSTimeInterval to microseconds and microtesla to tesla:
    m_reading.setX(qreal(field.x) / 1e6);
    m_reading.setY(qreal(field.y) / 1e6);
    m_reading.setZ(qreal(field.z) / 1e6);
    newReadingAvailable();
}

QT_END_NAMESPACE
