/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
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

#include "qsharednetworksession_p.h"
#include "qbearerengine_p.h"
#include <QThreadStorage>

#ifndef QT_NO_BEARERMANAGEMENT

QT_BEGIN_NAMESPACE

QThreadStorage<QSharedNetworkSessionManager *> tls;

inline QSharedNetworkSessionManager* sharedNetworkSessionManager()
{
    QSharedNetworkSessionManager* rv = tls.localData();
    if (!rv) {
        rv = new QSharedNetworkSessionManager;
        tls.setLocalData(rv);
    }
    return rv;
}

static void doDeleteLater(QObject* obj)
{
    obj->deleteLater();
}

QSharedPointer<QNetworkSession> QSharedNetworkSessionManager::getSession(QNetworkConfiguration config)
{
    QSharedNetworkSessionManager *m(sharedNetworkSessionManager());
    //if already have a session, return it
    if (m->sessions.contains(config)) {
        QSharedPointer<QNetworkSession> p = m->sessions.value(config).toStrongRef();
        if (!p.isNull())
            return p;
    }
    //otherwise make one
    QSharedPointer<QNetworkSession> session(new QNetworkSession(config), doDeleteLater);
    m->sessions[config] = session;
    return session;
}

void QSharedNetworkSessionManager::setSession(QNetworkConfiguration config, QSharedPointer<QNetworkSession> session)
{
    QSharedNetworkSessionManager *m(sharedNetworkSessionManager());
    m->sessions[config] = session;
}

uint qHash(const QNetworkConfiguration& config)
{
    return ((uint)config.type()) | (((uint)config.bearerType()) << 8) | (((uint)config.purpose()) << 16);
}

QT_END_NAMESPACE

#endif // QT_NO_BEARERMANAGEMENT
