/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "q3mainwindow_container.h"

#include <Qt3Support/Q3MainWindow>

#include <QtCore/qdebug.h>
#include <QtGui/QToolBar>
#include <QtGui/QMenuBar>
#include <QtGui/QStatusBar>

#include <Qt3Support/Q3ToolBar>

QT_BEGIN_NAMESPACE

Q3MainWindowContainer::Q3MainWindowContainer(Q3MainWindow *widget, QObject *parent)
    : QObject(parent),
      m_mainWindow(widget)
{}

int Q3MainWindowContainer::count() const
{
    return m_widgets.count();
}

QWidget *Q3MainWindowContainer::widget(int index) const
{
    if (index == -1)
        return 0;

    return m_widgets.at(index);
}

int Q3MainWindowContainer::currentIndex() const
{
    return m_mainWindow->centralWidget() ? 0 : -1;
}

void Q3MainWindowContainer::setCurrentIndex(int index)
{
    Q_UNUSED(index);
}

void Q3MainWindowContainer::addWidget(QWidget *widget)
{
    if (qobject_cast<QToolBar*>(widget)) {
        m_widgets.append(widget);
    } else if (qobject_cast<Q3ToolBar*>(widget)) {
        m_widgets.append(widget);
    } else if (qobject_cast<QMenuBar*>(widget)) {
        (void) m_mainWindow->menuBar();
        m_widgets.append(widget);
    } else if (qobject_cast<QStatusBar*>(widget)) {
        (void) m_mainWindow->statusBar();
        m_widgets.append(widget);
    } else {
        Q_ASSERT(m_mainWindow->centralWidget() == 0);
        widget->setParent(m_mainWindow);
        m_mainWindow->setCentralWidget(widget);
        m_widgets.prepend(widget);
    }
}

void Q3MainWindowContainer::insertWidget(int index, QWidget *widget)
{
    m_widgets.insert(index, widget);
}

void Q3MainWindowContainer::remove(int index)
{
    m_widgets.removeAt(index);
}

Q3MainWindowContainerFactory::Q3MainWindowContainerFactory(QExtensionManager *parent)
    : QExtensionFactory(parent)
{
}

QObject *Q3MainWindowContainerFactory::createExtension(QObject *object, const QString &iid, QObject *parent) const
{
    if (iid != Q_TYPEID(QDesignerContainerExtension))
        return 0;

    if (Q3MainWindow *w = qobject_cast<Q3MainWindow*>(object))
        return new Q3MainWindowContainer(w, parent);

    return 0;
}


QT_END_NAMESPACE
