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

#include "qplatformdefs.h"

#include "qbytearray.h"
#include "qstring.h"

#include "string.h"

QT_BEGIN_NAMESPACE

#ifndef QT_VSNPRINTF

/*!
    \relates QByteArray

    A portable \c vsnprintf() function. Will call \c ::vsnprintf(), \c
    ::_vsnprintf(), or \c ::vsnprintf_s depending on the system, or
    fall back to an internal version.

    \a fmt is the \c printf() format string. The result is put into
    \a str, which is a buffer of at least \a n bytes.

    The caller is responsible to call \c va_end() on \a ap.

    \warning Since vsnprintf() shows different behavior on certain
    platforms, you should not rely on the return value or on the fact
    that you will always get a 0 terminated string back.

    Ideally, you should never call this function but use QString::asprintf()
    instead.

    \sa qsnprintf(), QString::asprintf()
*/

int qvsnprintf(char *str, size_t n, const char *fmt, va_list ap)
{
    if (!str || !fmt)
        return -1;

    const QByteArray ba = QString::vasprintf(fmt, ap).toLocal8Bit();

    if (n > 0) {
        size_t blen = qMin(size_t(ba.length()), size_t(n - 1));
        memcpy(str, ba.constData(), blen);
        str[blen] = '\0'; // make sure str is always 0 terminated
    }

    return ba.length();
}

#else

QT_BEGIN_INCLUDE_NAMESPACE
#include <stdio.h>
QT_END_INCLUDE_NAMESPACE

int qvsnprintf(char *str, size_t n, const char *fmt, va_list ap)
{
    return QT_VSNPRINTF(str, n, fmt, ap);
}

#endif

/*!
    \target bytearray-qsnprintf
    \relates QByteArray

    A portable snprintf() function, calls qvsnprintf.

    \a fmt is the \c printf() format string. The result is put into
    \a str, which is a buffer of at least \a n bytes.

    \warning Call this function only when you know what you are doing
    since it shows different behavior on certain platforms.
    Use QString::asprintf() to format a string instead.

    \sa qvsnprintf(), QString::asprintf()
*/

int qsnprintf(char *str, size_t n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    int ret = qvsnprintf(str, n, fmt, ap);
    va_end(ap);

    return ret;
}

QT_END_NAMESPACE
