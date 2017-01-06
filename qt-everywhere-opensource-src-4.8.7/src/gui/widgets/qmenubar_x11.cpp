/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qmenubar_x11_p.h"

#ifndef QT_NO_MENUBAR

#include "qapplication.h"
#include "qdebug.h"
#include "qevent.h"
#include "qmenu.h"
#include "qmenubar.h"

#include <private/qfactoryloader_p.h>

QT_BEGIN_NAMESPACE

QX11MenuBar::~QX11MenuBar()
{
}

void QX11MenuBar::init(QMenuBar *_menuBar)
{
    nativeMenuBar = -1;
    menuBar = _menuBar;
}

void QX11MenuBar::setVisible(bool visible)
{
    menuBar->QWidget::setVisible(visible);
}

void QX11MenuBar::actionEvent(QActionEvent *e)
{
    Q_UNUSED(e);
}

void QX11MenuBar::handleReparent(QWidget *oldParent, QWidget *newParent, QWidget *oldWindow, QWidget *newWindow)
{
    Q_UNUSED(oldParent)
    Q_UNUSED(newParent)
    Q_UNUSED(oldWindow)
    Q_UNUSED(newWindow)
}

bool QX11MenuBar::allowCornerWidgets() const
{
    return true;
}

void QX11MenuBar::popupAction(QAction *)
{
}

void QX11MenuBar::setNativeMenuBar(bool value)
{
    if (nativeMenuBar == -1 || (value != bool(nativeMenuBar))) {
        nativeMenuBar = value;
    }
}

bool QX11MenuBar::isNativeMenuBar() const
{
    return false;
}

bool QX11MenuBar::shortcutsHandledByNativeMenuBar() const
{
    return false;
}

bool QX11MenuBar::menuBarEventFilter(QObject *, QEvent *)
{
    return false;
}

struct QX11MenuBarFactory : public QPlatformMenuBarFactoryInterface
{
    QAbstractPlatformMenuBar *create() { return new QX11MenuBar; }
    virtual QStringList keys() const { return QStringList(); }
};

QPlatformMenuBarFactoryInterface *qt_guiPlatformMenuBarFactory()
{
    static QPlatformMenuBarFactoryInterface *factory = 0;
    if (!factory) {
#ifndef QT_NO_LIBRARY
        QFactoryLoader loader(QPlatformMenuBarFactoryInterface_iid, QLatin1String("/menubar"));
        factory = qobject_cast<QPlatformMenuBarFactoryInterface *>(loader.instance(QLatin1String("default")));
#endif // QT_NO_LIBRARY
        if(!factory) {
            static QX11MenuBarFactory def;
            factory = &def;
        }
    }
    return factory;
}

QT_END_NAMESPACE

#endif // QT_NO_MENUBAR
