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

#include "listwidget_taskmenu.h"
#include "listwidgeteditor.h"
#include "qdesigner_utils_p.h"
#include <qdesigner_command_p.h>

#include <QtDesigner/QDesignerFormWindowInterface>

#include <QtGui/QAction>
#include <QtGui/QStyle>
#include <QtGui/QLineEdit>
#include <QtGui/QStyleOption>

#include <QtCore/QEvent>
#include <QtCore/QVariant>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

using namespace qdesigner_internal;

ListWidgetTaskMenu::ListWidgetTaskMenu(QListWidget *button, QObject *parent)
    : QDesignerTaskMenu(button, parent),
      m_listWidget(button)
{
    m_editItemsAction = new QAction(this);
    m_editItemsAction->setText(tr("Edit Items..."));
    connect(m_editItemsAction, SIGNAL(triggered()), this, SLOT(editItems()));
    m_taskActions.append(m_editItemsAction);

    QAction *sep = new QAction(this);
    sep->setSeparator(true);
    m_taskActions.append(sep);
}

ListWidgetTaskMenu::~ListWidgetTaskMenu()
{
}

QAction *ListWidgetTaskMenu::preferredEditAction() const
{
    return m_editItemsAction;
}

QList<QAction*> ListWidgetTaskMenu::taskActions() const
{
    return m_taskActions + QDesignerTaskMenu::taskActions();
}

void ListWidgetTaskMenu::editItems()
{
    m_formWindow = QDesignerFormWindowInterface::findFormWindow(m_listWidget);
    if (m_formWindow.isNull())
        return;

    Q_ASSERT(m_listWidget != 0);

    ListWidgetEditor dlg(m_formWindow, m_listWidget->window());
    ListContents oldItems = dlg.fillContentsFromListWidget(m_listWidget);
    if (dlg.exec() == QDialog::Accepted) {
        ListContents items = dlg.contents();
        if (items != oldItems) {
            ChangeListContentsCommand *cmd = new ChangeListContentsCommand(m_formWindow);
            cmd->init(m_listWidget, oldItems, items);
            cmd->setText(tr("Change List Contents"));
            m_formWindow->commandHistory()->push(cmd);
        }
    }
}

void ListWidgetTaskMenu::updateSelection()
{
    if (m_editor)
        m_editor->deleteLater();
}

QT_END_NAMESPACE
