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

#ifndef TESTQGEOPOSITIONINFOSOURCE_P_H
#define TESTQGEOPOSITIONINFOSOURCE_P_H

#include <QtPositioning/qgeopositioninfosource.h>

#ifdef TST_GEOCLUEMOCK_ENABLED
#include "geocluemock.h"
#include <QThread>
#endif

#include <QTest>
#include <QObject>

QT_BEGIN_NAMESPACE
class QGeoPositionInfoSource;
QT_END_NAMESPACE

class TestQGeoPositionInfoSource : public QObject
{
    Q_OBJECT

public:
    TestQGeoPositionInfoSource(QObject *parent = 0);

    static TestQGeoPositionInfoSource *createDefaultSourceTest();

public slots:
    void test_slot1();
    void test_slot2();

protected:
    virtual QGeoPositionInfoSource *createTestSource() = 0;

    // MUST be called by subclasses if they override respective test slots
    void base_initTestCase();
    void base_init();
    void base_cleanup();
    void base_cleanupTestCase();

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();

    void constructor_withParent();

    void constructor_noParent();

    void updateInterval();

    void setPreferredPositioningMethods();
    void setPreferredPositioningMethods_data();

    void preferredPositioningMethods();

    void createDefaultSource();

    void setUpdateInterval();
    void setUpdateInterval_data();

    void lastKnownPosition();
    void lastKnownPosition_data();

    void minimumUpdateInterval();

    void startUpdates_testIntervals();
    void startUpdates_testIntervalChangesWhileRunning();
    void startUpdates_testDefaultInterval();
    void startUpdates_testZeroInterval();
    void startUpdates_moreThanOnce();

    void stopUpdates();
    void stopUpdates_withoutStart();

    void requestUpdate();
    void requestUpdate_data();

    void requestUpdate_validTimeout();
    void requestUpdate_defaultTimeout();
    void requestUpdate_timeoutLessThanMinimumInterval();
    void requestUpdate_repeatedCalls();
    void requestUpdate_overlappingCalls();

    void requestUpdateAfterStartUpdates_ZeroInterval();
    void requestUpdateAfterStartUpdates_SmallInterval();
    void requestUpdateBeforeStartUpdates_ZeroInterval();
    void requestUpdateBeforeStartUpdates_SmallInterval();

    void removeSlotForRequestTimeout();
    void removeSlotForPositionUpdated();

private:
    QGeoPositionInfoSource *m_source;
    bool m_testingDefaultSource;
    bool m_testSlot2Called;
};

#endif
