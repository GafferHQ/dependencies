/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#ifndef UI_DELEGATES_MANAGER_H
#define UI_DELEGATES_MANAGER_H

#include "qglobal.h"
#include "web_contents_adapter.h"
#include "web_contents_adapter_client.h"

#include <QCoreApplication>
#include <QExplicitlySharedDataPointer>
#include <QPoint>
#include <QQmlComponent>
#include <QSharedPointer>
#include <QUrl>

#define FOR_EACH_COMPONENT_TYPE(F, SEPARATOR) \
    F(Menu, menu) SEPARATOR \
    F(MenuItem, menuItem) SEPARATOR \
    F(MenuSeparator, menuSeparator) SEPARATOR \
    F(AlertDialog, alertDialog) SEPARATOR \
    F(ConfirmDialog, confirmDialog) SEPARATOR \
    F(PromptDialog, promptDialog) SEPARATOR \
    F(FilePicker, filePicker) SEPARATOR \
    F(MessageBubble, messageBubble) SEPARATOR \
    F(AuthenticationDialog, authenticationDialog) SEPARATOR

#define COMMA_SEPARATOR ,
#define SEMICOLON_SEPARATOR ;
#define ENUM_DECLARATION(TYPE, COMPONENT) \
    TYPE
#define MEMBER_DECLARATION(TYPE, COMPONENT) \
    QQmlComponent *COMPONENT##Component

QT_BEGIN_NAMESPACE
class QObject;
class QQmlContext;
class QQuickItem;
class QQuickWebEngineView;
QT_END_NAMESPACE

namespace QtWebEngineCore {
class AuthenticationDialogController;
class JavaScriptDialogController;
class FilePickerController;

const char *defaultPropertyName(QObject *obj);

class MenuItemHandler : public QObject {
Q_OBJECT
public:
    MenuItemHandler(QObject *parent);

Q_SIGNALS:
    void triggered();
};

class UIDelegatesManager
{
    Q_DECLARE_TR_FUNCTIONS(UIDelegatesManager)
public:
    enum ComponentType {
        Invalid = -1,
        FOR_EACH_COMPONENT_TYPE(ENUM_DECLARATION, COMMA_SEPARATOR)
        ComponentTypeCount
    };

    UIDelegatesManager(QQuickWebEngineView *);

    void addMenuItem(MenuItemHandler *menuItemHandler, const QString &text, const QString &iconName = QString(), bool enabled = true);
    void addMenuSeparator(QObject *menu);
    QObject *addMenu(QObject *parentMenu, const QString &title, const QPoint &pos = QPoint());
    QQmlContext *creationContextForComponent(QQmlComponent *);
    void showDialog(QSharedPointer<JavaScriptDialogController>);
    void showDialog(QSharedPointer<AuthenticationDialogController>);
    void showFilePicker(FilePickerController *controller);
    void showMessageBubble(const QRect &anchor, const QString &mainText, const QString &subText);
    void hideMessageBubble();
    void moveMessageBubble(const QRect &anchor);

private:
    bool ensureComponentLoaded(ComponentType);

    QQuickWebEngineView *m_view;
    QScopedPointer<QQuickItem> m_messageBubbleItem;

    FOR_EACH_COMPONENT_TYPE(MEMBER_DECLARATION, SEMICOLON_SEPARATOR)

    Q_DISABLE_COPY(UIDelegatesManager)

};

} // namespace QtWebEngineCore

#endif // UI_DELEGATES_MANAGER_H
