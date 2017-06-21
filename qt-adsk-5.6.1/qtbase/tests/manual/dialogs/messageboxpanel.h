/****************************************************************************
**
** Copyright (C) 2013 2013 Thorbjørn Lund Martsum - tmartsum[at]gmail.com
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef MESSAGEBOXPANEL_H
#define MESSAGEBOXPANEL_H

#include <QWidget>
#include <QCheckBox>

QT_BEGIN_NAMESPACE
class QComboBox;
class QCheckBox;
class QPushButton;
class QLineEdit;
class QValidator;
class QLabel;
class QMessageBox;
class QCheckBox;
QT_END_NAMESPACE

class MessageBoxPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MessageBoxPanel(QWidget *parent = 0);
    ~MessageBoxPanel();

public slots:
    void doExec();
    void doShowApply();

private:
    QComboBox *m_iconComboBox;
    QLineEdit *m_textInMsgBox;
    QLineEdit *m_informativeText;
    QLineEdit *m_detailedtext;
    QLineEdit *m_buttonsMask;
    QPushButton *m_btnExec;
    QPushButton *m_btnShowApply;
    QValidator *m_validator;
    QLabel *m_resultLabel;
    QCheckBox *m_chkReallocMsgBox;
    QLineEdit *m_checkboxText;
    QLabel *m_checkBoxResult;
    QMessageBox *m_msgbox;
    void setupMessageBox(QMessageBox &box);
};

#endif
