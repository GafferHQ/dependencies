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

#ifndef COMPLEXWIDGETS_H
#define COMPLEXWIDGETS_H

#include <QtCore/qpointer.h>
#include <QtGui/qaccessiblewidget.h>
#include <QtGui/qabstractitemview.h>
#include <QtGui/qaccessible2.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_ACCESSIBILITY

class QAbstractButton;
class QHeaderView;
class QTabBar;
class QComboBox;
class QTitleBar;
class QAbstractScrollArea;
class QScrollArea;

#ifndef QT_NO_SCROLLAREA
class QAccessibleAbstractScrollArea : public QAccessibleWidgetEx
{
public:
    explicit QAccessibleAbstractScrollArea(QWidget *widget);

    enum AbstractScrollAreaElement {
        Self = 0,
        Viewport,
        HorizontalContainer,
        VerticalContainer,
        CornerWidget,
        Undefined
    };

    QString text(Text textType, int child) const;
    void setText(Text textType, int child, const QString &text);
    State state(int child) const;
    QVariant invokeMethodEx(QAccessible::Method method, int child, const QVariantList &params);
    int childCount() const;
    int indexOfChild(const QAccessibleInterface *child) const;
    bool isValid() const;
    int navigate(RelationFlag relation, int entry, QAccessibleInterface **target) const;
    QRect rect(int child) const;
    int childAt(int x, int y) const;

//protected:
    QAbstractScrollArea *abstractScrollArea() const;

private:
    QWidgetList accessibleChildren() const;
    AbstractScrollAreaElement elementType(QWidget *widget) const;
    bool isLeftToRight() const;
};

class QAccessibleScrollArea : public QAccessibleAbstractScrollArea
{
public:
    explicit QAccessibleScrollArea(QWidget *widget);
};

#endif // QT_NO_SCROLLAREA

#ifndef QT_NO_ITEMVIEWS
class QAccessibleHeader : public QAccessibleWidgetEx
{
    Q_ACCESSIBLE_OBJECT
public:
    explicit QAccessibleHeader(QWidget *w);

    int childCount() const;

    QRect rect(int child) const;
    QString text(Text t, int child) const;
    Role role(int child) const;
    State state(int child) const;

protected:
    QHeaderView *header() const;
};

class QAccessibleItemRow: public QAccessibleInterface
{
    friend class QAccessibleItemView;
public:
    QAccessibleItemRow(QAbstractItemView *view, const QModelIndex &index = QModelIndex(), bool isHeader = false);
    QRect rect(int child) const;
    QString text(Text t, int child) const;
    void setText(Text t, int child, const QString &text);
    bool isValid() const;
    QObject *object() const;
    Role role(int child) const;
    State state(int child) const;

    int childCount() const;
    int indexOfChild(const QAccessibleInterface *) const;
    QList<QModelIndex> children() const;

    Relation relationTo(int child, const QAccessibleInterface *other, int otherChild) const;
    int childAt(int x, int y) const;
    int navigate(RelationFlag relation, int index, QAccessibleInterface **iface) const;

    int userActionCount(int child) const;
    QString actionText(int action, Text t, int child) const;
    bool doAction(int action, int child, const QVariantList &params = QVariantList());

    QModelIndex childIndex(int child) const;

    QHeaderView *horizontalHeader() const;  //used by QAccessibleItemView
private:
    static QAbstractItemView::CursorAction toCursorAction(Relation rel);
    int logicalFromChild(QHeaderView *header, int child) const;
    int treeLevel() const;
    QHeaderView *verticalHeader() const;
    QString text_helper(int child) const;

    QPersistentModelIndex row;
    QPointer<QAbstractItemView> view;
    bool m_header;
};

class QAccessibleItemView: public QAccessibleAbstractScrollArea, public QAccessibleTableInterface
{
    Q_ACCESSIBLE_OBJECT
public:
    explicit QAccessibleItemView(QWidget *w);

    QObject *object() const;
    Role role(int child) const;
    State state(int child) const;
    QRect rect(int child) const;
    int childAt(int x, int y) const;
    int childCount() const;
    QString text(Text t, int child) const;
    void setText(Text t, int child, const QString &text);
    int indexOfChild(const QAccessibleInterface *iface) const;

    QModelIndex childIndex(int child) const;
    int entryFromIndex(const QModelIndex &index) const;
    bool isValid() const;
    int navigate(RelationFlag relation, int index, QAccessibleInterface **iface) const;

    QAccessibleInterface *accessibleAt(int row, int column);
    QAccessibleInterface *caption();
    int childIndex(int rowIndex, int columnIndex);
    QString columnDescription(int column);
    int columnSpan(int row, int column);
    QAccessibleInterface *columnHeader();
    int columnIndex(int childIndex);
    int columnCount();
    int rowCount();
    int selectedColumnCount();
    int selectedRowCount();
    QString rowDescription(int row);
    int rowSpan(int row, int column);
    QAccessibleInterface *rowHeader();
    int rowIndex(int childIndex);
    int selectedRows(int maxRows, QList<int> *rows);
    int selectedColumns(int maxColumns, QList<int> *columns);
    QAccessibleInterface *summary();
    bool isColumnSelected(int column);
    bool isRowSelected(int row);
    bool isSelected(int row, int column);
    void selectRow(int row);
    void selectColumn(int column);
    void unselectRow(int row);
    void unselectColumn(int column);
    void cellAtIndex(int index, int *row, int *column, int *rowSpan,
                     int *columnSpan, bool *isSelected);

    QHeaderView *horizontalHeader() const;
    QHeaderView *verticalHeader() const;
    bool isValidChildRole(QAccessible::Role role) const;

protected:
    QAbstractItemView *itemView() const;
    QModelIndex index(int row, int column) const;

private:
    inline bool atViewport() const {
        return atVP;
    };
    QAccessible::Role expectedRoleOfChildren() const;

    bool atVP;
};

#endif

#ifndef QT_NO_TABBAR
class QAccessibleTabBar : public QAccessibleWidgetEx
{
    Q_ACCESSIBLE_OBJECT
public:
    explicit QAccessibleTabBar(QWidget *w);

    int childCount() const;

    QRect rect(int child) const;
    QString text(Text t, int child) const;
    Role role(int child) const;
    State state(int child) const;
    int navigate(RelationFlag relation, int entry,
                         QAccessibleInterface **target) const;

    bool doAction(int action, int child, const QVariantList &params);
    QString actionText(int action, Text t, int child) const;
    int userActionCount(int child) const;
    bool setSelected(int child, bool on, bool extend);
    QVector<int> selection() const;

protected:
    QTabBar *tabBar() const;

private:
    QAbstractButton *button(int child) const;
};
#endif // QT_NO_TABBAR

#ifndef QT_NO_COMBOBOX
class QAccessibleComboBox : public QAccessibleWidgetEx
{
    Q_ACCESSIBLE_OBJECT
public:
    explicit QAccessibleComboBox(QWidget *w);

    enum ComboBoxElements {
        ComboBoxSelf        = 0,
        CurrentText,
        OpenList,
        PopupList
    };

    int childCount() const;
    int childAt(int x, int y) const;
    int indexOfChild(const QAccessibleInterface *child) const;
    int navigate(RelationFlag rel, int entry, QAccessibleInterface **target) const;

    QString text(Text t, int child) const;
    QRect rect(int child) const;
    Role role(int child) const;
    State state(int child) const;

    bool doAction(int action, int child, const QVariantList &params);
    QString actionText(int action, Text t, int child) const;

protected:
    QComboBox *comboBox() const;
};
#endif // QT_NO_COMBOBOX

#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE

#endif // COMPLEXWIDGETS_H
