/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDBus module of the Qt Toolkit.
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

#include "qdbusvirtualobject.h"

#ifndef QT_NO_DBUS

QT_BEGIN_NAMESPACE

QDBusVirtualObject::QDBusVirtualObject(QObject *parent) :
    QObject(parent)
{
}

QDBusVirtualObject::~QDBusVirtualObject()
{
}

QT_END_NAMESPACE


/*!
    \internal
    \class QDBusVirtualObject
    \inmodule QtDBus
    \since 4.8

    \brief The QDBusVirtualObject class is used to handle several DBus paths with one class.
*/

/*!
    \internal
    \fn bool QDBusVirtualObject::handleMessage(const QDBusMessage &message, const QDBusConnection &connection) = 0

    This function needs to handle all messages to the path of the
    virtual object, when the SubPath option is specified.
    The service, path, interface and methos are all part of the message.
    Must return true when the message is handled, otherwise false (will generate dbus error message).
*/


/*!
    \internal
    \fn QString QDBusVirtualObject::introspect(const QString &path) const

    This function needs to handle the introspection of the
    virtual object. It must return xml of the form:

    \code
<interface name="org.qtproject.QtDBus.MyObject" >
    <property access="readwrite" type="i" name="prop1" />
</interface>
    \endcode

    If you pass the SubPath option, this introspection has to include all child nodes.
    Otherwise QDBus handles the introspection of the child nodes.
*/

#endif // QT_NO_DBUS
