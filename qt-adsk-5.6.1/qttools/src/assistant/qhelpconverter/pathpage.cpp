/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#include <QtWidgets/QFileDialog>

#include "pathpage.h"

QT_BEGIN_NAMESPACE

PathPage::PathPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Source File Paths"));
    setSubTitle(tr("Specify the paths where the sources files "
        "are located. By default, all files in those directories "
        "matched by the file filter will be included."));

    m_ui.setupUi(this);
    connect(m_ui.addButton, SIGNAL(clicked()),
        this, SLOT(addPath()));
    connect(m_ui.removeButton, SIGNAL(clicked()),
        this, SLOT(removePath()));

    m_ui.filterLineEdit->setText(QLatin1String("*.html, *.htm, *.png, *.jpg, *.css"));

    registerField(QLatin1String("sourcePathList"), m_ui.pathListWidget);
    m_firstTime = true;
}

void PathPage::setPath(const QString &path)
{
    if (!m_firstTime)
        return;

    m_ui.pathListWidget->addItem(path);
    m_firstTime = false;
    m_ui.pathListWidget->setCurrentRow(0);
}

QStringList PathPage::paths() const
{
    QStringList lst;
    for (int i = 0; i<m_ui.pathListWidget->count(); ++i)
        lst.append(m_ui.pathListWidget->item(i)->text());
    return lst;
}

QStringList PathPage::filters() const
{
    QStringList lst;
    foreach (const QString &s, m_ui.filterLineEdit->text().split(QLatin1Char(','))) {
        lst.append(s.trimmed());
    }
    return lst;
}

void PathPage::addPath()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        tr("Source File Path"));
    if (!dir.isEmpty())
        m_ui.pathListWidget->addItem(dir);
}

void PathPage::removePath()
{
    QListWidgetItem *i = m_ui.pathListWidget
        ->takeItem(m_ui.pathListWidget->currentRow());
    delete i;
    if (!m_ui.pathListWidget->count())
        m_ui.removeButton->setEnabled(false);
}

QT_END_NAMESPACE
