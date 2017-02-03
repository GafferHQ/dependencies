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

#ifndef QACCESSIBLEMENU_H
#define QACCESSIBLEMENU_H

#include <QtGui/qaccessiblewidget.h>
#include <QtGui/qaccessible2.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_ACCESSIBILITY

#ifndef QT_NO_MENU
class QMenu;
class QMenuBar;
class QAction;

class QAccessibleMenu : public QAccessibleWidgetEx
{
public:
    explicit QAccessibleMenu(QWidget *w);

    int childCount() const;
    int childAt(int x, int y) const;

    QRect rect(int child) const;
    QString text(Text t, int child) const;
    Role role(int child) const;
    State state(int child) const;
    int navigate(RelationFlag relation, int entry, QAccessibleInterface **target) const;
    int indexOfChild( const QAccessibleInterface *child ) const;

    QString actionText(int action, QAccessible::Text text, int child) const;
    bool doAction(int action, int child, const QVariantList &params);

protected:
    QMenu *menu() const;
};

#ifndef QT_NO_MENUBAR
class QAccessibleMenuBar : public QAccessibleWidgetEx
{
public:
    explicit QAccessibleMenuBar(QWidget *w);

    int childCount() const;
    int childAt(int x, int y) const;

    QRect rect(int child) const;
    QString text(Text t, int child) const;
    Role role(int child) const;
    State state(int child) const;
    int navigate(RelationFlag relation, int entry, QAccessibleInterface **target) const;
    int indexOfChild( const QAccessibleInterface *child ) const;

    QString actionText(int action, QAccessible::Text text, int child) const;
    bool doAction(int action, int child, const QVariantList &params);

protected:
    QMenuBar *menuBar() const;
};
#endif // QT_NO_MENUBAR



class QAccessibleMenuItem : public QAccessibleActionInterface, public QAccessibleInterfaceEx
{
    Q_ACCESSIBLE_OBJECT
public:
    explicit QAccessibleMenuItem(QWidget *owner, QAction *w);

    virtual ~QAccessibleMenuItem();
    virtual QString actionText ( int action, Text t, int child ) const;
    virtual int childAt ( int x, int y ) const;
    virtual int childCount () const;
    virtual bool doAction ( int action, int child, const QVariantList & params = QVariantList() );
    virtual int indexOfChild ( const QAccessibleInterface * child ) const;
    virtual bool isValid () const;
    virtual int navigate ( RelationFlag relation, int entry, QAccessibleInterface ** target ) const;
    virtual QObject * object () const;
    virtual QRect rect ( int child ) const;
    virtual Relation relationTo ( int child, const QAccessibleInterface * other, int otherChild ) const;
    virtual Role role ( int child ) const;
    virtual void setText ( Text t, int child, const QString & text );
    virtual State state ( int child ) const;
    virtual QString text ( Text t, int child ) const;
    virtual int userActionCount ( int child ) const;

    // action interface
    virtual int actionCount();
    virtual void doAction(int actionIndex);
    virtual QString description(int);
    virtual QString name(int);
    virtual QString localizedName(int);
    virtual QStringList keyBindings(int);

    virtual QVariant invokeMethodEx(Method, int, const QVariantList &);

    QWidget *owner() const;
protected:
    QAction *action() const;
private:
    QAction *m_action;
    QWidget *m_owner; // can hold either QMenu or the QMenuBar that contains the action
};

#endif // QT_NO_MENU

QT_END_NAMESPACE
#endif // QT_NO_ACCESSIBILITY
#endif // QACCESSIBLEMENU_H
