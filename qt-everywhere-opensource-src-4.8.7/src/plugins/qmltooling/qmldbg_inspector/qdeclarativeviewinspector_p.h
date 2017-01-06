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

#ifndef QDECLARATIVEVIEWINSPECTOR_P_H
#define QDECLARATIVEVIEWINSPECTOR_P_H

#include "qdeclarativeviewinspector.h"

#include <QtCore/QWeakPointer>
#include <QtCore/QPointF>

#include "QtDeclarative/private/qdeclarativeinspectorservice_p.h"

namespace QmlJSDebugger {

class QDeclarativeViewInspector;
class LiveSelectionTool;
class ZoomTool;
class ColorPickerTool;
class LiveLayerItem;
class BoundingRectHighlighter;
class AbstractLiveEditTool;

class QDeclarativeViewInspectorPrivate : public QObject
{
    Q_OBJECT
public:
    QDeclarativeViewInspectorPrivate(QDeclarativeViewInspector *);
    ~QDeclarativeViewInspectorPrivate();

    QDeclarativeView *view;
    QDeclarativeViewInspector *q;
    QWeakPointer<QWidget> viewport;

    QList<QWeakPointer<QGraphicsObject> > currentSelection;

    LiveSelectionTool *selectionTool;
    ZoomTool *zoomTool;
    ColorPickerTool *colorPickerTool;
    LiveLayerItem *manipulatorLayer;

    BoundingRectHighlighter *boundingRectHighlighter;

    void setViewport(QWidget *widget);

    void clearEditorItems();
    void changeToSelectTool();
    QList<QGraphicsItem*> filterForSelection(QList<QGraphicsItem*> &itemlist) const;

    QList<QGraphicsItem*> selectableItems(const QPoint &pos) const;
    QList<QGraphicsItem*> selectableItems(const QPointF &scenePos) const;
    QList<QGraphicsItem*> selectableItems(const QRectF &sceneRect, Qt::ItemSelectionMode selectionMode) const;

    void setSelectedItemsForTools(const QList<QGraphicsItem *> &items);
    void setSelectedItems(const QList<QGraphicsItem *> &items);
    QList<QGraphicsItem *> selectedItems() const;

    void clearHighlight();
    void highlight(const QList<QGraphicsObject *> &item);
    inline void highlight(QGraphicsObject *item)
    { highlight(QList<QGraphicsObject*>() << item); }

    void changeToSingleSelectTool();
    void changeToMarqueeSelectTool();
    void changeToZoomTool();
    void changeToColorPickerTool();

public slots:
    void _q_onStatusChanged(QDeclarativeView::Status status);

    void _q_removeFromSelection(QObject *);

public:
    static QDeclarativeViewInspectorPrivate *get(QDeclarativeViewInspector *v) { return v->d_func(); }
};

} // namespace QmlJSDebugger

#endif // QDECLARATIVEVIEWINSPECTOR_P_H
