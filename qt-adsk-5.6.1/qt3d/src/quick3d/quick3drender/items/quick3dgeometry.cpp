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

#include "quick3dgeometry_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {
namespace Quick {

Quick3DGeometry::Quick3DGeometry(QObject *parent)
    : QObject(parent)
{
}

QQmlListProperty<Qt3DRender::QAbstractAttribute> Quick3DGeometry::attributeList()
{
    return QQmlListProperty<Qt3DRender::QAbstractAttribute>(this, 0,
                                                            &Quick3DGeometry::appendAttribute,
                                                            &Quick3DGeometry::attributesCount,
                                                            &Quick3DGeometry::attributeAt,
                                                            &Quick3DGeometry::clearAttributes);
}

void Quick3DGeometry::appendAttribute(QQmlListProperty<Qt3DRender::QAbstractAttribute> *list, Qt3DRender::QAbstractAttribute *attribute)
{
    Quick3DGeometry *geometry = static_cast<Quick3DGeometry *>(list->object);
    geometry->m_managedAttributes.append(attribute);
    geometry->parentGeometry()->addAttribute(attribute);
}

Qt3DRender::QAbstractAttribute *Quick3DGeometry::attributeAt(QQmlListProperty<Qt3DRender::QAbstractAttribute> *list, int index)
{
    Quick3DGeometry *geometry = static_cast<Quick3DGeometry *>(list->object);
    return geometry->parentGeometry()->attributes().at(index);
}

int Quick3DGeometry::attributesCount(QQmlListProperty<Qt3DRender::QAbstractAttribute> *list)
{
    Quick3DGeometry *geometry = static_cast<Quick3DGeometry *>(list->object);
    return geometry->parentGeometry()->attributes().count();
}

void Quick3DGeometry::clearAttributes(QQmlListProperty<Qt3DRender::QAbstractAttribute> *list)
{
    Quick3DGeometry *geometry = static_cast<Quick3DGeometry *>(list->object);
    QVector<Qt3DRender::QAbstractAttribute *> &managedAttributes = geometry->m_managedAttributes;
    Q_FOREACH (Qt3DRender::QAbstractAttribute *attribute, managedAttributes)
        geometry->parentGeometry()->removeAttribute(attribute);
    managedAttributes.clear();
}

} // namespace Quick
} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
