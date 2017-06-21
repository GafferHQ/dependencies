/****************************************************************************
**
** Copyright (C) 2013 BogDan Vatra <bogdan@kde.org>
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

#ifndef ANDROIDJNISENSORS_H
#define ANDROIDJNISENSORS_H

#include <jni.h>

#include <QtCore/QVector>
#include <QtCore/QString>

namespace AndroidSensors
{
    // must be in sync with https://developer.android.com/reference/android/hardware/Sensor.html#TYPE_ACCELEROMETER
    enum AndroidSensorType
    {
        TYPE_ACCELEROMETER = 1,
        TYPE_AMBIENT_TEMPERATURE = 13, //Added in API level 14
        TYPE_GAME_ROTATION_VECTOR = 15, //Added in API level 18
        TYPE_GRAVITY = 9,
        TYPE_GYROSCOPE = 4,
        TYPE_GYROSCOPE_UNCALIBRATED = 16, //Added in API level 18
        TYPE_LIGHT = 5,
        TYPE_LINEAR_ACCELERATION = 10,
        TYPE_MAGNETIC_FIELD = 2,
        TYPE_MAGNETIC_FIELD_UNCALIBRATED = 14, //Added in API level 18
        TYPE_ORIENTATION = 3, //This constant was deprecated in API level 8. use SensorManager.getOrientation() instead.
        TYPE_PRESSURE = 6,
        TYPE_PROXIMITY = 8,
        TYPE_RELATIVE_HUMIDITY = 12, //Added in API level 14
        TYPE_ROTATION_VECTOR = 11,
        TYPE_SIGNIFICANT_MOTION = 17, //Added in API level 18
        TYPE_TEMPERATURE = 7 //This constant was deprecated in API level 14. use Sensor.TYPE_AMBIENT_TEMPERATURE instead.
    };

    struct AndroidSensorsListenerInterface
    {
        virtual ~AndroidSensorsListenerInterface() {}
        virtual void onAccuracyChanged(jint accuracy) = 0;
        virtual void onSensorChanged(jlong timestamp, const jfloat *values, uint size) = 0;
    };

    QVector<AndroidSensorType> availableSensors();
    QString sensorDescription(AndroidSensorType sensor);
    qreal sensorMaximumRange(AndroidSensorType sensor);
    bool registerListener(AndroidSensorType sensor, AndroidSensorsListenerInterface *listener, int dataRate = 0);
    bool unregisterListener(AndroidSensorType sensor, AndroidSensorsListenerInterface *listener);
    qreal getCompassAzimuth(jfloat *accelerometerReading, jfloat *magnetometerReading);
}

#endif // ANDROIDJNISENSORS_H
