/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2015 Intel Corporation.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDBus module of the Qt Toolkit.
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

#include "qdbusserver.h"
#include "qdbusconnection_p.h"
#include "qdbusconnectionmanager_p.h"
#include "qdbusutil_p.h"

#ifndef QT_NO_DBUS

QT_BEGIN_NAMESPACE

/*!
    \class QDBusServer
    \inmodule QtDBus

    \brief The QDBusServer class provides peer-to-peer communication
    between processes on the same computer.
*/

/*!
    Constructs a QDBusServer with the given \a address, and the given
    \a parent.
*/
QDBusServer::QDBusServer(const QString &address, QObject *parent)
    : QObject(parent), d(0)
{
    if (address.isEmpty())
        return;

    if (!qdbus_loadLibDBus())
        return;

    emit QDBusConnectionManager::instance()->serverRequested(address, this);
    QObject::connect(d, SIGNAL(newServerConnection(QDBusConnectionPrivate*)),
                     this, SLOT(_q_newConnection(QDBusConnectionPrivate*)), Qt::QueuedConnection);
}

/*!
    Constructs a QDBusServer with the given \a parent. The server will listen
    for connections in \c {/tmp} (on Unix systems) or on a TCP port bound to
    localhost (elsewhere).
*/
QDBusServer::QDBusServer(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_UNIX
    // Use Unix sockets on Unix systems only
    const QString address = QStringLiteral("unix:tmpdir=/tmp");
#else
    const QString address = QStringLiteral("tcp:");
#endif

    if (!qdbus_loadLibDBus()) {
        d = 0;
        return;
    }

    emit QDBusConnectionManager::instance()->serverRequested(address, this);
    QObject::connect(d, SIGNAL(newServerConnection(QDBusConnectionPrivate*)),
                     this, SLOT(_q_newConnection(QDBusConnectionPrivate*)), Qt::QueuedConnection);
}

/*!
    Destructs a QDBusServer
*/
QDBusServer::~QDBusServer()
{
    if (QDBusConnectionManager::instance()) {
        QMutexLocker locker(&QDBusConnectionManager::instance()->mutex);
        Q_FOREACH (const QString &name, d->serverConnectionNames) {
            QDBusConnectionManager::instance()->removeConnection(name);
        }
        d->serverConnectionNames.clear();
    }
    d->ref.store(0);
    d->deleteLater();
}

/*!
    Returns \c true if this QDBusServer object is connected.

    If it isn't connected, you need to call the constructor again.
*/
bool QDBusServer::isConnected() const
{
    return d && d->server && q_dbus_server_get_is_connected(d->server);
}

/*!
    Returns the last error that happened in this server.

    This function is provided for low-level code.
*/
QDBusError QDBusServer::lastError() const
{
    return d ? d->lastError : QDBusError(QDBusError::Disconnected, QDBusUtil::disconnectedErrorMessage());
}

/*!
    Returns the address this server is associated with.
*/
QString QDBusServer::address() const
{
    QString addr;
    if (d && d->server) {
        char *c = q_dbus_server_get_address(d->server);
        addr = QString::fromUtf8(c);
        q_dbus_free(c);
    }

    return addr;
}

/*!
    \since 5.3

    If \a value is set to true, an incoming connection can proceed even if the
    connecting client is not authenticated as a user.

    By default, this value is false.

    \sa isAnonymousAuthenticationAllowed()
*/
void QDBusServer::setAnonymousAuthenticationAllowed(bool value)
{
    d->anonymousAuthenticationAllowed = value;
}

/*!
    \since 5.3

    Returns true if anonymous authentication is allowed.

    \sa setAnonymousAuthenticationAllowed()
*/
bool QDBusServer::isAnonymousAuthenticationAllowed() const
{
    return d->anonymousAuthenticationAllowed;
}

/*!
  \fn void QDBusServer::newConnection(const QDBusConnection &connection)

  This signal is emitted when a new client connection \a connection is
  established to the server.
 */

QT_END_NAMESPACE

#include "moc_qdbusserver.cpp"

#endif // QT_NO_DBUS
