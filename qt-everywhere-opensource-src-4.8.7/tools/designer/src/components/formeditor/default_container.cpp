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

#include "default_container.h"
#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

template <class Container>
static inline void setCurrentContainerIndex(int index, Container *container)
{
    const bool blocked = container->signalsBlocked();
    container->blockSignals(true);
    container->setCurrentIndex(index);
    container->blockSignals(blocked);
}

static inline void ensureNoParent(QWidget *widget)
{
    if (widget->parentWidget())
        widget->setParent(0);
}

static const char *PageLabel = "Page";

namespace qdesigner_internal {

// --------- QStackedWidgetContainer
QStackedWidgetContainer::QStackedWidgetContainer(QStackedWidget *widget, QObject *parent) :
    QObject(parent),
    m_widget(widget)
{
}

void QStackedWidgetContainer::setCurrentIndex(int index)
{
    setCurrentContainerIndex(index, m_widget);
}

void QStackedWidgetContainer::addWidget(QWidget *widget)
{
    ensureNoParent(widget);
    m_widget->addWidget(widget);
}

void QStackedWidgetContainer::insertWidget(int index, QWidget *widget)
{
    ensureNoParent(widget);
    m_widget->insertWidget(index, widget);
}

void QStackedWidgetContainer::remove(int index)
{
    m_widget->removeWidget(widget(index));
}

// --------- QTabWidgetContainer
QTabWidgetContainer::QTabWidgetContainer(QTabWidget *widget, QObject *parent) :
    QObject(parent),
    m_widget(widget)
{
}

void QTabWidgetContainer::setCurrentIndex(int index)
{
    setCurrentContainerIndex(index, m_widget);
}

void QTabWidgetContainer::addWidget(QWidget *widget)
{
    ensureNoParent(widget);
    m_widget->addTab(widget, QString::fromUtf8(PageLabel));
}

void QTabWidgetContainer::insertWidget(int index, QWidget *widget)
{
    ensureNoParent(widget);
    m_widget->insertTab(index, widget, QString::fromUtf8(PageLabel));
}

void QTabWidgetContainer::remove(int index)
{
    m_widget->removeTab(index);
}

// ------------------- QToolBoxContainer
QToolBoxContainer::QToolBoxContainer(QToolBox *widget, QObject *parent) :
    QObject(parent),
    m_widget(widget)
{
}

void QToolBoxContainer::setCurrentIndex(int index)
{
    setCurrentContainerIndex(index, m_widget);
}

void QToolBoxContainer::addWidget(QWidget *widget)
{
    ensureNoParent(widget);
    m_widget->addItem(widget, QString::fromUtf8(PageLabel));
}

void QToolBoxContainer::insertWidget(int index, QWidget *widget)
{
    ensureNoParent(widget);
    m_widget->insertItem(index, widget, QString::fromUtf8(PageLabel));
}

void QToolBoxContainer::remove(int index)
{
    m_widget->removeItem(index);
}

// ------------------- QScrollAreaContainer
// We pass on active=true only if there are no children yet.
// If there are children, it is a legacy custom widget QScrollArea that has an internal,
// unmanaged child, in which case we deactivate the extension (otherwise we crash).
// The child will then not show up in the task menu

QScrollAreaContainer::QScrollAreaContainer(QScrollArea *widget, QObject *parent) :
    QObject(parent),
    SingleChildContainer<QScrollArea>(widget, widget->widget() == 0)
{
}
// ------------------- QDockWidgetContainer
QDockWidgetContainer::QDockWidgetContainer(QDockWidget *widget, QObject *parent) :
    QObject(parent),
    SingleChildContainer<QDockWidget>(widget)
{
}

}

QT_END_NAMESPACE
