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

#include "qnativewifiengine.h"
#include "platformdefs.h"

#include <QtCore/qmutex.h>
#include <QtCore/private/qmutexpool_p.h>
#include <QtCore/qlibrary.h>

#include <QtNetwork/private/qbearerplugin_p.h>

#include <QtCore/qdebug.h>

#ifndef QT_NO_BEARERMANAGEMENT

QT_BEGIN_NAMESPACE

static void resolveLibrary()
{
    static QBasicAtomicInt triedResolve = Q_BASIC_ATOMIC_INITIALIZER(false);

    if (!triedResolve.loadAcquire()) {
#ifndef QT_NO_THREAD
        QMutexLocker locker(QMutexPool::globalInstanceGet(&local_WlanOpenHandle));
#endif

        if (!triedResolve.load()) {
            QLibrary wlanapiLib(QLatin1String("wlanapi"));
            local_WlanOpenHandle = (WlanOpenHandleProto)
                wlanapiLib.resolve("WlanOpenHandle");
            local_WlanRegisterNotification = (WlanRegisterNotificationProto)
                wlanapiLib.resolve("WlanRegisterNotification");
            local_WlanEnumInterfaces = (WlanEnumInterfacesProto)
                wlanapiLib.resolve("WlanEnumInterfaces");
            local_WlanGetAvailableNetworkList = (WlanGetAvailableNetworkListProto)
                wlanapiLib.resolve("WlanGetAvailableNetworkList");
            local_WlanQueryInterface = (WlanQueryInterfaceProto)
                wlanapiLib.resolve("WlanQueryInterface");
            local_WlanConnect = (WlanConnectProto)
                wlanapiLib.resolve("WlanConnect");
            local_WlanDisconnect = (WlanDisconnectProto)
                wlanapiLib.resolve("WlanDisconnect");
            local_WlanScan = (WlanScanProto)
                wlanapiLib.resolve("WlanScan");
            local_WlanFreeMemory = (WlanFreeMemoryProto)
                wlanapiLib.resolve("WlanFreeMemory");
            local_WlanCloseHandle = (WlanCloseHandleProto)
                wlanapiLib.resolve("WlanCloseHandle");

            triedResolve.storeRelease(true);
        }
    }
}

class QNativeWifiEnginePlugin : public QBearerEnginePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QBearerEngineFactoryInterface" FILE "nativewifi.json")

public:
    QNativeWifiEnginePlugin();
    ~QNativeWifiEnginePlugin();

    QBearerEngine *create(const QString &key) const;
};

QNativeWifiEnginePlugin::QNativeWifiEnginePlugin()
{
}

QNativeWifiEnginePlugin::~QNativeWifiEnginePlugin()
{
}

QBearerEngine *QNativeWifiEnginePlugin::create(const QString &key) const
{
    if (key != QLatin1String("nativewifi"))
        return 0;

    resolveLibrary();

    // native wifi dll not available
    if (!local_WlanOpenHandle)
        return 0;

    QNativeWifiEngine *engine = new QNativeWifiEngine;

    // could not initialise subsystem
    if (engine && !engine->available()) {
        delete engine;
        return 0;
    }

    return engine;
}

QT_END_NAMESPACE

#include "main.moc"

#endif // QT_NO_BEARERMANAGEMENT
