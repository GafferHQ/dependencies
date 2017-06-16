/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include "mainwindow.h"

#include "qdbusviewer.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMenu>
#include <QtWidgets/QAction>
#include <QtWidgets/QMessageBox>

#include <QtDBus/QDBusConnection>
#include <QtCore/QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *quitAction = fileMenu->addAction(tr("&Quit"), this, SLOT(close()));
    quitAction->setShortcut(QKeySequence::Quit);
    quitAction->setMenuRole(QAction::QuitRole);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutAction = helpMenu->addAction(tr("&About"));
    aboutAction->setMenuRole(QAction::AboutRole);
    QObject::connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));

    QAction *aboutQtAction = helpMenu->addAction(tr("About &Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    QObject::connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

    tabWidget = new QTabWidget;
    setCentralWidget(tabWidget);

    sessionBusViewer = new QDBusViewer(QDBusConnection::sessionBus());
    systemBusViewer = new QDBusViewer(QDBusConnection::systemBus());
    tabWidget->addTab(sessionBusViewer, tr("Session Bus"));
    tabWidget->addTab(systemBusViewer, tr("System Bus"));

    restoreSettings();
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::addCustomBusTab(const QString &busAddress)
{
    QDBusConnection connection = QDBusConnection::connectToBus(busAddress, "QDBusViewer");
    if (connection.isConnected()) {
        QDBusViewer *customBusViewer = new QDBusViewer(connection);
        tabWidget->addTab(customBusViewer, tr("Custom Bus"));
    }
}

void MainWindow::about()
{
    QMessageBox box(this);

    box.setText(QString::fromLatin1("<center><img src=\":/qt-project.org/qdbusviewer/images/qdbusviewer-128.png\">"
                "<h3>%1</h3>"
                "<p>Version %2</p></center>"
                "<p>Copyright (C) %3 The Qt Company Ltd.</p>")
            .arg(tr("D-Bus Viewer"), QLatin1String(QT_VERSION_STR), QStringLiteral("2016")));
    box.setWindowTitle(tr("D-Bus Viewer"));
    box.exec();
}

static inline QString windowGeometryKey() { return QStringLiteral("WindowGeometry"); }
static inline QString sessionTabGroup() { return QStringLiteral("SessionTab"); }
static inline QString systemTabGroup() { return QStringLiteral("SystemTab"); }

void MainWindow::saveSettings()
{
    QSettings settings;

    settings.setValue(windowGeometryKey(), saveGeometry());

    settings.beginGroup(sessionTabGroup());
    sessionBusViewer->saveState(&settings);
    settings.endGroup();

    settings.beginGroup(systemTabGroup());
    systemBusViewer->saveState(&settings);
    settings.endGroup();
}

void MainWindow::restoreSettings()
{
    QSettings settings;

    restoreGeometry(settings.value(windowGeometryKey()).toByteArray());

    settings.beginGroup(sessionTabGroup());
    sessionBusViewer->restoreState(&settings);
    settings.endGroup();

    settings.beginGroup(systemTabGroup());
    systemBusViewer->restoreState(&settings);
    settings.endGroup();
}
