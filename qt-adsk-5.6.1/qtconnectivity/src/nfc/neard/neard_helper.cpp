/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2014 BasysKom GmbH.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include <QDBusMetaType>
#include "neard_helper_p.h"
#include "dbusobjectmanager_p.h"

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_NFC_NEARD)
Q_GLOBAL_STATIC(NeardHelper, neardHelper)

NeardHelper::NeardHelper(QObject *parent) :
    QObject(parent)
{
    qDBusRegisterMetaType<InterfaceList>();
    qDBusRegisterMetaType<ManagedObjectList>();

    m_dbusObjectManager = new OrgFreedesktopDBusObjectManagerInterface(QStringLiteral("org.neard"),
                                                                       QStringLiteral("/"),
                                                                       QDBusConnection::systemBus(),
                                                                       this);
    if (!m_dbusObjectManager->isValid()) {
        qCCritical(QT_NFC_NEARD) << "dbus object manager invalid";
        return;
    }

    connect(m_dbusObjectManager, SIGNAL(InterfacesAdded(QDBusObjectPath,InterfaceList)),
            this,                SLOT(interfacesAdded(QDBusObjectPath,InterfaceList)));
    connect(m_dbusObjectManager, SIGNAL(InterfacesRemoved(QDBusObjectPath,QStringList)),
            this,                SLOT(interfacesRemoved(QDBusObjectPath,QStringList)));
}

NeardHelper *NeardHelper::instance()
{
    return neardHelper();
}

OrgFreedesktopDBusObjectManagerInterface *NeardHelper::dbusObjectManager()
{
    return m_dbusObjectManager;
}

void NeardHelper::interfacesAdded(const QDBusObjectPath &path, InterfaceList interfaceList)
{
    foreach (const QString &key, interfaceList.keys()) {
        if (key == QStringLiteral("org.neard.Tag")) {
            emit tagFound(path);
            break;
        }
        if (key == QStringLiteral("org.neard.Record")) {
            emit recordFound(path);
            break;
        }
    }
}

void NeardHelper::interfacesRemoved(const QDBusObjectPath &path, const QStringList &list)
{
    if (list.contains(QStringLiteral("org.neard.Record"))) {
        qCDebug(QT_NFC_NEARD) << "record removed" << path.path();
        emit recordRemoved(path);
    } else if (list.contains(QStringLiteral("org.neard.Tag"))) {
        qCDebug(QT_NFC_NEARD) << "tag removed" << path.path();
        emit tagRemoved(path);
    }
}

QT_END_NAMESPACE
