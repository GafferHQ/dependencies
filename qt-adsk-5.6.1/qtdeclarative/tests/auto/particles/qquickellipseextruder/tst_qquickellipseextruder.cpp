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

#include <qmath.h>
#include <QtTest/QtTest>
#include "../shared/particlestestsshared.h"
#include <private/qquickparticlesystem_p.h>
#include <private/qabstractanimation_p.h>

#include "../../shared/util.h"

class tst_qquickellipseextruder : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickellipseextruder() {}

private slots:
    void initTestCase();
    void test_basic();
private:
    bool inCircle(qreal x, qreal y, qreal r, bool borderOnly=false);
};

void tst_qquickellipseextruder::initTestCase()
{
    QQmlDataTest::initTestCase();
    QUnifiedTimer::instance()->setConsistentTiming(true);
}

bool tst_qquickellipseextruder::inCircle(qreal x, qreal y, qreal r, bool borderOnly)
{
    x -= r;
    y -= r;
    if (myFuzzyCompare(x,0) && myFuzzyCompare(y,0))
        return !borderOnly;
    qreal mag = qSqrt(x*x + y*y);
    if (borderOnly)
        return myFuzzyCompare(mag, r); //Need myFuzzyCompare for smaller Epsilon than qFuzzyCompare
    else
        return mag - EPSILON < r;
}

void tst_qquickellipseextruder::test_basic()
{
    QQuickView* view = createView(testFileUrl("basic.qml"), 600);
    QQuickParticleSystem* system = view->rootObject()->findChild<QQuickParticleSystem*>("system");
    ensureAnimTime(600, system->m_animation);

    //Filled
    QVERIFY(extremelyFuzzyCompare(system->groupData[0]->size(), 500, 10));
    foreach (QQuickParticleData *d, system->groupData[0]->data) {
        if (d->t == -1)
            continue; //Particle data unused

        QVERIFY(inCircle(d->x, d->y, 160, false));
        QCOMPARE(d->vx, 0.f);
        QCOMPARE(d->vy, 0.f);
        QCOMPARE(d->ax, 0.f);
        QCOMPARE(d->ay, 0.f);
        QCOMPARE(d->lifeSpan, 0.5f);
        QCOMPARE(d->size, 32.f);
        QCOMPARE(d->endSize, 32.f);
        QVERIFY(myFuzzyLEQ(d->t, ((qreal)system->timeInt/1000.0)));
    }

    //Just border
    QCOMPARE(system->groupData[1]->size(), 500);
    foreach (QQuickParticleData *d, system->groupData[1]->data) {
        if (d->t == -1)
            continue; //Particle data unused

        QVERIFY(inCircle(d->x, d->y, 160, true));
        QCOMPARE(d->vx, 0.f);
        QCOMPARE(d->vy, 0.f);
        QCOMPARE(d->ax, 0.f);
        QCOMPARE(d->ay, 0.f);
        QCOMPARE(d->lifeSpan, 0.5f);
        QCOMPARE(d->size, 32.f);
        QCOMPARE(d->endSize, 32.f);
        QVERIFY(myFuzzyLEQ(d->t, ((qreal)system->timeInt/1000.0)));
    }
    delete view;
}

QTEST_MAIN(tst_qquickellipseextruder);

#include "tst_qquickellipseextruder.moc"
