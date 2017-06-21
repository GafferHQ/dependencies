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

#include <QtCore/QString>
#include <QtTest/QtTest>

#include "qgeocameracapabilities_p.h"
#include "qgeotiledmap_p.h"

QT_USE_NAMESPACE

class tst_QGeoCameraCapabilities : public QObject
{
    Q_OBJECT

public:
    tst_QGeoCameraCapabilities();

private:
    void populateGeoCameraCapabilitiesData();

private Q_SLOTS:
    void constructorTest_data();
    void constructorTest();
    void minimumZoomLevelTest();
    void maximumZoomLevelTest();
    void supportsBearingTest();
    void supportsRollingTest();
    void supportsTiltingTest();
    void minimumTiltTest();
    void maximumTiltTest();
    void operatorsTest_data();
    void operatorsTest();
    void isValidTest();
};

tst_QGeoCameraCapabilities::tst_QGeoCameraCapabilities()
{
}

void tst_QGeoCameraCapabilities::populateGeoCameraCapabilitiesData(){
    QTest::addColumn<double>("minimumZoomLevel");
    QTest::addColumn<double>("maximumZoomLevel");
    QTest::addColumn<double>("minimumTilt");
    QTest::addColumn<double>("maximumTilt");
    QTest::addColumn<bool>("bearingSupport");
    QTest::addColumn<bool>("rollingSupport");
    QTest::addColumn<bool>("tiltingSupport");
    QTest::newRow("zeros") << 0.0 << 0.0 << 0.0 << 0.0 << false << false << false;
    QTest::newRow("valid") << 1.0 << 2.0 << 0.5 << 1.5 << true << true << true;
    QTest::newRow("negative values") << 0.0 << 0.5 << -0.5 << -0.1 << true << true << true;
}

void tst_QGeoCameraCapabilities::constructorTest_data(){
    populateGeoCameraCapabilitiesData();
}

void tst_QGeoCameraCapabilities::constructorTest()
{
    QFETCH(double, minimumZoomLevel);
    QFETCH(double, maximumZoomLevel);
    QFETCH(double, minimumTilt);
    QFETCH(double, maximumTilt);
    QFETCH(bool, bearingSupport);
    QFETCH(bool, rollingSupport);
    QFETCH(bool, tiltingSupport);

    // contructor test with default values
    QGeoCameraCapabilities cameraCapabilities;
    QGeoCameraCapabilities cameraCapabilities2(cameraCapabilities);
    QCOMPARE(cameraCapabilities.minimumZoomLevel(), cameraCapabilities2.minimumZoomLevel());
    QCOMPARE(cameraCapabilities.maximumZoomLevel(), cameraCapabilities2.maximumZoomLevel());
    QVERIFY2(cameraCapabilities.supportsBearing() == cameraCapabilities2.supportsBearing(), "Copy constructor failed for bearing support");
    QVERIFY2(cameraCapabilities.supportsRolling() == cameraCapabilities2.supportsRolling(), "Copy constructor failed for rolling support ");
    QVERIFY2(cameraCapabilities.supportsTilting() == cameraCapabilities2.supportsTilting(), "Copy constructor failed for tilting support");
    QCOMPARE(cameraCapabilities.minimumTilt(), cameraCapabilities2.minimumTilt());
    QCOMPARE(cameraCapabilities.maximumTilt(), cameraCapabilities2.maximumTilt());

    // constructor test after setting values
    cameraCapabilities.setMinimumZoomLevel(minimumZoomLevel);
    cameraCapabilities.setMaximumZoomLevel(maximumZoomLevel);
    cameraCapabilities.setMinimumTilt(minimumTilt);
    cameraCapabilities.setMaximumTilt(maximumTilt);
    cameraCapabilities.setSupportsBearing(bearingSupport);
    cameraCapabilities.setSupportsRolling(rollingSupport);
    cameraCapabilities.setSupportsTilting(tiltingSupport);

    QGeoCameraCapabilities cameraCapabilities3(cameraCapabilities);
    // test the correctness of the constructor copy
    QCOMPARE(cameraCapabilities3.minimumZoomLevel(), minimumZoomLevel);
    QCOMPARE(cameraCapabilities3.maximumZoomLevel(), maximumZoomLevel);
    QCOMPARE(cameraCapabilities3.minimumTilt(), minimumTilt);
    QCOMPARE(cameraCapabilities3.maximumTilt(), maximumTilt);
    QVERIFY2(cameraCapabilities3.supportsBearing() == bearingSupport, "Copy constructor failed for bearing support");
    QVERIFY2(cameraCapabilities3.supportsRolling() == rollingSupport, "Copy constructor failed for rolling support ");
    QVERIFY2(cameraCapabilities3.supportsTilting() == tiltingSupport, "Copy constructor failed for tilting support");
    // verify that values have not changed after a constructor copy
    QCOMPARE(cameraCapabilities.minimumZoomLevel(), cameraCapabilities3.minimumZoomLevel());
    QCOMPARE(cameraCapabilities.maximumZoomLevel(), cameraCapabilities3.maximumZoomLevel());
    QVERIFY2(cameraCapabilities.supportsBearing() == cameraCapabilities3.supportsBearing(), "Copy constructor failed for bearing support");
    QVERIFY2(cameraCapabilities.supportsRolling() == cameraCapabilities3.supportsRolling(), "Copy constructor failed for rolling support ");
    QVERIFY2(cameraCapabilities.supportsTilting() == cameraCapabilities3.supportsTilting(), "Copy constructor failed for tilting support");
    QCOMPARE(cameraCapabilities.minimumTilt(), cameraCapabilities3.minimumTilt());
    QCOMPARE(cameraCapabilities.maximumTilt(), cameraCapabilities3.maximumTilt());
}

void tst_QGeoCameraCapabilities::minimumZoomLevelTest()
{
    QGeoCameraCapabilities cameraCapabilities;
    cameraCapabilities.setMinimumZoomLevel(1.5);
    QCOMPARE(cameraCapabilities.minimumZoomLevel(), 1.5);

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QCOMPARE(cameraCapabilities2.minimumZoomLevel(), 1.5);
    cameraCapabilities.setMinimumZoomLevel(2.5);
    QCOMPARE(cameraCapabilities2.minimumZoomLevel(), 1.5);
}

void tst_QGeoCameraCapabilities::maximumZoomLevelTest()
{
    QGeoCameraCapabilities cameraCapabilities;
    cameraCapabilities.setMaximumZoomLevel(3.5);
    QCOMPARE(cameraCapabilities.maximumZoomLevel(), 3.5);

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QCOMPARE(cameraCapabilities2.maximumZoomLevel(), 3.5);
    cameraCapabilities.setMaximumZoomLevel(4.5);
    QCOMPARE(cameraCapabilities2.maximumZoomLevel(), 3.5);
}

void tst_QGeoCameraCapabilities::supportsBearingTest(){
    QGeoCameraCapabilities cameraCapabilities;
    QVERIFY(!cameraCapabilities.supportsBearing());
    cameraCapabilities.setSupportsBearing(true);
    QVERIFY2(cameraCapabilities.supportsBearing(), "Camera capabilities should support bearing");

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QVERIFY(cameraCapabilities2.supportsBearing());
    cameraCapabilities.setSupportsBearing(false);
    QVERIFY2(cameraCapabilities2.supportsBearing(), "Camera capabilities should support bearing");
}

void tst_QGeoCameraCapabilities::supportsRollingTest(){
    QGeoCameraCapabilities cameraCapabilities;
    QVERIFY(!cameraCapabilities.supportsRolling());
    cameraCapabilities.setSupportsRolling(true);
    QVERIFY2(cameraCapabilities.supportsRolling(), "Camera capabilities should support rolling");

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QVERIFY(cameraCapabilities2.supportsRolling());
    cameraCapabilities.setSupportsRolling(false);
    QVERIFY2(cameraCapabilities2.supportsRolling(), "Camera capabilities should support rolling");
}

void tst_QGeoCameraCapabilities::supportsTiltingTest(){
    QGeoCameraCapabilities cameraCapabilities;
    QVERIFY(!cameraCapabilities.supportsTilting());
    cameraCapabilities.setSupportsTilting(true);
    QVERIFY2(cameraCapabilities.supportsTilting(), "Camera capabilities should support tilting");

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QVERIFY(cameraCapabilities2.supportsTilting());
    cameraCapabilities.setSupportsTilting(false);
    QVERIFY2(cameraCapabilities2.supportsTilting(), "Camera capabilities should support tilting");
}

void tst_QGeoCameraCapabilities::minimumTiltTest(){
    QGeoCameraCapabilities cameraCapabilities;
    QCOMPARE(cameraCapabilities.minimumTilt(),0.0);
    cameraCapabilities.setMinimumTilt(0.5);
    QCOMPARE(cameraCapabilities.minimumTilt(),0.5);

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QCOMPARE(cameraCapabilities2.minimumTilt(), 0.5);
    cameraCapabilities.setMinimumTilt(1.5);
    QCOMPARE(cameraCapabilities2.minimumTilt(), 0.5);
}

void tst_QGeoCameraCapabilities::maximumTiltTest(){
    QGeoCameraCapabilities cameraCapabilities;
    QCOMPARE(cameraCapabilities.maximumTilt(),0.0);
    cameraCapabilities.setMaximumTilt(1.5);
    QCOMPARE(cameraCapabilities.maximumTilt(),1.5);

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QCOMPARE(cameraCapabilities2.maximumTilt(), 1.5);
    cameraCapabilities.setMaximumTilt(2.5);
    QCOMPARE(cameraCapabilities2.maximumTilt(), 1.5);
}

void tst_QGeoCameraCapabilities::operatorsTest_data(){
    populateGeoCameraCapabilitiesData();
}

void tst_QGeoCameraCapabilities::operatorsTest(){

    QFETCH(double, minimumZoomLevel);
    QFETCH(double, maximumZoomLevel);
    QFETCH(double, minimumTilt);
    QFETCH(double, maximumTilt);
    QFETCH(bool, bearingSupport);
    QFETCH(bool, rollingSupport);
    QFETCH(bool, tiltingSupport);

    QGeoCameraCapabilities cameraCapabilities;
    cameraCapabilities.setMinimumZoomLevel(minimumZoomLevel);
    cameraCapabilities.setMaximumZoomLevel(maximumZoomLevel);
    cameraCapabilities.setMinimumTilt(minimumTilt);
    cameraCapabilities.setMaximumTilt(maximumTilt);
    cameraCapabilities.setSupportsBearing(bearingSupport);
    cameraCapabilities.setSupportsRolling(rollingSupport);
    cameraCapabilities.setSupportsTilting(tiltingSupport);
    QGeoCameraCapabilities cameraCapabilities2;
    cameraCapabilities2 = cameraCapabilities;
    // test the correctness of the assignment
    QCOMPARE(cameraCapabilities2.minimumZoomLevel(), minimumZoomLevel);
    QCOMPARE(cameraCapabilities2.maximumZoomLevel(), maximumZoomLevel);
    QCOMPARE(cameraCapabilities2.minimumTilt(), minimumTilt);
    QCOMPARE(cameraCapabilities2.maximumTilt(), maximumTilt);
    QVERIFY2(cameraCapabilities2.supportsBearing() == bearingSupport, "Copy constructor failed for bearing support");
    QVERIFY2(cameraCapabilities2.supportsRolling() == rollingSupport, "Copy constructor failed for rolling support ");
    QVERIFY2(cameraCapabilities2.supportsTilting() == tiltingSupport, "Copy constructor failed for tilting support");
    // verify that values have not changed after a constructor copy
    QCOMPARE(cameraCapabilities.minimumZoomLevel(), cameraCapabilities2.minimumZoomLevel());
    QCOMPARE(cameraCapabilities.maximumZoomLevel(), cameraCapabilities2.maximumZoomLevel());
    QVERIFY2(cameraCapabilities.supportsBearing() == cameraCapabilities2.supportsBearing(), "Copy constructor failed for bearing support");
    QVERIFY2(cameraCapabilities.supportsRolling() == cameraCapabilities2.supportsRolling(), "Copy constructor failed for rolling support ");
    QVERIFY2(cameraCapabilities.supportsTilting() == cameraCapabilities2.supportsTilting(), "Copy constructor failed for tilting support");
    QCOMPARE(cameraCapabilities.minimumTilt(), cameraCapabilities2.minimumTilt());
    QCOMPARE(cameraCapabilities.maximumTilt(), cameraCapabilities2.maximumTilt());
}

void tst_QGeoCameraCapabilities::isValidTest(){
    QGeoCameraCapabilities cameraCapabilities;
    QVERIFY2(!cameraCapabilities.isValid(), "Camera capabilities should default to invalid");
    cameraCapabilities.setSupportsBearing(true);
    QVERIFY2(cameraCapabilities.isValid(), "Camera capabilities should be valid");

    QGeoCameraCapabilities cameraCapabilities2;
    QVERIFY2(!cameraCapabilities2.isValid(), "Camera capabilities should default to invalid");
    cameraCapabilities2.setSupportsRolling(true);
    QVERIFY2(cameraCapabilities2.isValid(), "Camera capabilities should be valid");

    QGeoCameraCapabilities cameraCapabilities3;
    QVERIFY2(!cameraCapabilities3.isValid(), "Camera capabilities should default to invalid");
    cameraCapabilities3.setSupportsTilting(true);
    QVERIFY2(cameraCapabilities3.isValid(), "Camera capabilities should be valid");

    QGeoCameraCapabilities cameraCapabilities4;
    QVERIFY2(!cameraCapabilities4.isValid(), "Camera capabilities should default to invalid");
    cameraCapabilities4.setMinimumZoomLevel(1.0);
    QVERIFY2(cameraCapabilities4.isValid(), "Camera capabilities should be valid");

    QGeoCameraCapabilities cameraCapabilities5;
    QVERIFY2(!cameraCapabilities5.isValid(), "Camera capabilities should default to invalid");
    cameraCapabilities5.setMaximumZoomLevel(1.5);
    QVERIFY2(cameraCapabilities5.isValid(), "Camera capabilities should be valid");

    QGeoCameraCapabilities cameraCapabilities6;
    QVERIFY2(!cameraCapabilities6.isValid(), "Camera capabilities should default to invalid");
    cameraCapabilities6.setMinimumTilt(0.2);
    QVERIFY2(cameraCapabilities6.isValid(), "Camera capabilities should be valid");

    QGeoCameraCapabilities cameraCapabilities7;
    QVERIFY2(!cameraCapabilities7.isValid(), "Camera capabilities should default to invalid");
    cameraCapabilities7.setMaximumTilt(0.8);
    QVERIFY2(cameraCapabilities7.isValid(), "Camera capabilities should be valid");
}

QTEST_APPLESS_MAIN(tst_QGeoCameraCapabilities)

#include "tst_qgeocameracapabilities.moc"
