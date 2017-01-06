/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "liverubberbandselectionmanipulator.h"

#include "../qdeclarativeviewinspector_p.h"

#include <QtGui/QGraphicsItem>

#include <QtCore/QDebug>

namespace QmlJSDebugger {

LiveRubberBandSelectionManipulator::LiveRubberBandSelectionManipulator(QGraphicsObject *layerItem,
                                                                       QDeclarativeViewInspector *editorView)
    : m_selectionRectangleElement(layerItem),
      m_editorView(editorView),
      m_beginFormEditorItem(0),
      m_isActive(false)
{
    m_selectionRectangleElement.hide();
}

void LiveRubberBandSelectionManipulator::clear()
{
    m_selectionRectangleElement.clear();
    m_isActive = false;
    m_beginPoint = QPointF();
    m_itemList.clear();
    m_oldSelectionList.clear();
}

QGraphicsItem *LiveRubberBandSelectionManipulator::topFormEditorItem(const QList<QGraphicsItem*>
                                                                     &itemList)
{
    if (itemList.isEmpty())
        return 0;

    return itemList.first();
}

void LiveRubberBandSelectionManipulator::begin(const QPointF &beginPoint)
{
    m_beginPoint = beginPoint;
    m_selectionRectangleElement.setRect(m_beginPoint, m_beginPoint);
    m_selectionRectangleElement.show();
    m_isActive = true;
    QDeclarativeViewInspectorPrivate *inspectorPrivate
            = QDeclarativeViewInspectorPrivate::get(m_editorView);
    m_beginFormEditorItem = topFormEditorItem(inspectorPrivate->selectableItems(beginPoint));
    m_oldSelectionList = m_editorView->selectedItems();
}

void LiveRubberBandSelectionManipulator::update(const QPointF &updatePoint)
{
    m_selectionRectangleElement.setRect(m_beginPoint, updatePoint);
}

void LiveRubberBandSelectionManipulator::end()
{
    m_oldSelectionList.clear();
    m_selectionRectangleElement.hide();
    m_isActive = false;
}

void LiveRubberBandSelectionManipulator::select(SelectionType selectionType)
{
    QDeclarativeViewInspectorPrivate *inspectorPrivate
            = QDeclarativeViewInspectorPrivate::get(m_editorView);
    QList<QGraphicsItem*> itemList
            = inspectorPrivate->selectableItems(m_selectionRectangleElement.rect(),
                                               Qt::IntersectsItemShape);
    QList<QGraphicsItem*> newSelectionList;

    foreach (QGraphicsItem* item, itemList) {
        if (item
                && item->parentItem()
                && !newSelectionList.contains(item)
                //&& m_beginFormEditorItem->childItems().contains(item) // TODO activate this test
                )
        {
            newSelectionList.append(item);
        }
    }

    if (newSelectionList.isEmpty() && m_beginFormEditorItem)
        newSelectionList.append(m_beginFormEditorItem);

    QList<QGraphicsItem*> resultList;

    switch (selectionType) {
    case AddToSelection: {
        resultList.append(m_oldSelectionList);
        resultList.append(newSelectionList);
    }
        break;
    case ReplaceSelection: {
        resultList.append(newSelectionList);
    }
        break;
    case RemoveFromSelection: {
        QSet<QGraphicsItem*> oldSelectionSet(m_oldSelectionList.toSet());
        QSet<QGraphicsItem*> newSelectionSet(newSelectionList.toSet());
        resultList.append(oldSelectionSet.subtract(newSelectionSet).toList());
    }
    }

    m_editorView->setSelectedItems(resultList);
}


void LiveRubberBandSelectionManipulator::setItems(const QList<QGraphicsItem*> &itemList)
{
    m_itemList = itemList;
}

QPointF LiveRubberBandSelectionManipulator::beginPoint() const
{
    return m_beginPoint;
}

bool LiveRubberBandSelectionManipulator::isActive() const
{
    return m_isActive;
}

} // namespace QmlJSDebugger
