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

#include "qthread.h"

#include "qplatformdefs.h"

#include <private/qcoreapplication_p.h>

#if defined(Q_OS_BLACKBERRY)
#  include <private/qeventdispatcher_blackberry_p.h>
#else
#  if !defined(QT_NO_GLIB)
#    include "../kernel/qeventdispatcher_glib_p.h"
#  endif
#  include <private/qeventdispatcher_unix_p.h>
#endif

#include "qthreadstorage.h"

#include "qthread_p.h"

#include "qdebug.h"

#include <sched.h>
#include <errno.h>

#ifdef Q_OS_BSD4
#include <sys/sysctl.h>
#endif
#ifdef Q_OS_VXWORKS
#  if (_WRS_VXWORKS_MAJOR > 6) || ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR >= 6))
#    include <vxCpuLib.h>
#    include <cpuset.h>
#    define QT_VXWORKS_HAS_CPUSET
#  endif
#endif

#ifdef Q_OS_HPUX
#include <sys/pstat.h>
#endif

#if defined(Q_OS_MAC)
# ifdef qDebug
#   define old_qDebug qDebug
#   undef qDebug
# endif
#if !defined(Q_OS_IOS)
# include <CoreServices/CoreServices.h>
#endif // !defined(Q_OS_IOS)

# ifdef old_qDebug
#   undef qDebug
#   define qDebug QT_NO_QDEBUG_MACRO
#   undef old_qDebug
# endif
#endif

#if defined(Q_OS_LINUX) && !defined(QT_LINUXBASE)
#include <sys/prctl.h>
#endif

#if defined(Q_OS_LINUX) && !defined(SCHED_IDLE)
// from linux/sched.h
# define SCHED_IDLE    5
#endif

#if defined(Q_OS_DARWIN) || !defined(Q_OS_OPENBSD) && defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && (_POSIX_THREAD_PRIORITY_SCHEDULING-0 >= 0)
#define QT_HAS_THREAD_PRIORITY_SCHEDULING
#endif


QT_BEGIN_NAMESPACE

#ifndef QT_NO_THREAD

enum { ThreadPriorityResetFlag = 0x80000000 };

#if defined(Q_OS_LINUX) && defined(__GLIBC__) && (defined(Q_CC_GNU) || defined(Q_CC_INTEL))
#define HAVE_TLS
#endif
#if defined(Q_CC_XLC) || defined (Q_CC_SUN)
#define HAVE_TLS
#endif

#ifdef HAVE_TLS
static __thread QThreadData *currentThreadData = 0;
#endif

static pthread_once_t current_thread_data_once = PTHREAD_ONCE_INIT;
static pthread_key_t current_thread_data_key;

static void destroy_current_thread_data(void *p)
{
#if defined(Q_OS_VXWORKS)
    // Calling setspecific(..., 0) sets the value to 0 for ALL threads.
    // The 'set to 1' workaround adds a bit of an overhead though,
    // since this function is called twice now.
    if (p == (void *)1)
        return;
#endif
    // POSIX says the value in our key is set to zero before calling
    // this destructor function, so we need to set it back to the
    // right value...
    pthread_setspecific(current_thread_data_key, p);
    QThreadData *data = static_cast<QThreadData *>(p);
    if (data->isAdopted) {
        QThread *thread = data->thread;
        Q_ASSERT(thread);
        QThreadPrivate *thread_p = static_cast<QThreadPrivate *>(QObjectPrivate::get(thread));
        Q_ASSERT(!thread_p->finished);
        thread_p->finish(thread);
    }
    data->deref();

    // ... but we must reset it to zero before returning so we aren't
    // called again (POSIX allows implementations to call destructor
    // functions repeatedly until all values are zero)
    pthread_setspecific(current_thread_data_key,
#if defined(Q_OS_VXWORKS)
                                                 (void *)1);
#else
                                                 0);
#endif
}

static void create_current_thread_data_key()
{
    pthread_key_create(&current_thread_data_key, destroy_current_thread_data);
}

static void destroy_current_thread_data_key()
{
    pthread_once(&current_thread_data_once, create_current_thread_data_key);
    pthread_key_delete(current_thread_data_key);

    // Reset current_thread_data_once in case we end up recreating
    // the thread-data in the rare case of QObject construction
    // after destroying the QThreadData.
    pthread_once_t pthread_once_init = PTHREAD_ONCE_INIT;
    current_thread_data_once = pthread_once_init;
}
Q_DESTRUCTOR_FUNCTION(destroy_current_thread_data_key)


// Utility functions for getting, setting and clearing thread specific data.
static QThreadData *get_thread_data()
{
#ifdef HAVE_TLS
    return currentThreadData;
#else
    pthread_once(&current_thread_data_once, create_current_thread_data_key);
    return reinterpret_cast<QThreadData *>(pthread_getspecific(current_thread_data_key));
#endif
}

static void set_thread_data(QThreadData *data)
{
#ifdef HAVE_TLS
    currentThreadData = data;
#endif
    pthread_once(&current_thread_data_once, create_current_thread_data_key);
    pthread_setspecific(current_thread_data_key, data);
}

static void clear_thread_data()
{
#ifdef HAVE_TLS
    currentThreadData = 0;
#endif
    pthread_setspecific(current_thread_data_key, 0);
}

void QThreadData::clearCurrentThreadData()
{
    clear_thread_data();
}

QThreadData *QThreadData::current()
{
    QThreadData *data = get_thread_data();
    if (!data) {
        void *a;
        if (QInternal::activateCallbacks(QInternal::AdoptCurrentThread, &a)) {
            QThread *adopted = static_cast<QThread*>(a);
            Q_ASSERT(adopted);
            data = QThreadData::get2(adopted);
            set_thread_data(data);
            adopted->d_func()->running = true;
            adopted->d_func()->finished = false;
            static_cast<QAdoptedThread *>(adopted)->init();
        } else {
            data = new QThreadData;
            QT_TRY {
                set_thread_data(data);
                data->thread = new QAdoptedThread(data);
            } QT_CATCH(...) {
                clear_thread_data();
                data->deref();
                data = 0;
                QT_RETHROW;
            }
            data->deref();
        }
        data->isAdopted = true;
        data->threadId = (Qt::HANDLE)pthread_self();
        if (!QCoreApplicationPrivate::theMainThread)
            QCoreApplicationPrivate::theMainThread = data->thread;
    }
    return data;
}


void QAdoptedThread::init()
{
    Q_D(QThread);
    d->thread_id = pthread_self();
}

/*
   QThreadPrivate
*/

#if defined(Q_C_CALLBACKS)
extern "C" {
#endif

typedef void*(*QtThreadCallback)(void*);

#if defined(Q_C_CALLBACKS)
}
#endif

#endif // QT_NO_THREAD

void QThreadPrivate::createEventDispatcher(QThreadData *data)
{
    QMutexLocker l(&data->postEventList.mutex);

#if defined(Q_OS_BLACKBERRY)
    data->eventDispatcher = new QEventDispatcherBlackberry;
#else
#if !defined(QT_NO_GLIB)
    if (qgetenv("QT_NO_GLIB").isEmpty()
        && qgetenv("QT_NO_THREADED_GLIB").isEmpty()
        && QEventDispatcherGlib::versionSupported())
        data->eventDispatcher = new QEventDispatcherGlib;
    else
#endif
    data->eventDispatcher = new QEventDispatcherUNIX;
#endif

    l.unlock();
    data->eventDispatcher->startingUp();
}

#ifndef QT_NO_THREAD

#if (defined(Q_OS_LINUX) || (defined(Q_OS_MAC) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6) || defined(Q_OS_QNX))
static void setCurrentThreadName(pthread_t threadId, const char *name)
{
#  if defined(Q_OS_LINUX) && !defined(QT_LINUXBASE)
    Q_UNUSED(threadId);
    prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0);
#  elif (defined(Q_OS_MAC) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
    Q_UNUSED(threadId);
    if (QSysInfo::MacintoshVersion >= QSysInfo::MV_10_6)
        pthread_setname_np(name);
#  elif defined(Q_OS_QNX)
    pthread_setname_np(threadId, name);
#  endif
}
#endif

void *QThreadPrivate::start(void *arg)
{
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_cleanup_push(QThreadPrivate::finish, arg);

    QThread *thr = reinterpret_cast<QThread *>(arg);
    QThreadData *data = QThreadData::get2(thr);

    // do we need to reset the thread priority?
    if (int(thr->d_func()->priority) & ThreadPriorityResetFlag) {
        thr->setPriority(QThread::Priority(thr->d_func()->priority & ~ThreadPriorityResetFlag));
    }

    data->threadId = (Qt::HANDLE)pthread_self();
    set_thread_data(data);

    data->ref();
    {
        QMutexLocker locker(&thr->d_func()->mutex);
        data->quitNow = thr->d_func()->exited;
    }

    // ### TODO: allow the user to create a custom event dispatcher
    createEventDispatcher(data);

#if (defined(Q_OS_LINUX) || (defined(Q_OS_MAC) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6) || defined(Q_OS_QNX))
    // sets the name of the current thread.
    QString objectName = thr->objectName();

    if (Q_LIKELY(objectName.isEmpty()))
        setCurrentThreadName(thr->d_func()->thread_id, thr->metaObject()->className());
    else
        setCurrentThreadName(thr->d_func()->thread_id, objectName.toLocal8Bit());

#endif

    emit thr->started();
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_testcancel();
    thr->run();

    pthread_cleanup_pop(1);

    return 0;
}

void QThreadPrivate::finish(void *arg)
{
    QThread *thr = reinterpret_cast<QThread *>(arg);
    QThreadPrivate *d = thr->d_func();

    QMutexLocker locker(&d->mutex);

    d->isInFinish = true;
    d->priority = QThread::InheritPriority;
    bool terminated = d->terminated;
    void *data = &d->data->tls;
    locker.unlock();
    if (terminated)
        emit thr->terminated();
    emit thr->finished();
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QThreadStorageData::finish((void **)data);
    locker.relock();
    d->terminated = false;

    QAbstractEventDispatcher *eventDispatcher = d->data->eventDispatcher;
    if (eventDispatcher) {
        d->data->eventDispatcher = 0;
        locker.unlock();
        eventDispatcher->closingDown();
        delete eventDispatcher;
        locker.relock();
    }

    d->thread_id = 0;
    d->running = false;
    d->finished = true;

    d->isInFinish = false;
    d->thread_done.wakeAll();
}




/**************************************************************************
 ** QThread
 *************************************************************************/

Qt::HANDLE QThread::currentThreadId()
{
    // requires a C cast here otherwise we run into trouble on AIX
    return (Qt::HANDLE)pthread_self();
}

#if defined(QT_LINUXBASE) && !defined(_SC_NPROCESSORS_ONLN)
// LSB doesn't define _SC_NPROCESSORS_ONLN.
#  define _SC_NPROCESSORS_ONLN 84
#endif

int QThread::idealThreadCount()
{
    int cores = -1;

#if defined(Q_OS_MAC) && !defined(Q_WS_QPA)
    // Mac OS X
    cores = MPProcessorsScheduled();
#elif defined(Q_OS_HPUX)
    // HP-UX
    struct pst_dynamic psd;
    if (pstat_getdynamic(&psd, sizeof(psd), 1, 0) == -1) {
        perror("pstat_getdynamic");
        cores = -1;
    } else {
        cores = (int)psd.psd_proc_cnt;
    }
#elif defined(Q_OS_BSD4)
    // FreeBSD, OpenBSD, NetBSD, BSD/OS
    size_t len = sizeof(cores);
    int mib[2];
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    if (sysctl(mib, 2, &cores, &len, NULL, 0) != 0) {
        perror("sysctl");
        cores = -1;
    }
#elif defined(Q_OS_IRIX)
    // IRIX
    cores = (int)sysconf(_SC_NPROC_ONLN);
#elif defined(Q_OS_INTEGRITY)
#if (__INTEGRITY_MAJOR_VERSION >= 10)
    // Integrity V10+ does support multicore CPUs
    Value processorCount;
    if (GetProcessorCount(CurrentTask(), &processorCount) == 0)
        cores = processorCount;
    else
#endif
    cores = 1;
#elif defined(Q_OS_VXWORKS)
    // VxWorks
#  if defined(QT_VXWORKS_HAS_CPUSET)
    cpuset_t cpus = vxCpuEnabledGet();
    cores = 0;

    // 128 cores should be enough for everyone ;)
    for (int i = 0; i < 128 && !CPUSET_ISZERO(cpus); ++i) {
        if (CPUSET_ISSET(cpus, i)) {
            CPUSET_CLR(cpus, i);
            cores++;
        }
    }
#  else
    // as of aug 2008 VxWorks < 6.6 only supports one single core CPU
    cores = 1;
#  endif
#else
    // the rest: Linux, Solaris, AIX, Tru64
    cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif

    return cores;
}

void QThread::yieldCurrentThread()
{
    sched_yield();
}

/*  \internal
    helper function to do thread sleeps, since usleep()/nanosleep()
    aren't reliable enough (in terms of behavior and availability)
*/
static void thread_sleep(struct timespec *ti)
{
    pthread_mutex_t mtx;
    pthread_cond_t cnd;

    pthread_mutex_init(&mtx, 0);
    pthread_cond_init(&cnd, 0);

    pthread_mutex_lock(&mtx);
    (void) pthread_cond_timedwait(&cnd, &mtx, ti);
    pthread_mutex_unlock(&mtx);

    pthread_cond_destroy(&cnd);
    pthread_mutex_destroy(&mtx);
}

void QThread::sleep(unsigned long secs)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    struct timespec ti;
    ti.tv_sec = tv.tv_sec + secs;
    ti.tv_nsec = (tv.tv_usec * 1000);
    thread_sleep(&ti);
}

void QThread::msleep(unsigned long msecs)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    struct timespec ti;

    ti.tv_nsec = (tv.tv_usec + (msecs % 1000) * 1000) * 1000;
    ti.tv_sec = tv.tv_sec + (msecs / 1000) + (ti.tv_nsec / 1000000000);
    ti.tv_nsec %= 1000000000;
    thread_sleep(&ti);
}

void QThread::usleep(unsigned long usecs)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    struct timespec ti;

    ti.tv_nsec = (tv.tv_usec + (usecs % 1000000)) * 1000;
    ti.tv_sec = tv.tv_sec + (usecs / 1000000) + (ti.tv_nsec / 1000000000);
    ti.tv_nsec %= 1000000000;
    thread_sleep(&ti);
}

#ifdef QT_HAS_THREAD_PRIORITY_SCHEDULING
// Does some magic and calculate the Unix scheduler priorities
// sched_policy is IN/OUT: it must be set to a valid policy before calling this function
// sched_priority is OUT only
static bool calculateUnixPriority(int priority, int *sched_policy, int *sched_priority)
{
#ifdef Q_OS_QNX
    // without Round Robin drawn intensive apps will hog the cpu
    // and make the system appear frozen
   *sched_policy = SCHED_RR;
#endif
#ifdef SCHED_IDLE
    if (priority == QThread::IdlePriority) {
        *sched_policy = SCHED_IDLE;
        *sched_priority = 0;
        return true;
    }
    const int lowestPriority = QThread::LowestPriority;
#else
    const int lowestPriority = QThread::IdlePriority;
#endif
    const int highestPriority = QThread::TimeCriticalPriority;

    int prio_min;
    int prio_max;
#if defined(Q_OS_VXWORKS) && defined(VXWORKS_DKM)
    // for other scheduling policies than SCHED_RR or SCHED_FIFO
    prio_min = SCHED_FIFO_LOW_PRI;
    prio_max = SCHED_FIFO_HIGH_PRI;

    if ((*sched_policy == SCHED_RR) || (*sched_policy == SCHED_FIFO))
#endif
    {
    prio_min = sched_get_priority_min(*sched_policy);
    prio_max = sched_get_priority_max(*sched_policy);
    }

    if (prio_min == -1 || prio_max == -1)
        return false;

    int prio;
    // crudely scale our priority enum values to the prio_min/prio_max
    prio = ((priority - lowestPriority) * (prio_max - prio_min) / highestPriority) + prio_min;
    prio = qMax(prio_min, qMin(prio_max, prio));

    *sched_priority = prio;
    return true;
}
#endif

void QThread::start(Priority priority)
{
    Q_D(QThread);
    QMutexLocker locker(&d->mutex);

    if (d->isInFinish)
        d->thread_done.wait(locker.mutex());

    if (d->running)
        return;

    d->running = true;
    d->finished = false;
    d->terminated = false;
    d->returnCode = 0;
    d->exited = false;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    d->priority = priority;

#if defined(QT_HAS_THREAD_PRIORITY_SCHEDULING)
    switch (priority) {
    case InheritPriority:
        {
            pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
            break;
        }

    default:
        {
            int sched_policy;
            if (pthread_attr_getschedpolicy(&attr, &sched_policy) != 0) {
                // failed to get the scheduling policy, don't bother
                // setting the priority
                qWarning("QThread::start: Cannot determine default scheduler policy");
                break;
            }

            int prio;
            if (!calculateUnixPriority(priority, &sched_policy, &prio)) {
                // failed to get the scheduling parameters, don't
                // bother setting the priority
                qWarning("QThread::start: Cannot determine scheduler priority range");
                break;
            }

            sched_param sp;
            sp.sched_priority = prio;

            if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED) != 0
                || pthread_attr_setschedpolicy(&attr, sched_policy) != 0
                || pthread_attr_setschedparam(&attr, &sp) != 0) {
                // could not set scheduling hints, fallback to inheriting them
                // we'll try again from inside the thread
                pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
                d->priority = Priority(priority | ThreadPriorityResetFlag);
            }
            break;
        }
    }
#endif // QT_HAS_THREAD_PRIORITY_SCHEDULING


    if (d->stackSize > 0) {
#if defined(_POSIX_THREAD_ATTR_STACKSIZE) && (_POSIX_THREAD_ATTR_STACKSIZE-0 > 0)
        int code = pthread_attr_setstacksize(&attr, d->stackSize);
#else
        int code = ENOSYS; // stack size not supported, automatically fail
#endif // _POSIX_THREAD_ATTR_STACKSIZE

        if (code) {
            qWarning("QThread::start: Thread stack size error: %s",
                     qPrintable(qt_error_string(code)));

            // we failed to set the stacksize, and as the documentation states,
            // the thread will fail to run...
            d->running = false;
            d->finished = false;
            return;
        }
    }

    int code =
        pthread_create(&d->thread_id, &attr, QThreadPrivate::start, this);
    if (code == EPERM) {
        // caller does not have permission to set the scheduling
        // parameters/policy
        pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
        code =
            pthread_create(&d->thread_id, &attr, QThreadPrivate::start, this);
    }

    pthread_attr_destroy(&attr);

    if (code) {
        qWarning("QThread::start: Thread creation error: %s", qPrintable(qt_error_string(code)));

        d->running = false;
        d->finished = false;
        d->thread_id = 0;
    }
}

void QThread::terminate()
{
    Q_D(QThread);
    QMutexLocker locker(&d->mutex);

    if (!d->thread_id)
        return;

    int code = pthread_cancel(d->thread_id);
    if (code) {
        qWarning("QThread::start: Thread termination error: %s",
                 qPrintable(qt_error_string((code))));
    } else {
        d->terminated = true;
    }
}

bool QThread::wait(unsigned long time)
{
    Q_D(QThread);
    QMutexLocker locker(&d->mutex);

    if (d->thread_id == pthread_self()) {
        qWarning("QThread::wait: Thread tried to wait on itself");
        return false;
    }

    if (d->finished || !d->running)
        return true;

    while (d->running) {
        if (!d->thread_done.wait(locker.mutex(), time))
            return false;
    }
    return true;
}

void QThread::setTerminationEnabled(bool enabled)
{
    QThread *thr = currentThread();
    Q_ASSERT_X(thr != 0, "QThread::setTerminationEnabled()",
               "Current thread was not started with QThread.");

    Q_UNUSED(thr)
    pthread_setcancelstate(enabled ? PTHREAD_CANCEL_ENABLE : PTHREAD_CANCEL_DISABLE, NULL);
    if (enabled)
        pthread_testcancel();
}

void QThread::setPriority(Priority priority)
{
    Q_D(QThread);
    QMutexLocker locker(&d->mutex);
    if (!d->running) {
        qWarning("QThread::setPriority: Cannot set priority, thread is not running");
        return;
    }

    d->priority = priority;

    // copied from start() with a few modifications:

#ifdef QT_HAS_THREAD_PRIORITY_SCHEDULING
    int sched_policy;
    sched_param param;

    if (pthread_getschedparam(d->thread_id, &sched_policy, &param) != 0) {
        // failed to get the scheduling policy, don't bother setting
        // the priority
        qWarning("QThread::setPriority: Cannot get scheduler parameters");
        return;
    }

    int prio;
    if (!calculateUnixPriority(priority, &sched_policy, &prio)) {
        // failed to get the scheduling parameters, don't
        // bother setting the priority
        qWarning("QThread::setPriority: Cannot determine scheduler priority range");
        return;
    }

    param.sched_priority = prio;
    int status = pthread_setschedparam(d->thread_id, sched_policy, &param);

# ifdef SCHED_IDLE
    // were we trying to set to idle priority and failed?
    if (status == -1 && sched_policy == SCHED_IDLE && errno == EINVAL) {
        // reset to lowest priority possible
        pthread_getschedparam(d->thread_id, &sched_policy, &param);
        param.sched_priority = sched_get_priority_min(sched_policy);
        pthread_setschedparam(d->thread_id, sched_policy, &param);
    }
# else
    Q_UNUSED(status);
# endif // SCHED_IDLE
#endif
}

#endif // QT_NO_THREAD

QT_END_NAMESPACE

