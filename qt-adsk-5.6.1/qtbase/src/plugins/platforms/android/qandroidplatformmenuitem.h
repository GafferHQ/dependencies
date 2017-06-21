/****************************************************************************
**
** Copyright (C) 2012 BogDan Vatra <bogdan@kde.org>
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

#ifndef QANDROIDPLATFORMMENUITEM_H
#define QANDROIDPLATFORMMENUITEM_H
#include <qpa/qplatformmenu.h>

QT_BEGIN_NAMESPACE

class QAndroidPlatformMenu;

class QAndroidPlatformMenuItem: public QPlatformMenuItem
{
public:
    QAndroidPlatformMenuItem();
    void setTag(quintptr tag);
    quintptr tag() const;

    void setText(const QString &text);
    QString text() const;

    void setIcon(const QIcon &icon);
    QIcon icon() const;

    void setMenu(QPlatformMenu *menu);
    QAndroidPlatformMenu *menu() const;

    void setVisible(bool isVisible);
    bool isVisible() const;

    void setIsSeparator(bool isSeparator);
    bool isSeparator() const;

    void setFont(const QFont &font);

    void setRole(MenuRole role);
    MenuRole role() const;

    void setCheckable(bool checkable);
    bool isCheckable() const;

    void setChecked(bool isChecked);
    bool isChecked() const;

    void setShortcut(const QKeySequence &shortcut);

    void setEnabled(bool enabled);
    bool isEnabled() const;

    void setIconSize(int size);

private:
    quintptr m_tag;
    QString m_text;
    QIcon m_icon;
    QAndroidPlatformMenu *m_menu;
    bool m_isVisible;
    bool m_isSeparator;
    MenuRole m_role;
    bool m_isCheckable;
    bool m_isChecked;
    bool m_isEnabled;
};

QT_END_NAMESPACE

#endif // QANDROIDPLATFORMMENUITEM_H
