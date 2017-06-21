/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest>
#include <QtQuick>

#include "gifrecorder.h"
#include "eventcapturer.h"

//#define GENERATE_EVENT_CODE

class tst_Gifs : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void tumblerWrap();
    void slider();
    void rangeSlider();
    void busyIndicator();
    void switchGif();
    void button();
    void tabBar();
    void menu();

private:
    void moveSmoothly(QQuickWindow *window, const QPoint &from, const QPoint &to, int movements,
        QEasingCurve::Type easingCurveType = QEasingCurve::OutQuint, int movementDelay = 15);

    QString dataDirPath;
    QDir outputDir;
};

void tst_Gifs::initTestCase()
{
    dataDirPath = QFINDTESTDATA("data");
    QVERIFY(!dataDirPath.isEmpty());
    qInfo() << "data directory:" << dataDirPath;

    outputDir = QDir(QDir::current().filePath("gifs"));
    QVERIFY(outputDir.exists() || QDir::current().mkpath("gifs"));
    qInfo() << "output directory:" << outputDir.absolutePath();
}

void tst_Gifs::moveSmoothly(QQuickWindow *window, const QPoint &from, const QPoint &to,
    int movements, QEasingCurve::Type easingCurveType, int movementDelay)
{
    QEasingCurve curve(easingCurveType);
    int xDifference = to.x() - from.x();
    int yDifference = to.y() - from.y();
    for (int movement = 0; movement < movements; ++movement) {
        QPoint pos = QPoint(
            from.x() + curve.valueForProgress(movement / qreal(qAbs(xDifference))) * xDifference,
            from.y() + curve.valueForProgress(movement / qreal(qAbs(yDifference))) * yDifference);
        QTest::mouseMove(window, pos, movementDelay);
    }
}

void tst_Gifs::tumblerWrap()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(4);
    gifRecorder.setQmlFileName("qtlabscontrols-tumbler-wrap.qml");

    gifRecorder.start();

    // Left as an example. Usually EventCapturer code would be removed after
    // the GIF has been generated.
    QQuickWindow *window = gifRecorder.window();
    EventCapturer eventCapturer;
#ifdef GENERATE_EVENT_CODE
    eventCapturer.setMoveEventTrimFlags(EventCapturer::TrimAll);
    eventCapturer.startCapturing(window, 4000);
#else
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(89, 75), 326);
    QTest::mouseMove(window, QPoint(89, 76), 31);
    QTest::mouseMove(window, QPoint(89, 80), 10);
    QTest::mouseMove(window, QPoint(93, 93), 10);
    QTest::mouseMove(window, QPoint(95, 101), 10);
    QTest::mouseMove(window, QPoint(97, 109), 11);
    QTest::mouseMove(window, QPoint(101, 125), 10);
    QTest::mouseMove(window, QPoint(103, 133), 11);
    QTest::mouseMove(window, QPoint(103, 141), 11);
    QTest::mouseMove(window, QPoint(105, 158), 10);
    QTest::mouseMove(window, QPoint(105, 162), 13);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(105, 162), 0);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(154, 130), 1098);
    QTest::mouseMove(window, QPoint(154, 129), 50);
    QTest::mouseMove(window, QPoint(153, 128), 0);
    QTest::mouseMove(window, QPoint(153, 125), 16);
    QTest::mouseMove(window, QPoint(152, 121), 0);
    QTest::mouseMove(window, QPoint(152, 117), 17);
    QTest::mouseMove(window, QPoint(151, 113), 0);
    QTest::mouseMove(window, QPoint(151, 106), 16);
    QTest::mouseMove(window, QPoint(150, 99), 1);
    QTest::mouseMove(window, QPoint(148, 93), 16);
    QTest::mouseMove(window, QPoint(148, 88), 0);
    QTest::mouseMove(window, QPoint(148, 84), 17);
    QTest::mouseMove(window, QPoint(147, 81), 0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(147, 81), 0);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(147, 74), 550);
    QTest::mouseMove(window, QPoint(147, 75), 17);
    QTest::mouseMove(window, QPoint(147, 76), 17);
    QTest::mouseMove(window, QPoint(147, 80), 0);
    QTest::mouseMove(window, QPoint(148, 85), 16);
    QTest::mouseMove(window, QPoint(148, 92), 0);
    QTest::mouseMove(window, QPoint(148, 103), 17);
    QTest::mouseMove(window, QPoint(150, 119), 17);
    QTest::mouseMove(window, QPoint(151, 138), 16);
    QTest::mouseMove(window, QPoint(151, 145), 1);
    QTest::mouseMove(window, QPoint(153, 151), 16);
    QTest::mouseMove(window, QPoint(153, 157), 0);
    QTest::mouseMove(window, QPoint(153, 163), 17);
    QTest::mouseMove(window, QPoint(153, 167), 0);
    QTest::mouseMove(window, QPoint(155, 171), 17);
    QTest::mouseMove(window, QPoint(155, 175), 0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(155, 175), 0);
#endif

    gifRecorder.waitForFinish();

    foreach (CapturedEvent event, eventCapturer.capturedEvents())
        qDebug().noquote() << event.cppCommand();
}

void tst_Gifs::slider()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(5);
    gifRecorder.setHighQuality(true);
    gifRecorder.setQmlFileName("qtlabscontrols-slider.qml");

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QQuickItem *slider = window->property("slider").value<QQuickItem*>();
    QVERIFY(slider);
    QQuickItem *handle = slider->property("handle").value<QQuickItem*>();
    QVERIFY(handle);

    const QPoint handleCenter = handle->mapToItem(window->contentItem(),
        QPoint(handle->width() / 2, handle->height() / 2)).toPoint();

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, handleCenter, 100);
    QPoint pos1 = handleCenter + QPoint(slider->width() * 0.3, 0);
    moveSmoothly(window, handleCenter, pos1, pos1.x() - handleCenter.x(), QEasingCurve::OutQuint, 10);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, pos1, 20);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, handleCenter, 100);
    const QPoint pos2 = QPoint(slider->width() - handleCenter.x() + slider->property("rightPadding").toInt(), handleCenter.y());
    moveSmoothly(window, pos1, pos2, pos2.x() - pos1.x(), QEasingCurve::OutQuint, 10);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, pos2, 20);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, pos2, 100);
    moveSmoothly(window, pos2, handleCenter, qAbs(handleCenter.x() - pos2.x()), QEasingCurve::OutQuint, 10);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, handleCenter, 20);

    gifRecorder.waitForFinish();
}

void tst_Gifs::rangeSlider()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(7);
    gifRecorder.setHighQuality(true);
    gifRecorder.setQmlFileName("qtlabscontrols-rangeslider.qml");

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QQuickItem *slider = window->property("slider").value<QQuickItem*>();
    QVERIFY(slider);
    QObject *first = slider->property("first").value<QObject*>();
    QVERIFY(first);
    QQuickItem *firstHandle = first->property("handle").value<QQuickItem*>();
    QVERIFY(firstHandle);
    QObject *second = slider->property("second").value<QObject*>();
    QVERIFY(second);
    QQuickItem *secondHandle = second->property("handle").value<QQuickItem*>();
    QVERIFY(secondHandle);

    const QPoint firstCenter = firstHandle->mapToItem(slider,
        QPoint(firstHandle->width() / 2, firstHandle->height() / 2)).toPoint();
    const QPoint secondCenter = secondHandle->mapToItem(slider,
        QPoint(secondHandle->width() / 2, secondHandle->height() / 2)).toPoint();

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, firstCenter, 100);
    const QPoint firstTarget = firstCenter + QPoint(slider->width() * 0.25, 0);
    moveSmoothly(window, firstCenter, firstTarget, firstTarget.x() - firstCenter.x());
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, firstTarget, 20);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, secondCenter, 100);
    const QPoint secondTarget = secondCenter - QPoint(slider->width() * 0.25, 0);
    moveSmoothly(window, secondCenter, secondTarget, qAbs(secondTarget.x() - secondCenter.x()));
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, secondTarget, 20);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, secondTarget, 100);
    moveSmoothly(window, secondTarget, secondCenter, qAbs(secondTarget.x() - secondCenter.x()));
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, secondCenter, 20);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, firstTarget, 100);
    moveSmoothly(window, firstTarget, firstCenter, firstTarget.x() - firstCenter.x());
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, firstCenter, 20);

    gifRecorder.waitForFinish();
}

void tst_Gifs::busyIndicator()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(3);
    gifRecorder.setHighQuality(true);
    gifRecorder.setQmlFileName("qtlabscontrols-busyindicator.qml");

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    // Record nothing for a bit to make it smoother.
    QTest::qWait(400);

    QQuickItem *busyIndicator = window->property("busyIndicator").value<QQuickItem*>();
    QVERIFY(busyIndicator);

    busyIndicator->setProperty("running", true);

    // 800 ms is the duration of one rotation animation cycle for BusyIndicator.
    QTest::qWait(800 * 2);

    busyIndicator->setProperty("running", false);

    gifRecorder.waitForFinish();
}

void tst_Gifs::switchGif()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(3);
    gifRecorder.setQmlFileName("qtlabscontrols-switch.qml");
    gifRecorder.setHighQuality(true);

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.8, window->height() / 2), 0);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.2, window->height() / 2), 800);

    gifRecorder.waitForFinish();
}

void tst_Gifs::button()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(3);
    gifRecorder.setQmlFileName("qtlabscontrols-button.qml");
    gifRecorder.setHighQuality(true);

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() / 2, window->height() / 2), 0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() / 2, window->height() / 2), 700);

    gifRecorder.waitForFinish();
}

void tst_Gifs::tabBar()
{
    const QString qmlFileName = QStringLiteral("qtlabscontrols-tabbar.qml");

    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(4);
    gifRecorder.setQmlFileName(qmlFileName);
    gifRecorder.setHighQuality(true);

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.6, window->height() / 2), 0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.6, window->height() / 2), 50);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.9, window->height() / 2), 400);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.9, window->height() / 2), 50);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.6, window->height() / 2), 800);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.6, window->height() / 2), 50);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.3, window->height() / 2), 400);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.3, window->height() / 2), 50);

    gifRecorder.waitForFinish();
}

void tst_Gifs::menu()
{
    const QString qmlFileName = QStringLiteral("qtlabscontrols-menu.qml");

    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(3);
    gifRecorder.setQmlFileName(qmlFileName);
    gifRecorder.setHighQuality(true);

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    const QQuickItem *fileButton = window->property("fileButton").value<QQuickItem*>();
    QVERIFY(fileButton);

    const QPoint fileButtonCenter = fileButton->mapToScene(QPointF(fileButton->width() / 2, fileButton->height() / 2)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, fileButtonCenter, 0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, fileButtonCenter, 200);

    const QObject *menu = window->property("menu").value<QObject*>();
    QVERIFY(menu);
    const QQuickItem *menuContentItem = menu->property("contentItem").value<QQuickItem*>();
    QVERIFY(menuContentItem);

    const QPoint lastItemPos = menuContentItem->mapToScene(QPointF(menuContentItem->width() / 2, menuContentItem->height() - 10)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, lastItemPos, 1000);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, lastItemPos, 300);

    gifRecorder.waitForFinish();
}

QTEST_MAIN(tst_Gifs)

#include "tst_gifs.moc"
