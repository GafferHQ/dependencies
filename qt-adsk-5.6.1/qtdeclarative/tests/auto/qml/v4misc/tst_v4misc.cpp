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

#include <private/qv4ssa_p.h>

class tst_v4misc: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void rangeSplitting_1();
    void rangeSplitting_2();
    void rangeSplitting_3();
};

QT_BEGIN_NAMESPACE
// Avoid QHash randomization so that the temp numbering is stable.
extern Q_CORE_EXPORT QBasicAtomicInt qt_qhash_seed; // from qhash.cpp
QT_END_NAMESPACE

using namespace QT_PREPEND_NAMESPACE(QV4::IR);

void tst_v4misc::initTestCase()
{
    qt_qhash_seed.store(0);
    QCOMPARE(qt_qhash_seed.load(), 0);
}

// split between two ranges
void tst_v4misc::rangeSplitting_1()
{
    LifeTimeInterval interval;
    interval.addRange(59, 59);
    interval.addRange(61, 62);
    interval.addRange(64, 64);
    interval.addRange(69, 71);
    interval.validate();
    QCOMPARE(interval.end(), 71);

    LifeTimeInterval newInterval = interval.split(66, 70);
    interval.validate();
    newInterval.validate();
    QVERIFY(newInterval.isSplitFromInterval());

    QCOMPARE(interval.ranges().size(), 3);
    QCOMPARE(interval.ranges()[0].start, 59);
    QCOMPARE(interval.ranges()[0].end, 59);
    QCOMPARE(interval.ranges()[1].start, 61);
    QCOMPARE(interval.ranges()[1].end, 62);
    QCOMPARE(interval.ranges()[2].start, 64);
    QCOMPARE(interval.ranges()[2].end, 64);
    QCOMPARE(interval.end(), 70);

    QCOMPARE(newInterval.ranges().size(), 1);
    QCOMPARE(newInterval.ranges()[0].start, 70);
    QCOMPARE(newInterval.ranges()[0].end, 71);
    QCOMPARE(newInterval.end(), 71);
}

// split in the middle of a range
void tst_v4misc::rangeSplitting_2()
{
    LifeTimeInterval interval;
    interval.addRange(59, 59);
    interval.addRange(61, 64);
    interval.addRange(69, 71);
    interval.validate();
    QCOMPARE(interval.end(), 71);

    LifeTimeInterval newInterval = interval.split(62, 64);
    interval.validate();
    newInterval.validate();
    QVERIFY(newInterval.isSplitFromInterval());

    QCOMPARE(interval.ranges().size(), 2);
    QCOMPARE(interval.ranges()[0].start, 59);
    QCOMPARE(interval.ranges()[0].end, 59);
    QCOMPARE(interval.ranges()[1].start, 61);
    QCOMPARE(interval.ranges()[1].end, 62);
    QCOMPARE(interval.end(), 64);

    QCOMPARE(newInterval.ranges().size(), 2);
    QCOMPARE(newInterval.ranges()[0].start, 64);
    QCOMPARE(newInterval.ranges()[0].end, 64);
    QCOMPARE(newInterval.ranges()[1].start, 69);
    QCOMPARE(newInterval.ranges()[1].end, 71);
    QCOMPARE(newInterval.end(), 71);
}

// split in the middle of a range, and let it never go back to active again
void tst_v4misc::rangeSplitting_3()
{
    LifeTimeInterval interval;
    interval.addRange(59, 59);
    interval.addRange(61, 64);
    interval.addRange(69, 71);
    interval.validate();
    QCOMPARE(interval.end(), 71);

    LifeTimeInterval newInterval = interval.split(64, LifeTimeInterval::InvalidPosition);
    interval.validate();
    newInterval.validate();
    QVERIFY(!newInterval.isValid());

    QCOMPARE(interval.ranges().size(), 2);
    QCOMPARE(interval.ranges()[0].start, 59);
    QCOMPARE(interval.ranges()[0].end, 59);
    QCOMPARE(interval.ranges()[1].start, 61);
    QCOMPARE(interval.ranges()[1].end, 64);
    QCOMPARE(interval.end(), 71);
}

QTEST_MAIN(tst_v4misc)

#include "tst_v4misc.moc"
