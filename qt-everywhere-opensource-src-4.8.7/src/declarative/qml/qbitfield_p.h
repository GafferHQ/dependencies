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

#ifndef QBITFIELD_P_H
#define QBITFIELD_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE

class QBitField
{
public:
    inline QBitField();
    inline QBitField(const quint32 *, int bits);
    inline QBitField(const QBitField &);
    inline ~QBitField();

    inline QBitField &operator=(const QBitField &);

    inline quint32 size() const;
    inline QBitField united(const QBitField &);
    inline bool testBit(int) const;

private:
    quint32 bits:31;
    quint32 *ownData;
    const quint32 *data;
};

QBitField::QBitField()
: bits(0), ownData(0), data(0)
{
}

QBitField::QBitField(const quint32 *bitData, int bitCount)
: bits((quint32)bitCount), ownData(0), data(bitData)
{
}

QBitField::QBitField(const QBitField &other)
: bits(other.bits), ownData(other.ownData), data(other.data)
{
    if (ownData) 
        ++(*ownData);
}

QBitField::~QBitField()
{
    if (ownData) 
        if(0 == --(*ownData)) delete [] ownData;
}

QBitField &QBitField::operator=(const QBitField &other)
{
    if (other.data == data)
        return *this;

    if (ownData) 
        if(0 == --(*ownData)) delete [] ownData;

    bits = other.bits;
    ownData = other.ownData;
    data = other.data;

    if (ownData) 
        ++(*ownData);

    return *this;
}

inline quint32 QBitField::size() const
{
    return bits;
}

QBitField QBitField::united(const QBitField &o)
{
    if (o.bits == 0) {
        return *this;
    } else if (bits == 0) {
        return o;
    } else {
        int max = (bits > o.bits)?bits:o.bits;
        int length = (max + 31) / 32;
        QBitField rv;
        rv.bits = max;
        rv.ownData = new quint32[length + 1];
        *(rv.ownData) = 1;
        rv.data = rv.ownData + 1;
        if (bits > o.bits) {
            ::memcpy((quint32 *)rv.data, data, length * sizeof(quint32));
            for (quint32 ii = 0; ii < (o.bits + quint32(31)) / 32; ++ii)
                ((quint32 *)rv.data)[ii] |= o.data[ii];
        } else {
            ::memcpy((quint32 *)rv.data, o.data, length * sizeof(quint32));
            for (quint32 ii = 0; ii < (bits + quint32(31)) / 32; ++ii)
                ((quint32 *)rv.data)[ii] |= data[ii];
        }
        return rv;
    }
}

bool QBitField::testBit(int b) const
{
    Q_ASSERT(b >= 0);
    if ((quint32)b < bits) {
        return data[b / 32] & (1 << (b % 32));
    } else {
        return false;
    }
}

QT_END_NAMESPACE

#endif // QBITFIELD_P_H
