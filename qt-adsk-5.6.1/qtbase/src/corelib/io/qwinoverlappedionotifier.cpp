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

#include "qwinoverlappedionotifier_p.h"
#include <qdebug.h>
#include <qatomic.h>
#include <qelapsedtimer.h>
#include <qmutex.h>
#include <qpointer.h>
#include <qqueue.h>
#include <qset.h>
#include <qthread.h>
#include <qt_windows.h>
#include <private/qobject_p.h>
#include <private/qiodevice_p.h>

QT_BEGIN_NAMESPACE

/*!
    \class QWinOverlappedIoNotifier
    \inmodule QtCore
    \brief The QWinOverlappedIoNotifier class provides support for overlapped I/O notifications on Windows.
    \since 5.0
    \internal

    The QWinOverlappedIoNotifier class makes it possible to use efficient
    overlapped (asynchronous) I/O notifications on Windows by using an
    I/O completion port.

    Once you have obtained a file handle, you can use setHandle() to get
    notifications for I/O operations. Whenever an I/O operation completes,
    the notified() signal is emitted which will pass the number of transferred
    bytes, the operation's error code and a pointer to the operation's
    OVERLAPPED object to the receiver.

    Every handle that supports overlapped I/O can be used by
    QWinOverlappedIoNotifier. That includes file handles, TCP sockets
    and named pipes.

    Note that you must not use ReadFileEx() and WriteFileEx() together
    with QWinOverlappedIoNotifier. They are not supported as they use a
    different I/O notification mechanism.

    The hEvent member in the OVERLAPPED structure passed to ReadFile()
    or WriteFile() is ignored and can be used for other purposes.

    \warning This class is only available on Windows.

    Due to peculiarities of the Windows I/O completion port API, users of
    QWinOverlappedIoNotifier must pay attention to the following restrictions:
    \list
    \li File handles with a QWinOverlappedIoNotifer are assigned to an I/O
        completion port until the handle is closed. It is impossible to
        disassociate the file handle from the I/O completion port.
    \li There can be only one QWinOverlappedIoNotifer per file handle. Creating
        another QWinOverlappedIoNotifier for that file, even with a duplicated
        handle, will fail.
    \li Certain Windows API functions are unavailable for file handles that are
        assigned to an I/O completion port. This includes the functions
        \c{ReadFileEx} and \c{WriteFileEx}.
    \endlist
    See also the remarks in the MSDN documentation for the
    \c{CreateIoCompletionPort} function.
*/

struct IOResult
{
    IOResult(DWORD n = 0, DWORD e = 0, OVERLAPPED *p = 0)
        : numberOfBytes(n), errorCode(e), overlapped(p)
    {}

    DWORD numberOfBytes;
    DWORD errorCode;
    OVERLAPPED *overlapped;
};


class QWinIoCompletionPort;

class QWinOverlappedIoNotifierPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QWinOverlappedIoNotifier)
public:
    QWinOverlappedIoNotifierPrivate()
        : hHandle(INVALID_HANDLE_VALUE)
    {
    }

    OVERLAPPED *waitForAnyNotified(int msecs);
    void notify(DWORD numberOfBytes, DWORD errorCode, OVERLAPPED *overlapped);
    void _q_notified();
    OVERLAPPED *dispatchNextIoResult();

    static QWinIoCompletionPort *iocp;
    static HANDLE iocpInstanceLock;
    static unsigned int iocpInstanceRefCount;
    HANDLE hHandle;
    HANDLE hSemaphore;
    HANDLE hResultsMutex;
    QAtomicInt waiting;
    QQueue<IOResult> results;
};

QWinIoCompletionPort *QWinOverlappedIoNotifierPrivate::iocp = 0;
HANDLE QWinOverlappedIoNotifierPrivate::iocpInstanceLock = CreateMutex(NULL, FALSE, NULL);
unsigned int QWinOverlappedIoNotifierPrivate::iocpInstanceRefCount = 0;


class QWinIoCompletionPort : protected QThread
{
public:
    QWinIoCompletionPort()
        : finishThreadKey(reinterpret_cast<ULONG_PTR>(this)),
          drainQueueKey(reinterpret_cast<ULONG_PTR>(this + 1)),
          hPort(INVALID_HANDLE_VALUE)
    {
        setObjectName(QLatin1String("I/O completion port thread"));
        HANDLE hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (!hIOCP) {
            qErrnoWarning("CreateIoCompletionPort failed.");
            return;
        }
        hPort = hIOCP;
        hQueueDrainedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!hQueueDrainedEvent) {
            qErrnoWarning("CreateEvent failed.");
            return;
        }
    }

    ~QWinIoCompletionPort()
    {
        PostQueuedCompletionStatus(hPort, 0, finishThreadKey, NULL);
        QThread::wait();
        CloseHandle(hPort);
        CloseHandle(hQueueDrainedEvent);
    }

    void registerNotifier(QWinOverlappedIoNotifierPrivate *notifier)
    {
        const HANDLE hHandle = notifier->hHandle;
        HANDLE hIOCP = CreateIoCompletionPort(hHandle, hPort,
                                              reinterpret_cast<ULONG_PTR>(notifier), 0);
        if (!hIOCP) {
            qErrnoWarning("Can't associate file handle %x with I/O completion port.", hHandle);
            return;
        }
        mutex.lock();
        notifiers += notifier;
        mutex.unlock();
        if (!QThread::isRunning())
            QThread::start();
    }

    void unregisterNotifier(QWinOverlappedIoNotifierPrivate *notifier)
    {
        mutex.lock();
        notifiers.remove(notifier);
        mutex.unlock();
    }

    void drainQueue()
    {
        QMutexLocker locker(&drainQueueMutex);
        ResetEvent(hQueueDrainedEvent);
        PostQueuedCompletionStatus(hPort, 0, drainQueueKey, NULL);
        WaitForSingleObject(hQueueDrainedEvent, INFINITE);
    }

    using QThread::isRunning;

protected:
    void run()
    {
        DWORD dwBytesRead;
        ULONG_PTR pulCompletionKey;
        OVERLAPPED *overlapped;
        DWORD msecs = INFINITE;

        forever {
            BOOL success = GetQueuedCompletionStatus(hPort,
                                                     &dwBytesRead,
                                                     &pulCompletionKey,
                                                     &overlapped,
                                                     msecs);

            DWORD errorCode = success ? ERROR_SUCCESS : GetLastError();
            if (!success && !overlapped) {
                if (!msecs) {
                    // Time out in drain mode. The completion status queue is empty.
                    msecs = INFINITE;
                    SetEvent(hQueueDrainedEvent);
                    continue;
                }
                qErrnoWarning(errorCode, "GetQueuedCompletionStatus failed.");
                return;
            }

            if (pulCompletionKey == finishThreadKey)
                return;
            if (pulCompletionKey == drainQueueKey) {
                // Enter drain mode.
                Q_ASSERT(msecs == INFINITE);
                msecs = 0;
                continue;
            }

            QWinOverlappedIoNotifierPrivate *notifier
                    = reinterpret_cast<QWinOverlappedIoNotifierPrivate *>(pulCompletionKey);
            mutex.lock();
            if (notifiers.contains(notifier))
                notifier->notify(dwBytesRead, errorCode, overlapped);
            mutex.unlock();
        }
    }

private:
    const ULONG_PTR finishThreadKey;
    const ULONG_PTR drainQueueKey;
    HANDLE hPort;
    QSet<QWinOverlappedIoNotifierPrivate *> notifiers;
    QMutex mutex;
    QMutex drainQueueMutex;
    HANDLE hQueueDrainedEvent;
};


QWinOverlappedIoNotifier::QWinOverlappedIoNotifier(QObject *parent)
    : QObject(*new QWinOverlappedIoNotifierPrivate, parent)
{
    Q_D(QWinOverlappedIoNotifier);
    WaitForSingleObject(d->iocpInstanceLock, INFINITE);
    if (!d->iocp)
        d->iocp = new QWinIoCompletionPort;
    d->iocpInstanceRefCount++;
    ReleaseMutex(d->iocpInstanceLock);

    d->hSemaphore = CreateSemaphore(NULL, 0, 255, NULL);
    d->hResultsMutex = CreateMutex(NULL, FALSE, NULL);
    connect(this, SIGNAL(_q_notify()), this, SLOT(_q_notified()), Qt::QueuedConnection);
}

QWinOverlappedIoNotifier::~QWinOverlappedIoNotifier()
{
    Q_D(QWinOverlappedIoNotifier);
    setEnabled(false);
    CloseHandle(d->hResultsMutex);
    CloseHandle(d->hSemaphore);

    WaitForSingleObject(d->iocpInstanceLock, INFINITE);
    if (!--d->iocpInstanceRefCount) {
        delete d->iocp;
        d->iocp = 0;
    }
    ReleaseMutex(d->iocpInstanceLock);
}

void QWinOverlappedIoNotifier::setHandle(Qt::HANDLE h)
{
    Q_D(QWinOverlappedIoNotifier);
    d->hHandle = h;
}

Qt::HANDLE QWinOverlappedIoNotifier::handle() const
{
    Q_D(const QWinOverlappedIoNotifier);
    return d->hHandle;
}

void QWinOverlappedIoNotifier::setEnabled(bool enabled)
{
    Q_D(QWinOverlappedIoNotifier);
    if (enabled)
        d->iocp->registerNotifier(d);
    else
        d->iocp->unregisterNotifier(d);
}

OVERLAPPED *QWinOverlappedIoNotifierPrivate::waitForAnyNotified(int msecs)
{
    if (!iocp->isRunning()) {
        qWarning("Called QWinOverlappedIoNotifier::waitForAnyNotified on inactive notifier.");
        return 0;
    }

    if (msecs == 0)
        iocp->drainQueue();

    const DWORD wfso = WaitForSingleObject(hSemaphore, msecs == -1 ? INFINITE : DWORD(msecs));
    switch (wfso) {
    case WAIT_OBJECT_0:
        return dispatchNextIoResult();
    case WAIT_TIMEOUT:
        return 0;
    default:
        qErrnoWarning("QWinOverlappedIoNotifier::waitForAnyNotified: WaitForSingleObject failed.");
        return 0;
    }
}

class QScopedAtomicIntIncrementor
{
public:
    QScopedAtomicIntIncrementor(QAtomicInt &i)
        : m_int(i)
    {
        ++m_int;
    }

    ~QScopedAtomicIntIncrementor()
    {
        --m_int;
    }

private:
    QAtomicInt &m_int;
};

/*!
 * Wait synchronously for any notified signal.
 *
 * The function returns a pointer to the OVERLAPPED object corresponding to the completed I/O
 * operation. In case no I/O operation was completed during the \a msec timeout, this function
 * returns a null pointer.
 */
OVERLAPPED *QWinOverlappedIoNotifier::waitForAnyNotified(int msecs)
{
    Q_D(QWinOverlappedIoNotifier);
    QScopedAtomicIntIncrementor saii(d->waiting);
    OVERLAPPED *result = d->waitForAnyNotified(msecs);
    return result;
}

/*!
 * Wait synchronously for the notified signal.
 *
 * The function returns true if the notified signal was emitted for
 * the I/O operation that corresponds to the OVERLAPPED object.
 */
bool QWinOverlappedIoNotifier::waitForNotified(int msecs, OVERLAPPED *overlapped)
{
    Q_D(QWinOverlappedIoNotifier);
    QScopedAtomicIntIncrementor saii(d->waiting);
    int t = msecs;
    QElapsedTimer stopWatch;
    stopWatch.start();
    forever {
        OVERLAPPED *triggeredOverlapped = waitForAnyNotified(t);
        if (!triggeredOverlapped)
            return false;
        if (triggeredOverlapped == overlapped)
            return true;
        t = qt_subtract_from_timeout(msecs, stopWatch.elapsed());
        if (t == 0)
            return false;
    }
}

/*!
  * Note: This function runs in the I/O completion port thread.
  */
void QWinOverlappedIoNotifierPrivate::notify(DWORD numberOfBytes, DWORD errorCode,
        OVERLAPPED *overlapped)
{
    Q_Q(QWinOverlappedIoNotifier);
    WaitForSingleObject(hResultsMutex, INFINITE);
    results.enqueue(IOResult(numberOfBytes, errorCode, overlapped));
    ReleaseMutex(hResultsMutex);
    ReleaseSemaphore(hSemaphore, 1, NULL);
    if (!waiting)
        emit q->_q_notify();
}

void QWinOverlappedIoNotifierPrivate::_q_notified()
{
    if (WaitForSingleObject(hSemaphore, 0) == WAIT_OBJECT_0)
        dispatchNextIoResult();
}

OVERLAPPED *QWinOverlappedIoNotifierPrivate::dispatchNextIoResult()
{
    Q_Q(QWinOverlappedIoNotifier);
    WaitForSingleObject(hResultsMutex, INFINITE);
    IOResult ioresult = results.dequeue();
    ReleaseMutex(hResultsMutex);
    emit q->notified(ioresult.numberOfBytes, ioresult.errorCode, ioresult.overlapped);
    return ioresult.overlapped;
}

QT_END_NAMESPACE

#include "moc_qwinoverlappedionotifier_p.cpp"
