/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#ifndef WIDGETSELECTION_H
#define WIDGETSELECTION_H

#include "formeditor_global.h"
#include <invisible_widget_p.h>

#include <QtCore/QHash>
#include <QtCore/QPointer>

QT_BEGIN_NAMESPACE

class QDesignerFormEditorInterface;
class QMouseEvent;
class QPaintEvent;

namespace qdesigner_internal {

class FormWindow;
class WidgetSelection;

class QT_FORMEDITOR_EXPORT WidgetHandle: public InvisibleWidget
{
    Q_OBJECT
public:
    enum Type
    {
        LeftTop,
        Top,
        RightTop,
        Right,
        RightBottom,
        Bottom,
        LeftBottom,
        Left,

        TypeCount
    };

    WidgetHandle(FormWindow *parent, Type t, WidgetSelection *s);
    void setWidget(QWidget *w);
    void setActive(bool a);
    void updateCursor();

    void setEnabled(bool) {}

    QDesignerFormEditorInterface *core() const;

protected:
    void paintEvent(QPaintEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);

private:
    void changeGridLayoutItemSpan();
    void changeFormLayoutItemSpan();
    void trySetGeometry(QWidget *w, int x, int y, int width, int height);
    void tryResize(QWidget *w, int width, int height);

private:
    QWidget *m_widget;
    const Type m_type;
    QPoint m_origPressPos;
    FormWindow *m_formWindow;
    WidgetSelection *m_sel;
    QRect m_geom, m_origGeom;
    bool m_active;
};

class QT_FORMEDITOR_EXPORT WidgetSelection: public QObject
{
    Q_OBJECT
public:
    WidgetSelection(FormWindow *parent);

    void setWidget(QWidget *w);
    bool isUsed() const;

    void updateActive();
    void updateGeometry();
    void hide();
    void show();
    void update();

    QWidget *widget() const;

    QDesignerFormEditorInterface *core() const;

    bool eventFilter(QObject *object, QEvent *event) Q_DECL_OVERRIDE;

    enum  WidgetState { UnlaidOut, LaidOut, ManagedGridLayout, ManagedFormLayout };
    static WidgetState widgetState(const QDesignerFormEditorInterface *core, QWidget *w);

private:
    WidgetHandle *m_handles[WidgetHandle::TypeCount];
    QPointer<QWidget> m_widget;
    FormWindow *m_formWindow;
};

}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // WIDGETSELECTION_H
