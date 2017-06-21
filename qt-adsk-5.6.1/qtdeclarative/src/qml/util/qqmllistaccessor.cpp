/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmllistaccessor_p.h"

#include <private/qqmlmetatype_p.h>

#include <QtCore/qstringlist.h>
#include <QtCore/qdebug.h>

// ### Remove me
#include <private/qqmlengine_p.h>

QT_BEGIN_NAMESPACE

QQmlListAccessor::QQmlListAccessor()
: m_type(Invalid)
{
}

QQmlListAccessor::~QQmlListAccessor()
{
}

QVariant QQmlListAccessor::list() const
{
    return d;
}

void QQmlListAccessor::setList(const QVariant &v, QQmlEngine *engine)
{
    d = v;

    // An incoming JS array as model is treated as a variant list, so we need to
    // convert it first with toVariant().
    if (d.userType() == qMetaTypeId<QJSValue>())
        d = d.value<QJSValue>().toVariant();

    QQmlEnginePrivate *enginePrivate = engine?QQmlEnginePrivate::get(engine):0;

    if (!d.isValid()) {
        m_type = Invalid;
    } else if (d.userType() == QVariant::StringList) {
        m_type = StringList;
    } else if (d.userType() == QMetaType::QVariantList) {
        m_type = VariantList;
    } else if (d.canConvert(QVariant::Int)) {
        m_type = Integer;
    } else if ((!enginePrivate && QQmlMetaType::isQObject(d.userType())) ||
               (enginePrivate && enginePrivate->isQObject(d.userType()))) {
        QObject *data = enginePrivate?enginePrivate->toQObject(d):QQmlMetaType::toQObject(d);
        d = QVariant::fromValue(data);
        m_type = Instance;
    } else if (d.userType() == qMetaTypeId<QQmlListReference>()) {
        m_type = ListProperty;
    } else {
        m_type = Instance;
    }
}

int QQmlListAccessor::count() const
{
    switch(m_type) {
    case StringList:
        return qvariant_cast<QStringList>(d).count();
    case VariantList:
        return qvariant_cast<QVariantList>(d).count();
    case ListProperty:
        return ((const QQmlListReference *)d.constData())->count();
    case Instance:
        return 1;
    case Integer:
        return d.toInt();
    default:
    case Invalid:
        return 0;
    }
}

QVariant QQmlListAccessor::at(int idx) const
{
    Q_ASSERT(idx >= 0 && idx < count());
    switch(m_type) {
    case StringList:
        return QVariant::fromValue(qvariant_cast<QStringList>(d).at(idx));
    case VariantList:
        return qvariant_cast<QVariantList>(d).at(idx);
    case ListProperty:
        return QVariant::fromValue(((const QQmlListReference *)d.constData())->at(idx));
    case Instance:
        return d;
    case Integer:
        return QVariant(idx);
    default:
    case Invalid:
        return QVariant();
    }
}

bool QQmlListAccessor::isValid() const
{
    return m_type != Invalid;
}

QT_END_NAMESPACE
