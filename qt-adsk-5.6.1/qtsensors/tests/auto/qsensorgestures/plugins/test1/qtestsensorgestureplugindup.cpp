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

#include <QObject>
#include <QtPlugin>
#include <QStringList>
#include <QTimer>

#include "qtestrecognizerdup.h"
#include "qtest2recognizerdup.h"

#include "qtestsensorgestureplugindup.h"

#include <qsensorgestureplugininterface.h>
#include <qsensorgesturerecognizer.h>
#include <qsensorgesturemanager.h>


QTestSensorGestureDupPlugin::QTestSensorGestureDupPlugin()
{
}

QTestSensorGestureDupPlugin::~QTestSensorGestureDupPlugin()
{
}

/*!
  Describes this gesture's possible gesture signals.
handled through the detected(const QString &) signal.
  */
QStringList QTestSensorGestureDupPlugin::gestureSignals() const
{
    QStringList list;
    Q_FOREACH (const QSensorGestureRecognizer* rec,recognizersList) {
        list.append(rec->gestureSignals());
    }
    return list;
}

QList <QSensorGestureRecognizer *> QTestSensorGestureDupPlugin::createRecognizers()
{
    QSensorGestureRecognizer *sRec = new QTestRecognizerDup(this);
    recognizersList.append(sRec);

    QSensorGestureRecognizer *sRec2 = new QTest2RecognizerDup(this);
    recognizersList.append(sRec2);

    return recognizersList;
}

QStringList QTestSensorGestureDupPlugin::supportedIds() const
{
    QStringList list;
    list << "QtSensors.test.dup";
    list << "QtSensors.test.dup";

    return list;
}

QList<QSensorGestureRecognizer*> QTestSensorGestureDupPlugin::recognizers() const
{
    return recognizersList;
}

QString QTestSensorGestureDupPlugin::name() const
{
    return "TestGesturesDup";
}
