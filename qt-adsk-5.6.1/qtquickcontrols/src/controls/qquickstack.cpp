/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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

#include "qquickstack_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype Stack
    \instantiates QQuickStack
    \inqmlmodule QtQuick.Controls
    \ingroup controls
    \brief Provides attached properties for items pushed onto a StackView.

    The Stack type provides attached properties for items pushed onto a \l StackView.
    It gives specific information about the item, such as its \l status and
    \l index in the stack \l view the item is in.

    \sa StackView
*/

QQuickStack::QQuickStack(QObject *object)
    : QObject(object),
      m_index(-1),
      m_status(Inactive),
      m_view(0)
{
}

QQuickStack *QQuickStack::qmlAttachedProperties(QObject *object)
{
    return new QQuickStack(object);
}

/*!
    \readonly
    \qmlattachedproperty int Stack::index

    This property holds the index of the item inside \l{view}{StackView},
    so that \l{StackView::get()}{StackView.get(index)} will return the item itself.
    If \l{Stack::view}{view} is \c null, \a index will be \c -1.
*/
int QQuickStack::index() const
{
    return m_index;
}

void QQuickStack::setIndex(int index)
{
    if (m_index != index) {
        m_index = index;
        emit indexChanged();
    }
}

/*!
    \readonly
    \qmlattachedproperty enumeration Stack::status

    This property holds the status of the item. It can have one of the following values:
    \list
    \li \c Stack.Inactive: the item is not visible
    \li \c Stack.Activating: the item is transitioning into becoming an active item on the stack
    \li \c Stack.Active: the item is on top of the stack
    \li \c Stack.Deactivating: the item is transitioning into becoming inactive
    \endlist
*/
QQuickStack::Status QQuickStack::status() const
{
    return m_status;
}

void QQuickStack::setStatus(Status status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

/*!
    \readonly
    \qmlattachedproperty StackView Stack::view

    This property holds the StackView the item is in. If the item is not inside
    a StackView, \a view will be \c null.
*/
QQuickItem *QQuickStack::view() const
{
    return m_view;
}

void QQuickStack::setView(QQuickItem *view)
{
    if (m_view != view) {
        m_view = view;
        emit viewChanged();
    }
}

QT_END_NAMESPACE
