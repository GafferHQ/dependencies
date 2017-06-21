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

#include "qquickabstractstyle_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype AbstractStyle
    \instantiates QQuickAbstractStyle
    \qmlabstract
    \internal
*/

/*!
    \qmlpropertygroup QtQuick.Controls.Styles::AbstractStyle::padding
    \qmlproperty int AbstractStyle::padding.top
    \qmlproperty int AbstractStyle::padding.left
    \qmlproperty int AbstractStyle::padding.right
    \qmlproperty int AbstractStyle::padding.bottom

    This grouped property holds the \c top, \c left, \c right and \c bottom padding.
*/

QQuickAbstractStyle::QQuickAbstractStyle(QObject *parent) : QObject(parent)
{
}

QQmlListProperty<QObject> QQuickAbstractStyle::data()
{
    return QQmlListProperty<QObject>(this, 0, &QQuickAbstractStyle::data_append, &QQuickAbstractStyle::data_count,
                                     &QQuickAbstractStyle::data_at, &QQuickAbstractStyle::data_clear);
}

void QQuickAbstractStyle::data_append(QQmlListProperty<QObject> *list, QObject *object)
{
    if (QQuickAbstractStyle *style = qobject_cast<QQuickAbstractStyle *>(list->object))
        style->m_data.append(object);
}

int QQuickAbstractStyle::data_count(QQmlListProperty<QObject> *list)
{
    if (QQuickAbstractStyle *style = qobject_cast<QQuickAbstractStyle *>(list->object))
        return style->m_data.count();
    return 0;
}

QObject *QQuickAbstractStyle::data_at(QQmlListProperty<QObject> *list, int index)
{
    if (QQuickAbstractStyle *style = qobject_cast<QQuickAbstractStyle *>(list->object))
        return style->m_data.at(index);
    return 0;
}

void QQuickAbstractStyle::data_clear(QQmlListProperty<QObject> *list)
{
    if (QQuickAbstractStyle *style = qobject_cast<QQuickAbstractStyle *>(list->object))
        style->m_data.clear();
}

QT_END_NAMESPACE
