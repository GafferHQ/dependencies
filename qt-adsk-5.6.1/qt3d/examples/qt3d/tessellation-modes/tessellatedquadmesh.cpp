/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include "tessellatedquadmesh.h"

#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qgeometry.h>

class TessellatedGeometry : public Qt3DRender::QGeometry
{
    Q_OBJECT
public:
    TessellatedGeometry(Qt3DCore::QNode *parent = Q_NULLPTR)
        : Qt3DRender::QGeometry(parent)
        , m_positionAttribute(new Qt3DRender::QAttribute(this))
        , m_vertexBuffer(new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer, this))
    {
        const float positionData[] = {
            -0.8f, -0.8f, 0.0f,
             0.8f, -0.8f, 0.0f,
             0.8f,  0.8f, 0.0f,
            -0.8f,  0.8f, 0.0f
        };

        const int nVerts = 4;
        const int size = nVerts * 3 * sizeof(float);
        QByteArray positionBytes;
        positionBytes.resize(size);
        memcpy(positionBytes.data(), positionData, size);

        m_vertexBuffer->setData(positionBytes);

        m_positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
        m_positionAttribute->setDataType(Qt3DRender::QAttribute::Float);
        m_positionAttribute->setDataSize(3);
        m_positionAttribute->setCount(nVerts);
        m_positionAttribute->setByteStride(3 * sizeof(float));
        m_positionAttribute->setBuffer(m_vertexBuffer);

        setVerticesPerPatch(4);
        addAttribute(m_positionAttribute);
    }

private:
    Qt3DRender::QAttribute *m_positionAttribute;
    Qt3DRender::QBuffer *m_vertexBuffer;
};

TessellatedQuadMesh::TessellatedQuadMesh(Qt3DCore::QNode *parent)
    : Qt3DRender::QGeometryRenderer(parent)
{
    setPrimitiveType(Qt3DRender::QGeometryRenderer::Patches);
    setGeometry(new TessellatedGeometry(this));
}

TessellatedQuadMesh::~TessellatedQuadMesh()
{
    QNode::cleanup();
}

#include "tessellatedquadmesh.moc"
