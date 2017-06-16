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

#include "linuxsysaccelerometer.h"
#include <QtCore/QDebug>
#include <QtCore/QtGlobal>
#include <QtCore/QFile>
#include <QtCore/QDebug>
#include <QtCore/QTimer>

#include <QtCore/QStringList>

#include <time.h>
#include <errno.h>

char const * const LinuxSysAccelerometer::id("linuxsys.accelerometer");

// This plugin reads the accelerometer from a sys interface.

// test machine (laptop):
// QT_ACCEL_FILEPATH=/sys/devices/platform/lis3lv02d/position
// QT_ACCEL_DATADIVISOR=7
// QT_ACCEL_DELIMITER=,

quint64 produceTimestamp()
{
    struct timespec tv;
    int ok;

#ifdef CLOCK_MONOTONIC_RAW
    ok = clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
    if (ok != 0)
#endif
    ok = clock_gettime(CLOCK_MONOTONIC, &tv);
    Q_ASSERT(ok == 0);

    quint64 result = (tv.tv_sec * 1000000ULL) + (tv.tv_nsec * 0.001); // scale to microseconds
    return result;
}

// TODO get output template from env and apply
// Currently this assumes the output is like:
// (x,y,z) or x,y,z

LinuxSysAccelerometer::LinuxSysAccelerometer(QSensor *sensor)
    : QSensorBackend(sensor)
    , m_timerid(0)
    , path(QString())
    , divisor(0)
    , delimiter(QString())
{
    setReading<QAccelerometerReading>(&m_reading);
    addDataRate(1, 100); // 100Hz
    addOutputRange(-22.418, 22.418, 0.17651); // 2G
// not sure how to retrieve proper range

    path = QString::fromLatin1(qgetenv("QT_ACCEL_FILEPATH"));
    bool ok;
    divisor = QString::fromLatin1(qgetenv("QT_ACCEL_DATADIVISOR")).toInt(&ok);
    if (divisor == 0 || !ok) {
        divisor = 1;
    }
    delimiter = QString::fromLatin1(qgetenv("QT_ACCEL_DELIMITER"));
    file.setFileName(path);
}

LinuxSysAccelerometer::~LinuxSysAccelerometer()
{
}

void LinuxSysAccelerometer::start()
{
    if (m_timerid)
        return;

    if (!openFile())
        return;

    int dataRate = sensor()->dataRate();
    if (dataRate == 0) {
        if (sensor()->availableDataRates().count())
            dataRate = sensor()->availableDataRates().first().second;
        else
            dataRate = 1;
    }

    int interval = 1000 / dataRate;

    if (interval)
        m_timerid = startTimer(interval);
}

void LinuxSysAccelerometer::stop()
{
    if (m_timerid) {
        killTimer(m_timerid);
        m_timerid = 0;
    }
    closeFile();
}

void LinuxSysAccelerometer::poll()
{
    if (!file.isOpen())
        return;

    file.seek(0);
    QString str = file.readLine();
    if (str.isEmpty()) {
        return;
    }
    str = str.simplified();

    if (!str.at(0).isNumber() && str.at(0) != '-') {
        str.remove(0,1);
    }

    if (!str.at(str.size()-1).isNumber()) {
        str.chop(1);
    }

    QStringList accelDataList = str.split(delimiter);

    m_reading.setTimestamp(produceTimestamp());
    m_reading.setX(-accelDataList.at(0).toFloat() / divisor);
    m_reading.setY(-accelDataList.at(1).toFloat() / divisor);
    m_reading.setZ(-accelDataList.at(2).toFloat() / divisor);

    newReadingAvailable();
}

void LinuxSysAccelerometer::timerEvent(QTimerEvent * /*event*/)
{
    poll();
}

bool LinuxSysAccelerometer::openFile()
{
    if (!path.isEmpty()
            && !file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open file" << strerror(errno);
        return false;
    }
    return true;
}

void LinuxSysAccelerometer::closeFile()
{
    file.close();
}


