/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

#include <QMainWindow>
#include <QApplication>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>

#include <QQmlError>
#include <QQuickView>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

private slots:
    void quickViewStatusChanged(QQuickView::Status);
    void sceneGraphError(QQuickWindow::SceneGraphError error, const QString &message);

private:
    QQuickView *m_quickView;
};

MainWindow::MainWindow()
    : m_quickView(new QQuickView)
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    m_quickView->setResizeMode(QQuickView::SizeRootObjectToView);
    connect(m_quickView, &QQuickView::statusChanged,
            this, &MainWindow::quickViewStatusChanged);
    connect(m_quickView, &QQuickWindow::sceneGraphError,
            this, &MainWindow::sceneGraphError);
    m_quickView->setSource(QUrl(QStringLiteral("qrc:///embeddedinwidgets/main.qml")));

    QWidget *container = QWidget::createWindowContainer(m_quickView);
    container->setMinimumSize(m_quickView->size());
    container->setFocusPolicy(Qt::TabFocus);

    layout->addWidget(new QLineEdit(QStringLiteral("A QLineEdit")));
    layout->addWidget(container);
    layout->addWidget(new QLineEdit(QStringLiteral("A QLineEdit")));
    setCentralWidget(centralWidget);

    QMenu *fileMenu = menuBar()->addMenu(tr("File"));
    fileMenu->addAction(tr("Quit"), qApp, &QCoreApplication::quit);
}

void MainWindow::quickViewStatusChanged(QQuickView::Status status)
{
    if (status == QQuickView::Error) {
        QStringList errors;
        foreach (const QQmlError &error, m_quickView->errors())
            errors.append(error.toString());
        statusBar()->showMessage(errors.join(QStringLiteral(", ")));
    }
}

void MainWindow::sceneGraphError(QQuickWindow::SceneGraphError, const QString &message)
{
    statusBar()->showMessage(message);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}

#include "main.moc"
