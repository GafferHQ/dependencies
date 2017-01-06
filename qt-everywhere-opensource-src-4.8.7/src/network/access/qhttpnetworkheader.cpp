/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
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

#include "qhttpnetworkheader_p.h"

#ifndef QT_NO_HTTP

QT_BEGIN_NAMESPACE

QHttpNetworkHeaderPrivate::QHttpNetworkHeaderPrivate(const QUrl &newUrl)
    :url(newUrl)
{
}

QHttpNetworkHeaderPrivate::QHttpNetworkHeaderPrivate(const QHttpNetworkHeaderPrivate &other)
    :QSharedData(other)
{
    url = other.url;
    fields = other.fields;
}

qint64 QHttpNetworkHeaderPrivate::contentLength() const
{
    bool ok = false;
    // We are not using the headerField() method here because servers might send us multiple content-length
    // headers which is crap (see QTBUG-15311). Therefore just take the first content-length header field.
    QByteArray value;
    QList<QPair<QByteArray, QByteArray> >::ConstIterator it = fields.constBegin(),
                                                        end = fields.constEnd();
    for ( ; it != end; ++it)
        if (qstricmp("content-length", it->first) == 0) {
            value = it->second;
            break;
        }

    qint64 length = value.toULongLong(&ok);
    if (ok)
        return length;
    return -1; // the header field is not set
}

void QHttpNetworkHeaderPrivate::setContentLength(qint64 length)
{
    setHeaderField("Content-Length", QByteArray::number(length));
}

QByteArray QHttpNetworkHeaderPrivate::headerField(const QByteArray &name, const QByteArray &defaultValue) const
{
    QList<QByteArray> allValues = headerFieldValues(name);
    if (allValues.isEmpty())
        return defaultValue;

    QByteArray result;
    bool first = true;
    foreach (const QByteArray &value, allValues) {
        if (!first)
            result += ", ";
        first = false;
        result += value;
    }
    return result;
}

QList<QByteArray> QHttpNetworkHeaderPrivate::headerFieldValues(const QByteArray &name) const
{
    QList<QByteArray> result;
    QList<QPair<QByteArray, QByteArray> >::ConstIterator it = fields.constBegin(),
                                                        end = fields.constEnd();
    for ( ; it != end; ++it)
        if (qstricmp(name.constData(), it->first) == 0)
            result += it->second;

    return result;
}

void QHttpNetworkHeaderPrivate::setHeaderField(const QByteArray &name, const QByteArray &data)
{
    QList<QPair<QByteArray, QByteArray> >::Iterator it = fields.begin();
    while (it != fields.end()) {
        if (qstricmp(name.constData(), it->first) == 0)
            it = fields.erase(it);
        else
            ++it;
    }
    fields.append(qMakePair(name, data));
}

bool QHttpNetworkHeaderPrivate::operator==(const QHttpNetworkHeaderPrivate &other) const
{
   return (url == other.url);
}


QT_END_NAMESPACE

#endif
