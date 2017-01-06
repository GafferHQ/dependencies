/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qelapsedtimer.h"
#include "qpair.h"
#include <e32std.h>
#include <sys/time.h>
#include <hal.h>

QT_BEGIN_NAMESPACE

// return quint64 to avoid sign-extension
static quint64 getMicrosecondFromTick()
{
    static TInt nanokernel_tick_period;
    if (!nanokernel_tick_period)
        HAL::Get(HAL::ENanoTickPeriod, nanokernel_tick_period);

    static quint32 highdword = 0;
    static quint32 lastval = 0;
    quint32 val = User::NTickCount();
    if (val < lastval)
        ++highdword;
    lastval = val;

    return nanokernel_tick_period * (val | (quint64(highdword) << 32));
}

timeval qt_gettime()
{
    timeval tv;
    quint64 now = getMicrosecondFromTick();
    tv.tv_sec = now / 1000000;
    tv.tv_usec = now % 1000000;

    return tv;
}

QElapsedTimer::ClockType QElapsedTimer::clockType()
{
    return TickCounter;
}

bool QElapsedTimer::isMonotonic()
{
    return true;
}

void QElapsedTimer::start()
{
    t1 = getMicrosecondFromTick();
    t2 = 0;
}

qint64 QElapsedTimer::restart()
{
    qint64 oldt1 = t1;
    t1 = getMicrosecondFromTick();
    t2 = 0;
    return (t1 - oldt1) / 1000;
}

qint64 QElapsedTimer::nsecsElapsed() const
{
    return (getMicrosecondFromTick() - t1) * 1000;
}

qint64 QElapsedTimer::elapsed() const
{
    return (getMicrosecondFromTick() - t1) / 1000;
}

qint64 QElapsedTimer::msecsSinceReference() const
{
    return t1 / 1000;
}

qint64 QElapsedTimer::msecsTo(const QElapsedTimer &other) const
{
    return (other.t1 - t1) / 1000;
}

qint64 QElapsedTimer::secsTo(const QElapsedTimer &other) const
{
    return msecsTo(other) / 1000000;
}

bool operator<(const QElapsedTimer &v1, const QElapsedTimer &v2)
{
    return (v1.t1 - v2.t1) < 0;
}

QT_END_NAMESPACE
