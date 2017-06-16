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

#include <QtQml/QQmlEngine>
#include <QtQml/QQmlExtensionPlugin>

#include "qqmlndefrecord.h"
#include "qdeclarativenearfield_p.h"
#include "qdeclarativendeffilter_p.h"
#include "qdeclarativendeftextrecord_p.h"
#include "qdeclarativendefurirecord_p.h"
#include "qdeclarativendefmimerecord_p.h"

QT_USE_NAMESPACE

class QNfcQmlPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")

public:
    void registerTypes(const char *uri)
    {
        Q_ASSERT(uri == QStringLiteral("QtNfc"));

        // @uri QtNfc

        // Register the 5.0 types
        int major = 5;
        int minor = 0;

        qmlRegisterType<QDeclarativeNearField>(uri, major, minor, "NearField");
        qmlRegisterType<QDeclarativeNdefFilter>(uri, major, minor, "NdefFilter");
        qmlRegisterType<QQmlNdefRecord>(uri, major, minor, "NdefRecord");
        qmlRegisterType<QDeclarativeNdefTextRecord>(uri, major, minor, "NdefTextRecord");
        qmlRegisterType<QDeclarativeNdefUriRecord>(uri, major, minor, "NdefUriRecord");
        qmlRegisterType<QDeclarativeNdefMimeRecord>(uri, major, minor, "NdefMimeRecord");

        // Register the 5.2 types
        minor = 2;
        qmlRegisterType<QDeclarativeNearField>(uri, major, minor, "NearField");
        qmlRegisterType<QDeclarativeNdefFilter>(uri, major, minor, "NdefFilter");
        qmlRegisterType<QQmlNdefRecord>(uri, major, minor, "NdefRecord");
        qmlRegisterType<QDeclarativeNdefTextRecord>(uri, major, minor, "NdefTextRecord");
        qmlRegisterType<QDeclarativeNdefUriRecord>(uri, major, minor, "NdefUriRecord");
        qmlRegisterType<QDeclarativeNdefMimeRecord>(uri, major, minor, "NdefMimeRecord");

        // Register the 5.4 types
        // introduces 5.4 version, other existing 5.2 exports become automatically available under 5.2-5.4l
        minor = 4;
        qmlRegisterType<QDeclarativeNearField>(uri, major, minor, "NearField");

        // Register the 5.5 types
        minor = 5;
        qmlRegisterType<QDeclarativeNearField, 1>(uri, major, minor, "NearField");

        // Register the 5.6 types
        minor = 6;
        qmlRegisterType<QDeclarativeNearField, 1>(uri, major, minor, "NearField");

    }
};

#include "plugin.moc"

