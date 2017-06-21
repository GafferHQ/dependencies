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

#include "qquickjumplistitem_p.h"
#include <QVariant>

QT_BEGIN_NAMESPACE

QQuickJumpListItem::QQuickJumpListItem(QObject *parent) :
    QObject(parent)
{
}

QQuickJumpListItem::~QQuickJumpListItem()
{
}

int QQuickJumpListItem::type() const
{
    return m_type;
}

void QQuickJumpListItem::setType(int type)
{
    m_type = type;
}

QWinJumpListItem *QQuickJumpListItem::toJumpListItem() const
{
    QWinJumpListItem *item = new QWinJumpListItem(QWinJumpListItem::Separator);
    switch (m_type) {
    case ItemTypeDestination:
        item->setType(QWinJumpListItem::Destination);
        item->setFilePath(property("filePath").toString());
        break;
    case ItemTypeLink:
        item->setType(QWinJumpListItem::Link);
        item->setFilePath(property("executablePath").toString());
        item->setArguments(QStringList(property("arguments").toStringList()));
        item->setDescription(property("description").toString());
        item->setTitle(property("title").toString());
        item->setIcon(QIcon(property("iconPath").toString()));
        break;
    }

    return item;
}

QT_END_NAMESPACE
