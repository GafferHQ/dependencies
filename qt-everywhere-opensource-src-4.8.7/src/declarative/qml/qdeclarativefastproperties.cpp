/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "private/qdeclarativefastproperties_p.h"

#include <private/qdeclarativeitem_p.h>

QT_BEGIN_NAMESPACE

// Adding entries to the QDeclarativeFastProperties class allows the QML 
// binding optimizer to bypass Qt's meta system and read and, more 
// importantly, subscribe to properties directly.  Any property that is
// primarily read from bindings is a candidate for inclusion as a fast 
// property.

static void QObject_objectName(QObject *object, void *output, QDeclarativeNotifierEndpoint *endpoint)
{
    if (endpoint) 
        endpoint->connect(QDeclarativeData::get(object, true)->objectNameNotifier());
    *((QString *)output) = object->objectName();
}

QDeclarativeFastProperties::QDeclarativeFastProperties()
{
    add(&QDeclarativeItem::staticMetaObject, QDeclarativeItem::staticMetaObject.indexOfProperty("parent"),
        QDeclarativeItemPrivate::parentProperty);
    add(&QObject::staticMetaObject, QObject::staticMetaObject.indexOfProperty("objectName"),
        QObject_objectName);    
}

int QDeclarativeFastProperties::accessorIndexForProperty(const QMetaObject *metaObject, int propertyIndex)
{
    Q_ASSERT(metaObject);
    Q_ASSERT(propertyIndex >= 0);

    // Find the "real" metaObject
    while (metaObject->propertyOffset() > propertyIndex) 
        metaObject = metaObject->superClass();

    QHash<QPair<const QMetaObject *, int>, int>::Iterator iter = 
        m_index.find(qMakePair(metaObject, propertyIndex));
    if (iter != m_index.end())
        return *iter;
    else
        return -1;
}

void QDeclarativeFastProperties::add(const QMetaObject *metaObject, int propertyIndex, Accessor accessor)
{
    Q_ASSERT(metaObject);
    Q_ASSERT(propertyIndex >= 0);

    // Find the "real" metaObject
    while (metaObject->propertyOffset() > propertyIndex) 
        metaObject = metaObject->superClass();

    QPair<const QMetaObject *, int> data = qMakePair(metaObject, propertyIndex);
    int accessorIndex = m_accessors.count();
    m_accessors.append(accessor);
    m_index.insert(data, accessorIndex);
}

QT_END_NAMESPACE
