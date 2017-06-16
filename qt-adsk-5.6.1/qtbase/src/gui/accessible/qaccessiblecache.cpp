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

#include "qaccessiblecache_p.h"

#ifndef QT_NO_ACCESSIBILITY

QT_BEGIN_NAMESPACE

/*!
    \class QAccessibleCache
    \internal
    \brief Maintains a cache of accessible interfaces.
*/

Q_GLOBAL_STATIC(QAccessibleCache, qAccessibleCache)

QAccessibleCache *QAccessibleCache::instance()
{
    return qAccessibleCache;
}

/*
  The ID is always in the range [INT_MAX+1, UINT_MAX].
  This makes it easy on windows to reserve the positive integer range
  for the index of a child and not clash with the unique ids.
*/
QAccessible::Id QAccessibleCache::acquireId() const
{
    static const QAccessible::Id FirstId = QAccessible::Id(INT_MAX) + 1;
    static QAccessible::Id lastUsedId = FirstId;

    while (idToInterface.contains(lastUsedId)) {
        // (wrap back when when we reach UINT_MAX - 1)
        // -1 because on Android -1 is taken for the "View" so just avoid it completely for consistency
        if (lastUsedId == UINT_MAX - 1)
            lastUsedId = FirstId;
        else
            ++lastUsedId;
    }

    return lastUsedId;
}

QAccessibleInterface *QAccessibleCache::interfaceForId(QAccessible::Id id) const
{
    return idToInterface.value(id);
}

QAccessible::Id QAccessibleCache::insert(QObject *object, QAccessibleInterface *iface) const
{
    Q_ASSERT(iface);
    Q_UNUSED(object)

    // object might be 0
    Q_ASSERT(!objectToId.contains(object));
    Q_ASSERT_X(!idToInterface.values().contains(iface), "", "Accessible interface inserted into cache twice!");

    QAccessible::Id id = acquireId();
    QObject *obj = iface->object();
    Q_ASSERT(object == obj);
    if (obj) {
        objectToId.insert(obj, id);
        connect(obj, &QObject::destroyed, this, &QAccessibleCache::objectDestroyed);
    }
    idToInterface.insert(id, iface);
    return id;
}

void QAccessibleCache::objectDestroyed(QObject* obj)
{
    QAccessible::Id id = objectToId.value(obj);
    if (id) {
        Q_ASSERT_X(idToInterface.contains(id), "", "QObject with accessible interface deleted, where interface not in cache!");
        deleteInterface(id, obj);
    }
}

void QAccessibleCache::deleteInterface(QAccessible::Id id, QObject *obj)
{
    QAccessibleInterface *iface = idToInterface.take(id);
    if (!obj)
        obj = iface->object();
    if (obj)
        objectToId.remove(obj);
    delete iface;

#ifdef Q_OS_MAC
    removeCocoaElement(id);
#endif
}

QT_END_NAMESPACE

#endif
