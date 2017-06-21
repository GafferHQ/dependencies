/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
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

#include "qgraphicsgridlayoutengine_p.h"

#ifndef QT_NO_GRAPHICSVIEW

#include "qgraphicslayoutitem_p.h"
#include "qgraphicslayout_p.h"
#include "qgraphicswidget.h"
#include <private/qgraphicswidget_p.h>

QT_BEGIN_NAMESPACE

bool QGraphicsGridLayoutEngineItem::isHidden() const
{
    if (QGraphicsItem *item = q_layoutItem->graphicsItem())
        return QGraphicsItemPrivate::get(item)->explicitlyHidden;
    return false;
}

/*!
  \internal

  If this returns true, the layout will arrange just as if the item was never added to the layout.
  (Note that this shouldn't lead to a "double spacing" where the item was hidden)
  ### Qt6: Move to QGraphicsLayoutItem and make virtual
*/
bool QGraphicsGridLayoutEngineItem::isIgnored() const
{
    return isHidden() && !q_layoutItem->sizePolicy().retainSizeWhenHidden();
}

/*
  returns \c true if the size policy returns \c true for either hasHeightForWidth()
  or hasWidthForHeight()
 */
bool QGraphicsGridLayoutEngineItem::hasDynamicConstraint() const
{
    return QGraphicsLayoutItemPrivate::get(q_layoutItem)->hasHeightForWidth()
        || QGraphicsLayoutItemPrivate::get(q_layoutItem)->hasWidthForHeight();
}

Qt::Orientation QGraphicsGridLayoutEngineItem::dynamicConstraintOrientation() const
{
    if (QGraphicsLayoutItemPrivate::get(q_layoutItem)->hasHeightForWidth())
        return Qt::Vertical;
    else //if (QGraphicsLayoutItemPrivate::get(q_layoutItem)->hasWidthForHeight())
        return Qt::Horizontal;
}


void QGraphicsGridLayoutEngine::setAlignment(QGraphicsLayoutItem *graphicsLayoutItem, Qt::Alignment alignment)
{
    if (QGraphicsGridLayoutEngineItem *gridEngineItem = findLayoutItem(graphicsLayoutItem)) {
        gridEngineItem->setAlignment(alignment);
        invalidate();
    }
}

Qt::Alignment QGraphicsGridLayoutEngine::alignment(QGraphicsLayoutItem *graphicsLayoutItem) const
{
    if (QGraphicsGridLayoutEngineItem *gridEngineItem = findLayoutItem(graphicsLayoutItem))
        return gridEngineItem->alignment();
    return 0;
}


void QGraphicsGridLayoutEngine::setStretchFactor(QGraphicsLayoutItem *layoutItem, int stretch,
                                         Qt::Orientation orientation)
{
    Q_ASSERT(stretch >= 0);

    if (QGraphicsGridLayoutEngineItem *item = findLayoutItem(layoutItem))
        item->setStretchFactor(stretch, orientation);
}

int QGraphicsGridLayoutEngine::stretchFactor(QGraphicsLayoutItem *layoutItem, Qt::Orientation orientation) const
{
    if (QGraphicsGridLayoutEngineItem *item = findLayoutItem(layoutItem))
        return item->stretchFactor(orientation);
    return 0;
}


QT_END_NAMESPACE

#endif // QT_NO_GRAPHICSVIEW
