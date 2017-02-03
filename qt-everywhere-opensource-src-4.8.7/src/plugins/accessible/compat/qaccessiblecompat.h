/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QACCESSIBLECOMPAT_H
#define QACCESSIBLECOMPAT_H

#include <QtGui/qaccessiblewidget.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_ACCESSIBILITY

class Q3ListView;
class Q3TextEdit;
class Q3IconView;
class Q3ListBox;

class Q3AccessibleScrollView : public QAccessibleWidget
{
public:
    Q3AccessibleScrollView(QWidget *w, Role role);

    virtual int itemAt(int x, int y) const;
    virtual QRect itemRect(int item) const;
    virtual int itemCount() const;
};

class QAccessibleListView : public Q3AccessibleScrollView
{
public:
    explicit QAccessibleListView(QWidget *o);

    int itemAt(int x, int y) const;
    QRect itemRect(int item) const;
    int itemCount() const;

    QString text(Text t, int child) const;
    Role role(int child) const;
    State state(int child) const;

    bool setSelected(int child, bool on, bool extend);
    void clearSelection();
    QVector<int> selection() const;

protected:
    Q3ListView *listView() const;
};

class QAccessibleIconView : public Q3AccessibleScrollView
{
public:
    explicit QAccessibleIconView(QWidget *o);

    int itemAt(int x, int y) const;
    QRect itemRect(int item) const;
    int itemCount() const;

    QString text(Text t, int child) const;
    Role role(int child) const;
    State state(int child) const;

    bool setSelected(int child, bool on, bool extend);
    void clearSelection();
    QVector<int> selection() const;

protected:
    Q3IconView *iconView() const;
};

class Q3AccessibleTextEdit : public Q3AccessibleScrollView
{
public:
    explicit Q3AccessibleTextEdit(QWidget *o);

    int itemAt(int x, int y) const;
    QRect itemRect(int item) const;
    int itemCount() const;

    QString text(Text t, int child) const;
    void setText(Text t, int control, const QString &text);
    Role role(int child) const;

protected:
    Q3TextEdit *textEdit() const;
};

class Q3WidgetStack;

class QAccessibleWidgetStack : public QAccessibleWidget
{
public:
    explicit QAccessibleWidgetStack(QWidget *o);

    int childCount() const;
    int indexOfChild(const QAccessibleInterface*) const;

    int childAt(int x, int y) const;

    int navigate(RelationFlag rel, int entry, QAccessibleInterface **target) const;

protected:
    Q3WidgetStack *widgetStack() const;
};

class QAccessibleListBox : public Q3AccessibleScrollView
{
public:
    explicit QAccessibleListBox(QWidget *o);

    int itemAt(int x, int y) const;
    QRect itemRect(int item) const;
    int itemCount() const;

    QString text(Text t, int child) const;
    Role role(int child) const;
    State state(int child) const;

    bool setSelected(int child, bool on, bool extend);
    void clearSelection();
    QVector<int> selection() const;

protected:
    Q3ListBox *listBox() const;
};

#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE

#endif // QACCESSIBLECOMPAT_H
