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

#ifndef CODASIGNALHANDLER_H
#define CODASIGNALHANDLER_H

#include "symbianutils/codamessage.h"
#include "symbianutils/symbiandevicemanager.h"

#include "symbianutils/codadevice.h"

enum CodaAction {
    ActionPingOnly = 0x0,
    ActionCopy = 0x1,
    ActionInstall = 0x2,
    ActionCopyInstall = ActionCopy | ActionInstall,
    ActionRun = 0x4,
    ActionDownload = 0x8,
    ActionCopyRun = ActionCopy | ActionRun,
    ActionInstallRun = ActionInstall | ActionRun,
    ActionCopyInstallRun = ActionCopy | ActionInstall | ActionRun
};

class CodaSignalHandlerPrivate;
class CodaSignalHandler : public QObject
{
    Q_OBJECT
public slots:
    void error(const QString &errorMessage);
    void logMessage(const QString &logMessage);
    void serialPong(const QString &codaVersion);
    void tcfEvent(const Coda::CodaEvent &event);
    void terminate();
private slots:
    void finished();
    void timeout();
signals:
    void done();
public:
    CodaSignalHandler();
    ~CodaSignalHandler();
    void init();
    int run();
    void setActionType(CodaAction action);
    void setAppFileName(const QString &fileName);
    void setCodaDevice(SymbianUtils::CodaDevicePtr &codeDevice);
    void setCommandLineArgs(const QString &args);
    void setCopyFileName(const QString &srcName, const QString &dstName);
    void setDownloadFileName(const QString &srcName, const QString &dstName);
    void setLogLevel(int level);
    void setSerialPortName(const QString &serialPortName);
    void setTimeout(const int msec);
private:
    void closeFile();
    void handleConnected(const Coda::CodaEvent &event);
    void handleActions();
    void handleAppExited(const Coda::CodaEvent &event);
    void handleAppRunning(const Coda::CodaCommandResult &result);
    void handleDebugSessionControlEnd(const Coda::CodaCommandResult &result);
    void handleDebugSessionControlStart(const Coda::CodaCommandResult &result);
    void handleFileSystemClose(const Coda::CodaCommandResult &result);
    void handleFileSystemOpen(const Coda::CodaCommandResult &result);
    void handleFileSystemRead(const Coda::CodaCommandResult &result);
    void handleFileSystemStart(const Coda::CodaCommandResult &result);
    void handleFileSystemWrite(const Coda::CodaCommandResult &result);
    void handleSymbianInstall(const Coda::CodaCommandResult &result);
    void initAppRunning();
    void initFileDownloading();
    void initFileInstallation();
    void initFileSending();
    void putSendNextChunk();
    void readNextChunk();
    void readAppId(Coda::JsonValue value);
    void reportError(const QString &message);
    void reportMessage(const QString &message);

    CodaSignalHandlerPrivate *d;
};

#endif // CODESIGNALHANDLER_H
