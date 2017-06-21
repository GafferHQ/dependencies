/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef INSPECTTOOL_H
#define INSPECTTOOL_H

#include "abstracttool.h"

#include <QtCore/QPointF>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

QT_BEGIN_NAMESPACE

class QQuickView;
class QQuickItem;

namespace QmlJSDebugger {

class QQuickViewInspector;
class HoverHighlight;

class InspectTool : public AbstractTool
{
    Q_OBJECT
public:
    enum ZoomDirection {
        ZoomIn,
        ZoomOut
    };

    InspectTool(QQuickViewInspector *inspector, QQuickView *view);
    ~InspectTool();

    void enable(bool enable);

    void leaveEvent(QEvent *);

    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void mouseDoubleClickEvent(QMouseEvent *);

    void hoverMoveEvent(QMouseEvent *);
#ifndef QT_NO_WHEELEVENT
    void wheelEvent(QWheelEvent *);
#endif

    void keyPressEvent(QKeyEvent *) {}
    void keyReleaseEvent(QKeyEvent *);

    void touchEvent(QTouchEvent *event);

private:
    QQuickViewInspector *inspector() const;
    qreal nextZoomScale(ZoomDirection direction);
    void scaleView(const qreal &factor, const QPointF &newcenter, const QPointF &oldcenter);
    void zoomIn();
    void zoomOut();
    void initializeDrag(const QPointF &pos);
    void dragItemToPosition();
    void moveItem(bool valid);
    void selectNextItem();
    void selectItem();

private slots:
    void zoomTo100();
    void showSelectedItemName();

private:
    bool m_originalSmooth;
    bool m_dragStarted;
    bool m_pinchStarted;
    bool m_didPressAndHold;
    bool m_tapEvent;
    QPointer<QQuickItem> m_contentItem;
    QPointF m_dragStartPosition;
    QPointF m_mousePosition;
    QPointF m_originalPosition;
    qreal m_smoothScaleFactor;
    qreal m_minScale;
    qreal m_maxScale;
    qreal m_originalScale;
    ulong m_touchTimestamp;
    QTimer m_pressAndHoldTimer;
    QTimer m_nameDisplayTimer;

    HoverHighlight *m_hoverHighlight;
    QQuickItem *m_lastItem;
    QQuickItem *m_lastClickedItem;
};

} // namespace QmlJSDebugger

QT_END_NAMESPACE

#endif // INSPECTTOOL_H
