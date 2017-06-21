/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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
#include "qplatformaccessibility.h"
#include <private/qfactoryloader_p.h>
#include "qaccessibleplugin.h"
#include "qaccessibleobject.h"
#include "qaccessiblebridge.h"
#include <QtGui/QGuiApplication>

#include <QDebug>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_ACCESSIBILITY

/* accessiblebridge plugin discovery stuff */
#ifndef QT_NO_LIBRARY
Q_GLOBAL_STATIC_WITH_ARGS(QFactoryLoader, bridgeloader,
    (QAccessibleBridgeFactoryInterface_iid, QLatin1String("/accessiblebridge")))
#endif

Q_GLOBAL_STATIC(QVector<QAccessibleBridge *>, bridges)

/*!
    \class QPlatformAccessibility
    \since 5.0
    \internal
    \preliminary
    \ingroup qpa
    \ingroup accessibility

    \brief The QPlatformAccessibility class is the base class for
    integrating accessibility backends

    \sa QAccessible
*/
QPlatformAccessibility::QPlatformAccessibility()
    : m_active(false)
{
}

QPlatformAccessibility::~QPlatformAccessibility()
{
}

void QPlatformAccessibility::notifyAccessibilityUpdate(QAccessibleEvent *event)
{
    initialize();

    if (!bridges() || bridges()->isEmpty())
        return;

    for (int i = 0; i < bridges()->count(); ++i)
        bridges()->at(i)->notifyAccessibilityUpdate(event);
}

void QPlatformAccessibility::setRootObject(QObject *o)
{
    initialize();
    if (bridges()->isEmpty())
        return;

    if (!o)
        return;

    for (int i = 0; i < bridges()->count(); ++i) {
        QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(o);
        bridges()->at(i)->setRootObject(iface);
    }
}

void QPlatformAccessibility::initialize()
{
    static bool isInit = false;
    if (isInit)
        return;
    isInit = true;      // ### not atomic

#ifndef QT_NO_LIBRARY
    typedef QMultiMap<int, QString> PluginKeyMap;
    typedef PluginKeyMap::const_iterator PluginKeyMapConstIterator;

    const PluginKeyMap keyMap = bridgeloader()->keyMap();
    QAccessibleBridgePlugin *factory = 0;
    int i = -1;
    const PluginKeyMapConstIterator cend = keyMap.constEnd();
    for (PluginKeyMapConstIterator it = keyMap.constBegin(); it != cend; ++it) {
        if (it.key() != i) {
            i = it.key();
            factory = qobject_cast<QAccessibleBridgePlugin*>(bridgeloader()->instance(i));
        }
        if (factory)
            if (QAccessibleBridge *bridge = factory->create(it.value()))
                bridges()->append(bridge);
    }
#endif
}

void QPlatformAccessibility::cleanup()
{
    qDeleteAll(*bridges());
}

void QPlatformAccessibility::setActive(bool active)
{
    m_active = active;
    QAccessible::setActive(active);
}

#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE
