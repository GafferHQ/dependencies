/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#ifndef ACCESSIBILITYSCENEMANAGER_H
#define ACCESSIBILITYSCENEMANAGER_H

#include <QtGui>

#include "optionswidget.h"

QString translateRole(QAccessible::Role role);
class AccessibilitySceneManager : public QObject
{
Q_OBJECT
public:
    AccessibilitySceneManager();
    void setRootWindow(QWindow * window) { m_window = window; }
    void setView(QGraphicsView *view) { m_view = view; }
    void setScene(QGraphicsScene *scene) { m_scene = scene; }
    void setTreeView(QGraphicsView *treeView) { m_treeView = treeView; }
    void setTreeScene(QGraphicsScene *treeScene) { m_treeScene = treeScene; }

    void setOptionsWidget(OptionsWidget *optionsWidget) { m_optionsWidget = optionsWidget; }
public slots:
    void populateAccessibilityScene();
    void updateAccessibilitySceneItemFlags();
    void populateAccessibilityTreeScene();
    void handleUpdate(QAccessibleEvent *event);
    void setSelected(QObject *object);

    void changeScale(int scale);
private:
    void updateItems(QObject *root);
    void updateItem(QObject *object);
    void updateItem(QGraphicsRectItem *item, QAccessibleInterface *interface);
    void updateItemFlags(QGraphicsRectItem *item, QAccessibleInterface *interface);

    void populateAccessibilityScene(QAccessibleInterface * interface, QGraphicsScene *scene);
    QGraphicsRectItem * processInterface(QAccessibleInterface * interface, QGraphicsScene *scene);

    struct TreeItem;
    TreeItem computeLevels(QAccessibleInterface * interface, int level);
    void populateAccessibilityTreeScene(QAccessibleInterface * interface);
    void addGraphicsItems(TreeItem item, int row, int xPos);

    bool isHidden(QAccessibleInterface *interface);

    QWindow *m_window;
    QGraphicsView *m_view;
    QGraphicsScene *m_scene;
    QGraphicsView *m_treeView;
    QGraphicsScene *m_treeScene;
    QGraphicsItem *m_rootItem;
    OptionsWidget *m_optionsWidget;
    QObject *m_selectedObject;

    QHash<QObject *, QGraphicsRectItem*> m_graphicsItems;
    QSet<QObject *> m_animatedObjects;

    struct TreeItem {
        QList<TreeItem> children;
        int width;
        QString name;
        QString role;
        QString description;
        QRect rect;
        QAccessible::State state;
        QObject *object;
        TreeItem() : width(0) {}
    };

    TreeItem m_rootTreeItem;
    int m_treeItemWidth;
    int m_treeItemHorizontalPadding;
    int m_treeItemHeight;
    int m_treeItemVerticalPadding;
};

#endif // ACCESSIBILITYSCENEMANAGER_H
