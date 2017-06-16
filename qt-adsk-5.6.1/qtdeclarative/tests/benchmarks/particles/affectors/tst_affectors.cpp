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

#include <qtest.h>
#include <QtTest/QtTest>
#include "../../../auto/particles/shared/particlestestsshared.h"
#include <private/qquickparticlesystem_p.h>

class tst_affectors : public QObject
{
    Q_OBJECT
public:
    tst_affectors();

private slots:
    void test_basic();
    void test_basic_data();
    void test_filtered();
    void test_filtered_data();
};

tst_affectors::tst_affectors()
{
}

void tst_affectors::test_basic_data()
{
    QTest::addColumn<int> ("dt");
    QTest::newRow("16ms") << 16;
    QTest::newRow("32ms") << 32;
    QTest::newRow("100ms") << 100;
    QTest::newRow("500ms") << 500;
}

void tst_affectors::test_filtered_data()
{
    QTest::addColumn<int> ("dt");
    QTest::newRow("16ms") << 16;
    QTest::newRow("32ms") << 32;
    QTest::newRow("100ms") << 100;
    QTest::newRow("500ms") << 500;
}

void tst_affectors::test_basic()
{
    QFETCH(int, dt);
    QQuickView* view = createView(QCoreApplication::applicationDirPath() + "/data/basic.qml");
    QQuickParticleSystem* system = view->rootObject()->findChild<QQuickParticleSystem*>("system");
    //Pretend we're running, but we manually advance the simulation
    system->m_running = true;
    system->m_animation = 0;
    system->reset();

    int curTime = 1;
    system->updateCurrentTime(curTime);//Fixed point and get init out of the way - including emission

    QBENCHMARK {
        curTime += dt;
        system->updateCurrentTime(curTime);
    }

    int stillAlive = 0;
    QVERIFY(extremelyFuzzyCompare(system->groupData[0]->size(), 1000, 10));//Small simulation variance is permissible.
    foreach (QQuickParticleData *d, system->groupData[0]->data) {
        if (d->t == -1)
            continue; //Particle data unused

        if (d->stillAlive())
            stillAlive++;
        QCOMPARE(d->x, 0.f);
        QCOMPARE(d->y, 0.f);
        QCOMPARE(d->vx, 0.f);
        QCOMPARE(d->vy, 0.f);
        QCOMPARE(d->ax, 0.f);
        QCOMPARE(d->ay, 0.f);
        QCOMPARE(d->size, 32.f);
        QCOMPARE(d->endSize, 32.f);
        QVERIFY(myFuzzyLEQ(d->t, ((qreal)system->timeInt/1000.0)));
    }
    QVERIFY(extremelyFuzzyCompare(stillAlive, 1000, 10));//Small simulation variance is permissible.
    delete view;
}

void tst_affectors::test_filtered()
{
    QFETCH(int, dt);
    QQuickView* view = createView(QCoreApplication::applicationDirPath() + "/data/filtered.qml");
    QQuickParticleSystem* system = view->rootObject()->findChild<QQuickParticleSystem*>("system");
    //Pretend we're running, but we manually advance the simulation
    system->m_running = true;
    system->m_animation = 0;
    system->reset();

    int curTime = 1;
    system->updateCurrentTime(curTime);//Fixed point and get init out of the way - including emission

    QBENCHMARK {
        curTime += dt;
        system->updateCurrentTime(curTime);
    }

    int stillAlive = 0;
    QVERIFY(extremelyFuzzyCompare(system->groupData[1]->size(), 1000, 10));//Small simulation variance is permissible.
    foreach (QQuickParticleData *d, system->groupData[1]->data) {
        if (d->t == -1)
            continue; //Particle data unused

        if (d->stillAlive())
            stillAlive++;
        QCOMPARE(d->x, 160.f);
        QCOMPARE(d->y, 160.f);
        QCOMPARE(d->vx, 0.f);
        QCOMPARE(d->vy, 0.f);
        QCOMPARE(d->ax, 0.f);
        QCOMPARE(d->ay, 0.f);
        QCOMPARE(d->size, 32.f);
        QCOMPARE(d->endSize, 32.f);
        QVERIFY(myFuzzyLEQ(d->t, ((qreal)system->timeInt/1000.0)));
    }
    QVERIFY(extremelyFuzzyCompare(stillAlive, 1000, 10));//Small simulation variance is permissible.
    delete view;
}

QTEST_MAIN(tst_affectors);

#include "tst_affectors.moc"
