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

#include "test_sensorimpl.h"
#include "test_sensor2impl.h"
#include <qsensorplugin.h>
#include <qsensorbackend.h>
#include <qsensormanager.h>
#include <QFile>
#include <QDebug>
#include <QTest>

class TestSensorPlugin : public QObject,
                         public QSensorPluginInterface,
                         public QSensorChangesInterface,
                         public QSensorBackendFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.qt-project.Qt.QSensorPluginInterface/1.0")
    Q_INTERFACES(QSensorPluginInterface QSensorChangesInterface)
public:
    void registerSensors()
    {
        static bool recursive = false;
        QVERIFY2(!recursive, "Recursively called TestSensorPlugin::registerSensors!");
        if (recursive) return;
        recursive = true;


        // This is bad code. It caused a crash due to recursively calling
        // loadPlugins() in qsensormanager.cpp (because loadPlugins() did
        // not set the pluginsLoaded flag soon enough).
        (void)QSensor::defaultSensorForType(TestSensor::type);

        QSensorManager::registerBackend(TestSensor::type, testsensorimpl::id, this);
        QSensorManager::registerBackend(TestSensor::type, "test sensor 2", this);
        QSensorManager::registerBackend(TestSensor2::type, testsensor2impl::id, this);
    }

    void sensorsChanged()
    {
        // Register a new type on initial load
        // This is testing the "don't emit availableSensorsChanged() too many times" functionality.
        if (!QSensorManager::isBackendRegistered(TestSensor::type, "test sensor 3"))
            QSensorManager::registerBackend(TestSensor::type, "test sensor 3", this);

        // When a sensor of type "a random type" is registered, register another sensor.
        // This is testing the "don't emit availableSensorsChanged() too many times" functionality.
        if (!QSensor::defaultSensorForType("a random type").isEmpty()) {
            if (!QSensorManager::isBackendRegistered("a random type 2", "random.dynamic"))
                QSensorManager::registerBackend("a random type 2", "random.dynamic", this);
        } else {
            if (QSensorManager::isBackendRegistered("a random type 2", "random.dynamic"))
                QSensorManager::unregisterBackend("a random type 2", "random.dynamic");
        }
    }

    QSensorBackend *createBackend(QSensor *sensor)
    {
        if (sensor->identifier() == testsensorimpl::id) {
            return new testsensorimpl(sensor);
        }
        if (sensor->identifier() == testsensor2impl::id) {
            return new testsensor2impl(sensor);
        }

        qWarning() << "Can't create backend" << sensor->identifier();
        return 0;
    }
};

Q_IMPORT_PLUGIN(TestSensorPlugin)

#include "test_sensorplugin.moc"
