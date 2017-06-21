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

#ifndef QQUICKVIEWINSPECTOR_H
#define QQUICKVIEWINSPECTOR_H

#include "abstractviewinspector.h"

#include <QtCore/QPointer>
#include <QtCore/QHash>
#include <QtQuick/QQuickView>

QT_BEGIN_NAMESPACE
class QQuickView;
class QQuickItem;

namespace QmlJSDebugger {

class InspectTool;
class SelectionHighlight;

class QQuickViewInspector : public AbstractViewInspector
{
    Q_OBJECT
public:
    explicit QQuickViewInspector(QQmlDebugService *service, QQuickView *view, QObject *parent = 0);

    // AbstractViewInspector
    void changeCurrentObjects(const QList<QObject*> &objects);
    void reparentQmlObject(QObject *object, QObject *newParent);
    Qt::WindowFlags windowFlags() const;
    void setWindowFlags(Qt::WindowFlags flags);
    QQmlEngine *declarativeEngine() const;

    QQuickView *view() const { return m_view; }
    QQuickItem *overlay() const { return m_overlay; }

    QQuickItem *topVisibleItemAt(const QPointF &pos) const;
    QList<QQuickItem *> itemsAt(const QPointF &pos) const;

    QList<QQuickItem *> selectedItems() const;
    void setSelectedItems(const QList<QQuickItem*> &items);

    QString titleForItem(QQuickItem *item) const;
    void showSelectedItemName(QQuickItem *item, const QPointF &point);

    void reloadQmlFile(const QHash<QString, QByteArray> &changesHash);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

    bool mouseMoveEvent(QMouseEvent *);

    void setShowAppOnTop(bool appOnTop);

private slots:
    void removeFromSelectedItems(QObject *);
    void onViewStatus(QQuickView::Status status);
    void applyAppOnTop();

private:
    bool syncSelectedItems(const QList<QQuickItem*> &items);

    QQuickView *m_view;
    QQuickItem *m_overlay;

    InspectTool *m_inspectTool;

    QList<QPointer<QQuickItem> > m_selectedItems;
    QHash<QQuickItem*, SelectionHighlight*> m_highlightItems;
    bool m_sendQmlReloadedMessage;
    bool m_appOnTop;
};

} // namespace QmlJSDebugger

QT_END_NAMESPACE

#endif // QQUICKVIEWINSPECTOR_H
