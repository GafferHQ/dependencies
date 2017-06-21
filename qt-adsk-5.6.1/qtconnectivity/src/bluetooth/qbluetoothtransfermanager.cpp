/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "qbluetoothtransfermanager.h"
#include "qbluetoothtransferrequest.h"
#include "qbluetoothtransferreply.h"
#ifdef QT_BLUEZ_BLUETOOTH
#include "qbluetoothtransferreply_bluez_p.h"
#elif QT_OSX_BLUETOOTH
#include "qbluetoothtransferreply_osx_p.h"
#else
#if !defined(QT_ANDROID_BLUETOOTH) && !defined(QT_IOS_BLUETOOTH)
#include "dummy/dummy_helper_p.h"
#endif
#endif

QT_BEGIN_NAMESPACE

/*!
    \class QBluetoothTransferManager
    \inmodule QtBluetooth
    \brief The QBluetoothTransferManager class transfers data to another device
    using Object Push Profile (OPP).

    \since 5.2

    QBluetoothTransferManager uses OBEX to send put commands to remote devices. A typical
    OBEX transfer is initialized as follows:

    \snippet doc_src_qtbluetooth.cpp sendfile

    Note that this API is not currently supported on Android.
*/

/*!
    \fn QBluetoothTransferReply *QBluetoothTransferManager::put(const QBluetoothTransferRequest &request, QIODevice *data)

    Sends the contents of \a data to the remote device identified by \a request, and returns a new
    QBluetoothTransferReply that can be used to track the request's progress. \a data must remain valid
    until the \l finished() signal is emitted.

    The returned \l QBluetoothTransferReply object must be immediately checked for its
    \l {QBluetoothTransferReply::error()}{error()} state. This is required in case
    this function detects an error during the initialization of the
    \l QBluetoothTransferReply. In such cases \l {QBluetoothTransferReply::isFinished()} returns
    \c true as well.

    If the platform does not support the Object Push profile, this function will return \c 0.
*/



/*!
    \fn void QBluetoothTransferManager::finished(QBluetoothTransferReply *reply)

    This signal is emitted when the transfer for \a reply finishes.
*/

/*!
    Constructs a new QBluetoothTransferManager with \a parent.
*/
QBluetoothTransferManager::QBluetoothTransferManager(QObject *parent)
:   QObject(parent)
{
    qRegisterMetaType<QBluetoothTransferReply*>();
    qRegisterMetaType<QBluetoothTransferReply::TransferError>();
}

/*!
    Destroys the QBluetoothTransferManager.
*/
QBluetoothTransferManager::~QBluetoothTransferManager()
{
}

QBluetoothTransferReply *QBluetoothTransferManager::put(const QBluetoothTransferRequest &request,
                                                        QIODevice *data)
{
#ifdef QT_BLUEZ_BLUETOOTH
    QBluetoothTransferReplyBluez *rep = new QBluetoothTransferReplyBluez(data, request, this);
    connect(rep, SIGNAL(finished(QBluetoothTransferReply*)), this, SIGNAL(finished(QBluetoothTransferReply*)));
    return rep;
#elif QT_OSX_BLUETOOTH
    QBluetoothTransferReply *reply = new QBluetoothTransferReplyOSX(data, request, this);
    connect(reply, SIGNAL(finished(QBluetoothTransferReply*)), this, SIGNAL(finished(QBluetoothTransferReply*)));
    return reply;
#else
    // Android and iOS have no implementation
#if !defined(QT_ANDROID_BLUETOOTH) && !defined(QT_IOS_BLUETOOTH)
    printDummyWarning();
#endif
    Q_UNUSED(request);
    Q_UNUSED(data);
    return 0;
#endif
}
#include "moc_qbluetoothtransfermanager.cpp"

QT_END_NAMESPACE
