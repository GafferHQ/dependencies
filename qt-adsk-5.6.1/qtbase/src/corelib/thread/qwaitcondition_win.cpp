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

#include "qwaitcondition.h"
#include "qnamespace.h"
#include "qmutex.h"
#include "qreadwritelock.h"
#include "qlist.h"
#include "qalgorithms.h"

#ifndef QT_NO_THREAD

#define Q_MUTEX_T void*
#include <private/qmutex_p.h>
#include <private/qreadwritelock_p.h>
#include <qt_windows.h>

QT_BEGIN_NAMESPACE

//***********************************************************************
// QWaitConditionPrivate
// **********************************************************************

class QWaitConditionEvent
{
public:
    inline QWaitConditionEvent() : priority(0), wokenUp(false)
    {
#ifndef Q_OS_WINRT
        event = CreateEvent(NULL, TRUE, FALSE, NULL);
#else
        event = CreateEventEx(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
#endif
    }
    inline ~QWaitConditionEvent() { CloseHandle(event); }
    int priority;
    bool wokenUp;
    HANDLE event;
};

typedef QList<QWaitConditionEvent *> EventQueue;

class QWaitConditionPrivate
{
public:
    QMutex mtx;
    EventQueue queue;
    EventQueue freeQueue;

    QWaitConditionEvent *pre();
    bool wait(QWaitConditionEvent *wce, unsigned long time);
    void post(QWaitConditionEvent *wce, bool ret);
};

QWaitConditionEvent *QWaitConditionPrivate::pre()
{
    mtx.lock();
    QWaitConditionEvent *wce =
        freeQueue.isEmpty() ? new QWaitConditionEvent : freeQueue.takeFirst();
#ifndef Q_OS_WINRT
    wce->priority = GetThreadPriority(GetCurrentThread());
#endif
    wce->wokenUp = false;

    // insert 'wce' into the queue (sorted by priority)
    int index = 0;
    for (; index < queue.size(); ++index) {
        QWaitConditionEvent *current = queue.at(index);
        if (current->priority < wce->priority)
            break;
    }
    queue.insert(index, wce);
    mtx.unlock();

    return wce;
}

bool QWaitConditionPrivate::wait(QWaitConditionEvent *wce, unsigned long time)
{
    // wait for the event
    bool ret = false;
#ifndef Q_OS_WINCE
    switch (WaitForSingleObjectEx(wce->event, time, FALSE)) {
#else
    switch (WaitForSingleObject(wce->event, time)) {
#endif

    default: break;

    case WAIT_OBJECT_0:
        ret = true;
        break;
    }
    return ret;
}

void QWaitConditionPrivate::post(QWaitConditionEvent *wce, bool ret)
{
    mtx.lock();

    // remove 'wce' from the queue
    queue.removeAll(wce);
    ResetEvent(wce->event);
    freeQueue.append(wce);

    // wakeups delivered after the timeout should be forwarded to the next waiter
    if (!ret && wce->wokenUp && !queue.isEmpty()) {
        QWaitConditionEvent *other = queue.first();
        SetEvent(other->event);
        other->wokenUp = true;
    }

    mtx.unlock();
}

//***********************************************************************
// QWaitCondition implementation
//***********************************************************************

QWaitCondition::QWaitCondition()
{
    d = new QWaitConditionPrivate;
}

QWaitCondition::~QWaitCondition()
{
    if (!d->queue.isEmpty()) {
        qWarning("QWaitCondition: Destroyed while threads are still waiting");
        qDeleteAll(d->queue);
    }

    qDeleteAll(d->freeQueue);
    delete d;
}

bool QWaitCondition::wait(QMutex *mutex, unsigned long time)
{
    if (!mutex)
        return false;
    if (mutex->isRecursive()) {
        qWarning("QWaitCondition::wait: Cannot wait on recursive mutexes");
        return false;
    }

    QWaitConditionEvent *wce = d->pre();
    mutex->unlock();

    bool returnValue = d->wait(wce, time);

    mutex->lock();
    d->post(wce, returnValue);

    return returnValue;
}

bool QWaitCondition::wait(QReadWriteLock *readWriteLock, unsigned long time)
{
    if (!readWriteLock || readWriteLock->d->accessCount == 0)
        return false;
    if (readWriteLock->d->accessCount < -1) {
        qWarning("QWaitCondition: cannot wait on QReadWriteLocks with recursive lockForWrite()");
        return false;
    }

    QWaitConditionEvent *wce = d->pre();
    int previousAccessCount = readWriteLock->d->accessCount;
    readWriteLock->unlock();

    bool returnValue = d->wait(wce, time);

    if (previousAccessCount < 0)
        readWriteLock->lockForWrite();
    else
        readWriteLock->lockForRead();
    d->post(wce, returnValue);

    return returnValue;
}

void QWaitCondition::wakeOne()
{
    // wake up the first waiting thread in the queue
    QMutexLocker locker(&d->mtx);
    for (int i = 0; i < d->queue.size(); ++i) {
        QWaitConditionEvent *current = d->queue.at(i);
        if (current->wokenUp)
            continue;
        SetEvent(current->event);
        current->wokenUp = true;
        break;
    }
}

void QWaitCondition::wakeAll()
{
    // wake up the all threads in the queue
    QMutexLocker locker(&d->mtx);
    for (int i = 0; i < d->queue.size(); ++i) {
        QWaitConditionEvent *current = d->queue.at(i);
        SetEvent(current->event);
        current->wokenUp = true;
    }
}

QT_END_NAMESPACE
#endif // QT_NO_THREAD
