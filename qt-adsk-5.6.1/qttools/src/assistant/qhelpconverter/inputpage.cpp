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

#include <QtCore/QFile>
#include <QtCore/QVariant>

#include <QtWidgets/QLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QApplication>

#include "inputpage.h"
#include "adpreader.h"

QT_BEGIN_NAMESPACE

InputPage::InputPage(AdpReader *reader, QWidget *parent)
    : QWizardPage(parent)
{
    m_adpReader = reader;
    setTitle(tr("Input File"));
    setSubTitle(tr("Specify the .adp or .dcf file you want "
        "to convert to the new Qt help project format and/or "
        "collection format."));

    m_ui.setupUi(this);
    connect(m_ui.browseButton, SIGNAL(clicked()),
        this, SLOT(getFileName()));

    registerField(QLatin1String("adpFileName"), m_ui.fileLineEdit);
}

void InputPage::getFileName()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Open file"), QString(),
        tr("Qt Help Files (*.adp *.dcf)"));
    if (!f.isEmpty())
        m_ui.fileLineEdit->setText(f);
}

bool InputPage::validatePage()
{
    QFile f(m_ui.fileLineEdit->text().trimmed());
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("File Open Error"),
            tr("The specified file could not be opened!"));
        return false;
    }

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    m_adpReader->readData(f.readAll());
    QApplication::restoreOverrideCursor();
    if (m_adpReader->hasError()) {
        QMessageBox::critical(this, tr("File Parsing Error"),
            tr("Parsing error in line %1!").arg(m_adpReader->lineNumber()));
        return false;
    }

    return true;
}

QT_END_NAMESPACE
