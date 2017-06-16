/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2015 The Qt Company Ltd.
 ** Contact: http://www.qt.io/licensing/
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#ifndef QQUICKJUMPLISTCATEGORY_P_H
#define QQUICKJUMPLISTCATEGORY_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qquickjumplistitem_p.h"

#include <QObject>
#include <QQmlListProperty>
#include <QWinJumpListCategory>
#include <QWinJumpListItem>

QT_BEGIN_NAMESPACE

class QQuickJumpListCategory : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<QObject> data READ data)
    Q_PROPERTY(QQmlListProperty<QQuickJumpListItem> items READ items)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibilityChanged)
    Q_CLASSINFO("DefaultProperty", "data")

public:
    explicit QQuickJumpListCategory(QObject *parent = 0);
    ~QQuickJumpListCategory();

    QString title() const;
    void setTitle(const QString &title);

    bool isVisible() const;
    void setVisible(bool visible);

    QQmlListProperty<QObject> data();
    QQmlListProperty<QQuickJumpListItem> items();

    QList<QWinJumpListItem *> toItemList() const;

Q_SIGNALS:
    void itemsChanged();
    void titleChanged();
    void visibilityChanged();

private:
    static void data_append(QQmlListProperty<QObject> *property, QObject *object);
    static int items_count(QQmlListProperty<QQuickJumpListItem> *property);
    static QQuickJumpListItem *items_at(QQmlListProperty<QQuickJumpListItem> *property, int index);

    bool m_visible;
    QString m_title;
    QList<QQuickJumpListItem *> m_items;
};

QT_END_NAMESPACE

#endif // QQUICKJUMPLISTCATEGORY_P_H
