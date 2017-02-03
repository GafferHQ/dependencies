/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QML Shaders plugin of the Qt Toolkit.
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

#include "qsggeometry.h"

QT_BEGIN_NAMESPACE


const QSGGeometry::AttributeSet &QSGGeometry::defaultAttributes_Point2D()
{
    static Attribute data[] = {
        { 0, 2, GL_FLOAT }
    };
    static AttributeSet attrs = { 1, sizeof(float) * 2, data };
    return attrs;
}


const QSGGeometry::AttributeSet &QSGGeometry::defaultAttributes_TexturedPoint2D()
{
    static Attribute data[] = {
        { 0, 2, GL_FLOAT },
        { 1, 2, GL_FLOAT }
    };
    static AttributeSet attrs = { 2, sizeof(float) * 4, data };
    return attrs;
}


const QSGGeometry::AttributeSet &QSGGeometry::defaultAttributes_ColoredPoint2D()
{
    static Attribute data[] = {
        { 0, 2, GL_FLOAT },
        { 1, 4, GL_UNSIGNED_BYTE }
    };
    static AttributeSet attrs = { 2, 2 * sizeof(float) + 4 * sizeof(char), data };
    return attrs;
}



QSGGeometry::QSGGeometry(const QSGGeometry::AttributeSet &attributes,
                         int vertexCount,
                         int indexCount,
                         int indexType)
    : m_drawing_mode(GL_TRIANGLE_STRIP)
    , m_vertex_count(0)
    , m_index_count(0)
    , m_index_type(indexType)
    , m_attributes(attributes)
    , m_data(0)
    , m_index_data_offset(-1)
    , m_owns_data(false)
{
    Q_ASSERT(m_attributes.count > 0);
    Q_ASSERT(m_attributes.stride > 0);

    // Because allocate reads m_vertex_count, m_index_count and m_owns_data, these
    // need to be set before calling allocate...
    allocate(vertexCount, indexCount);
}

QSGGeometry::~QSGGeometry()
{
    if (m_owns_data)
        qFree(m_data);
}

void *QSGGeometry::indexData()
{
    return m_index_data_offset < 0
            ? 0
            : ((char *) m_data + m_index_data_offset);
}

const void *QSGGeometry::indexData() const
{
    return m_index_data_offset < 0
            ? 0
            : ((char *) m_data + m_index_data_offset);
}

void QSGGeometry::setDrawingMode(GLenum mode)
{
    m_drawing_mode = mode;
}

void QSGGeometry::allocate(int vertexCount, int indexCount)
{
    if (vertexCount == m_vertex_count && indexCount == m_index_count)
        return;

    m_vertex_count = vertexCount;
    m_index_count = indexCount;

    bool canUsePrealloc = m_index_count <= 0;
    int vertexByteSize = m_attributes.stride * m_vertex_count;

    if (m_owns_data)
        qFree(m_data);

    if (canUsePrealloc && vertexByteSize <= (int) sizeof(m_prealloc)) {
        m_data = (void *) &m_prealloc[0];
        m_index_data_offset = -1;
        m_owns_data = false;
    } else {
        Q_ASSERT(m_index_type == GL_UNSIGNED_INT || m_index_type == GL_UNSIGNED_SHORT);
        int indexByteSize = indexCount * (m_index_type == GL_UNSIGNED_SHORT ? sizeof(quint16) : sizeof(quint32));
        m_data = (void *) qMalloc(vertexByteSize + indexByteSize);
        m_index_data_offset = vertexByteSize;
        m_owns_data = true;
    }

}

void QSGGeometry::updateRectGeometry(QSGGeometry *g, const QRectF &rect)
{
    Point2D *v = g->vertexDataAsPoint2D();
    v[0].x = rect.left();
    v[0].y = rect.top();

    v[1].x = rect.right();
    v[1].y = rect.top();

    v[2].x = rect.left();
    v[2].y = rect.bottom();

    v[3].x = rect.right();
    v[3].y = rect.bottom();
}

void QSGGeometry::updateTexturedRectGeometry(QSGGeometry *g, const QRectF &rect, const QRectF &textureRect)
{
    TexturedPoint2D *v = g->vertexDataAsTexturedPoint2D();
    v[0].x = rect.left();
    v[0].y = rect.top();
    v[0].tx = textureRect.left();
    v[0].ty = textureRect.top();

    v[1].x = rect.right();
    v[1].y = rect.top();
    v[1].tx = textureRect.right();
    v[1].ty = textureRect.top();

    v[2].x = rect.left();
    v[2].y = rect.bottom();
    v[2].tx = textureRect.left();
    v[2].ty = textureRect.bottom();

    v[3].x = rect.right();
    v[3].y = rect.bottom();
    v[3].tx = textureRect.right();
    v[3].ty = textureRect.bottom();
}

QT_END_NAMESPACE
