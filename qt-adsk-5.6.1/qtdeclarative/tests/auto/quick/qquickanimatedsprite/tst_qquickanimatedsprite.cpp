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
#include <QtTest/QtTest>
#include "../../shared/util.h"
#include <QtQuick/qquickview.h>
#include <private/qabstractanimation_p.h>
#include <private/qquickanimatedsprite_p.h>
#include <private/qquickitem_p.h>
#include <QtGui/qpainter.h>
#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglfunctions.h>
#include <QtGui/qoffscreensurface.h>

class tst_qquickanimatedsprite : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickanimatedsprite(){}

private slots:
    void initTestCase();
    void test_properties();
    void test_runningChangedSignal();
    void test_frameChangedSignal();
    void test_largeAnimation_data();
    void test_largeAnimation();
    void test_reparenting();
};

void tst_qquickanimatedsprite::initTestCase()
{
    QQmlDataTest::initTestCase();
    QUnifiedTimer::instance()->setConsistentTiming(true);
}

void tst_qquickanimatedsprite::test_properties()
{
    QQuickView *window = new QQuickView(0);

    window->setSource(testFileUrl("basic.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QVERIFY(window->rootObject());
    QQuickAnimatedSprite* sprite = window->rootObject()->findChild<QQuickAnimatedSprite*>("sprite");
    QVERIFY(sprite);

    QTRY_VERIFY(sprite->running());
    QVERIFY(!sprite->paused());
    QVERIFY(sprite->interpolate());
    QCOMPARE(sprite->loops(), 30);

    sprite->setRunning(false);
    QVERIFY(!sprite->running());
    sprite->setInterpolate(false);
    QVERIFY(!sprite->interpolate());

    delete window;
}

void tst_qquickanimatedsprite::test_runningChangedSignal()
{
    QQuickView *window = new QQuickView(0);

    window->setSource(testFileUrl("runningChange.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QVERIFY(window->rootObject());
    QQuickAnimatedSprite* sprite = window->rootObject()->findChild<QQuickAnimatedSprite*>("sprite");
    QVERIFY(sprite);

    QVERIFY(!sprite->running());

    QSignalSpy runningChangedSpy(sprite, SIGNAL(runningChanged(bool)));
    sprite->setRunning(true);
    QTRY_COMPARE(runningChangedSpy.count(), 1);
    QTRY_VERIFY(!sprite->running());
    QTRY_COMPARE(runningChangedSpy.count(), 2);

    delete window;
}

template <typename T>
static bool isWithinRange(T min, T value, T max)
{
    Q_ASSERT(min < max);
    return min <= value && value <= max;
}

void tst_qquickanimatedsprite::test_frameChangedSignal()
{
    QQuickView *window = new QQuickView(0);

    window->setSource(testFileUrl("frameChange.qml"));
    window->show();

    QVERIFY(window->rootObject());
    QQuickAnimatedSprite* sprite = window->rootObject()->findChild<QQuickAnimatedSprite*>("sprite");
    QSignalSpy frameChangedSpy(sprite, SIGNAL(currentFrameChanged(int)));
    QVERIFY(sprite);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QVERIFY(!sprite->running());
    QVERIFY(!sprite->paused());
    QCOMPARE(sprite->loops(), 3);
    QCOMPARE(sprite->frameCount(), 6);
    QCOMPARE(frameChangedSpy.count(), 0);

    frameChangedSpy.clear();
    sprite->setRunning(true);
    QTRY_VERIFY(!sprite->running());
    QCOMPARE(frameChangedSpy.count(), 3*6 + 1);

    int prevFrame = -1;
    int loopCounter = 0;
    int maxFrame = 0;
    while (!frameChangedSpy.isEmpty()) {
        QList<QVariant> args = frameChangedSpy.takeFirst();
        int frame = args.first().toInt();
        if (frame < prevFrame) {
            ++loopCounter;
        } else {
            QVERIFY(frame > prevFrame);
        }
        maxFrame = qMax(frame, maxFrame);
        prevFrame = frame;
    }
    QCOMPARE(loopCounter, 3);

    delete window;
}

void tst_qquickanimatedsprite::test_largeAnimation_data()
{
    QTest::addColumn<bool>("frameSync");

    QTest::newRow("frameSync") << true;
    QTest::newRow("no_frameSync") << false;

}

class AnimationImageProvider : public QQuickImageProvider
{
public:
    AnimationImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
    }

    QPixmap requestPixmap(const QString &/*id*/, QSize *size, const QSize &requestedSize)
    {
        if (requestedSize.isValid())
            qWarning() << "requestPixmap called with requestedSize of" << requestedSize;
        // 40 frames.
        const int nFrames = 40; // 40 is good for texture max width of 4096, 64 is good for 16384

        const int frameWidth = 512;
        const int frameHeight = 64;

        const QSize pixSize(frameWidth, nFrames * frameHeight);
        QPixmap pixmap(pixSize);
        pixmap.fill();

        for (int i = 0; i < nFrames; ++i) {
            QImage frame(frameWidth, frameHeight, QImage::Format_ARGB32_Premultiplied);
            frame.fill(Qt::white);
            QPainter p1(&frame);
            p1.setRenderHint(QPainter::Antialiasing, true);
            QRect r(0,0, frameWidth, frameHeight);
            p1.setBrush(QBrush(Qt::red, Qt::SolidPattern));
            p1.drawPie(r, i*360*16/nFrames, 90*16);
            p1.drawText(r, QString::number(i));
            p1.end();

            QPainter p2(&pixmap);
            p2.drawImage(0, i * frameHeight, frame);
        }

        if (size)
            *size = pixSize;
        return pixmap;
    }
};

void tst_qquickanimatedsprite::test_largeAnimation()
{
    QFETCH(bool, frameSync);

    QQuickView *window = new QQuickView(0);
    window->engine()->addImageProvider(QLatin1String("test"), new AnimationImageProvider);
    window->setSource(testFileUrl("largeAnimation.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QVERIFY(window->rootObject());
    QQuickAnimatedSprite* sprite = window->rootObject()->findChild<QQuickAnimatedSprite*>("sprite");

    QVERIFY(sprite);

    QVERIFY(!sprite->running());
    QVERIFY(!sprite->paused());
    QCOMPARE(sprite->loops(), 3);
    QCOMPARE(sprite->frameCount(), 40);
    sprite->setProperty("frameSync", frameSync);
    if (!frameSync)
        sprite->setProperty("frameDuration", 30);

    QSignalSpy frameChangedSpy(sprite, SIGNAL(currentFrameChanged(int)));
    sprite->setRunning(true);
    QTRY_VERIFY_WITH_TIMEOUT(!sprite->running(), 100000 /* make sure we wait until its done*/ );
    if (frameSync)
        QVERIFY(isWithinRange(3*40, frameChangedSpy.count(), 3*40 + 1));
    int prevFrame = -1;
    int loopCounter = 0;
    int maxFrame = 0;
    while (!frameChangedSpy.isEmpty()) {
        QList<QVariant> args = frameChangedSpy.takeFirst();
        int frame = args.first().toInt();
        if (frame < prevFrame) {
            ++loopCounter;
        } else {
            QVERIFY(frame > prevFrame);
        }
        maxFrame = qMax(frame, maxFrame);
        prevFrame = frame;
    }
    int maxTextureSize;
    QOpenGLContext ctx;
    ctx.create();
    QOffscreenSurface offscreenSurface;
    offscreenSurface.setFormat(ctx.format());
    offscreenSurface.create();
    QVERIFY(ctx.makeCurrent(&offscreenSurface));
    ctx.functions()->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    ctx.doneCurrent();
    maxTextureSize /= 512;
    QVERIFY(maxFrame > maxTextureSize); // make sure we go beyond the texture width limitation
    QCOMPARE(loopCounter, sprite->loops());
    delete window;
}

void tst_qquickanimatedsprite::test_reparenting()
{
    QQuickView window;
    window.setSource(testFileUrl("basic.qml"));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(window.rootObject());
    QQuickAnimatedSprite* sprite = window.rootObject()->findChild<QQuickAnimatedSprite*>("sprite");
    QVERIFY(sprite);

    QTRY_VERIFY(sprite->running());
    sprite->setParentItem(0);

    sprite->setParentItem(window.rootObject());
    // don't crash (QTBUG-51162)
    sprite->polish();
    QTRY_COMPARE(QQuickItemPrivate::get(sprite)->polishScheduled, true);
    QTRY_COMPARE(QQuickItemPrivate::get(sprite)->polishScheduled, false);
}

QTEST_MAIN(tst_qquickanimatedsprite)

#include "tst_qquickanimatedsprite.moc"
