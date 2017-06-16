/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwebenginescriptcollection.h"
#include "qwebenginescriptcollection_p.h"

#include "user_script_controller_host.h"

using QtWebEngineCore::UserScript;

/*!
    \class QWebEngineScriptCollection
    \inmodule QtWebEngineWidgets
    \since 5.5
    \brief The QWebEngineScriptCollection class represents a collection of user scripts.

    QWebEngineScriptCollection manages a set of user scripts.

    Use QWebEnginePage::scripts() and QWebEngineProfile::scripts() to access
    the collection of scripts associated with a single page or a
    number of pages sharing the same profile.
*/

/*!
    \fn QWebEngineScriptCollection::isEmpty() const

    Returns \c true if the collection is empty; otherwise returns \c false.
*/

/*!
    \fn QWebEngineScriptCollection::size() const

    Returns the number of elements in the collection.
*/

QWebEngineScriptCollection::QWebEngineScriptCollection(QWebEngineScriptCollectionPrivate *collectionPrivate)
    :d(collectionPrivate)
{
}

/*!
    Destroys the collection.
*/
QWebEngineScriptCollection::~QWebEngineScriptCollection()
{
}

/*!
    Returns the number of elements in the collection.
 */

int QWebEngineScriptCollection::count() const
{
    return d->count();
}

/*!
    Returns \c true if the collection contains an occurrence of \a value; otherwise returns
    \c false.
 */

bool QWebEngineScriptCollection::contains(const QWebEngineScript &value) const
{
    return d->contains(value);
}

/*!
 * Returns the first script found in the collection with the name \a name, or a null
 * QWebEngineScript if none was found.
 * \note The order in which the script collection is traversed is undefined, which means this should
 * be used when the unicity is guaranteed at the application level.
 * \sa findScripts()
 */

QWebEngineScript QWebEngineScriptCollection::findScript(const QString &name) const
{
    return d->find(name);
}

/*!
    Returns the list of scripts in the collection with the name \a name, or an empty list if none
    was found.
 */

QList<QWebEngineScript> QWebEngineScriptCollection::findScripts(const QString &name) const
{
    return d->toList(name);
}
/*!
    Inserts the script \a s into the collection.
 */
void QWebEngineScriptCollection::insert(const QWebEngineScript &s)
{
    d->insert(s);
}
/*!
    Inserts scripts from the list \a list into the collection.
 */
void QWebEngineScriptCollection::insert(const QList<QWebEngineScript> &list)
{
    d->reserve(list.size());
    Q_FOREACH (const QWebEngineScript &s, list)
        d->insert(s);
}

/*!
    Removes \a script from the collection.

    Returns \c true if the script was found and successfully removed from the collection; otherwise
    returns \c false.
 */
bool QWebEngineScriptCollection::remove(const QWebEngineScript &script)
{
    return d->remove(script);
}

/*!
 * Removes all scripts from this collection.
 */
void QWebEngineScriptCollection::clear()
{
    d->clear();
}

/*!
    Returns a list with the values of the scripts used in this collection.
 */
QList<QWebEngineScript> QWebEngineScriptCollection::toList() const
{
    return d->toList();
}


QWebEngineScriptCollectionPrivate::QWebEngineScriptCollectionPrivate(QtWebEngineCore::UserScriptControllerHost *controller, QtWebEngineCore::WebContentsAdapter *webContents)
    : m_scriptController(controller)
    , m_contents(webContents)
{
}

int QWebEngineScriptCollectionPrivate::count() const
{
    return m_scriptController->registeredScripts(m_contents).count();
}

bool QWebEngineScriptCollectionPrivate::contains(const QWebEngineScript &s) const
{
    return m_scriptController->containsUserScript(*s.d, m_contents);
}

void QWebEngineScriptCollectionPrivate::insert(const QWebEngineScript &script)
{
    if (!script.d)
        return;
    m_scriptController->addUserScript(*script.d, m_contents);
}

bool QWebEngineScriptCollectionPrivate::remove(const QWebEngineScript &script)
{
    if (!script.d)
        return false;
    return m_scriptController->removeUserScript(*script.d, m_contents);
}

QList<QWebEngineScript> QWebEngineScriptCollectionPrivate::toList(const QString &scriptName) const
{
    QList<QWebEngineScript> ret;
    Q_FOREACH (const UserScript &script, m_scriptController->registeredScripts(m_contents))
        if (scriptName.isNull() || scriptName == script.name())
            ret.append(QWebEngineScript(script));
    return ret;
}

QWebEngineScript QWebEngineScriptCollectionPrivate::find(const QString &name) const
{
    Q_FOREACH (const UserScript &script, m_scriptController->registeredScripts(m_contents))
        if (name == script.name())
            return QWebEngineScript(script);
    return QWebEngineScript();
}

void QWebEngineScriptCollectionPrivate::clear()
{
    m_scriptController->clearAllScripts(m_contents);
}

void QWebEngineScriptCollectionPrivate::reserve(int capacity)
{
    m_scriptController->reserve(m_contents, capacity);
}

void QWebEngineScriptCollectionPrivate::rebindToContents(QtWebEngineCore::WebContentsAdapter *page)
{
    Q_ASSERT(m_contents);
    Q_ASSERT(page);
    Q_ASSERT(m_contents != page);

    Q_FOREACH (const UserScript &script, m_scriptController->registeredScripts(m_contents)) {
        m_scriptController->addUserScript(script, page);
    }
    m_contents = page;
}
