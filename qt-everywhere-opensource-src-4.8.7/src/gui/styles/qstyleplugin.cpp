/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qstyleplugin.h"
#include "qstyle.h"

QT_BEGIN_NAMESPACE

/*!
    \class QStylePlugin
    \brief The QStylePlugin class provides an abstract base for custom QStyle plugins.

    \ingroup plugins

    QStylePlugin is a simple plugin interface that makes it easy
    to create custom styles that can be loaded dynamically into
    applications using the QStyleFactory class.

    Writing a style plugin is achieved by subclassing this base class,
    reimplementing the pure virtual keys() and create() functions, and
    exporting the class using the Q_EXPORT_PLUGIN2() macro. See \l
    {How to Create Qt Plugins} for details.

    \sa QStyleFactory, QStyle
*/

/*!
    \fn QStringList QStylePlugin::keys() const

    Returns the list of style keys this plugin supports.

    These keys are usually the class names of the custom styles that
    are implemented in the plugin.

    \sa create()
*/

/*!
    \fn QStyle *QStylePlugin::create(const QString& key)

    Creates and returns a QStyle object for the given style \a key.
    If a plugin cannot create a style, it should return 0 instead.

    The style key is usually the class name of the required
    style. Note that the keys are case insensitive. For example:

    \snippet doc/src/snippets/qstyleplugin/main.cpp 0
    \codeline
    \snippet doc/src/snippets/qstyleplugin/main.cpp 1
    \snippet doc/src/snippets/qstyleplugin/main.cpp 2

    \sa keys()
*/

/*!
    Constructs a style plugin with the given \a parent.

    Note that this constructor is invoked automatically by the
    Q_EXPORT_PLUGIN2() macro, so there is no need for calling it
    explicitly.
*/
QStylePlugin::QStylePlugin(QObject *parent)
    : QObject(parent)
{
}

/*!
    Destroys the style plugin.

    Note that Qt destroys a plugin automatically when it is no longer
    used, so there is no need for calling the destructor explicitly.
*/
QStylePlugin::~QStylePlugin()
{
}

QT_END_NAMESPACE
