/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include <QtDesigner/extension.h>

QT_BEGIN_NAMESPACE

/*!
    \class QAbstractExtensionFactory

    \brief The QAbstractExtensionFactory class provides an interface
    for extension factories in Qt Designer.

    \inmodule QtDesigner

    QAbstractExtensionFactory is not intended to be instantiated
    directly; use the QExtensionFactory instead.

    In \QD, extension factories are used to look up and create named
    extensions as they are required. For that reason, when
    implementing a custom extension, you must also create a
    QExtensionFactory, i.e a class that is able to make an instance of
    your extension, and register it using \QD's \l
    {QExtensionManager}{extension manager}.

    When an extension is required, \QD's \l
    {QExtensionManager}{extension manager} will run through all its
    registered factories calling QExtensionFactory::createExtension()
    for each until the first one that is able to create the requested
    extension for the selected object, is found. This factory will
    then make an instance of the extension.

    \sa QExtensionFactory, QExtensionManager
*/

/*!
    \fn QAbstractExtensionFactory::~QAbstractExtensionFactory()

    Destroys the extension factory.
*/

/*!
    \fn QObject *QAbstractExtensionFactory::extension(QObject *object, const QString &iid) const

    Returns the extension specified by \a iid for the given \a object.
*/


/*!
    \class QAbstractExtensionManager

    \brief The QAbstractExtensionManager class provides an interface
    for extension managers in Qt Designer.

    \inmodule QtDesigner

    QAbstractExtensionManager is not intended to be instantiated
    directly; use the QExtensionManager instead.

    In \QD, extension are not created until they are required. For
    that reason, when implementing a custom extension, you must also
    create a QExtensionFactory, i.e a class that is able to make an
    instance of your extension, and register it using \QD's \l
    {QExtensionManager}{extension manager}.

    When an extension is required, \QD's \l
    {QExtensionManager}{extension manager} will run through all its
    registered factories calling QExtensionFactory::createExtension()
    for each until the first one that is able to create the requested
    extension for the selected object, is found. This factory will
    then make an instance of the extension.

    \sa QExtensionManager, QExtensionFactory
*/

/*!
    \fn QAbstractExtensionManager::~QAbstractExtensionManager()

    Destroys the extension manager.
*/

/*!
    \fn void QAbstractExtensionManager::registerExtensions(QAbstractExtensionFactory *factory, const QString &iid)

    Register the given extension \a factory with the extension
    specified by \a iid.
*/

/*!
    \fn void QAbstractExtensionManager::unregisterExtensions(QAbstractExtensionFactory *factory, const QString &iid)

    Unregister the given \a factory with the extension specified by \a
    iid.
*/

/*!
    \fn QObject *QAbstractExtensionManager::extension(QObject *object, const QString &iid) const

    Returns the extension, specified by \a iid, for the given \a
    object.
*/

/*!
   \fn T qt_extension(QAbstractExtensionManager* manager, QObject *object)

   \relates QExtensionManager

   Returns the extension of the given \a object cast to type T if the
   object is of type T (or of a subclass); otherwise returns 0. The
   extension is retrieved using the given extension \a manager.

   \snippet doc/src/snippets/code/tools_designer_src_lib_extension_extension.cpp 0

   When implementing a custom widget plugin, a pointer to \QD's
   current QDesignerFormEditorInterface object (\c formEditor) is
   provided by the QDesignerCustomWidgetInterface::initialize()
   function's parameter.

   If the widget in the example above doesn't have a defined
   QDesignerPropertySheetExtension, \c propertySheet will be a null
   pointer.

*/

/*!
   \macro Q_DECLARE_EXTENSION_INTERFACE(ExtensionName, Identifier)

   \relates QExtensionManager

   Associates the given \a Identifier (a string literal) to the
   extension class called \a ExtensionName. The \a Identifier must be
   unique. For example:

   \snippet doc/src/snippets/code/tools_designer_src_lib_extension_extension.cpp 1

   Using the company and product names is a good way to ensure
   uniqueness of the identifier.

   When implementing a custom extension class, you must use
   Q_DECLARE_EXTENSION_INTERFACE() to enable usage of the
   qt_extension() function. The macro is normally located right after the
   class definition for \a ExtensionName, in the associated header
   file.

   \sa Q_DECLARE_INTERFACE()
*/

QT_END_NAMESPACE
