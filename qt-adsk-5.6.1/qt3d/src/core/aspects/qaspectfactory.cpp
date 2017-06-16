/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qaspectfactory_p.h"

#include <QtGlobal>

#include <QDebug>

#include <Qt3DCore/QAbstractAspect>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

typedef QHash<QString, QAspectFactory::CreateFunction> defaultFactories_t;
Q_GLOBAL_STATIC(defaultFactories_t, defaultFactories)
typedef QHash<const QMetaObject*, QString> defaultAspectNames_t;
Q_GLOBAL_STATIC(defaultAspectNames_t, defaultAspectNames)

QT3DCORESHARED_EXPORT void qt3d_QAspectFactory_addDefaultFactory(const QString &name,
                                                                 const QMetaObject *metaObject,
                                                                 QAspectFactory::CreateFunction factory)
{
    defaultFactories->insert(name, factory);
    defaultAspectNames->insert(metaObject, name);
}

QAspectFactory::QAspectFactory()
    : m_factories(*defaultFactories),
      m_aspectNames(*defaultAspectNames)
{
}

QAspectFactory::QAspectFactory(const QAspectFactory &other)
    : m_factories(other.m_factories),
      m_aspectNames(other.m_aspectNames)
{
}

QAspectFactory::~QAspectFactory()
{
}

QAspectFactory &QAspectFactory::operator=(const QAspectFactory &other)
{
    m_factories = other.m_factories;
    m_aspectNames = other.m_aspectNames;
    return *this;
}

QStringList QAspectFactory::availableFactories() const
{
    return m_factories.keys();
}

QAbstractAspect *QAspectFactory::createAspect(const QString &aspect, QObject *parent) const
{
    if (m_factories.contains(aspect)) {
        return m_factories.value(aspect)(parent);
    } else {
        qWarning() << "Unsupported aspect name:" << aspect << "please check registrations";
        return Q_NULLPTR;
    }
}

QString QAspectFactory::aspectName(QAbstractAspect *aspect) const
{
    return m_aspectNames.value(aspect->metaObject());
}

} // namespace Qt3DCore

QT_END_NAMESPACE
