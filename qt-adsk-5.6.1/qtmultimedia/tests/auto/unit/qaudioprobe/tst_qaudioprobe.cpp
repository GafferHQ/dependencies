/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include <QtTest/QtTest>
#include <QDebug>

#include <qaudioprobe.h>
#include <qaudiorecorder.h>

//TESTED_COMPONENT=src/multimedia

#include "mockmediaserviceprovider.h"
#include "mockmediarecorderservice.h"
#include "mockmediaobject.h"

QT_USE_NAMESPACE

class tst_QAudioProbe: public QObject
{
    Q_OBJECT

public slots:
    void init();
    void cleanup();

private slots:
    void testNullService();
    void testNullControl();
    void testRecorder();
    void testRecorderDeleteRecorder();
    void testRecorderDeleteProbe();
    void testMediaObject();

private:
    QAudioRecorder *recorder;
    MockMediaRecorderControl *mockMediaRecorderControl;
    MockMediaRecorderService  *mockMediaRecorderService;
    MockMediaServiceProvider *mockProvider;
};

void tst_QAudioProbe::init()
{
    mockMediaRecorderControl = new MockMediaRecorderControl(this);
    mockMediaRecorderService = new MockMediaRecorderService(this, mockMediaRecorderControl);
    mockProvider = new MockMediaServiceProvider(mockMediaRecorderService);
    mockProvider->deleteServiceOnRelease = true;
    recorder = 0;

    QMediaServiceProvider::setDefaultServiceProvider(mockProvider);
}

void tst_QAudioProbe::cleanup()
{
    delete recorder;
    delete mockProvider;
    delete mockMediaRecorderControl;
    mockMediaRecorderControl = 0;
    mockMediaRecorderService = 0;
    mockProvider = 0;
    recorder = 0;
}

void tst_QAudioProbe::testNullService()
{
    mockProvider->service = 0;
    recorder = new QAudioRecorder;

    QVERIFY(!recorder->isAvailable());
    QCOMPARE(recorder->availability(), QMultimedia::ServiceMissing);

    QAudioProbe probe;
    QVERIFY(!probe.isActive());
    QVERIFY(!probe.setSource(recorder));
    QVERIFY(!probe.isActive());
    delete recorder;
    recorder = 0;
    QVERIFY(!probe.isActive());
}


void tst_QAudioProbe::testNullControl()
{
    mockMediaRecorderService->hasControls = false;
    recorder = new QAudioRecorder;

    QVERIFY(!recorder->isAvailable());
    QCOMPARE(recorder->availability(), QMultimedia::ServiceMissing);

    QAudioProbe probe;
    QVERIFY(!probe.isActive());
    QVERIFY(!probe.setSource(recorder));
    QVERIFY(!probe.isActive());
    delete recorder;
    recorder = 0;
    QVERIFY(!probe.isActive());
}

void tst_QAudioProbe::testRecorder()
{
    recorder = new QAudioRecorder;
    QVERIFY(recorder->isAvailable());

    QAudioProbe probe;
    QVERIFY(!probe.isActive());
    QVERIFY(probe.setSource(recorder));
    QVERIFY(probe.isActive());
    probe.setSource((QMediaRecorder*)0);
    QVERIFY(!probe.isActive());
}

void tst_QAudioProbe::testRecorderDeleteRecorder()
{
    recorder = new QAudioRecorder;
    QVERIFY(recorder->isAvailable());

    QAudioProbe probe;
    QVERIFY(!probe.isActive());
    QVERIFY(probe.setSource(recorder));
    QVERIFY(probe.isActive());

    delete recorder;
    recorder = 0;
    QVERIFY(!probe.isActive());
    probe.setSource((QMediaRecorder*)0);
    QVERIFY(!probe.isActive());
}

void tst_QAudioProbe::testRecorderDeleteProbe()
{
    recorder = new QAudioRecorder;
    QVERIFY(recorder->isAvailable());

    QAudioProbe *probe = new QAudioProbe;
    QVERIFY(!probe->isActive());
    QVERIFY(probe->setSource(recorder));
    QVERIFY(probe->isActive());

    delete probe;
    QVERIFY(recorder->isAvailable());
}

void tst_QAudioProbe::testMediaObject()
{
    QMediaObject *object = new MockMediaObject(0, mockMediaRecorderService);
    QVERIFY(object->isAvailable());

    QAudioProbe probe;
    QVERIFY(!probe.isActive());
    QVERIFY(probe.setSource(object));
    QVERIFY(probe.isActive());
    probe.setSource((QMediaObject*)0);
    QVERIFY(!probe.isActive());
    delete object;
}

QTEST_GUILESS_MAIN(tst_QAudioProbe)

#include "tst_qaudioprobe.moc"
