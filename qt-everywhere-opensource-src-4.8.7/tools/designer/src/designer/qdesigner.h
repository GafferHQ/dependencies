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

#ifndef QDESIGNER_H
#define QDESIGNER_H

#include <QtCore/QPointer>
#include <QtGui/QApplication>

QT_BEGIN_NAMESPACE

#define qDesigner \
    (static_cast<QDesigner*>(QCoreApplication::instance()))

class QDesignerWorkbench;
class QDesignerToolWindow;
class MainWindowBase;
class QDesignerServer;
class QDesignerClient;
class QErrorMessage;

class QDesigner: public QApplication
{
    Q_OBJECT
public:
    QDesigner(int &argc, char **argv);
    virtual ~QDesigner();

    QDesignerWorkbench *workbench() const;
    QDesignerServer *server() const;
    MainWindowBase *mainWindow() const;
    void setMainWindow(MainWindowBase *tw);

protected:
    bool event(QEvent *ev);

signals:
    void initialized();

public slots:
    void showErrorMessage(const char *message);

private slots:
    void initialize();
    void callCreateForm();

private:
    bool parseCommandLineArgs(QStringList &fileNames, QString &resourceDir);
    void showErrorMessageBox(const QString &);

    QDesignerServer *m_server;
    QDesignerClient *m_client;
    QDesignerWorkbench *m_workbench;
    QPointer<MainWindowBase> m_mainWindow;
    QPointer<QErrorMessage> m_errorMessageDialog;

    QString m_initializationErrors;
    QString m_lastErrorMessage;
    bool m_suppressNewFormShow;
};

QT_END_NAMESPACE

#endif // QDESIGNER_H
