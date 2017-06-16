/****************************************************************************
**
** Copyright (C) 2012 Research In Motion
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

#ifndef QBBENGINE_H
#define QBBENGINE_H

#include "../qbearerengine_impl.h"

#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>

#ifndef QT_NO_BEARERMANAGEMENT

struct bps_event_t;

QT_BEGIN_NAMESPACE

class QNetworkConfigurationPrivate;
class QNetworkSessionPrivate;

class QBBEngine : public QBearerEngineImpl, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit QBBEngine(QObject *parent = 0);
    ~QBBEngine();

    QString getInterfaceFromId(const QString &id) Q_DECL_OVERRIDE;
    bool hasIdentifier(const QString &id) Q_DECL_OVERRIDE;

    void connectToId(const QString &id) Q_DECL_OVERRIDE;
    void disconnectFromId(const QString &id) Q_DECL_OVERRIDE;

    Q_INVOKABLE void initialize() Q_DECL_OVERRIDE;
    Q_INVOKABLE void requestUpdate() Q_DECL_OVERRIDE;

    QNetworkSession::State sessionStateForId(const QString &id) Q_DECL_OVERRIDE;

    QNetworkConfigurationManager::Capabilities capabilities() const Q_DECL_OVERRIDE;

    QNetworkSessionPrivate *createSessionBackend() Q_DECL_OVERRIDE;

    QNetworkConfigurationPrivatePointer defaultConfiguration() Q_DECL_OVERRIDE;

    bool requiresPolling() const Q_DECL_OVERRIDE;

    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) Q_DECL_OVERRIDE;

protected:
    void updateConfiguration(const char *interface);
    void removeConfiguration(const QString &id);

private Q_SLOTS:
    void doRequestUpdate();

private:
    QHash<QString, QString> configurationInterface;

    mutable QMutex pollingMutex;

    bool pollingRequired;
    bool initialized;
};


QT_END_NAMESPACE

#endif // QT_NO_BEARERMANAGEMENT

#endif // QBBENGINE_H
