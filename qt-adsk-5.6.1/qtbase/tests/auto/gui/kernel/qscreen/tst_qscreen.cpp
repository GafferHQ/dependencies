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

#include <qscreen.h>
#include <qpa/qwindowsysteminterface.h>

#include <QtTest/QtTest>

class tst_QScreen: public QObject
{
    Q_OBJECT

private slots:
    void angleBetween_data();
    void angleBetween();
    void transformBetween_data();
    void transformBetween();
    void orientationChange();
};

void tst_QScreen::angleBetween_data()
{
    QTest::addColumn<uint>("oa");
    QTest::addColumn<uint>("ob");
    QTest::addColumn<int>("expected");

    QTest::newRow("Portrait Portrait")
        << uint(Qt::PortraitOrientation)
        << uint(Qt::PortraitOrientation)
        << 0;

    QTest::newRow("Portrait Landscape")
        << uint(Qt::PortraitOrientation)
        << uint(Qt::LandscapeOrientation)
        << 270;

    QTest::newRow("Portrait InvertedPortrait")
        << uint(Qt::PortraitOrientation)
        << uint(Qt::InvertedPortraitOrientation)
        << 180;

    QTest::newRow("Portrait InvertedLandscape")
        << uint(Qt::PortraitOrientation)
        << uint(Qt::InvertedLandscapeOrientation)
        << 90;

    QTest::newRow("InvertedLandscape InvertedPortrait")
        << uint(Qt::InvertedLandscapeOrientation)
        << uint(Qt::InvertedPortraitOrientation)
        << 90;

    QTest::newRow("InvertedLandscape Landscape")
        << uint(Qt::InvertedLandscapeOrientation)
        << uint(Qt::LandscapeOrientation)
        << 180;

    QTest::newRow("Landscape Primary")
        << uint(Qt::LandscapeOrientation)
        << uint(Qt::PrimaryOrientation)
        << QGuiApplication::primaryScreen()->angleBetween(Qt::LandscapeOrientation, QGuiApplication::primaryScreen()->primaryOrientation());
}

void tst_QScreen::angleBetween()
{
    QFETCH( uint, oa );
    QFETCH( uint, ob );
    QFETCH( int, expected );

    Qt::ScreenOrientation a = Qt::ScreenOrientation(oa);
    Qt::ScreenOrientation b = Qt::ScreenOrientation(ob);

    QCOMPARE(QGuiApplication::primaryScreen()->angleBetween(a, b), expected);
    QCOMPARE(QGuiApplication::primaryScreen()->angleBetween(b, a), (360 - expected) % 360);
}

void tst_QScreen::transformBetween_data()
{
    QTest::addColumn<uint>("oa");
    QTest::addColumn<uint>("ob");
    QTest::addColumn<QRect>("rect");
    QTest::addColumn<QTransform>("expected");

    QRect rect(0, 0, 480, 640);

    QTest::newRow("Portrait Portrait")
        << uint(Qt::PortraitOrientation)
        << uint(Qt::PortraitOrientation)
        << rect
        << QTransform();

    QTest::newRow("Portrait Landscape")
        << uint(Qt::PortraitOrientation)
        << uint(Qt::LandscapeOrientation)
        << rect
        << QTransform(0, -1, 1, 0, 0, rect.height());

    QTest::newRow("Portrait InvertedPortrait")
        << uint(Qt::PortraitOrientation)
        << uint(Qt::InvertedPortraitOrientation)
        << rect
        << QTransform(-1, 0, 0, -1, rect.width(), rect.height());

    QTest::newRow("Portrait InvertedLandscape")
        << uint(Qt::PortraitOrientation)
        << uint(Qt::InvertedLandscapeOrientation)
        << rect
        << QTransform(0, 1, -1, 0, rect.width(), 0);

    QTest::newRow("InvertedLandscape InvertedPortrait")
        << uint(Qt::InvertedLandscapeOrientation)
        << uint(Qt::InvertedPortraitOrientation)
        << rect
        << QTransform(0, 1, -1, 0, rect.width(), 0);

    QTest::newRow("InvertedLandscape Landscape")
        << uint(Qt::InvertedLandscapeOrientation)
        << uint(Qt::LandscapeOrientation)
        << rect
        << QTransform(-1, 0, 0, -1, rect.width(), rect.height());

    QTest::newRow("Landscape Primary")
        << uint(Qt::LandscapeOrientation)
        << uint(Qt::PrimaryOrientation)
        << rect
        << QGuiApplication::primaryScreen()->transformBetween(Qt::LandscapeOrientation, QGuiApplication::primaryScreen()->primaryOrientation(), rect);
}

void tst_QScreen::transformBetween()
{
    QFETCH( uint, oa );
    QFETCH( uint, ob );
    QFETCH( QRect, rect );
    QFETCH( QTransform, expected );

    Qt::ScreenOrientation a = Qt::ScreenOrientation(oa);
    Qt::ScreenOrientation b = Qt::ScreenOrientation(ob);

    QCOMPARE(QGuiApplication::primaryScreen()->transformBetween(a, b, rect), expected);
}

void tst_QScreen::orientationChange()
{
    qRegisterMetaType<Qt::ScreenOrientation>("Qt::ScreenOrientation");

    QScreen *screen = QGuiApplication::primaryScreen();

    screen->setOrientationUpdateMask(Qt::LandscapeOrientation | Qt::PortraitOrientation);

    QWindowSystemInterface::handleScreenOrientationChange(screen, Qt::LandscapeOrientation);
    QWindowSystemInterface::flushWindowSystemEvents();
    QTRY_COMPARE(screen->orientation(), Qt::LandscapeOrientation);

    QWindowSystemInterface::handleScreenOrientationChange(screen, Qt::PortraitOrientation);
    QWindowSystemInterface::flushWindowSystemEvents();
    QTRY_COMPARE(screen->orientation(), Qt::PortraitOrientation);

    QSignalSpy spy(screen, SIGNAL(orientationChanged(Qt::ScreenOrientation)));

    QWindowSystemInterface::handleScreenOrientationChange(screen, Qt::InvertedLandscapeOrientation);
    QWindowSystemInterface::flushWindowSystemEvents();
    QWindowSystemInterface::handleScreenOrientationChange(screen, Qt::InvertedPortraitOrientation);
    QWindowSystemInterface::flushWindowSystemEvents();
    QWindowSystemInterface::handleScreenOrientationChange(screen, Qt::LandscapeOrientation);
    QWindowSystemInterface::flushWindowSystemEvents();

    QTRY_COMPARE(screen->orientation(), Qt::LandscapeOrientation);
    QCOMPARE(spy.count(), 1);

    spy.clear();
    QWindowSystemInterface::handleScreenOrientationChange(screen, Qt::InvertedLandscapeOrientation);
    QWindowSystemInterface::flushWindowSystemEvents();
    QTRY_COMPARE(screen->orientation(), Qt::LandscapeOrientation);
    QCOMPARE(spy.count(), 0);

    screen->setOrientationUpdateMask(screen->orientationUpdateMask() | Qt::InvertedLandscapeOrientation);
    QTRY_COMPARE(screen->orientation(), Qt::InvertedLandscapeOrientation);
    QCOMPARE(spy.count(), 1);
}

#include <tst_qscreen.moc>
QTEST_MAIN(tst_QScreen);
