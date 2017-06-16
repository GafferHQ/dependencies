/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QBEARERENGINE_IMPL_H
#define QBEARERENGINE_IMPL_H

#include <QtNetwork/private/qbearerengine_p.h>

QT_BEGIN_NAMESPACE

class QBearerEngineImpl : public QBearerEngine
{
    Q_OBJECT

public:
    enum ConnectionError {
        InterfaceLookupError = 0,
        ConnectError,
        OperationNotSupported,
        DisconnectionError,
    };

    QBearerEngineImpl(QObject *parent = 0) : QBearerEngine(parent) {}
    ~QBearerEngineImpl() {}

    virtual void connectToId(const QString &id) = 0;
    virtual void disconnectFromId(const QString &id) = 0;

    virtual QString getInterfaceFromId(const QString &id) = 0;

    virtual QNetworkSession::State sessionStateForId(const QString &id) = 0;

    virtual quint64 bytesWritten(const QString &) { return Q_UINT64_C(0); }
    virtual quint64 bytesReceived(const QString &) { return Q_UINT64_C(0); }
    virtual quint64 startTime(const QString &) { return Q_UINT64_C(0); }

Q_SIGNALS:
    void connectionError(const QString &id, QBearerEngineImpl::ConnectionError error);
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QBearerEngineImpl::ConnectionError)

#endif // QBEARERENGINE_IMPL_H
