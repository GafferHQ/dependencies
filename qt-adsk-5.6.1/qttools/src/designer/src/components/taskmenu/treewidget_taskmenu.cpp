/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "treewidget_taskmenu.h"
#include "treewidgeteditor.h"

#include <QtDesigner/QDesignerFormWindowInterface>

#include <QtWidgets/QAction>
#include <QtWidgets/QStyle>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyleOption>

#include <QtCore/QEvent>
#include <QtCore/QVariant>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

using namespace qdesigner_internal;

TreeWidgetTaskMenu::TreeWidgetTaskMenu(QTreeWidget *button, QObject *parent)
    : QDesignerTaskMenu(button, parent),
      m_treeWidget(button),
      m_editItemsAction(new QAction(tr("Edit Items..."), this))
{
    connect(m_editItemsAction, &QAction::triggered, this, &TreeWidgetTaskMenu::editItems);
    m_taskActions.append(m_editItemsAction);

    QAction *sep = new QAction(this);
    sep->setSeparator(true);
    m_taskActions.append(sep);
}


TreeWidgetTaskMenu::~TreeWidgetTaskMenu()
{
}

QAction *TreeWidgetTaskMenu::preferredEditAction() const
{
    return m_editItemsAction;
}

QList<QAction*> TreeWidgetTaskMenu::taskActions() const
{
    return m_taskActions + QDesignerTaskMenu::taskActions();
}

void TreeWidgetTaskMenu::editItems()
{
    m_formWindow = QDesignerFormWindowInterface::findFormWindow(m_treeWidget);
    if (m_formWindow.isNull())
        return;

    Q_ASSERT(m_treeWidget != 0);

    TreeWidgetEditorDialog dlg(m_formWindow, m_treeWidget->window());
    TreeWidgetContents oldCont = dlg.fillContentsFromTreeWidget(m_treeWidget);
    if (dlg.exec() == QDialog::Accepted) {
        TreeWidgetContents newCont = dlg.contents();
        if (newCont != oldCont) {
            ChangeTreeContentsCommand *cmd = new ChangeTreeContentsCommand(m_formWindow);
            cmd->init(m_treeWidget, oldCont, newCont);
            m_formWindow->commandHistory()->push(cmd);
        }
    }
}

void TreeWidgetTaskMenu::updateSelection()
{
    if (m_editor)
        m_editor->deleteLater();
}

QT_END_NAMESPACE
