/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Copyright (C) 2015 The Qt Company Ltd and/or its subsidiary(-ies).
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

#ifndef _USE_MATH_DEFINES
# define _USE_MATH_DEFINES // For MSVC
#endif

#include "qcylindermesh.h"
#include "qcylindergeometry.h"
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qbufferfunctor.h>
#include <Qt3DRender/qattribute.h>
#include <qmath.h>
#include <QVector3D>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
 * \qmltype CylinderMesh
 * \instantiates Qt3DRender::QCylinderMesh
 * \inqmlmodule Qt3D.Render
 * \brief A cylindrical mesh.
 */

/*!
 * \qmlproperty int CylinderMesh::rings
 *
 * Holds the number of rings in the mesh.
 */

/*!
 * \qmlproperty int CylinderMesh::slices
 *
 * Holds the number of slices in the mesh.
 */

/*!
 * \qmlproperty float CylinderMesh::radius
 *
 * Holds the radius of the cylinder.
 */

/*!
 * \qmlproperty float CylinderMesh::length
 *
 * Holds the length of the cylinder.
 */

/*!
 * \class Qt3DRender::QCylinderMesh
 * \inmodule Qt3DRender
 *
 * \inherits Qt3DRender::QGeometryRenderer
 *
 * \brief A cylindrical mesh.
 */

/*!
 * Constructs a new QCylinderMesh with \a parent.
 */
QCylinderMesh::QCylinderMesh(QNode *parent)
    : QGeometryRenderer(parent)
{
    QCylinderGeometry *geometry = new QCylinderGeometry(this);
    QObject::connect(geometry, &QCylinderGeometry::radiusChanged, this, &QCylinderMesh::radiusChanged);
    QObject::connect(geometry, &QCylinderGeometry::ringsChanged, this, &QCylinderMesh::ringsChanged);
    QObject::connect(geometry, &QCylinderGeometry::slicesChanged, this, &QCylinderMesh::slicesChanged);
    QObject::connect(geometry, &QCylinderGeometry::lengthChanged, this, &QCylinderMesh::lengthChanged);

    QGeometryRenderer::setGeometry(geometry);
}

/*!
 * Destroys this cylinder mesh.
 */
QCylinderMesh::~QCylinderMesh()
{
    QNode::cleanup();
}

void QCylinderMesh::setRings(int rings)
{
    static_cast<QCylinderGeometry *>(geometry())->setRings(rings);
}

void QCylinderMesh::setSlices(int slices)
{
    static_cast<QCylinderGeometry *>(geometry())->setSlices(slices);
}

void QCylinderMesh::setRadius(float radius)
{
    static_cast<QCylinderGeometry *>(geometry())->setRadius(radius);
}

void QCylinderMesh::setLength(float length)
{
    static_cast<QCylinderGeometry *>(geometry())->setLength(length);
}

/*!
 * \property QCylinderMesh::rings
 *
 * Holds the number of rings in the mesh.
 */
int QCylinderMesh::rings() const
{
    return static_cast<QCylinderGeometry *>(geometry())->rings();
}

/*!
 * \property QCylinderMesh::slices
 *
 * Holds the number of slices in the mesh.
 */
int QCylinderMesh::slices() const
{
    return static_cast<QCylinderGeometry *>(geometry())->slices();
}

/*!
 * \property QCylinderMesh::radius
 *
 * Holds the radius of the cylinder.
 */
float QCylinderMesh::radius() const
{
    return static_cast<QCylinderGeometry *>(geometry())->radius();
}

/*!
 * \property QCylinderMesh::length
 *
 * Holds the length of the cylinder.
 */
float QCylinderMesh::length() const
{
    return static_cast<QCylinderGeometry *>(geometry())->length();
}

} // namespace Qt3DRender

QT_END_NAMESPACE
