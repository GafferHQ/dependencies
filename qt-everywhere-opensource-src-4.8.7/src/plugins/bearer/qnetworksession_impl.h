/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QNETWORKSESSION_IMPL_H
#define QNETWORKSESSION_IMPL_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qbearerengine_impl.h"

#include <QtNetwork/private/qnetworkconfigmanager_p.h>
#include <QtNetwork/private/qnetworksession_p.h>

#ifndef QT_NO_BEARERMANAGEMENT

QT_BEGIN_NAMESPACE

class QBearerEngineImpl;

class QNetworkSessionPrivateImpl : public QNetworkSessionPrivate
{
    Q_OBJECT

public:
    QNetworkSessionPrivateImpl()
        : startTime(0), sessionTimeout(-1)
    {}
    ~QNetworkSessionPrivateImpl()
    {}

    //called by QNetworkSession constructor and ensures
    //that the state is immediately updated (w/o actually opening
    //a session). Also this function should take care of 
    //notification hooks to discover future state changes.
    void syncStateWithInterface();

#ifndef QT_NO_NETWORKINTERFACE
    QNetworkInterface currentInterface() const;
#endif
    QVariant sessionProperty(const QString& key) const;
    void setSessionProperty(const QString& key, const QVariant& value);

    void open();
    void close();
    void stop();
    void migrate();
    void accept();
    void ignore();
    void reject();

    QString errorString() const; //must return translated string
    QNetworkSession::SessionError error() const;

    quint64 bytesWritten() const;
    quint64 bytesReceived() const;
    quint64 activeTime() const;

private Q_SLOTS:
    void networkConfigurationsChanged();
    void configurationChanged(QNetworkConfigurationPrivatePointer config);
    void forcedSessionClose(const QNetworkConfiguration &config);
    void connectionError(const QString &id, QBearerEngineImpl::ConnectionError error);
    void decrementTimeout();

private:
    void updateStateFromServiceNetwork();
    void updateStateFromActiveConfig();

private:
    QBearerEngineImpl *engine;

    quint64 startTime;

    QNetworkSession::SessionError lastError;

    int sessionTimeout;

    bool opened;
};

QT_END_NAMESPACE

#endif // QT_NO_BEARERMANAGEMENT

#endif // QNETWORKSESSION_IMPL_H
