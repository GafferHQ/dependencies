/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtGui/QMessageBox>

#include "outputpage.h"

QT_BEGIN_NAMESPACE

OutputPage::OutputPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Output File Names"));
    setSubTitle(tr("Specify the file names for the output files."));
    setButtonText(QWizard::NextButton, tr("Convert..."));

    m_ui.setupUi(this);
    connect(m_ui.projectLineEdit, SIGNAL(textChanged(QString)),
        this, SIGNAL(completeChanged()));
    connect(m_ui.collectionLineEdit, SIGNAL(textChanged(QString)),
        this, SIGNAL(completeChanged()));

    registerField(QLatin1String("ProjectFileName"),
        m_ui.projectLineEdit);
    registerField(QLatin1String("CollectionFileName"),
        m_ui.collectionLineEdit);
}

void OutputPage::setPath(const QString &path)
{
    m_path = path;
}

void OutputPage::setCollectionComponentEnabled(bool enabled)
{
    m_ui.collectionLineEdit->setEnabled(enabled);
    m_ui.label_2->setEnabled(enabled);
}

bool OutputPage::isComplete() const
{
    if (m_ui.projectLineEdit->text().isEmpty()
        || m_ui.collectionLineEdit->text().isEmpty())
        return false;
    return true;
}

bool OutputPage::validatePage()
{
    return checkFile(m_ui.projectLineEdit->text(),
        tr("Qt Help Project File"))
        && checkFile(m_ui.collectionLineEdit->text(),
        tr("Qt Help Collection Project File"));
}

bool OutputPage::checkFile(const QString &fileName, const QString &title)
{
    QFile fi(m_path + QDir::separator() + fileName);
    if (!fi.exists())
        return true;
    
    if (QMessageBox::warning(this, title,
        tr("The specified file %1 already exist.\n\nDo you want to remove it?")
        .arg(fileName), tr("Remove"), tr("Cancel")) == 0) {
        return fi.remove();
    }
    return false;
}

QT_END_NAMESPACE
