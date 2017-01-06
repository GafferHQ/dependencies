/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include "remoteconnection.h"

QByteArray strwinerror(DWORD errorcode)
{
    QByteArray out(512, 0);

    DWORD ok = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM,
        0,
        errorcode,
        0,
        out.data(),
        out.size(),
        0
    );

    if (!ok) {
        qsnprintf(out.data(), out.size(),
            "(error %d; additionally, error %d while looking up error string)",
            (int)errorcode, (int)GetLastError());
    }
    else {
        out.resize(qstrlen(out.constData()));
        if (out.endsWith("\r\n"))
            out.chop(2);

        /* Append error number to error message for good measure */
        out.append(" (0x");
        out.append(QByteArray::number(uint(errorcode), 16).rightJustified(8, '0'));
        out.append(")");
    }
    return out;
}

AbstractRemoteConnection::AbstractRemoteConnection()
{
}

AbstractRemoteConnection::~AbstractRemoteConnection()
{
}


// Slow but should be ok...
bool AbstractRemoteConnection::moveFile(const QString &src, const QString &dest, bool FailIfExists)
{
    bool result = copyFile(src, dest, FailIfExists);
    deleteFile(src);
    return result;
}

// Slow but should be ok...
bool AbstractRemoteConnection::moveDirectory(const QString &src, const QString &dest, bool recursive)
{
    bool result = copyDirectory(src, dest, true);
    deleteDirectory(src, recursive);
    return result;
}

