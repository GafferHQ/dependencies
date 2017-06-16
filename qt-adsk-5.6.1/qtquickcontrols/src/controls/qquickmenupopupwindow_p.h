/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKMENUPOPUPWINDOW_H
#define QQUICKMENUPOPUPWINDOW_H

#include "qquickpopupwindow_p.h"

QT_BEGIN_NAMESPACE

class QQuickMenu1;
class QQuickMenuBar1;

class QQuickMenuPopupWindow : public QQuickPopupWindow
{
    Q_OBJECT
public:
    QQuickMenuPopupWindow(QQuickMenu1 *menu);

    void setItemAt(QQuickItem *menuItem);
    void setParentWindow(QWindow *effectiveParentWindow, QQuickWindow *parentWindow);
    void setGeometry(int posx, int posy, int w, int h);

    void setParentItem(QQuickItem *);

    QQuickMenu1 *menu() const;
public Q_SLOTS:
    void setToBeDeletedLater();

protected Q_SLOTS:
    void updateSize();
    void updatePosition();

Q_SIGNALS:
    void willBeDeletedLater();

protected:
    void focusInEvent(QFocusEvent *);
    void exposeEvent(QExposeEvent *);
    bool shouldForwardEventAfterDismiss(QMouseEvent *) const;

private:
    QQuickItem *m_itemAt;
    QPointF m_oldItemPos;
    QPointF m_initialPos;
    QPointer<QQuickWindow> m_logicalParentWindow;
    QQuickMenu1 *m_menu;
};

QT_END_NAMESPACE

#endif // QQUICKMENUPOPUPWINDOW_H
