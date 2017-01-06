/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtGui>

#include "mysortfilterproxymodel.h"
#include "window.h"

//! [0]
Window::Window()
{
    proxyModel = new MySortFilterProxyModel(this);
    proxyModel->setDynamicSortFilter(true);
    //! [0]

    //! [1]
    sourceView = new QTreeView;
    sourceView->setRootIsDecorated(false);
    sourceView->setAlternatingRowColors(true);
    //! [1]

    QHBoxLayout *sourceLayout = new QHBoxLayout;
    //! [2]
    sourceLayout->addWidget(sourceView);
#if defined(Q_OS_SYMBIAN) || defined(Q_WS_MAEMO_5) || defined(Q_WS_SIMULATOR)
    sourceWidget = new QWidget;
    sourceWidget->setLayout(sourceLayout);
#else
    sourceGroupBox = new QGroupBox(tr("Original Model"));
    sourceGroupBox->setLayout(sourceLayout);
#endif
    //! [2]

    //! [3]
    filterCaseSensitivityCheckBox = new QCheckBox(tr("Case sensitive filter"));
    filterCaseSensitivityCheckBox->setChecked(true);

    filterPatternLineEdit = new QLineEdit;
    filterPatternLineEdit->setText("Grace|Sports");

    filterPatternLabel = new QLabel(tr("&Filter pattern:"));
    filterPatternLabel->setBuddy(filterPatternLineEdit);

    filterSyntaxComboBox = new QComboBox;
    filterSyntaxComboBox->addItem(tr("Regular expression"), QRegExp::RegExp);
    filterSyntaxComboBox->addItem(tr("Wildcard"), QRegExp::Wildcard);
    filterSyntaxComboBox->addItem(tr("Fixed string"), QRegExp::FixedString);

    fromDateEdit = new QDateEdit;
    fromDateEdit->setDate(QDate(1970, 01, 01));
    fromLabel = new QLabel(tr("F&rom:"));
    fromLabel->setBuddy(fromDateEdit);

    toDateEdit = new QDateEdit;
    toDateEdit->setDate(QDate(2099, 12, 31));
    toLabel = new QLabel(tr("&To:"));
    toLabel->setBuddy(toDateEdit);

    connect(filterPatternLineEdit, SIGNAL(textChanged(QString)),
            this, SLOT(textFilterChanged()));
    connect(filterSyntaxComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(textFilterChanged()));
    connect(filterCaseSensitivityCheckBox, SIGNAL(toggled(bool)),
            this, SLOT(textFilterChanged()));
    connect(fromDateEdit, SIGNAL(dateChanged(QDate)),
            this, SLOT(dateFilterChanged()));
    connect(toDateEdit, SIGNAL(dateChanged(QDate)),
            //! [3] //! [4]
            this, SLOT(dateFilterChanged()));
    //! [4]

    //! [5]
    proxyView = new QTreeView;
    proxyView->setRootIsDecorated(false);
    proxyView->setAlternatingRowColors(true);
    proxyView->setModel(proxyModel);
    proxyView->setSortingEnabled(true);
    proxyView->sortByColumn(1, Qt::AscendingOrder);

#if defined(Q_OS_SYMBIAN) || defined(Q_WS_MAEMO_5) || defined(Q_WS_SIMULATOR)
    QGridLayout *filterLayout = new QGridLayout;
    filterLayout->addWidget(filterPatternLabel, 0, 0);
    filterLayout->addWidget(filterPatternLineEdit, 0, 1);
    filterLayout->addWidget(filterSyntaxComboBox, 0, 2);
    filterLayout->addWidget(filterCaseSensitivityCheckBox, 1, 0, 1, 3);
    filterLayout->addWidget(fromLabel, 2, 0);
    filterLayout->addWidget(fromDateEdit, 2, 1, 1, 2);
    filterLayout->addWidget(toLabel, 3, 0);
    filterLayout->addWidget(toDateEdit, 3, 1, 1, 2);

    filterWidget = new QWidget;
    filterWidget->setLayout(filterLayout);

    QHBoxLayout *proxyLayout = new QHBoxLayout;
    proxyLayout->addWidget(proxyView);

    proxyWidget = new QWidget;
    proxyWidget->setLayout(proxyLayout);
#else
    QGridLayout *proxyLayout = new QGridLayout;
    proxyLayout->addWidget(proxyView, 0, 0, 1, 3);
    proxyLayout->addWidget(filterPatternLabel, 1, 0);
    proxyLayout->addWidget(filterPatternLineEdit, 1, 1);
    proxyLayout->addWidget(filterSyntaxComboBox, 1, 2);
    proxyLayout->addWidget(filterCaseSensitivityCheckBox, 2, 0, 1, 3);
    proxyLayout->addWidget(fromLabel, 3, 0);
    proxyLayout->addWidget(fromDateEdit, 3, 1, 1, 2);
    proxyLayout->addWidget(toLabel, 4, 0);
    proxyLayout->addWidget(toDateEdit, 4, 1, 1, 2);

    proxyGroupBox = new QGroupBox(tr("Sorted/Filtered Model"));
    proxyGroupBox->setLayout(proxyLayout);
#endif
    //! [5]

    //! [6]
#if defined(Q_OS_SYMBIAN) || defined(Q_WS_MAEMO_5) || defined(Q_WS_SIMULATOR)
    QTabWidget *tabWidget = new QTabWidget;
    tabWidget->addTab(sourceWidget, "Original");
    tabWidget->addTab(filterWidget, "Filters");
    tabWidget->addTab(proxyWidget, "Sorted");

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(tabWidget);
    setLayout(mainLayout);
    setWindowTitle(tr("Custom Model"));
#else
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(sourceGroupBox);
    mainLayout->addWidget(proxyGroupBox);
    setLayout(mainLayout);

    setWindowTitle(tr("Custom Sort/Filter Model"));
    resize(500, 450);
#endif
}
//! [6]

//! [7]
void Window::setSourceModel(QAbstractItemModel *model)
{
    proxyModel->setSourceModel(model);
    sourceView->setModel(model);
}
//! [7]

//! [8]
void Window::textFilterChanged()
{
    QRegExp::PatternSyntax syntax =
            QRegExp::PatternSyntax(filterSyntaxComboBox->itemData(
                    filterSyntaxComboBox->currentIndex()).toInt());
    Qt::CaseSensitivity caseSensitivity =
            filterCaseSensitivityCheckBox->isChecked() ? Qt::CaseSensitive
                : Qt::CaseInsensitive;

    QRegExp regExp(filterPatternLineEdit->text(), caseSensitivity, syntax);
    proxyModel->setFilterRegExp(regExp);
}
//! [8]

//! [9]
void Window::dateFilterChanged()
{
    proxyModel->setFilterMinimumDate(fromDateEdit->date());
    proxyModel->setFilterMaximumDate(toDateEdit->date());
}
//! [9]
