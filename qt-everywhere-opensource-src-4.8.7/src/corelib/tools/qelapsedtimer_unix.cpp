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
#ifdef Q_OS_VXWORKS
#include "qfunctions_vxworks.h"
#else
#include <sys/time.h>
#include <time.h>
#endif
#include <unistd.h>

#if defined(QT_NO_CLOCK_MONOTONIC) || defined(QT_BOOTSTRAPPED)
// turn off the monotonic clock
# ifdef _POSIX_MONOTONIC_CLOCK
#  undef _POSIX_MONOTONIC_CLOCK
# endif
# define _POSIX_MONOTONIC_CLOCK -1
#endif

QT_BEGIN_NAMESPACE

#if (_POSIX_MONOTONIC_CLOCK-0 != 0)
static const bool monotonicClockChecked = true;
static const bool monotonicClockAvailable = _POSIX_MONOTONIC_CLOCK > 0;
#else
static int monotonicClockChecked = false;
static int monotonicClockAvailable = false;
#endif

#ifdef Q_CC_GNU
# define is_likely(x) __builtin_expect((x), 1)
#else
# define is_likely(x) (x)
#endif
#define load_acquire(x) ((volatile const int&)(x))
#define store_release(x,v) ((volatile int&)(x) = (v))

static void unixCheckClockType()
{
#if (_POSIX_MONOTONIC_CLOCK-0 == 0)
    if (is_likely(load_acquire(monotonicClockChecked)))
        return;

# if defined(_SC_MONOTONIC_CLOCK)
    // detect if the system support monotonic timers
    long x = sysconf(_SC_MONOTONIC_CLOCK);
    store_release(monotonicClockAvailable, x >= 200112L);
# endif

    store_release(monotonicClockChecked, true);
#endif
}

static inline qint64 fractionAdjustment()
{
    // disabled, but otherwise indicates bad usage of QElapsedTimer
    //Q_ASSERT(monotonicClockChecked);

    if (monotonicClockAvailable) {
        // the monotonic timer is measured in nanoseconds
        // 1 ms = 1000000 ns
        return 1000*1000ull;
    } else {
        // gettimeofday is measured in microseconds
        // 1 ms = 1000 us
        return 1000;
    }
}

bool QElapsedTimer::isMonotonic()
{
    unixCheckClockType();
    return monotonicClockAvailable;
}

QElapsedTimer::ClockType QElapsedTimer::clockType()
{
    unixCheckClockType();
    return monotonicClockAvailable ? MonotonicClock : SystemTime;
}

static inline void do_gettime(qint64 *sec, qint64 *frac)
{
#if (_POSIX_MONOTONIC_CLOCK-0 >= 0)
    unixCheckClockType();
    if (is_likely(monotonicClockAvailable)) {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        *sec = ts.tv_sec;
        *frac = ts.tv_nsec;
        return;
    }
#endif
    // use gettimeofday
    struct timeval tv;
    ::gettimeofday(&tv, 0);
    *sec = tv.tv_sec;
    *frac = tv.tv_usec;
}

// used in qcore_unix.cpp and qeventdispatcher_unix.cpp
timeval qt_gettime()
{
    qint64 sec, frac;
    do_gettime(&sec, &frac);

    timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = frac;
    if (monotonicClockAvailable)
        tv.tv_usec /= 1000;

    return tv;
}

static qint64 elapsedAndRestart(qint64 sec, qint64 frac,
                                qint64 *nowsec, qint64 *nowfrac)
{
    do_gettime(nowsec, nowfrac);
    sec = *nowsec - sec;
    frac = *nowfrac - frac;
    return sec * Q_INT64_C(1000) + frac / fractionAdjustment();
}

void QElapsedTimer::start()
{
    do_gettime(&t1, &t2);
}

qint64 QElapsedTimer::restart()
{
    return elapsedAndRestart(t1, t2, &t1, &t2);
}

qint64 QElapsedTimer::nsecsElapsed() const
{
    qint64 sec, frac;
    do_gettime(&sec, &frac);
    sec = sec - t1;
    frac = frac - t2;
    if (!monotonicClockAvailable)
        frac *= 1000;
    return sec * Q_INT64_C(1000000000) + frac;
}

qint64 QElapsedTimer::elapsed() const
{
    qint64 sec, frac;
    return elapsedAndRestart(t1, t2, &sec, &frac);
}

qint64 QElapsedTimer::msecsSinceReference() const
{
    return t1 * Q_INT64_C(1000) + t2 / fractionAdjustment();
}

qint64 QElapsedTimer::msecsTo(const QElapsedTimer &other) const
{
    qint64 secs = other.t1 - t1;
    qint64 fraction = other.t2 - t2;
    return secs * Q_INT64_C(1000) + fraction / fractionAdjustment();
}

qint64 QElapsedTimer::secsTo(const QElapsedTimer &other) const
{
    return other.t1 - t1;
}

bool operator<(const QElapsedTimer &v1, const QElapsedTimer &v2)
{
    return v1.t1 < v2.t1 || (v1.t1 == v2.t1 && v1.t2 < v2.t2);
}

QT_END_NAMESPACE
