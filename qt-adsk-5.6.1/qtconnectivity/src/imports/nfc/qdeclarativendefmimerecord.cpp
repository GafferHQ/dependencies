/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include "qdeclarativendefmimerecord_p.h"

/*!
    \qmltype NdefMimeRecord
    \since 5.2
    \brief Represents an NFC MIME record.

    \ingroup connectivity-nfc
    \inqmlmodule QtNfc

    \inherits NdefRecord

    The NdefMimeRecord type can contain data with an associated MIME type.  The data is
    accessible from the uri in the \l {NdefMimeRecord::uri}{uri} property.
*/

/*!
    \qmlproperty string NdefMimeRecord::uri

    This property hold the URI from which the MIME data can be fetched from.  Currently this
    property returns a data url.
*/

Q_DECLARE_NDEFRECORD(QDeclarativeNdefMimeRecord, QNdefRecord::Mime, ".*")

static inline QNdefRecord createMimeRecord()
{
    QNdefRecord mimeRecord;
    mimeRecord.setTypeNameFormat(QNdefRecord::Mime);
    return mimeRecord;
}

static inline QNdefRecord castToMimeRecord(const QNdefRecord &record)
{
    if (record.typeNameFormat() != QNdefRecord::Mime)
        return createMimeRecord();
    return record;
}

QDeclarativeNdefMimeRecord::QDeclarativeNdefMimeRecord(QObject *parent)
:   QQmlNdefRecord(createMimeRecord(), parent)
{
}

QDeclarativeNdefMimeRecord::QDeclarativeNdefMimeRecord(const QNdefRecord &record, QObject *parent)
:   QQmlNdefRecord(castToMimeRecord(record), parent)
{
}

QDeclarativeNdefMimeRecord::~QDeclarativeNdefMimeRecord()
{
}

QString QDeclarativeNdefMimeRecord::uri() const
{
    QByteArray dataUri = "data:" + record().type() + ";base64," + record().payload().toBase64();
    return QString::fromLatin1(dataUri.constData(), dataUri.length());
}
