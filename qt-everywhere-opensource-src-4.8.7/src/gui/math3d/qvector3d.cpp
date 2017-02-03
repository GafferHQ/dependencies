/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qvector3d.h"
#include "qvector2d.h"
#include "qvector4d.h"
#include <QtCore/qmath.h>
#include <QtCore/qvariant.h>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_VECTOR3D

/*!
    \class QVector3D
    \brief The QVector3D class represents a vector or vertex in 3D space.
    \since 4.6
    \ingroup painting-3D

    Vectors are one of the main building blocks of 3D representation and
    drawing.  They consist of three coordinates, traditionally called
    x, y, and z.

    The QVector3D class can also be used to represent vertices in 3D space.
    We therefore do not need to provide a separate vertex class.

    \bold{Note:} By design values in the QVector3D instance are stored as \c float.
    This means that on platforms where the \c qreal arguments to QVector3D
    functions are represented by \c double values, it is possible to
    lose precision.

    \sa QVector2D, QVector4D, QQuaternion
*/

/*!
    \fn QVector3D::QVector3D()

    Constructs a null vector, i.e. with coordinates (0, 0, 0).
*/

/*!
    \fn QVector3D::QVector3D(qreal xpos, qreal ypos, qreal zpos)

    Constructs a vector with coordinates (\a xpos, \a ypos, \a zpos).
*/

/*!
    \fn QVector3D::QVector3D(const QPoint& point)

    Constructs a vector with x and y coordinates from a 2D \a point, and a
    z coordinate of 0.
*/

/*!
    \fn QVector3D::QVector3D(const QPointF& point)

    Constructs a vector with x and y coordinates from a 2D \a point, and a
    z coordinate of 0.
*/

#ifndef QT_NO_VECTOR2D

/*!
    Constructs a 3D vector from the specified 2D \a vector.  The z
    coordinate is set to zero.

    \sa toVector2D()
*/
QVector3D::QVector3D(const QVector2D& vector)
{
    xp = vector.xp;
    yp = vector.yp;
    zp = 0.0f;
}

/*!
    Constructs a 3D vector from the specified 2D \a vector.  The z
    coordinate is set to \a zpos.

    \sa toVector2D()
*/
QVector3D::QVector3D(const QVector2D& vector, qreal zpos)
{
    xp = vector.xp;
    yp = vector.yp;
    zp = zpos;
}

#endif

#ifndef QT_NO_VECTOR4D

/*!
    Constructs a 3D vector from the specified 4D \a vector.  The w
    coordinate is dropped.

    \sa toVector4D()
*/
QVector3D::QVector3D(const QVector4D& vector)
{
    xp = vector.xp;
    yp = vector.yp;
    zp = vector.zp;
}

#endif

/*!
    \fn bool QVector3D::isNull() const

    Returns true if the x, y, and z coordinates are set to 0.0,
    otherwise returns false.
*/

/*!
    \fn qreal QVector3D::x() const

    Returns the x coordinate of this point.

    \sa setX(), y(), z()
*/

/*!
    \fn qreal QVector3D::y() const

    Returns the y coordinate of this point.

    \sa setY(), x(), z()
*/

/*!
    \fn qreal QVector3D::z() const

    Returns the z coordinate of this point.

    \sa setZ(), x(), y()
*/

/*!
    \fn void QVector3D::setX(qreal x)

    Sets the x coordinate of this point to the given \a x coordinate.

    \sa x(), setY(), setZ()
*/

/*!
    \fn void QVector3D::setY(qreal y)

    Sets the y coordinate of this point to the given \a y coordinate.

    \sa y(), setX(), setZ()
*/

/*!
    \fn void QVector3D::setZ(qreal z)

    Sets the z coordinate of this point to the given \a z coordinate.

    \sa z(), setX(), setY()
*/

/*!
    Returns the normalized unit vector form of this vector.

    If this vector is null, then a null vector is returned.  If the length
    of the vector is very close to 1, then the vector will be returned as-is.
    Otherwise the normalized form of the vector of length 1 will be returned.

    \sa length(), normalize()
*/
QVector3D QVector3D::normalized() const
{
    // Need some extra precision if the length is very small.
    double len = double(xp) * double(xp) +
                 double(yp) * double(yp) +
                 double(zp) * double(zp);
    if (qFuzzyIsNull(len - 1.0f))
        return *this;
    else if (!qFuzzyIsNull(len))
        return *this / qSqrt(len);
    else
        return QVector3D();
}

/*!
    Normalizes the currect vector in place.  Nothing happens if this
    vector is a null vector or the length of the vector is very close to 1.

    \sa length(), normalized()
*/
void QVector3D::normalize()
{
    // Need some extra precision if the length is very small.
    double len = double(xp) * double(xp) +
                 double(yp) * double(yp) +
                 double(zp) * double(zp);
    if (qFuzzyIsNull(len - 1.0f) || qFuzzyIsNull(len))
        return;

    len = qSqrt(len);

    xp /= len;
    yp /= len;
    zp /= len;
}

/*!
    \fn QVector3D &QVector3D::operator+=(const QVector3D &vector)

    Adds the given \a vector to this vector and returns a reference to
    this vector.

    \sa operator-=()
*/

/*!
    \fn QVector3D &QVector3D::operator-=(const QVector3D &vector)

    Subtracts the given \a vector from this vector and returns a reference to
    this vector.

    \sa operator+=()
*/

/*!
    \fn QVector3D &QVector3D::operator*=(qreal factor)

    Multiplies this vector's coordinates by the given \a factor, and
    returns a reference to this vector.

    \sa operator/=()
*/

/*!
    \fn QVector3D &QVector3D::operator*=(const QVector3D& vector)
    \overload

    Multiplies the components of this vector by the corresponding
    components in \a vector.

    Note: this is not the same as the crossProduct() of this
    vector and \a vector.

    \sa crossProduct()
*/

/*!
    \fn QVector3D &QVector3D::operator/=(qreal divisor)

    Divides this vector's coordinates by the given \a divisor, and
    returns a reference to this vector.

    \sa operator*=()
*/

/*!
    Returns the dot product of \a v1 and \a v2.
*/
qreal QVector3D::dotProduct(const QVector3D& v1, const QVector3D& v2)
{
    return v1.xp * v2.xp + v1.yp * v2.yp + v1.zp * v2.zp;
}

/*!
    Returns the cross-product of vectors \a v1 and \a v2, which corresponds
    to the normal vector of a plane defined by \a v1 and \a v2.

    \sa normal()
*/
QVector3D QVector3D::crossProduct(const QVector3D& v1, const QVector3D& v2)
{
    return QVector3D(v1.yp * v2.zp - v1.zp * v2.yp,
                    v1.zp * v2.xp - v1.xp * v2.zp,
                    v1.xp * v2.yp - v1.yp * v2.xp, 1);
}

/*!
    Returns the normal vector of a plane defined by vectors \a v1 and \a v2,
    normalized to be a unit vector.

    Use crossProduct() to compute the cross-product of \a v1 and \a v2 if you
    do not need the result to be normalized to a unit vector.

    \sa crossProduct(), distanceToPlane()
*/
QVector3D QVector3D::normal(const QVector3D& v1, const QVector3D& v2)
{
    return crossProduct(v1, v2).normalized();
}

/*!
    \overload

    Returns the normal vector of a plane defined by vectors
    \a v2 - \a v1 and \a v3 - \a v1, normalized to be a unit vector.

    Use crossProduct() to compute the cross-product of \a v2 - \a v1 and
    \a v3 - \a v1 if you do not need the result to be normalized to a
    unit vector.

    \sa crossProduct(), distanceToPlane()
*/
QVector3D QVector3D::normal
        (const QVector3D& v1, const QVector3D& v2, const QVector3D& v3)
{
    return crossProduct((v2 - v1), (v3 - v1)).normalized();
}

/*!
    Returns the distance from this vertex to a plane defined by
    the vertex \a plane and a \a normal unit vector.  The \a normal
    parameter is assumed to have been normalized to a unit vector.

    The return value will be negative if the vertex is below the plane,
    or zero if it is on the plane.

    \sa normal(), distanceToLine()
*/
qreal QVector3D::distanceToPlane
        (const QVector3D& plane, const QVector3D& normal) const
{
    return dotProduct(*this - plane, normal);
}

/*!
    \overload

    Returns the distance from this vertex a plane defined by
    the vertices \a plane1, \a plane2 and \a plane3.

    The return value will be negative if the vertex is below the plane,
    or zero if it is on the plane.

    The two vectors that define the plane are \a plane2 - \a plane1
    and \a plane3 - \a plane1.

    \sa normal(), distanceToLine()
*/
qreal QVector3D::distanceToPlane
    (const QVector3D& plane1, const QVector3D& plane2, const QVector3D& plane3) const
{
    QVector3D n = normal(plane2 - plane1, plane3 - plane1);
    return dotProduct(*this - plane1, n);
}

/*!
    Returns the distance that this vertex is from a line defined
    by \a point and the unit vector \a direction.

    If \a direction is a null vector, then it does not define a line.
    In that case, the distance from \a point to this vertex is returned.

    \sa distanceToPlane()
*/
qreal QVector3D::distanceToLine
        (const QVector3D& point, const QVector3D& direction) const
{
    if (direction.isNull())
        return (*this - point).length();
    QVector3D p = point + dotProduct(*this - point, direction) * direction;
    return (*this - p).length();
}

/*!
    \fn bool operator==(const QVector3D &v1, const QVector3D &v2)
    \relates QVector3D

    Returns true if \a v1 is equal to \a v2; otherwise returns false.
    This operator uses an exact floating-point comparison.
*/

/*!
    \fn bool operator!=(const QVector3D &v1, const QVector3D &v2)
    \relates QVector3D

    Returns true if \a v1 is not equal to \a v2; otherwise returns false.
    This operator uses an exact floating-point comparison.
*/

/*!
    \fn const QVector3D operator+(const QVector3D &v1, const QVector3D &v2)
    \relates QVector3D

    Returns a QVector3D object that is the sum of the given vectors, \a v1
    and \a v2; each component is added separately.

    \sa QVector3D::operator+=()
*/

/*!
    \fn const QVector3D operator-(const QVector3D &v1, const QVector3D &v2)
    \relates QVector3D

    Returns a QVector3D object that is formed by subtracting \a v2 from \a v1;
    each component is subtracted separately.

    \sa QVector3D::operator-=()
*/

/*!
    \fn const QVector3D operator*(qreal factor, const QVector3D &vector)
    \relates QVector3D

    Returns a copy of the given \a vector,  multiplied by the given \a factor.

    \sa QVector3D::operator*=()
*/

/*!
    \fn const QVector3D operator*(const QVector3D &vector, qreal factor)
    \relates QVector3D

    Returns a copy of the given \a vector,  multiplied by the given \a factor.

    \sa QVector3D::operator*=()
*/

/*!
    \fn const QVector3D operator*(const QVector3D &v1, const QVector3D& v2)
    \relates QVector3D

    Multiplies the components of \a v1 by the corresponding components in \a v2.

    Note: this is not the same as the crossProduct() of \a v1 and \a v2.

    \sa QVector3D::crossProduct()
*/

/*!
    \fn const QVector3D operator-(const QVector3D &vector)
    \relates QVector3D
    \overload

    Returns a QVector3D object that is formed by changing the sign of
    all three components of the given \a vector.

    Equivalent to \c {QVector3D(0,0,0) - vector}.
*/

/*!
    \fn const QVector3D operator/(const QVector3D &vector, qreal divisor)
    \relates QVector3D

    Returns the QVector3D object formed by dividing all three components of
    the given \a vector by the given \a divisor.

    \sa QVector3D::operator/=()
*/

/*!
    \fn bool qFuzzyCompare(const QVector3D& v1, const QVector3D& v2)
    \relates QVector3D

    Returns true if \a v1 and \a v2 are equal, allowing for a small
    fuzziness factor for floating-point comparisons; false otherwise.
*/

#ifndef QT_NO_VECTOR2D

/*!
    Returns the 2D vector form of this 3D vector, dropping the z coordinate.

    \sa toVector4D(), toPoint()
*/
QVector2D QVector3D::toVector2D() const
{
    return QVector2D(xp, yp, 1);
}

#endif

#ifndef QT_NO_VECTOR4D

/*!
    Returns the 4D form of this 3D vector, with the w coordinate set to zero.

    \sa toVector2D(), toPoint()
*/
QVector4D QVector3D::toVector4D() const
{
    return QVector4D(xp, yp, zp, 0.0f, 1);
}

#endif

/*!
    \fn QPoint QVector3D::toPoint() const

    Returns the QPoint form of this 3D vector. The z coordinate
    is dropped.

    \sa toPointF(), toVector2D()
*/

/*!
    \fn QPointF QVector3D::toPointF() const

    Returns the QPointF form of this 3D vector. The z coordinate
    is dropped.

    \sa toPoint(), toVector2D()
*/

/*!
    Returns the 3D vector as a QVariant.
*/
QVector3D::operator QVariant() const
{
    return QVariant(QVariant::Vector3D, this);
}

/*!
    Returns the length of the vector from the origin.

    \sa lengthSquared(), normalized()
*/
qreal QVector3D::length() const
{
    return qSqrt(xp * xp + yp * yp + zp * zp);
}

/*!
    Returns the squared length of the vector from the origin.
    This is equivalent to the dot product of the vector with itself.

    \sa length(), dotProduct()
*/
qreal QVector3D::lengthSquared() const
{
    return xp * xp + yp * yp + zp * zp;
}

#ifndef QT_NO_DEBUG_STREAM

QDebug operator<<(QDebug dbg, const QVector3D &vector)
{
    dbg.nospace() << "QVector3D("
        << vector.x() << ", " << vector.y() << ", " << vector.z() << ')';
    return dbg.space();
}

#endif

#ifndef QT_NO_DATASTREAM

/*!
    \fn QDataStream &operator<<(QDataStream &stream, const QVector3D &vector)
    \relates QVector3D

    Writes the given \a vector to the given \a stream and returns a
    reference to the stream.

    \sa {Serializing Qt Data Types}
*/

QDataStream &operator<<(QDataStream &stream, const QVector3D &vector)
{
    stream << double(vector.x()) << double(vector.y())
           << double(vector.z());
    return stream;
}

/*!
    \fn QDataStream &operator>>(QDataStream &stream, QVector3D &vector)
    \relates QVector3D

    Reads a 3D vector from the given \a stream into the given \a vector
    and returns a reference to the stream.

    \sa {Serializing Qt Data Types}
*/

QDataStream &operator>>(QDataStream &stream, QVector3D &vector)
{
    double x, y, z;
    stream >> x;
    stream >> y;
    stream >> z;
    vector.setX(qreal(x));
    vector.setY(qreal(y));
    vector.setZ(qreal(z));
    return stream;
}

#endif // QT_NO_DATASTREAM

#endif // QT_NO_VECTOR3D

QT_END_NAMESPACE
