/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
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

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

#include "qmenu.h"
#include "qmenubar.h"
#include "qmenubar_p.h"
#include "qmacnativewidget_mac.h"

#include <QtCore/QDebug>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <qpa/qplatformnativeinterface.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_MENU

namespace {
// TODO use QtMacExtras copy of this function when available.
inline QPlatformNativeInterface::NativeResourceForIntegrationFunction resolvePlatformFunction(const QByteArray &functionName)
{
    QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface();
    QPlatformNativeInterface::NativeResourceForIntegrationFunction function =
        nativeInterface->nativeResourceFunctionForIntegration(functionName);
    if (!function)
         qWarning() << "Qt could not resolve function" << functionName
                    << "from QGuiApplication::platformNativeInterface()->nativeResourceFunctionForIntegration()";
    return function;
}
} //namespsace


/*!
    \since 5.2

    Returns the native NSMenu for this menu. Available on OS X only.

    \note Qt sets the delegate on the native menu. If you need to set your own
    delegate, make sure you save the original one and forward any calls to it.
*/
NSMenu *QMenu::toNSMenu()
{
    // Call into the cocoa platform plugin: qMenuToNSMenu(platformMenu())
    QPlatformNativeInterface::NativeResourceForIntegrationFunction function = resolvePlatformFunction("qmenutonsmenu");
    if (function) {
        typedef void* (*QMenuToNSMenuFunction)(QPlatformMenu *platformMenu);
        return reinterpret_cast<NSMenu *>(reinterpret_cast<QMenuToNSMenuFunction>(function)(platformMenu()));
    }
    return nil;
}


/*!
    \since 5.2

    Set this menu to be the dock menu available by option-clicking
    on the application dock icon. Available on OS X only.
*/
void QMenu::setAsDockMenu()
{
    // Call into the cocoa platform plugin: setDockMenu(platformMenu())
    QPlatformNativeInterface::NativeResourceForIntegrationFunction function = resolvePlatformFunction("setdockmenu");
    if (function) {
        typedef void (*SetDockMenuFunction)(QPlatformMenu *platformMenu);
        reinterpret_cast<SetDockMenuFunction>(function)(platformMenu());
    }
}


/*! \fn void qt_mac_set_dock_menu(QMenu *menu)
    \relates QMenu
    \deprecated

    Sets this \a menu to be the dock menu available by option-clicking
    on the application dock icon. Available on OS X only.

    Deprecated; use \l QMenu::setAsDockMenu() instead.
*/

void QMenuPrivate::moveWidgetToPlatformItem(QWidget *widget, QPlatformMenuItem* item)
{
    QMacNativeWidget *container = new QMacNativeWidget;
    QObject::connect(platformMenu, SIGNAL(destroyed()), container, SLOT(deleteLater()));
    container->resize(widget->sizeHint());
    widget->setParent(container);
    widget->setVisible(true);

    NSView *containerView = container->nativeView();
    QWindow *containerWindow = container->windowHandle();
    Qt::WindowFlags wf = containerWindow->flags();
    containerWindow->setFlags(wf | Qt::SubWindow);
    [(NSView *)widget->winId() setAutoresizingMask:NSViewWidthSizable];

    item->setNativeContents((WId)containerView);
    container->show();
}

#endif //QT_NO_MENU

#ifndef QT_NO_MENUBAR

/*!
    \since 5.2

    Returns the native NSMenu for this menu bar. Available on OS X only.

    \note Qt may set the delegate on the native menu bar. If you need to set your
    own delegate, make sure you save the original one and forward any calls to it.
*/
NSMenu *QMenuBar::toNSMenu()
{
    // Call into the cocoa platform plugin: qMenuBarToNSMenu(platformMenuBar())
    QPlatformNativeInterface::NativeResourceForIntegrationFunction function = resolvePlatformFunction("qmenubartonsmenu");
    if (function) {
        typedef void* (*QMenuBarToNSMenuFunction)(QPlatformMenuBar *platformMenuBar);
        return reinterpret_cast<NSMenu *>(reinterpret_cast<QMenuBarToNSMenuFunction>(function)(platformMenuBar()));
    }
    return nil;
}
#endif //QT_NO_MENUBAR

QT_END_NAMESPACE

