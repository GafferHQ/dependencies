/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#include "qstringbuilder.h"
#include <QtCore/qtextcodec.h>

QT_BEGIN_NAMESPACE

/*!
    \class QStringBuilder
    \inmodule QtCore
    \internal
    \reentrant
    \since 4.6

    \brief The QStringBuilder class is a template class that provides a facility to build up QStrings from smaller chunks.

    \ingroup tools
    \ingroup shared
    \ingroup string-processing


    To build a QString by multiple concatenations, QString::operator+()
    is typically used. This causes \e{n - 1} reallocations when building
    a string from \e{n} chunks.

    QStringBuilder uses expression templates to collect the individual
    chunks, compute the total size, allocate the required amount of
    memory for the final QString object, and copy the chunks into the
    allocated memory.

    The QStringBuilder class is not to be used explicitly in user
    code.  Instances of the class are created as return values of the
    operator%() function, acting on objects of type QString,
    QLatin1String, QStringRef, QChar, QCharRef,
    QLatin1Char, and \c char.

    Concatenating strings with operator%() generally yields better
    performance then using \c QString::operator+() on the same chunks
    if there are three or more of them, and performs equally well in other
    cases.

    \sa QLatin1String, QString
*/

/*! \fn QStringBuilder::QStringBuilder(const A &a, const B &b)
  Constructs a QStringBuilder from \a a and \a b.
 */

/* \fn QStringBuilder::operator%(const A &a, const B &b)

    Returns a \c QStringBuilder object that is converted to a QString object
    when assigned to a variable of QString type or passed to a function that
    takes a QString parameter.

    This function is usable with arguments of type \c QString,
    \c QLatin1String, \c QStringRef,
    \c QChar, \c QCharRef, \c QLatin1Char, and \c char.
*/

/* \fn QByteArray QStringBuilder::toLatin1() const
  Returns a Latin-1 representation of the string as a QByteArray.  The
  returned byte array is undefined if the string contains non-Latin1
  characters.
 */
/* \fn QByteArray QStringBuilder::toUtf8() const
  Returns a UTF-8 representation of the string as a QByteArray.
 */


/*!
    \internal
 */
void QAbstractConcatenable::convertFromAscii(const char *a, int len, QChar *&out)
{
    if (len == -1) {
        if (!a)
            return;
        while (*a && uchar(*a) < 0x80U)
            *out++ = QLatin1Char(*a++);
        if (!*a)
            return;
    } else {
        int i;
        for (i = 0; i < len && uchar(a[i]) < 0x80U; ++i)
            *out++ = QLatin1Char(a[i]);
        if (i == len)
            return;
        a += i;
        len -= i;
    }

    // we need to complement with UTF-8 appending
    QString tmp = QString::fromUtf8(a, len);
    memcpy(out, reinterpret_cast<const char *>(tmp.constData()), sizeof(QChar) * tmp.size());
    out += tmp.size();
}

QT_END_NAMESPACE
