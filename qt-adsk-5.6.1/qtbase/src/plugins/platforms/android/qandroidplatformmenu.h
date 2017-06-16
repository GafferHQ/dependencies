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

#ifndef QANDROIDPLATFORMMENU_H
#define QANDROIDPLATFORMMENU_H

#include <qpa/qplatformmenu.h>
#include <qvector.h>
#include <qmutex.h>

QT_BEGIN_NAMESPACE

class QAndroidPlatformMenuItem;
class QAndroidPlatformMenu: public QPlatformMenu
{
public:
    typedef QVector<QAndroidPlatformMenuItem *> PlatformMenuItemsType;

public:
    QAndroidPlatformMenu();
    ~QAndroidPlatformMenu();

    void insertMenuItem(QPlatformMenuItem *menuItem, QPlatformMenuItem *before);
    void removeMenuItem(QPlatformMenuItem *menuItem);
    void syncMenuItem(QPlatformMenuItem *menuItem);
    void syncSeparatorsCollapsible(bool enable);

    void setTag(quintptr tag);
    quintptr tag() const;
    void setText(const QString &text);
    QString text() const;
    void setIcon(const QIcon &icon);
    QIcon icon() const;
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setVisible(bool visible);
    bool isVisible() const;
    void showPopup(const QWindow *parentWindow, const QRect &targetRect, const QPlatformMenuItem *item);

    QPlatformMenuItem *menuItemAt(int position) const;
    QPlatformMenuItem *menuItemForTag(quintptr tag) const;

    PlatformMenuItemsType menuItems() const;
    QMutex *menuItemsMutex();

private:
    PlatformMenuItemsType m_menuItems;
    quintptr m_tag;
    QString m_text;
    QIcon m_icon;
    bool m_enabled;
    bool m_isVisible;
    QMutex m_menuItemsMutex;
};

QT_END_NAMESPACE

#endif // QANDROIDPLATFORMMENU_H
