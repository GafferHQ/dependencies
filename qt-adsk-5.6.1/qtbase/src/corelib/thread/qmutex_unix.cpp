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

#include "qplatformdefs.h"
#include "qmutex.h"
#include "qstring.h"
#include "qelapsedtimer.h"

#ifndef QT_NO_THREAD
#include "qatomic.h"
#include "qmutex_p.h"
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#if defined(Q_OS_VXWORKS) && defined(wakeup)
#undef wakeup
#endif

QT_BEGIN_NAMESPACE

static void report_error(int code, const char *where, const char *what)
{
    if (code != 0)
        qWarning("%s: %s failure: %s", where, what, qPrintable(qt_error_string(code)));
}

QMutexPrivate::QMutexPrivate()
    : wakeup(false)
{
    report_error(pthread_mutex_init(&mutex, NULL), "QMutex", "mutex init");
    qt_initialize_pthread_cond(&cond, "QMutex");
}

QMutexPrivate::~QMutexPrivate()
{
    report_error(pthread_cond_destroy(&cond), "QMutex", "cv destroy");
    report_error(pthread_mutex_destroy(&mutex), "QMutex", "mutex destroy");
}

bool QMutexPrivate::wait(int timeout)
{
    report_error(pthread_mutex_lock(&mutex), "QMutex::lock", "mutex lock");
    int errorCode = 0;
    while (!wakeup) {
        if (timeout < 0) {
            errorCode = pthread_cond_wait(&cond, &mutex);
        } else {
            timespec ti;
            qt_abstime_for_timeout(&ti, timeout);
            errorCode = pthread_cond_timedwait(&cond, &mutex, &ti);
        }
        if (errorCode) {
            if (errorCode == ETIMEDOUT) {
                if (wakeup)
                    errorCode = 0;
                break;
            }
            report_error(errorCode, "QMutex::lock()", "cv wait");
        }
    }
    bool ret = wakeup;
    wakeup = false;
    report_error(pthread_mutex_unlock(&mutex), "QMutex::lock", "mutex unlock");
    return ret;
}

void QMutexPrivate::wakeUp() Q_DECL_NOTHROW
{
    report_error(pthread_mutex_lock(&mutex), "QMutex::unlock", "mutex lock");
    wakeup = true;
    report_error(pthread_cond_signal(&cond), "QMutex::unlock", "cv signal");
    report_error(pthread_mutex_unlock(&mutex), "QMutex::unlock", "mutex unlock");
}


QT_END_NAMESPACE

#endif // QT_NO_THREAD
