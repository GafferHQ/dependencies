/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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
#ifndef COMUTILS_H
#define COMUTILS_H

#if !defined(_WINDOWS_) && !defined(_WINDOWS_H) && !defined(__WINDOWS__)
#error Must include windows.h first!
#endif

#include <ocidl.h>
#include <QtCore/qstring.h>

QT_BEGIN_NAMESPACE

class QVariant;

// Originally QVariantToVARIANT copied from ActiveQt - renamed to avoid conflicts in static builds.
bool QVariant2VARIANT(const QVariant &var, VARIANT &arg, const QByteArray &typeName, bool out);

inline BSTR QStringToBSTR(const QString &str)
{
    return SysAllocStringLen(reinterpret_cast<const OLECHAR *>(str.unicode()), UINT(str.length()));
}

QT_END_NAMESPACE

#endif // COMUTILS_H

