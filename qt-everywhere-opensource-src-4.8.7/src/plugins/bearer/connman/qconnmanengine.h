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

#ifndef QCONNMANENGINE_P_H
#define QCONNMANENGINE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of the QLibrary class.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include "../qbearerengine_impl.h"

#include "qconnmanservice_linux_p.h"

#include <QMap>
#include <QVariant>

#ifndef QT_NO_BEARERMANAGEMENT
#ifndef QT_NO_DBUS

QT_BEGIN_NAMESPACE

class QConnmanEngine : public QBearerEngineImpl
{
    Q_OBJECT

public:
    QConnmanEngine(QObject *parent = 0);
    ~QConnmanEngine();

    bool connmanAvailable() const;

    virtual QString getInterfaceFromId(const QString &id);
    bool hasIdentifier(const QString &id);

    virtual void connectToId(const QString &id);
    virtual void disconnectFromId(const QString &id);

    Q_INVOKABLE void initialize();
    Q_INVOKABLE void requestUpdate();

    QNetworkSession::State sessionStateForId(const QString &id);
    QNetworkSessionPrivate *createSessionBackend();

    virtual quint64 bytesWritten(const QString &id);
    virtual quint64 bytesReceived(const QString &id);
    virtual quint64 startTime(const QString &id);


    virtual QNetworkConfigurationManager::Capabilities capabilities() const;
    virtual QNetworkConfigurationPrivatePointer defaultConfiguration();

    void configurationChange(const QString &id);
    QList<QNetworkConfigurationPrivate *> getConfigurations();


private Q_SLOTS:

    void doRequestUpdate();
    void servicePropertyChangedContext(const QString &,const QString &,const QDBusVariant &);
    void propertyChangedContext(const QString &,const QString &,const QDBusVariant &);
    void technologyPropertyChangedContext(const QString &,const QString &, const QDBusVariant &);

private:
    QConnmanManagerInterface *connmanManager;

    QList<QNetworkConfigurationPrivate *> foundConfigurations;

    QString serviceFromId(const QString &id);
    QString networkFromId(const QString &id);

    QNetworkConfiguration::StateFlags getStateForService(const QString &service);
    QNetworkConfiguration::BearerType typeToBearer(const QString &type);

    void removeConfiguration(const QString &servicePath);
    void addServiceConfiguration(const QString &servicePath);
    QDateTime activeTime;


    QMap<QString,QConnmanTechnologyInterface *> technologies; // techpath, tech interface
    QMap<QString,QString> configInterfaces; // id, interface name
    QList<QString> serviceNetworks; //servpath

    QNetworkConfiguration::BearerType ofonoTechToBearerType(const QString &type);
    bool isRoamingAllowed(const QString &context);
protected:
    bool requiresPolling() const;
};


QT_END_NAMESPACE

#endif // QT_NO_DBUS
#endif // QT_NO_BEARERMANAGEMENT

#endif

