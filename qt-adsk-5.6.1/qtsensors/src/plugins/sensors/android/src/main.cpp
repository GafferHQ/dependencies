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

#include <qplugin.h>
#include <qsensorplugin.h>
#include <qsensorbackend.h>
#include <qsensormanager.h>
#include <qaccelerometer.h>
#include <qcompass.h>
#include "androidaccelerometer.h"
#include "androidcompass.h"
#include "androidgyroscope.h"
#include "androidlight.h"
#include "androidmagnetometer.h"
#include "androidpressure.h"
#include "androidproximity.h"
#include "androidrotation.h"
#include "androidtemperature.h"

using namespace AndroidSensors;

class AndroidSensorPlugin : public QObject, public QSensorPluginInterface, public QSensorBackendFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QSensorPluginInterface/1.0" FILE "plugin.json")
    Q_INTERFACES(QSensorPluginInterface)
public:
    void registerSensors()
    {
        bool accelerometer = false;
        bool magnetometer = false;
        foreach (AndroidSensorType sensor, availableSensors()) {
            switch (sensor) {
            case TYPE_ACCELEROMETER:
                QSensorManager::registerBackend(QAccelerometer::type, QByteArray::number(sensor), this);
                accelerometer = true;
                break;
            case TYPE_AMBIENT_TEMPERATURE:
            case TYPE_TEMPERATURE:
                QSensorManager::registerBackend(QAmbientTemperatureSensor::type, QByteArray::number(sensor), this);
                break;
            case TYPE_GRAVITY:
                break; // add the gravity sensor backend
            case TYPE_GYROSCOPE:
                QSensorManager::registerBackend(QGyroscope::type, QByteArray::number(sensor), this);
                break;
            case TYPE_LIGHT:
                QSensorManager::registerBackend(QLightSensor::type, QByteArray::number(sensor), this);
                break; // add the light sensor backend
            case TYPE_LINEAR_ACCELERATION:
                break; // add the linear acceleration sensor backend
            case TYPE_MAGNETIC_FIELD:
                QSensorManager::registerBackend(QMagnetometer::type, QByteArray::number(sensor), this);
                magnetometer = true;
                break;
            case TYPE_ORIENTATION:
                break; // add the orientation sensor backend
            case TYPE_PRESSURE:
                QSensorManager::registerBackend(QPressureSensor::type, QByteArray::number(sensor), this);
                break;
            case TYPE_PROXIMITY:
                QSensorManager::registerBackend(QProximitySensor::type, QByteArray::number(sensor), this);
                break;
            case TYPE_RELATIVE_HUMIDITY:
                break; // add the relative humidity sensor backend
            case TYPE_ROTATION_VECTOR:
                QSensorManager::registerBackend(QRotationSensor::type, QByteArray::number(sensor), this);
                break;

            case TYPE_GAME_ROTATION_VECTOR:
            case TYPE_GYROSCOPE_UNCALIBRATED:
            case TYPE_MAGNETIC_FIELD_UNCALIBRATED:
            case TYPE_SIGNIFICANT_MOTION:
                break; // add backends for API level 18 sensors
            }
        }
        if (accelerometer && magnetometer)
            QSensorManager::registerBackend(QCompass::type, AndroidCompass::id, this);
    }

    QSensorBackend *createBackend(QSensor *sensor)
    {
        if (sensor->identifier() == AndroidCompass::id)
            return new AndroidCompass(sensor);

        AndroidSensorType type = static_cast<AndroidSensorType>(sensor->identifier().toInt());
        switch (type) {
        case TYPE_ACCELEROMETER: {
            QAccelerometer * const accelerometer = qobject_cast<QAccelerometer *>(sensor);
            AndroidSensors::AndroidSensorType type
             = accelerometer ? AndroidAccelerometer::modeToSensor(accelerometer->accelerationMode())
             : AndroidSensors::TYPE_ACCELEROMETER;
            return new AndroidAccelerometer(type, sensor);
        }
        case TYPE_AMBIENT_TEMPERATURE:
        case TYPE_TEMPERATURE:
            return new AndroidTemperature(type, sensor);
        case TYPE_GRAVITY:
            break; // add the gravity sensor backend
        case TYPE_GYROSCOPE:
            return new AndroidGyroscope(type, sensor);
        case TYPE_LIGHT:
            return new AndroidLight(type, sensor);
        case TYPE_LINEAR_ACCELERATION:
            break; // add the linear acceleration sensor backend
        case TYPE_MAGNETIC_FIELD:
            return new AndroidMagnetometer(type, sensor);
        case TYPE_ORIENTATION:
            break; // add the orientation sensor backend
        case TYPE_PRESSURE:
            return new AndroidPressure(type, sensor);
        case TYPE_PROXIMITY:
            return new AndroidProximity(type, sensor);
        case TYPE_RELATIVE_HUMIDITY:
            break; // add the relative humidity sensor backend
        case TYPE_ROTATION_VECTOR:
            return new AndroidRotation(type, sensor);

        case TYPE_GAME_ROTATION_VECTOR:
        case TYPE_GYROSCOPE_UNCALIBRATED:
        case TYPE_MAGNETIC_FIELD_UNCALIBRATED:
        case TYPE_SIGNIFICANT_MOTION:
            break; // add backends for API level 18 sensors
        }
        return 0;
    }
};

Q_IMPORT_PLUGIN (AndroidSensorPlugin) // automatically register the plugin

#include "main.moc"

