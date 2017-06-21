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

#include "objloader_p.h"

#include "qmesh.h"
#include "qbuffer.h"
#include "qattribute.h"
#include <Qt3DRender/private/qaxisalignedboundingbox_p.h>

#include <Qt3DRender/private/renderlogging_p.h>
#include <QFile>
#include <QTextStream>
#include <QVector>
#include <QRegExp>

#include <Qt3DRender/qgeometry.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qbuffer.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

inline uint qHash(const FaceIndices &faceIndices)
{
    return faceIndices.positionIndex
            + 10 * faceIndices.texCoordIndex
            + 100 * faceIndices.normalIndex;
}

ObjLoader::ObjLoader()
    : m_loadTextureCoords( true ),
      m_generateTangents( true ),
      m_centerMesh( false )
{
}

bool ObjLoader::load(const QString& fileName , const QString &subMesh)
{
    QFile file(fileName);
    if (!file.open(::QIODevice::ReadOnly|::QIODevice::Text)) {
        qCDebug(Render::Io) << "Could not open file" << fileName << "for reading";
        return false;
    }

    return load(&file, subMesh);
}

static void addFaceVertex(const FaceIndices &faceIndices,
                          QVector<FaceIndices> &faceIndexVector,
                          QHash<FaceIndices, unsigned int> &faceIndexMap)
{
    if (faceIndices.positionIndex != std::numeric_limits<unsigned int>::max()) {
        faceIndexVector.append(faceIndices);
        if (!faceIndexMap.contains(faceIndices))
            faceIndexMap.insert(faceIndices, faceIndexMap.size());
    } else {
        qCWarning(Render::Io) << "Missing position index";
    }
}

bool ObjLoader::load(::QIODevice *ioDev, const QString &subMesh)
{
    Q_CHECK_PTR(ioDev);
    if (!ioDev->isOpen()) {
        qCWarning(Render::Io) << "iodevice" << ioDev << "not open for reading";
        return false;
    }

    int faceCount = 0;

    // Parse faces taking into account each vertex in a face can index different indices
    // for the positions, normals and texture coords;
    // Generate unique vertices (in OpenGL parlance) and output to m_points, m_texCoords,
    // m_normals and calculate mapping from faces to unique indices
    QVector<QVector3D> positions;
    QVector<QVector3D> normals;
    QVector<QVector2D> texCoords;
    QHash<FaceIndices, unsigned int> faceIndexMap;
    QVector<FaceIndices> faceIndexVector;

    bool skipping = false;
    int positionsOffset = 0;
    int normalsOffset = 0;
    int texCoordsOffset = 0;

    QRegExp subMeshMatch(subMesh);
    if (!subMeshMatch.isValid())
        subMeshMatch.setPattern(QString(QStringLiteral("^(%1)$")).arg(subMesh));
    Q_ASSERT(subMeshMatch.isValid());

    QTextStream stream(ioDev);
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        line = line.simplified();

        if (line.length() > 0 && line.at(0) != QChar::fromLatin1('#')) {
            const QVector<QStringRef> tokens = line.splitRef(QChar::fromLatin1(' '));

            if (tokens.first() == QStringLiteral("v")) {
                if (tokens.size() < 4) {
                    qCWarning(Render::Io) << "Unsupported number of components in vertex";
                } else {
                    if (!skipping) {
                        float x = tokens.at(1).toFloat();
                        float y = tokens.at(2).toFloat();
                        float z = tokens.at(3).toFloat();
                        positions.append(QVector3D( x, y, z ));
                    } else {
                        positionsOffset++;
                    }
                }
            } else if (tokens.first() == QStringLiteral("vt") && m_loadTextureCoords) {
                if (tokens.size() < 3) {
                    qCWarning(Render::Io) << "Unsupported number of components in texture coordinate";
                } else {
                    if (!skipping) {
                        // Process texture coordinate
                        float s = tokens.at(1).toFloat();
                        float t = tokens.at(2).toFloat();
                        //FlipUVs
                        t = 1.0f - t;
                        texCoords.append(QVector2D( s, t ));
                    } else {
                        texCoordsOffset++;
                    }
                }
            } else if (tokens.first() == QStringLiteral("vn")) {
                if (tokens.size() < 4) {
                    qCWarning(Render::Io) << "Unsupported number of components in vertex normal";
                } else {
                    if (!skipping) {
                        float x = tokens.at(1).toFloat();
                        float y = tokens.at(2).toFloat();
                        float z = tokens.at(3).toFloat();
                        normals.append(QVector3D( x, y, z ));
                    } else {
                        normalsOffset++;
                    }
                }
            } else if (!skipping && tokens.first() == QStringLiteral("f")) {
                // Process face
                ++faceCount;

                int faceVertices = tokens.size() - 1;

                QVector<FaceIndices> face;
                face.reserve(faceVertices);

                for (int i = 0; i < faceVertices; i++) {
                    FaceIndices faceIndices;
                    const QVector<QStringRef> indices = tokens.at(i + 1).split(QChar::fromLatin1('/'));
                    switch (indices.size()) {
                    case 3:
                        faceIndices.normalIndex = indices.at(2).toInt() - 1 - normalsOffset;  // fall through
                    case 2:
                        faceIndices.texCoordIndex = indices.at(1).toInt() - 1 - texCoordsOffset; // fall through
                    case 1:
                        faceIndices.positionIndex = indices.at(0).toInt() - 1 - positionsOffset;
                        break;
                    default:
                        qCWarning(Render::Io) << "Unsupported number of indices in face element";
                    }

                    face.append(faceIndices);
                }

                // If number of edges in face is greater than 3,
                // decompose into triangles as a triangle fan.
                FaceIndices v0 = face[0];
                FaceIndices v1 = face[1];
                FaceIndices v2 = face[2];

                // First face
                addFaceVertex(v0, faceIndexVector, faceIndexMap);
                addFaceVertex(v1, faceIndexVector, faceIndexMap);
                addFaceVertex(v2, faceIndexVector, faceIndexMap);

                for ( int i = 3; i < face.size(); ++i ) {
                    v1 = v2;
                    v2 = face[i];
                    addFaceVertex(v0, faceIndexVector, faceIndexMap);
                    addFaceVertex(v1, faceIndexVector, faceIndexMap);
                    addFaceVertex(v2, faceIndexVector, faceIndexMap);
                }

                // end of face
            } else if (tokens.first() == QStringLiteral("o")) {
                if (tokens.size() < 2) {
                    qCWarning(Render::Io) << "Missing submesh name";
                } else {
                    if (!subMesh.isEmpty() ) {
                        QString objName = tokens.at(1).toString();
                        skipping = subMeshMatch.indexIn(objName) < 0;
                    }
                }
            }
        } // end of input line
    } // while (!stream.atEnd())

    updateIndices(positions, normals, texCoords, faceIndexMap, faceIndexVector);

    if (m_normals.isEmpty())
        generateAveragedNormals(m_points, m_normals, m_indices);

    if (m_generateTangents && !m_texCoords.isEmpty())
        generateTangents(m_points, m_normals, m_indices, m_texCoords, m_tangents);

    if (m_centerMesh)
        center(m_points);

    qCDebug(Render::Io) << "Loaded mesh:";
    qCDebug(Render::Io) << " " << m_points.size() << "points";
    qCDebug(Render::Io) << " " << faceCount << "faces";
    qCDebug(Render::Io) << " " << m_indices.size() / 3 << "triangles.";
    qCDebug(Render::Io) << " " << m_normals.size() << "normals";
    qCDebug(Render::Io) << " " << m_tangents.size() << "tangents ";
    qCDebug(Render::Io) << " " << m_texCoords.size() << "texture coordinates.";

    return true;
}

QGeometry *ObjLoader::geometry() const
{
    QByteArray bufferBytes;
    const int count = m_points.size();
    const quint32 elementSize = 3 + (hasTextureCoordinates() ? 2 : 0)
            + (hasNormals() ? 3 : 0)
            + (hasTangents() ? 4 : 0);
    const quint32 stride = elementSize * sizeof(float);
    bufferBytes.resize(stride * count);
    float* fptr = reinterpret_cast<float*>(bufferBytes.data());

    for (int index = 0; index < count; ++index) {
        *fptr++ = m_points.at(index).x();
        *fptr++ = m_points.at(index).y();
        *fptr++ = m_points.at(index).z();

        if (hasTextureCoordinates()) {
            *fptr++ = m_texCoords.at(index).x();
            *fptr++ = m_texCoords.at(index).y();
        }

        if (hasNormals()) {
            *fptr++ = m_normals.at(index).x();
            *fptr++ = m_normals.at(index).y();
            *fptr++ = m_normals.at(index).z();
        }

        if (hasTangents()) {
            *fptr++ = m_tangents.at(index).x();
            *fptr++ = m_tangents.at(index).y();
            *fptr++ = m_tangents.at(index).z();
            *fptr++ = m_tangents.at(index).w();
        }
    } // of buffer filling loop

    QBuffer *buf(new QBuffer(QBuffer::VertexBuffer));
    buf->setData(bufferBytes);

    QGeometry *geometry = new QGeometry();
    QAttribute *positionAttribute = new QAttribute(buf, QAttribute::defaultPositionAttributeName(), QAttribute::Float, 3, count, 0, stride);
    geometry->addAttribute(positionAttribute);
    quint32 offset = sizeof(float) * 3;

    if (hasTextureCoordinates()) {
        QAttribute *texCoordAttribute = new QAttribute(buf, QAttribute::defaultTextureCoordinateAttributeName(),  QAttribute::Float, 2, count, offset, stride);
        geometry->addAttribute(texCoordAttribute);
        offset += sizeof(float) * 2;
    }

    if (hasNormals()) {
        QAttribute *normalAttribute = new QAttribute(buf, QAttribute::defaultNormalAttributeName(), QAttribute::Float, 3, count, offset, stride);
        geometry->addAttribute(normalAttribute);
        offset += sizeof(float) * 3;
    }

    if (hasTangents()) {
        QAttribute *tangentAttribute = new QAttribute(buf, QAttribute::defaultTangentAttributeName(),QAttribute::Float, 4, count, offset, stride);
        geometry->addAttribute(tangentAttribute);
        offset += sizeof(float) * 4;
    }

    QByteArray indexBytes;
    QAttribute::DataType ty;
    if (m_indices.size() < 65536) {
        // we can use USHORT
        ty = QAttribute::UnsignedShort;
        indexBytes.resize(m_indices.size() * sizeof(quint16));
        quint16* usptr = reinterpret_cast<quint16*>(indexBytes.data());
        for (int i=0; i<m_indices.size(); ++i)
            *usptr++ = static_cast<quint16>(m_indices.at(i));
    } else {
        // use UINT - no conversion needed, but let's ensure int is 32-bit!
        ty = QAttribute::UnsignedInt;
        Q_ASSERT(sizeof(int) == sizeof(quint32));
        indexBytes.resize(m_indices.size() * sizeof(quint32));
        memcpy(indexBytes.data(), reinterpret_cast<const char*>(m_indices.data()), indexBytes.size());
    }

    QBuffer *indexBuffer(new QBuffer(QBuffer::IndexBuffer));
    indexBuffer->setData(indexBytes);
    QAttribute *indexAttribute = new QAttribute(indexBuffer, ty, 1, m_indices.size());
    indexAttribute->setAttributeType(QAttribute::IndexAttribute);
    geometry->addAttribute(indexAttribute);

    return geometry;
}

void ObjLoader::updateIndices(const QVector<QVector3D> &positions,
                              const QVector<QVector3D> &normals,
                              const QVector<QVector2D> &texCoords,
                              const QHash<FaceIndices, unsigned int> &faceIndexMap,
                              const QVector<FaceIndices> &faceIndexVector)
{
    // Iterate over the faceIndexMap and pull out pos, texCoord and normal data
    // thereby generating unique vertices of data (by OpenGL definition)
    const int vertexCount = faceIndexMap.size();
    const bool hasTexCoords = !texCoords.isEmpty();
    const bool hasNormals = !normals.isEmpty();

    m_points.resize(vertexCount);
    m_texCoords.clear();
    if (hasTexCoords)
        m_texCoords.resize(vertexCount);
    m_normals.clear();
    if (hasNormals)
        m_normals.resize(vertexCount);

    for (QHash<FaceIndices, unsigned int>::const_iterator it = faceIndexMap.begin(), endIt = faceIndexMap.end(); it != endIt; ++it) {
        m_points[it.value()] = positions[it.key().positionIndex];
        if (hasTexCoords)
            m_texCoords[it.value()] = std::numeric_limits<unsigned int>::max() != it.key().texCoordIndex ? texCoords[it.key().texCoordIndex] : QVector2D();
        if (hasNormals)
            m_normals[it.value()] = normals[it.key().normalIndex];
    }

    // Now iterate over the face indices and lookup the unique vertex index
    const int indexCount = faceIndexVector.size();
    m_indices.clear();
    m_indices.reserve(indexCount);
    foreach (const FaceIndices &faceIndices, faceIndexVector) {
        const unsigned int i = faceIndexMap.value(faceIndices);
        m_indices.append(i);
    }
}

void ObjLoader::generateAveragedNormals( const QVector<QVector3D>& points,
                                         QVector<QVector3D>& normals,
                                         const QVector<unsigned int>& faces ) const
{
    for ( int i = 0; i < points.size(); ++i )
        normals.append( QVector3D() );

    for ( int i = 0; i < faces.size(); i += 3 )
    {
        const QVector3D& p1 = points[ faces[i]   ];
        const QVector3D& p2 = points[ faces[i+1] ];
        const QVector3D& p3 = points[ faces[i+2] ];

        QVector3D a = p2 - p1;
        QVector3D b = p3 - p1;
        QVector3D n = QVector3D::crossProduct( a, b ).normalized();

        normals[ faces[i]   ] += n;
        normals[ faces[i+1] ] += n;
        normals[ faces[i+2] ] += n;
    }

    for ( int i = 0; i < normals.size(); ++i )
        normals[i].normalize();
}

void ObjLoader::generateTangents( const QVector<QVector3D>& points,
                                  const QVector<QVector3D>& normals,
                                  const QVector<unsigned  int>& faces,
                                  const QVector<QVector2D>& texCoords,
                                  QVector<QVector4D>& tangents ) const
{
    tangents.clear();
    QVector<QVector3D> tan1Accum;
    QVector<QVector3D> tan2Accum;

    for ( int i = 0; i < points.size(); i++ )
    {
        tan1Accum.append( QVector3D() );
        tan2Accum.append( QVector3D() );
        tangents.append( QVector4D() );
    }

    // Compute the tangent vector
    for ( int i = 0; i < faces.size(); i += 3 )
    {
        const QVector3D& p1 = points[ faces[i] ];
        const QVector3D& p2 = points[ faces[i+1] ];
        const QVector3D& p3 = points[ faces[i+2] ];

        const QVector2D& tc1 = texCoords[ faces[i] ];
        const QVector2D& tc2 = texCoords[ faces[i+1] ];
        const QVector2D& tc3 = texCoords[ faces[i+2] ];

        QVector3D q1 = p2 - p1;
        QVector3D q2 = p3 - p1;
        float s1 = tc2.x() - tc1.x(), s2 = tc3.x() - tc1.x();
        float t1 = tc2.y() - tc1.y(), t2 = tc3.y() - tc1.y();
        float r = 1.0f / ( s1 * t2 - s2 * t1 );
        QVector3D tan1( ( t2 * q1.x() - t1 * q2.x() ) * r,
                        ( t2 * q1.y() - t1 * q2.y() ) * r,
                        ( t2 * q1.z() - t1 * q2.z() ) * r );
        QVector3D tan2( ( s1 * q2.x() - s2 * q1.x() ) * r,
                        ( s1 * q2.y() - s2 * q1.y() ) * r,
                        ( s1 * q2.z() - s2 * q1.z() ) * r );
        tan1Accum[ faces[i]   ] += tan1;
        tan1Accum[ faces[i+1] ] += tan1;
        tan1Accum[ faces[i+2] ] += tan1;
        tan2Accum[ faces[i]   ] += tan2;
        tan2Accum[ faces[i+1] ] += tan2;
        tan2Accum[ faces[i+2] ] += tan2;
    }

    for ( int i = 0; i < points.size(); ++i )
    {
        const QVector3D& n = normals[i];
        QVector3D& t1 = tan1Accum[i];
        QVector3D& t2 = tan2Accum[i];

        // Gram-Schmidt orthogonalize
        tangents[i] = QVector4D( QVector3D( t1 - QVector3D::dotProduct( n, t1 ) * n ).normalized(), 0.0f );

        // Store handedness in w
        tangents[i].setW( ( QVector3D::dotProduct( QVector3D::crossProduct( n, t1 ), t2 ) < 0.0f ) ? -1.0f : 1.0f );
    }
}

void ObjLoader::center( QVector<QVector3D>& points )
{
    if ( points.isEmpty() )
        return;

    QAxisAlignedBoundingBox bb(points);
    QVector3D center = bb.center();

    // Translate center of the AABB to the origin
    for ( int i = 0; i < points.size(); ++i )
    {
        QVector3D& point = points[i];
        point = point - center;
    }
}

}

QT_END_NAMESPACE
