/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include "qtgradientviewdialog.h"
#include "qtgradientmanager.h"
#include <QtGui/QPushButton>

QT_BEGIN_NAMESPACE

QtGradientViewDialog::QtGradientViewDialog(QWidget *parent)
    : QDialog(parent)
{
    m_ui.setupUi(this);
    m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(m_ui.gradientView, SIGNAL(currentGradientChanged(QString)),
            this, SLOT(slotGradientSelected(QString)));
    connect(m_ui.gradientView, SIGNAL(gradientActivated(QString)),
            this, SLOT(slotGradientActivated(QString)));
}

void QtGradientViewDialog::setGradientManager(QtGradientManager *manager)
{
    m_ui.gradientView->setGradientManager(manager);
}

QGradient QtGradientViewDialog::getGradient(bool *ok, QtGradientManager *manager, QWidget *parent, const QString &caption)
{
    QtGradientViewDialog dlg(parent);
    dlg.setGradientManager(manager);
    dlg.setWindowTitle(caption);
    QGradient grad = QLinearGradient();
    const int res = dlg.exec();
    if (res == QDialog::Accepted)
        grad = dlg.m_ui.gradientView->gradientManager()->gradients().value(dlg.m_ui.gradientView->currentGradient());
    if (ok)
        *ok = res == QDialog::Accepted;
    return grad;
}

void QtGradientViewDialog::slotGradientSelected(const QString &id)
{
    m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!id.isEmpty());
}

void QtGradientViewDialog::slotGradientActivated(const QString &id)
{
    Q_UNUSED(id)
    accept();
}

QT_END_NAMESPACE
