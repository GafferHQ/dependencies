/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QIOSMENU_H
#define QIOSMENU_H

#import <UIKit/UIKit.h>

#include <QtCore/QtCore>
#include <qpa/qplatformmenu.h>

#import "quiview.h"

class QIOSMenu;
@class QUIMenuController;
@class QUIPickerView;

class QIOSMenuItem : public QPlatformMenuItem
{
public:
    QIOSMenuItem();

    void setTag(quintptr tag) Q_DECL_OVERRIDE;
    quintptr tag()const Q_DECL_OVERRIDE;

    void setText(const QString &text) Q_DECL_OVERRIDE;
    void setIcon(const QIcon &) Q_DECL_OVERRIDE {}
    void setMenu(QPlatformMenu *) Q_DECL_OVERRIDE;
    void setVisible(bool isVisible) Q_DECL_OVERRIDE;
    void setIsSeparator(bool) Q_DECL_OVERRIDE;
    void setFont(const QFont &) Q_DECL_OVERRIDE {}
    void setRole(MenuRole role) Q_DECL_OVERRIDE;
    void setCheckable(bool) Q_DECL_OVERRIDE {}
    void setChecked(bool) Q_DECL_OVERRIDE {}
    void setShortcut(const QKeySequence&) Q_DECL_OVERRIDE;
    void setEnabled(bool enabled) Q_DECL_OVERRIDE;
    void setIconSize(int) Q_DECL_OVERRIDE {}

    quintptr m_tag;
    bool m_visible;
    QString m_text;
    MenuRole m_role;
    bool m_enabled;
    bool m_separator;
    QIOSMenu *m_menu;
    QKeySequence m_shortcut;

private:
    QString removeMnemonics(const QString &original);
};

typedef QList<QIOSMenuItem *> QIOSMenuItemList;

class QIOSMenu : public QPlatformMenu
{
public:
    QIOSMenu();
    ~QIOSMenu();

    void insertMenuItem(QPlatformMenuItem *menuItem, QPlatformMenuItem *before) Q_DECL_OVERRIDE;
    void removeMenuItem(QPlatformMenuItem *menuItem) Q_DECL_OVERRIDE;
    void syncMenuItem(QPlatformMenuItem *) Q_DECL_OVERRIDE;
    void syncSeparatorsCollapsible(bool) Q_DECL_OVERRIDE {}

    void setTag(quintptr tag) Q_DECL_OVERRIDE;
    quintptr tag()const Q_DECL_OVERRIDE;

    void setText(const QString &) Q_DECL_OVERRIDE;
    void setIcon(const QIcon &) Q_DECL_OVERRIDE {}
    void setEnabled(bool enabled) Q_DECL_OVERRIDE;
    void setVisible(bool visible) Q_DECL_OVERRIDE;
    void setMenuType(MenuType type) Q_DECL_OVERRIDE;

    void showPopup(const QWindow *parentWindow, const QRect &targetRect, const QPlatformMenuItem *item) Q_DECL_OVERRIDE;
    void dismiss() Q_DECL_OVERRIDE;

    QPlatformMenuItem *menuItemAt(int position) const Q_DECL_OVERRIDE;
    QPlatformMenuItem *menuItemForTag(quintptr tag) const Q_DECL_OVERRIDE;

    void handleItemSelected(QIOSMenuItem *menuItem);

    static QIOSMenu *currentMenu() { return m_currentMenu; }
    static id menuActionTarget() { return m_currentMenu ? m_currentMenu->m_menuController : 0; }

protected:
    bool eventFilter(QObject *obj, QEvent *event) Q_DECL_OVERRIDE;

private:
    quintptr m_tag;
    bool m_enabled;
    bool m_visible;
    QString m_text;
    MenuType m_menuType;
    MenuType m_effectiveMenuType;
    QPointer<QWindow> m_parentWindow;
    QRect m_targetRect;
    const QIOSMenuItem *m_targetItem;
    QUIMenuController *m_menuController;
    QUIPickerView *m_pickerView;
    QIOSMenuItemList m_menuItems;

    static QIOSMenu *m_currentMenu;

    void updateVisibility();
    void toggleShowUsingUIMenuController(bool show);
    void toggleShowUsingUIPickerView(bool show);
    QIOSMenuItemList visibleMenuItems() const;
    QIOSMenuItemList filterFirstResponderActions(const QIOSMenuItemList &menuItems);
    void repositionMenu();
};

#endif // QIOSMENU_H
