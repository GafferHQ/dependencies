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

#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"
#include "qdesigner_appearanceoptions.h"

#include <QtDesigner/private/abstractoptionspage_p.h>

#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtGui/QFileDialog>
#include <QtGui/QPushButton>

QT_BEGIN_NAMESPACE

PreferencesDialog::PreferencesDialog(QDesignerFormEditorInterface *core, QWidget *parentWidget) :
    QDialog(parentWidget),
    m_ui(new Ui::PreferencesDialog()),
    m_core(core)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    m_ui->setupUi(this);

    m_optionsPages = core->optionsPages();

    m_ui->m_optionTabWidget->clear();
    foreach (QDesignerOptionsPageInterface *optionsPage, m_optionsPages) {
        QWidget *page = optionsPage->createPage(this);
        m_ui->m_optionTabWidget->addTab(page, optionsPage->name());
        if (QDesignerAppearanceOptionsWidget *appearanceWidget = qobject_cast<QDesignerAppearanceOptionsWidget *>(page))
            connect(appearanceWidget, SIGNAL(uiModeChanged(bool)), this, SLOT(slotUiModeChanged(bool)));
    }

    connect(m_ui->m_dialogButtonBox, SIGNAL(rejected()), this, SLOT(slotRejected()));
    connect(m_ui->m_dialogButtonBox, SIGNAL(accepted()), this, SLOT(slotAccepted()));
    connect(applyButton(), SIGNAL(clicked()), this, SLOT(slotApply()));
}

PreferencesDialog::~PreferencesDialog()
{
    delete m_ui;
}

QPushButton *PreferencesDialog::applyButton() const
{
    return m_ui->m_dialogButtonBox->button(QDialogButtonBox::Apply);
}

void PreferencesDialog::slotApply()
{
    foreach (QDesignerOptionsPageInterface *optionsPage, m_optionsPages)
        optionsPage->apply();
}

void PreferencesDialog::slotAccepted()
{
    slotApply();
    closeOptionPages();
    accept();
}

void PreferencesDialog::slotRejected()
{
    closeOptionPages();
    reject();
}

void PreferencesDialog::slotUiModeChanged(bool modified)
{
    // Cannot "apply" a ui mode change (destroy the dialogs parent)
    applyButton()->setEnabled(!modified);
}

void PreferencesDialog::closeOptionPages()
{
    foreach (QDesignerOptionsPageInterface *optionsPage, m_optionsPages)
        optionsPage->finish();
}

QT_END_NAMESPACE
