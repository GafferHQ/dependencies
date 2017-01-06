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

#include "boundingrecthighlighter.h"

#include "../qdeclarativeviewinspector.h"
#include "../qmlinspectorconstants.h"

#include <QtGui/QGraphicsPolygonItem>

#include <QtCore/QTimer>
#include <QtCore/QObject>
#include <QtCore/QDebug>

namespace QmlJSDebugger {

BoundingBox::BoundingBox(QGraphicsObject *itemToHighlight, QGraphicsItem *parentItem,
                         QObject *parent)
    : QObject(parent),
      highlightedObject(itemToHighlight),
      highlightPolygon(0),
      highlightPolygonEdge(0)
{
    highlightPolygon = new BoundingBoxPolygonItem(parentItem);
    highlightPolygonEdge = new BoundingBoxPolygonItem(parentItem);

    highlightPolygon->setPen(QPen(QColor(0, 22, 159)));
    highlightPolygonEdge->setPen(QPen(QColor(158, 199, 255)));

    highlightPolygon->setFlag(QGraphicsItem::ItemIsSelectable, false);
    highlightPolygonEdge->setFlag(QGraphicsItem::ItemIsSelectable, false);
}

BoundingBox::~BoundingBox()
{
    highlightedObject.clear();
}

BoundingBoxPolygonItem::BoundingBoxPolygonItem(QGraphicsItem *item) : QGraphicsPolygonItem(item)
{
    QPen pen;
    pen.setColor(QColor(108, 141, 221));
    pen.setWidth(1);
    setPen(pen);
}

int BoundingBoxPolygonItem::type() const
{
    return Constants::EditorItemType;
}

BoundingRectHighlighter::BoundingRectHighlighter(QDeclarativeViewInspector *view) :
    LiveLayerItem(view->declarativeView()->scene()),
    m_view(view)
{
}

BoundingRectHighlighter::~BoundingRectHighlighter()
{

}

void BoundingRectHighlighter::clear()
{
    foreach (BoundingBox *box, m_boxes)
        freeBoundingBox(box);
}

BoundingBox *BoundingRectHighlighter::boxFor(QGraphicsObject *item) const
{
    foreach (BoundingBox *box, m_boxes) {
        if (box->highlightedObject.data() == item)
            return box;
    }
    return 0;
}

void BoundingRectHighlighter::highlight(QList<QGraphicsObject*> items)
{
    if (items.isEmpty())
        return;

    QList<BoundingBox *> newBoxes;
    foreach (QGraphicsObject *itemToHighlight, items) {
        BoundingBox *box = boxFor(itemToHighlight);
        if (!box)
            box = createBoundingBox(itemToHighlight);

        newBoxes << box;
    }
    qSort(newBoxes);

    if (newBoxes != m_boxes) {
        clear();
        m_boxes << newBoxes;
    }

    highlightAll();
}

void BoundingRectHighlighter::highlight(QGraphicsObject* itemToHighlight)
{
    if (!itemToHighlight)
        return;

    BoundingBox *box = boxFor(itemToHighlight);
    if (!box) {
        box = createBoundingBox(itemToHighlight);
        m_boxes << box;
        qSort(m_boxes);
    }

    highlightAll();
}

BoundingBox *BoundingRectHighlighter::createBoundingBox(QGraphicsObject *itemToHighlight)
{
    if (!m_freeBoxes.isEmpty()) {
        BoundingBox *box = m_freeBoxes.last();
        if (box->highlightedObject.isNull()) {
            box->highlightedObject = itemToHighlight;
            box->highlightPolygon->show();
            box->highlightPolygonEdge->show();
            m_freeBoxes.removeLast();
            return box;
        }
    }

    BoundingBox *box = new BoundingBox(itemToHighlight, this, this);

    connect(itemToHighlight, SIGNAL(xChanged()), this, SLOT(refresh()));
    connect(itemToHighlight, SIGNAL(yChanged()), this, SLOT(refresh()));
    connect(itemToHighlight, SIGNAL(widthChanged()), this, SLOT(refresh()));
    connect(itemToHighlight, SIGNAL(heightChanged()), this, SLOT(refresh()));
    connect(itemToHighlight, SIGNAL(rotationChanged()), this, SLOT(refresh()));
    connect(itemToHighlight, SIGNAL(destroyed(QObject*)), this, SLOT(itemDestroyed(QObject*)));

    return box;
}

void BoundingRectHighlighter::removeBoundingBox(BoundingBox *box)
{
    delete box;
    box = 0;
}

void BoundingRectHighlighter::freeBoundingBox(BoundingBox *box)
{
    if (!box->highlightedObject.isNull()) {
        disconnect(box->highlightedObject.data(), SIGNAL(xChanged()), this, SLOT(refresh()));
        disconnect(box->highlightedObject.data(), SIGNAL(yChanged()), this, SLOT(refresh()));
        disconnect(box->highlightedObject.data(), SIGNAL(widthChanged()), this, SLOT(refresh()));
        disconnect(box->highlightedObject.data(), SIGNAL(heightChanged()), this, SLOT(refresh()));
        disconnect(box->highlightedObject.data(), SIGNAL(rotationChanged()), this, SLOT(refresh()));
    }

    box->highlightedObject.clear();
    box->highlightPolygon->hide();
    box->highlightPolygonEdge->hide();
    m_boxes.removeOne(box);
    m_freeBoxes << box;
}

void BoundingRectHighlighter::itemDestroyed(QObject *obj)
{
    foreach (BoundingBox *box, m_boxes) {
        if (box->highlightedObject.data() == obj) {
            freeBoundingBox(box);
            break;
        }
    }
}

void BoundingRectHighlighter::highlightAll()
{
    foreach (BoundingBox *box, m_boxes) {
        if (box && box->highlightedObject.isNull()) {
            // clear all highlights
            clear();
            return;
        }
        QGraphicsObject *item = box->highlightedObject.data();

        QRectF boundingRectInSceneSpace(item->mapToScene(item->boundingRect()).boundingRect());
        QRectF boundingRectInLayerItemSpace = mapRectFromScene(boundingRectInSceneSpace);
        QRectF bboxRect = m_view->adjustToScreenBoundaries(boundingRectInLayerItemSpace);
        QRectF edgeRect = bboxRect;
        edgeRect.adjust(-1, -1, 1, 1);

        box->highlightPolygon->setPolygon(QPolygonF(bboxRect));
        box->highlightPolygonEdge->setPolygon(QPolygonF(edgeRect));
    }
}

void BoundingRectHighlighter::refresh()
{
    if (!m_boxes.isEmpty())
        highlightAll();
}


} // namespace QmlJSDebugger
