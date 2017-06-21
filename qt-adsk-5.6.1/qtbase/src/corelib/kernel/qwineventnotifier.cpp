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

#include "qwineventnotifier.h"

#ifdef Q_OS_WINRT
#include "qeventdispatcher_winrt_p.h"
#else
#include "qeventdispatcher_win_p.h"
#endif
#include "qcoreapplication.h"

#include <private/qthread_p.h>

QT_BEGIN_NAMESPACE

class QWinEventNotifierPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QWinEventNotifier)
public:
    QWinEventNotifierPrivate()
    : handleToEvent(0), enabled(false) {}
    QWinEventNotifierPrivate(HANDLE h, bool e)
    : handleToEvent(h), enabled(e) {}

    HANDLE handleToEvent;
    bool enabled;
};

/*!
    \class QWinEventNotifier
    \inmodule QtCore
    \since 5.0
    \brief The QWinEventNotifier class provides support for the Windows Wait functions.

    The QWinEventNotifier class makes it possible to use the wait
    functions on windows in a asynchronous manner. With this class,
    you can register a HANDLE to an event and get notification when
    that event becomes signalled. The state of the event is not modified
    in the process so if it is a manual reset event you will need to
    reset it after the notification.

    Once you have created a event object using Windows API such as
    CreateEvent() or OpenEvent(), you can create an event notifier to
    monitor the event handle. If the event notifier is enabled, it will
    emit the activated() signal whenever the corresponding event object
    is signalled.

    The setEnabled() function allows you to disable as well as enable the
    event notifier. It is generally advisable to explicitly enable or
    disable the event notifier. A disabled notifier does nothing when the
    event object is signalled (the same effect as not creating the
    event notifier).  Use the isEnabled() function to determine the
    notifier's current status.

    Finally, you can use the setHandle() function to register a new event
    object, and the handle() function to retrieve the event handle.

    \b{Further information:}
    Although the class is called QWinEventNotifier, it can be used for
    certain other objects which are so-called synchronization
    objects, such as Processes, Threads, Waitable timers.

    \warning This class is only available on Windows.
*/

/*!
    \fn void QWinEventNotifier::activated(HANDLE hEvent)

    This signal is emitted whenever the event notifier is enabled and
    the corresponding HANDLE is signalled.

    The state of the event is not modified in the process, so if it is a
    manual reset event, you will need to reset it after the notification.

    The object is passed in the \a hEvent parameter.

    \sa handle()
*/

/*!
    Constructs an event notifier with the given \a parent.
*/

QWinEventNotifier::QWinEventNotifier(QObject *parent)
  : QObject(*new QWinEventNotifierPrivate, parent)
{}

/*!
    Constructs an event notifier with the given \a parent. It enables
    the notifier, and watches for the event \a hEvent.

    The notifier is enabled by default, i.e. it emits the activated() signal
    whenever the corresponding event is signalled. However, it is generally
    advisable to explicitly enable or disable the event notifier.

    \sa setEnabled(), isEnabled()
*/

QWinEventNotifier::QWinEventNotifier(HANDLE hEvent, QObject *parent)
 : QObject(*new QWinEventNotifierPrivate(hEvent, false), parent)
{
    Q_D(QWinEventNotifier);
    QAbstractEventDispatcher *eventDispatcher = d->threadData->eventDispatcher.load();
    if (Q_UNLIKELY(!eventDispatcher)) {
        qWarning("QWinEventNotifier: Can only be used with threads started with QThread");
        return;
    }
    eventDispatcher->registerEventNotifier(this);
    d->enabled = true;
}

/*!
    Destroys this notifier.
*/

QWinEventNotifier::~QWinEventNotifier()
{
    setEnabled(false);
}

/*!
    Register the HANDLE \a hEvent. The old HANDLE will be automatically
    unregistered.

    \b Note: The notifier will be disabled as a side effect and needs
    to be re-enabled.

    \sa handle(), setEnabled()
*/

void QWinEventNotifier::setHandle(HANDLE hEvent)
{
    Q_D(QWinEventNotifier);
    setEnabled(false);
    d->handleToEvent = hEvent;
}

/*!
    Returns the HANDLE that has been registered in the notifier.

    \sa setHandle()
*/

HANDLE  QWinEventNotifier::handle() const
{
    Q_D(const QWinEventNotifier);
    return d->handleToEvent;
}

/*!
    Returns \c true if the notifier is enabled; otherwise returns \c false.

    \sa setEnabled()
*/

bool QWinEventNotifier::isEnabled() const
{
    Q_D(const QWinEventNotifier);
    return d->enabled;
}

/*!
    If \a enable is true, the notifier is enabled; otherwise the notifier
    is disabled.

    \sa isEnabled(), activated()
*/

void QWinEventNotifier::setEnabled(bool enable)
{
    Q_D(QWinEventNotifier);
    if (d->enabled == enable)                        // no change
        return;
    d->enabled = enable;

    QAbstractEventDispatcher *eventDispatcher = d->threadData->eventDispatcher.load();
    if (!eventDispatcher) // perhaps application is shutting down
        return;
    if (Q_UNLIKELY(thread() != QThread::currentThread())) {
        qWarning("QWinEventNotifier: Event notifiers cannot be enabled or disabled from another thread");
        return;
    }

    if (enable)
        eventDispatcher->registerEventNotifier(this);
    else
        eventDispatcher->unregisterEventNotifier(this);
}

/*!
    \reimp
*/

bool QWinEventNotifier::event(QEvent * e)
{
    Q_D(QWinEventNotifier);
    if (e->type() == QEvent::ThreadChange) {
        if (d->enabled) {
            QMetaObject::invokeMethod(this, "setEnabled", Qt::QueuedConnection,
                                      Q_ARG(bool, true));
            setEnabled(false);
        }
    }
    QObject::event(e);                        // will activate filters
    if (e->type() == QEvent::WinEventAct) {
        emit activated(d->handleToEvent, QPrivateSignal());
        return true;
    }
    return false;
}

QT_END_NAMESPACE
