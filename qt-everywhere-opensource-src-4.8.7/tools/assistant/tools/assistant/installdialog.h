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

#ifndef INSTALLDIALOG_H
#define INSTALLDIALOG_H

#include <QtCore/QQueue>
#include <QtGui/QDialog>
#include <QtNetwork/QHttpResponseHeader>
#include "ui_installdialog.h"

#ifndef QT_NO_HTTP

QT_BEGIN_NAMESPACE

class QHttp;
class QBuffer;
class QFile;
class QHelpEngineCore;

class InstallDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InstallDialog(QHelpEngineCore *helpEngine, QWidget *parent = 0,
        const QString &host = QString(), int port = -1);
    ~InstallDialog();

    QStringList installedDocumentations() const;

private slots:
    void init();
    void cancelDownload();
    void install();
    void httpRequestFinished(int requestId, bool error);
    void readResponseHeader(const QHttpResponseHeader &responseHeader);
    void updateDataReadProgress(int bytesRead, int totalBytes);
    void updateInstallButton();
    void browseDirectories();

private:
    void downloadNextFile();
    void updateDocItemList();
    void installFile(const QString &fileName);

    Ui::InstallDialog m_ui;
    QHelpEngineCore *m_helpEngine;
    QHttp *m_http;
    QBuffer *m_buffer;
    QFile *m_file;
    bool m_httpAborted;
    int m_docInfoId;
    int m_docId;
    QQueue<QListWidgetItem*> m_itemsToInstall;
    QString m_currentCheckSum;
    QString m_windowTitle;
    QStringList m_installedDocumentations;
    QString m_host;
    int m_port;
};

QT_END_NAMESPACE

#endif

#endif // INSTALLDIALOG_H
