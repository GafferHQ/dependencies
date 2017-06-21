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

#include "qeventdispatcher_winrt_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>
#include <QtCore/QHash>
#include <QtCore/qfunctions_winrt.h>
#include <private/qabstracteventdispatcher_p.h>
#include <private/qcoreapplication_p.h>

#include <functional>
#include <wrl.h>
#include <windows.foundation.h>
#include <windows.system.threading.h>
#include <windows.ui.core.h>
#include <windows.applicationmodel.core.h>
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::System::Threading;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::UI::Core;
using namespace ABI::Windows::ApplicationModel::Core;

QT_BEGIN_NAMESPACE

#define INTERRUPT_HANDLE 0
#define INVALID_TIMER_ID -1

struct WinRTTimerInfo : public QAbstractEventDispatcher::TimerInfo {
    WinRTTimerInfo(int timerId = INVALID_TIMER_ID, int interval = 0, Qt::TimerType timerType = Qt::CoarseTimer,
                   QObject *obj = 0, quint64 tt = 0) :
        QAbstractEventDispatcher::TimerInfo(timerId, interval, timerType),
        inEvent(false), object(obj), targetTime(tt)
    {
    }

    bool inEvent;
    QObject *object;
    quint64 targetTime;
};

class AgileDispatchedHandler : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>, IDispatchedHandler, IAgileObject>
{
public:
    AgileDispatchedHandler(const std::function<HRESULT()> &delegate)
        : delegate(delegate)
    {
    }

    HRESULT __stdcall Invoke()
    {
        return delegate();
    }

private:
    std::function<HRESULT()> delegate;
};

class QEventDispatcherWinRTPrivate : public QAbstractEventDispatcherPrivate
{
    Q_DECLARE_PUBLIC(QEventDispatcherWinRT)

public:
    QEventDispatcherWinRTPrivate();
    ~QEventDispatcherWinRTPrivate();

private:
    QHash<int, QObject *> timerIdToObject;
    QVector<WinRTTimerInfo> timerInfos;
    QHash<HANDLE, int> timerHandleToId;
    QHash<int, HANDLE> timerIdToHandle;
    QHash<int, HANDLE> timerIdToCancelHandle;

    void addTimer(int id, int interval, Qt::TimerType type, QObject *obj,
                     HANDLE handle, HANDLE cancelHandle)
    {
        // Zero timer events do not need these handles.
        if (interval > 0) {
            timerHandleToId.insert(handle, id);
            timerIdToHandle.insert(id, handle);
            timerIdToCancelHandle.insert(id, cancelHandle);
        }
        timerIdToObject.insert(id, obj);
        const quint64 targetTime = qt_msectime() + interval;
        const WinRTTimerInfo info(id, interval, type, obj, targetTime);
        if (id >= timerInfos.size())
            timerInfos.resize(id + 1);
        timerInfos[id] = info;
        timerIdToObject.insert(id, obj);
    }

    bool removeTimer(int id)
    {
        if (id >= timerInfos.size())
            return false;

        WinRTTimerInfo &info = timerInfos[id];
        if (info.timerId == INVALID_TIMER_ID)
            return false;

        if (info.interval > 0 && (!timerIdToHandle.contains(id) || !timerIdToCancelHandle.contains(id)))
            return false;

        info.timerId = INVALID_TIMER_ID;

        // Remove invalid timerinfos from the vector's end, if the timer with the highest id was removed
        int lastTimer = timerInfos.size() - 1;
        while (lastTimer >= 0 && timerInfos.at(lastTimer).timerId == INVALID_TIMER_ID)
            --lastTimer;
        if (lastTimer >= 0 && lastTimer != timerInfos.size() - 1)
            timerInfos.resize(lastTimer + 1);
        timerIdToObject.remove(id);
        // ... remove handle from all lists
        if (info.interval > 0) {
            HANDLE handle = timerIdToHandle.take(id);
            timerHandleToId.remove(handle);
            SetEvent(timerIdToCancelHandle.take(id));
        }
        return true;
    }
};

QEventDispatcherWinRT::QEventDispatcherWinRT(QObject *parent)
    : QAbstractEventDispatcher(*new QEventDispatcherWinRTPrivate, parent)
{
}

QEventDispatcherWinRT::QEventDispatcherWinRT(QEventDispatcherWinRTPrivate &dd, QObject *parent)
    : QAbstractEventDispatcher(dd, parent)
{ }

QEventDispatcherWinRT::~QEventDispatcherWinRT()
{
}

HRESULT QEventDispatcherWinRT::runOnXamlThread(const std::function<HRESULT ()> &delegate, bool waitForRun)
{
    static __declspec(thread) ICoreDispatcher *dispatcher = nullptr;
    if (!dispatcher) {
        HRESULT hr;
        ComPtr<ICoreImmersiveApplication> application;
        hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_ApplicationModel_Core_CoreApplication).Get(),
                                    IID_PPV_ARGS(&application));
        ComPtr<ICoreApplicationView> view;
        hr = application->get_MainView(&view);
        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<ICoreWindow> window;
        hr = view->get_CoreWindow(&window);
        Q_ASSERT_SUCCEEDED(hr);
        if (!window) {
            // In case the application is launched via activation
            // there might not be a main view (eg ShareTarget).
            // Hence iterate through the available views and try to find
            // a dispatcher in there
            ComPtr<IVectorView<CoreApplicationView*>> appViews;
            hr = application->get_Views(&appViews);
            Q_ASSERT_SUCCEEDED(hr);
            quint32 count;
            hr = appViews->get_Size(&count);
            Q_ASSERT_SUCCEEDED(hr);
            for (quint32 i = 0; i < count; ++i) {
                hr = appViews->GetAt(i, &view);
                Q_ASSERT_SUCCEEDED(hr);
                hr = view->get_CoreWindow(&window);
                Q_ASSERT_SUCCEEDED(hr);
                if (window) {
                    hr = window->get_Dispatcher(&dispatcher);
                    Q_ASSERT_SUCCEEDED(hr);
                    if (dispatcher)
                        break;
                }
            }
            Q_ASSERT(dispatcher);
        } else {
            hr = window->get_Dispatcher(&dispatcher);
            Q_ASSERT_SUCCEEDED(hr);
        }
    }

    HRESULT hr;
    boolean onXamlThread;
    hr = dispatcher->get_HasThreadAccess(&onXamlThread);
    Q_ASSERT_SUCCEEDED(hr);
    if (onXamlThread) // Already there
        return delegate();

    ComPtr<IAsyncAction> op;
    hr = dispatcher->RunAsync(CoreDispatcherPriority_Normal, Make<AgileDispatchedHandler>(delegate).Get(), &op);
    if (FAILED(hr) || !waitForRun)
        return hr;
    return QWinRTFunctions::await(op);
}

bool QEventDispatcherWinRT::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    Q_D(QEventDispatcherWinRT);

    DWORD waitTime = 0;
    do {
        // Additional user events have to be handled before timer events, but the function may not
        // return yet.
        const bool userEventsSent = sendPostedEvents(flags);

        const QVector<HANDLE> timerHandles = d->timerIdToHandle.values().toVector();
        if (waitTime)
            emit aboutToBlock();
        bool timerEventsSent = false;
        DWORD waitResult = WaitForMultipleObjectsEx(timerHandles.count(), timerHandles.constData(), FALSE, waitTime, TRUE);
        while (waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + timerHandles.count()) {
            timerEventsSent = true;
            const HANDLE handle = timerHandles.value(waitResult - WAIT_OBJECT_0);
            ResetEvent(handle);
            const int timerId = d->timerHandleToId.value(handle);
            if (timerId == INTERRUPT_HANDLE)
                break;

            WinRTTimerInfo &info = d->timerInfos[timerId];
            Q_ASSERT(info.timerId != INVALID_TIMER_ID);

            QCoreApplication::postEvent(this, new QTimerEvent(timerId));

            // Update timer's targetTime
            const quint64 targetTime = qt_msectime() + info.interval;
            info.targetTime = targetTime;
            waitResult = WaitForMultipleObjectsEx(timerHandles.count(), timerHandles.constData(), FALSE, 0, TRUE);
        }
        emit awake();
        if (timerEventsSent || userEventsSent)
            return true;

        // We cannot wait infinitely like on other platforms, as
        // WaitForMultipleObjectsEx might not return.
        // For instance win32 uses MsgWaitForMultipleObjects to hook
        // into the native event loop, while WinRT handles those
        // via callbacks.
        waitTime = 1;
    } while (flags & QEventLoop::WaitForMoreEvents);
    return false;
}

bool QEventDispatcherWinRT::sendPostedEvents(QEventLoop::ProcessEventsFlags flags)
{
    Q_UNUSED(flags);
    if (hasPendingEvents()) {
        QCoreApplication::sendPostedEvents();
        return true;
    }
    return false;
}

bool QEventDispatcherWinRT::hasPendingEvents()
{
    return qGlobalPostedEventsCount();
}

void QEventDispatcherWinRT::registerSocketNotifier(QSocketNotifier *notifier)
{
    Q_UNUSED(notifier);
    Q_UNIMPLEMENTED();
}
void QEventDispatcherWinRT::unregisterSocketNotifier(QSocketNotifier *notifier)
{
    Q_UNUSED(notifier);
    Q_UNIMPLEMENTED();
}

void QEventDispatcherWinRT::registerTimer(int timerId, int interval, Qt::TimerType timerType, QObject *object)
{
    Q_UNUSED(timerType);

    if (timerId < 1 || interval < 0 || !object) {
#ifndef QT_NO_DEBUG
        qWarning("QEventDispatcherWinRT::registerTimer: invalid arguments");
#endif
        return;
    } else if (object->thread() != thread() || thread() != QThread::currentThread()) {
#ifndef QT_NO_DEBUG
        qWarning("QEventDispatcherWinRT::registerTimer: timers cannot be started from another thread");
#endif
        return;
    }

    Q_D(QEventDispatcherWinRT);
    // Don't use timer factory for zero-delay timers
    if (interval == 0u) {
        d->addTimer(timerId, interval, timerType, object, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE);
        QCoreApplication::postEvent(this, new QTimerEvent(timerId));
        return;
    }

    TimeSpan period;
    // TimeSpan is based on 100-nanosecond units
    period.Duration = qMax(qint64(1), qint64(interval) * 10000);
    const HANDLE handle = CreateEventEx(NULL, NULL, CREATE_EVENT_MANUAL_RESET, SYNCHRONIZE | EVENT_MODIFY_STATE);
    const HANDLE cancelHandle = CreateEventEx(NULL, NULL, CREATE_EVENT_MANUAL_RESET, SYNCHRONIZE|EVENT_MODIFY_STATE);
    HRESULT hr = runOnXamlThread([cancelHandle, handle, period]() {
        static ComPtr<IThreadPoolTimerStatics> timerFactory;
        HRESULT hr;
        if (!timerFactory) {
            hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_System_Threading_ThreadPoolTimer).Get(),
                                      &timerFactory);
            Q_ASSERT_SUCCEEDED(hr);
        }
        IThreadPoolTimer *timer;
        hr = timerFactory->CreatePeriodicTimerWithCompletion(
        Callback<ITimerElapsedHandler>([handle, cancelHandle](IThreadPoolTimer *timer) {
            DWORD cancelResult = WaitForSingleObjectEx(cancelHandle, 0, TRUE);
            if (cancelResult == WAIT_OBJECT_0) {
                timer->Cancel();
                return S_OK;
            }
            if (!SetEvent(handle)) {
                Q_ASSERT_X(false, "QEventDispatcherWinRT::registerTimer",
                           "SetEvent should never fail here");
                return S_OK;
            }
            return S_OK;
        }).Get(), period,
        Callback<ITimerDestroyedHandler>([handle, cancelHandle](IThreadPoolTimer *) {
            CloseHandle(handle);
            CloseHandle(cancelHandle);
            return S_OK;
        }).Get(), &timer);
        RETURN_HR_IF_FAILED("Failed to create periodic timer");
        return hr;
    }, false);
    if (FAILED(hr)) {
        CloseHandle(handle);
        CloseHandle(cancelHandle);
        return;
    }
    d->addTimer(timerId, interval, timerType, object, handle, cancelHandle);
}

bool QEventDispatcherWinRT::unregisterTimer(int timerId)
{
    if (timerId < 1) {
#ifndef QT_NO_DEBUG
        qWarning("QEventDispatcherWinRT::unregisterTimer: invalid argument");
#endif
        return false;
    }
    if (thread() != QThread::currentThread()) {
#ifndef QT_NO_DEBUG
        qWarning("QEventDispatcherWinRT::unregisterTimer: timers cannot be stopped from another thread");
#endif
        return false;
    }

    // As we post all timer events internally, they have to pe removed to prevent stray events
    QCoreApplicationPrivate::removePostedTimerEvent(this, timerId);
    Q_D(QEventDispatcherWinRT);
    return d->removeTimer(timerId);
}

bool QEventDispatcherWinRT::unregisterTimers(QObject *object)
{
    if (!object) {
#ifndef QT_NO_DEBUG
        qWarning("QEventDispatcherWinRT::unregisterTimers: invalid argument");
#endif
        return false;
    }
    QThread *currentThread = QThread::currentThread();
    if (object->thread() != thread() || thread() != currentThread) {
#ifndef QT_NO_DEBUG
        qWarning("QEventDispatcherWinRT::unregisterTimers: timers cannot be stopped from another thread");
#endif
        return false;
    }

    Q_D(QEventDispatcherWinRT);
    foreach (int id, d->timerIdToObject.keys()) {
        if (d->timerIdToObject.value(id) == object)
            unregisterTimer(id);
    }

    return true;
}

QList<QAbstractEventDispatcher::TimerInfo> QEventDispatcherWinRT::registeredTimers(QObject *object) const
{
    if (!object) {
#ifndef QT_NO_DEBUG
        qWarning("QEventDispatcherWinRT:registeredTimers: invalid argument");
#endif
        return QList<TimerInfo>();
    }

    Q_D(const QEventDispatcherWinRT);
    QList<TimerInfo> timerInfos;
    foreach (const WinRTTimerInfo &info, d->timerInfos) {
        if (info.object == object && info.timerId != INVALID_TIMER_ID)
            timerInfos.append(info);
    }
    return timerInfos;
}

bool QEventDispatcherWinRT::registerEventNotifier(QWinEventNotifier *notifier)
{
    Q_UNUSED(notifier);
    Q_UNIMPLEMENTED();
    return false;
}

void QEventDispatcherWinRT::unregisterEventNotifier(QWinEventNotifier *notifier)
{
    Q_UNUSED(notifier);
    Q_UNIMPLEMENTED();
}

int QEventDispatcherWinRT::remainingTime(int timerId)
{
    if (timerId < 1) {
#ifndef QT_NO_DEBUG
        qWarning("QEventDispatcherWinRT::remainingTime: invalid argument");
#endif
        return -1;
    }

    Q_D(QEventDispatcherWinRT);
    const WinRTTimerInfo timerInfo = d->timerInfos.at(timerId);
    if (timerInfo.timerId == INVALID_TIMER_ID) {
#ifndef QT_NO_DEBUG
        qWarning("QEventDispatcherWinRT::remainingTime: timer id %d not found", timerId);
#endif
        return -1;
    }

    const quint64 currentTime = qt_msectime();
    if (currentTime < timerInfo.targetTime) {
        // time to wait
        return timerInfo.targetTime - currentTime;
    } else {
        return 0;
    }

    return -1;
}

void QEventDispatcherWinRT::wakeUp()
{
}

void QEventDispatcherWinRT::interrupt()
{
    Q_D(QEventDispatcherWinRT);
    SetEvent(d->timerIdToHandle.value(INTERRUPT_HANDLE));
}

void QEventDispatcherWinRT::flush()
{
}

void QEventDispatcherWinRT::startingUp()
{
}

void QEventDispatcherWinRT::closingDown()
{
}

bool QEventDispatcherWinRT::event(QEvent *e)
{
    Q_D(QEventDispatcherWinRT);
    switch (e->type()) {
    case QEvent::Timer: {
        QTimerEvent *timerEvent = static_cast<QTimerEvent *>(e);
        const int id = timerEvent->timerId();
        Q_ASSERT(id < d->timerInfos.size());
        WinRTTimerInfo &info = d->timerInfos[id];
        Q_ASSERT(info.timerId != INVALID_TIMER_ID);

        if (info.inEvent) // but don't allow event to recurse
            break;
        info.inEvent = true;

        QTimerEvent te(id);
        QCoreApplication::sendEvent(d->timerIdToObject.value(id), &te);

        // The timer might have been removed in the meanwhile
        if (id >= d->timerInfos.size())
            break;

        info = d->timerInfos[id];
        if (info.timerId == INVALID_TIMER_ID)
            break;

        if (info.interval == 0 && info.inEvent) {
            // post the next zero timer event as long as the timer was not restarted
            QCoreApplication::postEvent(this, new QTimerEvent(id));
        }
        info.inEvent = false;
    }
    default:
        break;
    }
    return QAbstractEventDispatcher::event(e);
}

QEventDispatcherWinRTPrivate::QEventDispatcherWinRTPrivate()
{
    const bool isGuiThread = QCoreApplication::instance() &&
            QThread::currentThread() == QCoreApplication::instance()->thread();
    CoInitializeEx(NULL, isGuiThread ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED);
    HANDLE interruptHandle = CreateEventEx(NULL, NULL, NULL, SYNCHRONIZE|EVENT_MODIFY_STATE);
    timerIdToHandle.insert(INTERRUPT_HANDLE, interruptHandle);
    timerHandleToId.insert(interruptHandle, INTERRUPT_HANDLE);
    timerInfos.reserve(256);
}

QEventDispatcherWinRTPrivate::~QEventDispatcherWinRTPrivate()
{
    CloseHandle(timerIdToHandle.value(INTERRUPT_HANDLE));
    CoUninitialize();
}

QT_END_NAMESPACE
