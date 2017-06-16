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

#include "instancebuffer.h"

#include <QtGui/qvector3d.h>

static const int rowCount = 20;
static const int colCount = 20;
static const int maxInstanceCount = rowCount * colCount;

InstanceBuffer::InstanceBuffer(Qt3DCore::QNode *parent)
    : Qt3DRender::QBuffer(QBuffer::VertexBuffer, parent)
    , m_instanceCount(maxInstanceCount)
{
    // Create some per instance data - position of each instance
    QByteArray ba;
    ba.resize(maxInstanceCount * sizeof(QVector3D));
    QVector3D *posData = reinterpret_cast<QVector3D *>(ba.data());
    for (int j = 0; j < rowCount; ++j) {
        const float z = float(j);
        for (int i = 0; i < colCount; ++i) {
            const float x = float(i);
            const QVector3D pos(x, 0.0f, z);
            *posData = pos;
            ++posData;
        }
    }

    // Put the data into the buffer
    setData(ba);
}

int InstanceBuffer::instanceCount() const
{
    return m_instanceCount;
}

void InstanceBuffer::setInstanceCount(int instanceCount)
{
    if (m_instanceCount == instanceCount)
        return;

    m_instanceCount = instanceCount;
    emit instanceCountChanged(instanceCount);
}

