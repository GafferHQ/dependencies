/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

// ask for the latest POSIX, just in case
#define _POSIX_C_SOURCE 200809L

#include "qelapsedtimer.h"
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <mach/mach_time.h>
#include <private/qcore_unix_p.h>

QT_BEGIN_NAMESPACE

QElapsedTimer::ClockType QElapsedTimer::clockType() Q_DECL_NOTHROW
{
    return MachAbsoluteTime;
}

bool QElapsedTimer::isMonotonic() Q_DECL_NOTHROW
{
    return true;
}

static mach_timebase_info_data_t info = {0,0};
static qint64 absoluteToNSecs(qint64 cpuTime)
{
    if (info.denom == 0)
        mach_timebase_info(&info);
#ifdef __LP64__
    __uint128_t nsecs = static_cast<__uint128_t>(cpuTime) * info.numer / info.denom;
    return static_cast<qint64>(nsecs);
#else
    qint64 nsecs = cpuTime * info.numer / info.denom;
    return nsecs;
#endif
}

static qint64 absoluteToMSecs(qint64 cpuTime)
{
    return absoluteToNSecs(cpuTime) / 1000000;
}

timespec qt_gettime() Q_DECL_NOTHROW
{
    timespec tv;

    uint64_t cpu_time = mach_absolute_time();
    uint64_t nsecs = absoluteToNSecs(cpu_time);
    tv.tv_sec = nsecs / 1000000000ull;
    tv.tv_nsec = nsecs - (tv.tv_sec * 1000000000ull);
    return tv;
}

void qt_nanosleep(timespec amount)
{
    // Mac doesn't have clock_nanosleep, but it does have nanosleep.
    // nanosleep is POSIX.1-1993

    int r;
    EINTR_LOOP(r, nanosleep(&amount, &amount));
}

void QElapsedTimer::start() Q_DECL_NOTHROW
{
    t1 = mach_absolute_time();
    t2 = 0;
}

qint64 QElapsedTimer::restart() Q_DECL_NOTHROW
{
    qint64 old = t1;
    t1 = mach_absolute_time();
    t2 = 0;

    return absoluteToMSecs(t1 - old);
}

qint64 QElapsedTimer::nsecsElapsed() const Q_DECL_NOTHROW
{
    uint64_t cpu_time = mach_absolute_time();
    return absoluteToNSecs(cpu_time - t1);
}

qint64 QElapsedTimer::elapsed() const Q_DECL_NOTHROW
{
    uint64_t cpu_time = mach_absolute_time();
    return absoluteToMSecs(cpu_time - t1);
}

qint64 QElapsedTimer::msecsSinceReference() const Q_DECL_NOTHROW
{
    return absoluteToMSecs(t1);
}

qint64 QElapsedTimer::msecsTo(const QElapsedTimer &other) const Q_DECL_NOTHROW
{
    return absoluteToMSecs(other.t1 - t1);
}

qint64 QElapsedTimer::secsTo(const QElapsedTimer &other) const Q_DECL_NOTHROW
{
    return msecsTo(other) / 1000;
}

bool operator<(const QElapsedTimer &v1, const QElapsedTimer &v2) Q_DECL_NOTHROW
{
    return v1.t1 < v2.t1;
}

QT_END_NAMESPACE
