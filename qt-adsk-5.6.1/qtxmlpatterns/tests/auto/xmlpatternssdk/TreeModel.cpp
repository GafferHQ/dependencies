/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QtDebug>

#include "TestContainer.h"

#include "TreeModel.h"

using namespace QPatternistSDK;

TreeModel::TreeModel(const QStringList columnData,
                     QObject *p) : QAbstractItemModel(p),
                                   m_root(0),
                                   m_columnData(columnData)
{
}

TreeModel::~TreeModel()
{
}

QVariant TreeModel::data(const QModelIndex &idx, int role) const
{
    if(!idx.isValid())
        return QVariant();

    TreeItem *item = static_cast<TreeItem *>(idx.internalPointer());
    Q_ASSERT(item);

    return item->data(static_cast<Qt::ItemDataRole>(role), idx.column());
}

QVariant TreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return m_columnData.value(section);

    return QVariant();
}

void TreeModel::childChanged(TreeItem *item)
{
    if (item) {
        const QModelIndex index = createIndex(item->row(), 0, item);
        dataChanged(index, index);
    } else {
        layoutChanged();
    }
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex &p) const
{
    const int c = columnCount(p);

    if(row < 0 || column < 0 || column >= c)
        return QModelIndex();

    TreeItem *parentItem;

    if(p.isValid())
        parentItem = static_cast<TreeItem *>(p.internalPointer());
    else
        parentItem = m_root;

    if(!parentItem)
        return QModelIndex();

    TreeItem *childItem = parentItem->child(row);

    if(childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex TreeModel::parent(const QModelIndex &idx) const
{
    if(!idx.isValid())
        return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem *>(idx.internalPointer());
    Q_ASSERT(childItem);
    TreeItem *parentItem = childItem->parent();

    if(!parentItem || parentItem == m_root)
        return QModelIndex();

    Q_ASSERT(parentItem);
    return createIndex(parentItem->row(), 0, parentItem);
}

Qt::ItemFlags TreeModel::flags(const QModelIndex &idx) const
{
    /* Not sure about this code. */
    if(!idx.isValid())
        return Qt::ItemFlags();

    return QAbstractItemModel::flags(idx);
}

int TreeModel::rowCount(const QModelIndex &p) const
{
    if(p.column() > 0)
        return 0;

    const TreeItem *parentItem;

    if(p.isValid())
        parentItem = static_cast<TreeItem *>(p.internalPointer());
    else
    {
        if(m_root)
            parentItem = m_root;
        else
            return 0;
    }

    return parentItem->childCount();
}

int TreeModel::columnCount(const QModelIndex &p) const
{
    if(p.isValid())
        return static_cast<TreeItem *>(p.internalPointer())->columnCount();
    else
        return m_columnData.count();
}

TreeItem *TreeModel::root() const
{
    return m_root;
}

void TreeModel::setRoot(TreeItem *r)
{
    TreeItem *const oldRoot = m_root;

    /* Notify views that we are radically changing. */
    beginResetModel();
    m_root = r;

    if(m_root)
        connect(r, SIGNAL(changed(TreeItem *)), SLOT(childChanged(TreeItem *)));

    endResetModel();

    delete oldRoot;
}

// vim: et:ts=4:sw=4:sts=4
