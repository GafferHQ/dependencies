/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
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

#include "testwidget.h"
#include "ui_testwidget.h"

#include <QApplication>
#include <QStyle>
#include <QFile>
#include <QFileDialog>
#include <QDesktopServices>
#include <QMessageBox>
#include <QProcess>
#include <QStatusBar>
#include <QUrl>
#include <QWinJumpList>
#include <QWinJumpListItem>
#include <QWinJumpListCategory>
#include <QDebug>

TestWidget::TestWidget(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TestWidget)
{
    ui->setupUi(this);

    connect(ui->actionUpdate, &QAction::triggered, this, &TestWidget::updateJumpList);
    connect(ui->actionOpen, &QAction::triggered, this, &TestWidget::openFile);
    connect(ui->actionExit, &QAction::triggered, QCoreApplication::quit);
    connect(ui->actionShow_in_Explorer, &QAction::triggered, this, &TestWidget::showInExplorer);
    connect(ui->actionRun_JumpListView, &QAction::triggered, this, &TestWidget::runJumpListView);
}

TestWidget::~TestWidget()
{
    delete ui;
}

QStringList TestWidget::supportedMimeTypes()
{
    return QStringList() << "text/x-c++src" << "text/x-csrc" << "text/x-chdr"
        << "text/x-c++hdr" << "text/x-qml" << "text/plain";
}

void TestWidget::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void TestWidget::showFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly|QIODevice::Text)) {
        const QString error = "Failed to open file " + QDir::toNativeSeparators(path)
            + ": " + file.errorString();
        QMessageBox::warning(this, "Error", error);
        return;
    }
    setText(QString::fromUtf8(file.readAll()));
}

void TestWidget::setText(const QString &text)
{
    ui->text->setPlainText(text);
}

void TestWidget::updateJumpList()
{
    QWinJumpList jumplist;
    if (!m_id.isEmpty())
        jumplist.setIdentifier(m_id);
    const QString applicationBinary = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    jumplist.recent()->setVisible(ui->cbShowRecent->isChecked());
    jumplist.frequent()->setVisible(ui->cbShowFrequent->isChecked());
    if (ui->cbRunFullscreen->isChecked()) {
        QWinJumpListItem *item = new QWinJumpListItem(QWinJumpListItem::Link);
        item->setTitle(ui->cbRunFullscreen->text());
        item->setFilePath(applicationBinary);
        item->setArguments(QStringList("-fullscreen"));
        item->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
        jumplist.tasks()->addItem(item);
    }
    if (ui->cbRunFusion->isChecked()) {
        jumplist.tasks()->addLink(style()->standardIcon(QStyle::SP_DesktopIcon),
                                  ui->cbRunFusion->text(),
                                  applicationBinary,
                                  (QStringList() << "-style" << "fusion"));
    }
    if (ui->cbRunText->isChecked()) {
        jumplist.tasks()->addSeparator();
        jumplist.tasks()->addLink(ui->cbRunText->text(),
                                  applicationBinary,
                                  QStringList("-text"));
    }
    jumplist.tasks()->setVisible(!jumplist.tasks()->isEmpty());
}

void TestWidget::openFile()
{
    QFileDialog fileDialog(this, "Open a Text File");
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setMimeTypeFilters(TestWidget::supportedMimeTypes());
    // Note: The native file dialog creates the frequent/recent entries.
    if (!ui->actionUse_Native_File_Dialog->isChecked())
        fileDialog.setOption(QFileDialog::DontUseNativeDialog);
    if (fileDialog.exec() == QDialog::Accepted)
        showFile(fileDialog.selectedFiles().first());
}

void TestWidget::showInExplorer()
{
    const QString path = QFile::decodeName(qgetenv("APPDATA"))
        + "/Microsoft/Windows/Recent/Automaticdestinations";
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void TestWidget::runJumpListView()
{
    const char binary[] = "JumpListsView";
    if (!QProcess::startDetached(binary))
        statusBar()->showMessage(QLatin1String("Unable to run ") + binary);
}
