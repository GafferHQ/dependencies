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

#include "qpictureformatplugin.h"
#if !defined(QT_NO_LIBRARY) && !defined(QT_NO_PICTURE)
#include "qpicture.h"

QT_BEGIN_NAMESPACE

/*!
    \obsolete

    \class QPictureFormatPlugin
    \brief The QPictureFormatPlugin class provides an abstract base
    for custom picture format plugins.

    \ingroup plugins
    \inmodule QtGui

    The picture format plugin is a simple plugin interface that makes
    it easy to create custom picture formats that can be used
    transparently by applications.

    Writing an picture format plugin is achieved by subclassing this
    base class, reimplementing the pure virtual functions
    loadPicture(), savePicture(), and installIOHandler(), and
    exporting the class with the Q_PLUGIN_METADATA() macro.

    The json file containing the metadata should contain one entry
    with the list of picture formats supported by the plugin:

    \code
    { "Keys": [ "mypictureformat" ] }
    \endcode

    \sa {How to Create Qt Plugins}
*/

/*!
    \fn bool QPictureFormatPlugin::installIOHandler(const QString &format)

    Installs a QPictureIO picture I/O handler for the picture format \a
    format. Returns \c true on success.
*/


/*!
    Constructs an picture format plugin with the given \a parent.
    This is invoked automatically by the moc generated code that exports the plugin.
*/
QPictureFormatPlugin::QPictureFormatPlugin(QObject *parent)
    : QObject(parent)
{
}

/*!
    Destroys the picture format plugin.

    You never have to call this explicitly. Qt destroys a plugin
    automatically when it is no longer used.
*/
QPictureFormatPlugin::~QPictureFormatPlugin()
{
}


/*!
    Loads the picture stored in the file called \a fileName, with the
    given \a format, into *\a picture. Returns \c true on success;
    otherwise returns \c false.

    \sa savePicture()
*/
bool QPictureFormatPlugin::loadPicture(const QString &format, const QString &fileName, QPicture *picture)
{
    Q_UNUSED(format)
    Q_UNUSED(fileName)
    Q_UNUSED(picture)
    return false;
}

/*!
    Saves the given \a picture into the file called \a fileName,
    using the specified \a format. Returns \c true on success; otherwise
    returns \c false.

    \sa loadPicture()
*/
bool QPictureFormatPlugin::savePicture(const QString &format, const QString &fileName, const QPicture &picture)
{
    Q_UNUSED(format)
    Q_UNUSED(fileName)
    Q_UNUSED(picture)
    return false;
}

#endif // QT_NO_LIBRARY || QT_NO_PICTURE

QT_END_NAMESPACE
