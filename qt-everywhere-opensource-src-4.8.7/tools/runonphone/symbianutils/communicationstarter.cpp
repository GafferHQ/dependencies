/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include "communicationstarter.h"
#include "bluetoothlistener.h"
#include "trkdevice.h"

#include <QtCore/QTimer>
#include <QtCore/QEventLoop>

namespace trk {

// --------------- AbstractBluetoothStarter
struct BaseCommunicationStarterPrivate {
    explicit BaseCommunicationStarterPrivate(const BaseCommunicationStarter::TrkDevicePtr &d);

    const BaseCommunicationStarter::TrkDevicePtr trkDevice;
    BluetoothListener *listener;
    QTimer *timer;
    int intervalMS;
    int attempts;
    int n;
    QString errorString;
    BaseCommunicationStarter::State state;
};

BaseCommunicationStarterPrivate::BaseCommunicationStarterPrivate(const BaseCommunicationStarter::TrkDevicePtr &d) :
        trkDevice(d),
        listener(0),
        timer(0),
        intervalMS(1000),
        attempts(-1),
        n(0),
        state(BaseCommunicationStarter::TimedOut)
{
}

BaseCommunicationStarter::BaseCommunicationStarter(const TrkDevicePtr &trkDevice, QObject *parent) :
        QObject(parent),
        d(new BaseCommunicationStarterPrivate(trkDevice))
{
}

BaseCommunicationStarter::~BaseCommunicationStarter()
{
    stopTimer();
    delete d;
}

void BaseCommunicationStarter::stopTimer()
{
    if (d->timer && d->timer->isActive())
        d->timer->stop();
}

bool BaseCommunicationStarter::initializeStartupResources(QString *errorMessage)
{
    errorMessage->clear();
    return true;
}

BaseCommunicationStarter::StartResult BaseCommunicationStarter::start()
{
    if (state() == Running) {
        d->errorString = QLatin1String("Internal error, attempt to re-start BaseCommunicationStarter.\n");
        return StartError;
    }
    // Before we instantiate timers, and such, try to open the device,
    // which should succeed if another listener is already running in
    // 'Watch' mode
    if (d->trkDevice->open(&(d->errorString)))
        return ConnectionSucceeded;
    // Pull up resources for next attempt
    d->n = 0;
    if (!initializeStartupResources(&(d->errorString)))
        return StartError;
    // Start timer
    if (!d->timer) {
        d->timer = new QTimer;
        connect(d->timer, SIGNAL(timeout()), this, SLOT(slotTimer()));
    }
    d->timer->setInterval(d->intervalMS);
    d->timer->setSingleShot(false);
    d->timer->start();
    d->state = Running;
    return Started;
}

BaseCommunicationStarter::State BaseCommunicationStarter::state() const
{
    return d->state;
}

int BaseCommunicationStarter::intervalMS() const
{
    return d->intervalMS;
}

void BaseCommunicationStarter::setIntervalMS(int i)
{
    d->intervalMS = i;
    if (d->timer)
        d->timer->setInterval(i);
}

int BaseCommunicationStarter::attempts() const
{
    return d->attempts;
}

void BaseCommunicationStarter::setAttempts(int a)
{
    d->attempts = a;
}

QString BaseCommunicationStarter::device() const
{
    return d->trkDevice->port();
}

QString BaseCommunicationStarter::errorString() const
{
    return d->errorString;
}

void BaseCommunicationStarter::slotTimer()
{
    ++d->n;
    // Check for timeout
    if (d->attempts >= 0 && d->n >= d->attempts) {
        stopTimer();
        d->errorString = tr("%1: timed out after %n attempts using an interval of %2ms.", 0, d->n)
                         .arg(d->trkDevice->port()).arg(d->intervalMS);
        d->state = TimedOut;
        emit timeout();
    } else {
        // Attempt n to connect?
        if (d->trkDevice->open(&(d->errorString))) {
            stopTimer();
            const QString msg = tr("%1: Connection attempt %2 succeeded.").arg(d->trkDevice->port()).arg(d->n);
            emit message(msg);
            d->state = Connected;
            emit connected();
        } else {
            const QString msg = tr("%1: Connection attempt %2 failed: %3 (retrying)...")
                                .arg(d->trkDevice->port()).arg(d->n).arg(d->errorString);
            emit message(msg);
        }
    }
}

// --------------- AbstractBluetoothStarter

AbstractBluetoothStarter::AbstractBluetoothStarter(const TrkDevicePtr &trkDevice, QObject *parent) :
    BaseCommunicationStarter(trkDevice, parent)
{
}

bool AbstractBluetoothStarter::initializeStartupResources(QString *errorMessage)
{
    // Create the listener and forward messages to it.
    BluetoothListener *listener = createListener();
    connect(this, SIGNAL(message(QString)), listener, SLOT(emitMessage(QString)));
    return listener->start(device(), errorMessage);
}

// -------- ConsoleBluetoothStarter
ConsoleBluetoothStarter::ConsoleBluetoothStarter(const TrkDevicePtr &trkDevice,
                                                 QObject *listenerParent,
                                                 QObject *parent) :
AbstractBluetoothStarter(trkDevice, parent),
m_listenerParent(listenerParent)
{
}

BluetoothListener *ConsoleBluetoothStarter::createListener()
{
    BluetoothListener *rc = new BluetoothListener(m_listenerParent);
    rc->setMode(BluetoothListener::Listen);
    rc->setPrintConsoleMessages(true);
    return rc;
}

bool ConsoleBluetoothStarter::startBluetooth(const TrkDevicePtr &trkDevice,
                                             QObject *listenerParent,
                                             int attempts,
                                             QString *errorMessage)
{
    // Set up a console starter to print to stdout.
    ConsoleBluetoothStarter starter(trkDevice, listenerParent);
    starter.setAttempts(attempts);
    switch (starter.start()) {
    case Started:
        break;
    case ConnectionSucceeded:
        return true;
    case StartError:
        *errorMessage = starter.errorString();
        return false;
    }
    // Run the starter with an event loop. @ToDo: Implement
    // some asynchronous keypress read to cancel.
    QEventLoop eventLoop;
    connect(&starter, SIGNAL(connected()), &eventLoop, SLOT(quit()));
    connect(&starter, SIGNAL(timeout()), &eventLoop, SLOT(quit()));
    eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
    if (starter.state() != AbstractBluetoothStarter::Connected) {
        *errorMessage = starter.errorString();
        return false;
    }
    return true;
}
} // namespace trk
