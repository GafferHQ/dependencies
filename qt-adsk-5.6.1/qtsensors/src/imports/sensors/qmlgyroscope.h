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

#ifndef QMLGYROSCOPE_H
#define QMLGYROSCOPE_H

#include "qmlsensor.h"

QT_BEGIN_NAMESPACE

class QGyroscope;

class QmlGyroscope : public QmlSensor
{
    Q_OBJECT
public:
    explicit QmlGyroscope(QObject *parent = 0);
    ~QmlGyroscope();


private:
    QSensor *sensor() const Q_DECL_OVERRIDE;
    QGyroscope *m_sensor;
    QmlSensorReading *createReading() const Q_DECL_OVERRIDE;
};

class QmlGyroscopeReading : public QmlSensorReading
{
    Q_OBJECT
    Q_PROPERTY(qreal x READ x NOTIFY xChanged)
    Q_PROPERTY(qreal y READ y NOTIFY yChanged)
    Q_PROPERTY(qreal z READ z NOTIFY zChanged)
public:
    explicit QmlGyroscopeReading(QGyroscope *sensor);
    ~QmlGyroscopeReading();

    qreal x() const;
    qreal y() const;
    qreal z() const;

Q_SIGNALS:
    void xChanged();
    void yChanged();
    void zChanged();

private:
    QSensorReading *reading() const Q_DECL_OVERRIDE;
    void readingUpdate() Q_DECL_OVERRIDE;
    QGyroscope *m_sensor;
    qreal m_x;
    qreal m_y;
    qreal m_z;
};

QT_END_NAMESPACE
#endif
