/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#include "qobjectcleanuphandler.h"

QT_BEGIN_NAMESPACE

/*!
    \class QObjectCleanupHandler
    \inmodule QtCore
    \brief The QObjectCleanupHandler class watches the lifetime of multiple QObjects.

    \ingroup objectmodel

    A QObjectCleanupHandler is useful whenever you need to know when a
    number of \l{QObject}s that are owned by someone else have been
    deleted. This is important, for example, when referencing memory
    in an application that has been allocated in a shared library.

    To keep track of some \l{QObject}s, create a
    QObjectCleanupHandler, and add() the objects you are interested
    in. If you are no longer interested in tracking a particular
    object, use remove() to remove it from the cleanup handler. If an
    object being tracked by the cleanup handler gets deleted by
    someone else it will automatically be removed from the cleanup
    handler. You can delete all the objects in the cleanup handler
    with clear(), or by destroying the cleanup handler. isEmpty()
    returns \c true if the QObjectCleanupHandler has no objects to keep
    track of.

    \sa QPointer
*/

/*!
    Constructs an empty QObjectCleanupHandler.
*/
QObjectCleanupHandler::QObjectCleanupHandler()
{
}

/*!
    Destroys the cleanup handler. All objects in this cleanup handler
    will be deleted.

    \sa clear()
*/
QObjectCleanupHandler::~QObjectCleanupHandler()
{
    clear();
}

/*!
    Adds \a object to this cleanup handler and returns the pointer to
    the object.

    \sa remove()
*/
QObject *QObjectCleanupHandler::add(QObject* object)
{
    if (!object)
        return 0;

    connect(object, SIGNAL(destroyed(QObject*)), this, SLOT(objectDestroyed(QObject*)));
    cleanupObjects.insert(0, object);
    return object;
}

/*!
    Removes the \a object from this cleanup handler. The object will
    not be destroyed.

    \sa add()
*/
void QObjectCleanupHandler::remove(QObject *object)
{
    int index;
    if ((index = cleanupObjects.indexOf(object)) != -1) {
        cleanupObjects.removeAt(index);
        disconnect(object, SIGNAL(destroyed(QObject*)), this, SLOT(objectDestroyed(QObject*)));
    }
}

/*!
    Returns \c true if this cleanup handler is empty or if all objects in
    this cleanup handler have been destroyed; otherwise return false.

    \sa add(), remove(), clear()
*/
bool QObjectCleanupHandler::isEmpty() const
{
    return cleanupObjects.isEmpty();
}

/*!
    Deletes all objects in this cleanup handler. The cleanup handler
    becomes empty.

    \sa isEmpty()
*/
void QObjectCleanupHandler::clear()
{
    while (!cleanupObjects.isEmpty())
        delete cleanupObjects.takeFirst();
}

void QObjectCleanupHandler::objectDestroyed(QObject *object)
{
    remove(object);
}

QT_END_NAMESPACE
