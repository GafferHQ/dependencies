/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qgenericplugin.h"

#ifndef QT_NO_LIBRARY

QT_BEGIN_NAMESPACE

/*!
    \class QGenericPlugin
    \ingroup plugins
    \inmodule QtGui

    \brief The QGenericPlugin class is an abstract base class for
    plugins.

    A mouse plugin can be created by subclassing
    QGenericPlugin and reimplementing the pure virtual create()
    function. By exporting the derived class using the
    Q_PLUGIN_METADATA() macro, The default implementation of the
    QGenericPluginFactory class will automatically detect the plugin and
    load the driver into the server application at run-time. See \l
    {How to Create Qt Plugins} for details.

    The json metadata file should contain a list of keys supported by this
    plugin.

    \sa QGenericPluginFactory
*/

/*!
    Constructs a plugin with the given \a parent.

    Note that this constructor is invoked automatically by the
    moc generated code that exports the plugin, so there is no need for calling it
    explicitly.
*/
QGenericPlugin::QGenericPlugin(QObject *parent)
    : QObject(parent)
{
}

/*!
    Destroys the plugin.

    Note that Qt destroys a plugin automatically when it is no longer
    used, so there is no need for calling the destructor explicitly.
*/
QGenericPlugin::~QGenericPlugin()
{
}

/*!
    \fn QObject* QGenericPlugin::create(const QString &key, const QString& specification)

    Implement this function to create a driver matching the type
    specified by the given \a key and \a specification parameters. Note that
    keys are case-insensitive.
*/

QT_END_NAMESPACE

#endif // QT_NO_LIBRARY
