/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

//TESTED_COMPONENT=src/location

#include "qnmeapositioninfosourceproxyfactory.h"
#include "../qgeopositioninfosource/testqgeopositioninfosource_p.h"
#include "../utils/qlocationtestutils_p.h"

#include <QtPositioning/qnmeapositioninfosource.h>
#include <QSignalSpy>
#include <QTest>

Q_DECLARE_METATYPE(QNmeaPositionInfoSource::UpdateMode)
Q_DECLARE_METATYPE(QGeoPositionInfo)

class DummyNmeaPositionInfoSource : public QNmeaPositionInfoSource
{
    Q_OBJECT

public:
    DummyNmeaPositionInfoSource(QNmeaPositionInfoSource::UpdateMode mode, QObject *parent = 0);

protected:
    virtual bool parsePosInfoFromNmeaData(const char *data,
                                          int size,
                                          QGeoPositionInfo *posInfo,
                                          bool *hasFix);

private:
    int callCount;
};

DummyNmeaPositionInfoSource::DummyNmeaPositionInfoSource(QNmeaPositionInfoSource::UpdateMode mode, QObject *parent) :
        QNmeaPositionInfoSource(mode, parent),
        callCount(0)
{
}

bool DummyNmeaPositionInfoSource::parsePosInfoFromNmeaData(const char* data,
                                                           int size,
                                                           QGeoPositionInfo *posInfo,
                                                           bool *hasFix)
{
    Q_UNUSED(data);
    Q_UNUSED(size);

    posInfo->setCoordinate(QGeoCoordinate(callCount * 1.0, callCount * 1.0, callCount * 1.0));
    posInfo->setTimestamp(QDateTime::currentDateTime().toUTC());
    *hasFix = true;
    ++callCount;

    return true;
}

class tst_DummyNmeaPositionInfoSource : public QObject
{
    Q_OBJECT

public:
    tst_DummyNmeaPositionInfoSource();

private slots:
    void initTestCase();
    void testOverloadedParseFunction();
};


tst_DummyNmeaPositionInfoSource::tst_DummyNmeaPositionInfoSource() {}

void tst_DummyNmeaPositionInfoSource::initTestCase()
{
    qRegisterMetaType<QGeoPositionInfo>();
}

void tst_DummyNmeaPositionInfoSource::testOverloadedParseFunction()
{
    DummyNmeaPositionInfoSource source(QNmeaPositionInfoSource::RealTimeMode);
    QNmeaPositionInfoSourceProxyFactory factory;
    QNmeaPositionInfoSourceProxy *proxy = static_cast<QNmeaPositionInfoSourceProxy*>(factory.createProxy(&source));

    QSignalSpy spy(proxy->source(), SIGNAL(positionUpdated(QGeoPositionInfo)));

    QGeoPositionInfo pos;

    proxy->source()->startUpdates();

    proxy->feedBytes(QString("The parser converts\n").toLatin1());

    QTRY_VERIFY_WITH_TIMEOUT((spy.count() == 1), 10000);
    pos = spy.at(0).at(0).value<QGeoPositionInfo>();

    QVERIFY((pos.coordinate().latitude() == 0.0)
        && (pos.coordinate().longitude() == 0.0)
        && (pos.coordinate().altitude() == 0.0));

    spy.clear();

    proxy->feedBytes(QString("any data it receives\n").toLatin1());

    QTRY_VERIFY_WITH_TIMEOUT((spy.count() == 1), 10000);
    pos = spy.at(0).at(0).value<QGeoPositionInfo>();

    QVERIFY((pos.coordinate().latitude() == 1.0)
        && (pos.coordinate().longitude() == 1.0)
        && (pos.coordinate().altitude() == 1.0));

    spy.clear();

    proxy->feedBytes(QString("into positions\n").toLatin1());

    QTRY_VERIFY_WITH_TIMEOUT((spy.count() == 1), 10000);
    pos = spy.at(0).at(0).value<QGeoPositionInfo>();

    QVERIFY((pos.coordinate().latitude() == 2.0)
        && (pos.coordinate().longitude() == 2.0)
        && (pos.coordinate().altitude() == 2.0));

    spy.clear();
}

#include "tst_dummynmeapositioninfosource.moc"

QTEST_GUILESS_MAIN(tst_DummyNmeaPositionInfoSource);
