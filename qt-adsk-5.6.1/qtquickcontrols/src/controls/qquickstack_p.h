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

#ifndef QQUICKSTACK_P_H
#define QQUICKSTACK_P_H

#include <QtQuick/qquickitem.h>

QT_BEGIN_NAMESPACE

class QQuickStack : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int index READ index NOTIFY indexChanged)
    Q_PROPERTY(int __index READ index WRITE setIndex NOTIFY indexChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(Status __status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(QQuickItem* view READ view NOTIFY viewChanged)
    Q_PROPERTY(QQuickItem* __view READ view WRITE setView NOTIFY viewChanged)
    Q_ENUMS(Status)

public:
    QQuickStack(QObject *object = 0);

    static QQuickStack *qmlAttachedProperties(QObject *object);

    int index() const;
    void setIndex(int index);

    enum Status {
        Inactive = 0,
        Deactivating = 1,
        Activating = 2,
        Active = 3
    };

    Status status() const;
    void setStatus(Status status);

    QQuickItem *view() const;
    void setView(QQuickItem *view);

signals:
    void statusChanged();
    void viewChanged();
    void indexChanged();

private:
    int m_index;
    Status m_status;
    QQuickItem *m_view;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickStack)
QML_DECLARE_TYPEINFO(QQuickStack, QML_HAS_ATTACHED_PROPERTIES)

#endif // QQUICKSTACK_P_H
