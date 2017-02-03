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

#include "launcher.h"
#include "trkutils.h"
#include "trkutils_p.h"
#include "trkdevice.h"
#include "bluetoothlistener.h"
#include "symbiandevicemanager.h"

#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QVariant>
#include <QtCore/QDebug>
#include <QtCore/QQueue>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QScopedPointer>

#include <cstdio>

namespace trk {

struct CrashReportState {
    CrashReportState();
    void clear();

    typedef uint Thread;
    typedef QList<Thread> Threads;
    Threads threads;

    QList<uint> registers;
    QByteArray stack;
    uint sp;
    uint fetchingStackPID;
    uint fetchingStackTID;
};

CrashReportState::CrashReportState()
{
    clear();
}

void CrashReportState::clear()
{
    threads.clear();
    stack.clear();
    sp = fetchingStackPID = fetchingStackTID = 0;
}

struct LauncherPrivate {
    struct TransferState {
        int currentFileName;
        uint copyFileHandle;
        QScopedPointer<QByteArray> data;
        qint64 position;
        QScopedPointer<QFile> localFile;
    };

    struct CopyState : public TransferState {
        QStringList sourceFileNames;
        QStringList destinationFileNames;
    };

    struct DownloadState : public TransferState {
        QString sourceFileName;
        QString destinationFileName;
    };

    explicit LauncherPrivate(const TrkDevicePtr &d);

    TrkDevicePtr m_device;
    QByteArray m_trkReadBuffer;
    Launcher::State m_state;

    void logMessage(const QString &msg);
    // Debuggee state
    Session m_session; // global-ish data (process id, target information)

    CopyState m_copyState;
    DownloadState m_downloadState;
    QString m_fileName;
    QString m_commandLineArgs;
    QStringList m_installFileNames;
    int m_currentInstallFileName;
    int m_verbose;
    Launcher::Actions m_startupActions;
    bool m_closeDevice;
    CrashReportState m_crashReportState;
    Launcher::InstallationMode m_installationMode;
    Launcher::InstallationMode m_currentInstallationStep;
    char m_installationDrive;
};

LauncherPrivate::LauncherPrivate(const TrkDevicePtr &d) :
    m_device(d),
    m_state(Launcher::Disconnected),
    m_verbose(0),
    m_closeDevice(true),
    m_installationMode(Launcher::InstallationModeSilentAndUser),
    m_currentInstallationStep(Launcher::InstallationModeSilent),
    m_installationDrive('C')
{
    if (m_device.isNull())
        m_device = TrkDevicePtr(new TrkDevice);
}

Launcher::Launcher(Actions startupActions,
                   const TrkDevicePtr &dev,
                   QObject *parent) :
    QObject(parent),
    d(new LauncherPrivate(dev))
{
    d->m_startupActions = startupActions;
    connect(d->m_device.data(), SIGNAL(messageReceived(trk::TrkResult)), this, SLOT(handleResult(trk::TrkResult)));
}

Launcher::~Launcher()
{
    // Destroyed before protocol was through: Close
    if (d->m_closeDevice && d->m_device->isOpen())
        d->m_device->close();
    emit destroyed(d->m_device->port());
    logMessage("Shutting down.\n");
    delete d;
}

Launcher::State Launcher::state() const
{
    return d->m_state;
}

void Launcher::setState(State s)
{
    if (s != d->m_state) {
        d->m_state = s;
        emit stateChanged(s);
    }
}

void Launcher::setInstallationMode(InstallationMode installation)
{
    d->m_installationMode = installation;
}

void Launcher::setInstallationDrive(char drive)
{
    d->m_installationDrive = drive;
}

void Launcher::addStartupActions(trk::Launcher::Actions startupActions)
{
    d->m_startupActions = Actions(d->m_startupActions | startupActions);
}

void Launcher::setTrkServerName(const QString &name)
{
    d->m_device->setPort(name);
}

QString Launcher::trkServerName() const
{
    return d->m_device->port();
}

TrkDevicePtr Launcher::trkDevice() const
{
    return d->m_device;
}

void Launcher::setFileName(const QString &name)
{
    d->m_fileName = name;
}

void Launcher::setCopyFileNames(const QStringList &srcNames, const QStringList &dstNames)
{
    d->m_copyState.sourceFileNames = srcNames;
    d->m_copyState.destinationFileNames = dstNames;
    d->m_copyState.currentFileName = 0;
}

void Launcher::setDownloadFileName(const QString &srcName, const QString &dstName)
{
    d->m_downloadState.sourceFileName = srcName;
    d->m_downloadState.destinationFileName = dstName;
}

void Launcher::setInstallFileNames(const QStringList &names)
{
    d->m_installFileNames = names;
    d->m_currentInstallFileName = 0;
}

void Launcher::setCommandLineArgs(const QString &args)
{
    d->m_commandLineArgs = args;
}

void Launcher::setSerialFrame(bool b)
{
    d->m_device->setSerialFrame(b);
}

bool Launcher::serialFrame() const
{
    return d->m_device->serialFrame();
}


bool Launcher::closeDevice() const
{
    return d->m_closeDevice;
}

void Launcher::setCloseDevice(bool c)
{
    d->m_closeDevice = c;
}

Launcher::InstallationMode Launcher::installationMode() const
{
    return d->m_installationMode;
}

char Launcher::installationDrive() const
{
    return d->m_installationDrive;
}

bool Launcher::startServer(QString *errorMessage)
{
    errorMessage->clear();
    d->m_crashReportState.clear();
    if (d->m_verbose) {
        QString msg;
        QTextStream str(&msg);
        str.setIntegerBase(16);
        str << "Actions=0x" << d->m_startupActions;
        str.setIntegerBase(10);
        str << " Port=" << trkServerName();
        if (!d->m_fileName.isEmpty())
            str << " Executable=" << d->m_fileName;
        if (!d->m_commandLineArgs.isEmpty())
            str << " Arguments= " << d->m_commandLineArgs;
        for (int i = 0; i < d->m_copyState.sourceFileNames.size(); ++i) {
            str << " Package/Source=" << d->m_copyState.sourceFileNames.at(i);
            str << " Remote Package/Destination=" << d->m_copyState.destinationFileNames.at(i);
        }
        if (!d->m_downloadState.sourceFileName.isEmpty())
            str << " Source=" << d->m_downloadState.sourceFileName;
        if (!d->m_downloadState.destinationFileName.isEmpty())
            str << " Destination=" << d->m_downloadState.destinationFileName;
        if (!d->m_installFileNames.isEmpty())
            foreach (const QString &installFileName, d->m_installFileNames)
                str << " Install file=" << installFileName;
        logMessage(msg);
    }
    if (d->m_startupActions & ActionCopy) {
        if (d->m_copyState.sourceFileNames.isEmpty()) {
            qWarning("No local filename given for copying package.");
            return false;
        } else if (d->m_copyState.destinationFileNames.isEmpty()) {
            qWarning("No remote filename given for copying package.");
            return false;
        }
    }
    if (d->m_startupActions & ActionInstall && d->m_installFileNames.isEmpty()) {
        qWarning("No package name given for installing.");
        return false;
    }
    if (d->m_startupActions & ActionRun && d->m_fileName.isEmpty()) {
        qWarning("No remote executable given for running.");
        return false;
    }
    if (!d->m_device->isOpen() && !d->m_device->open(errorMessage))
        return false;
    setState(Connecting);
    // Set up the temporary 'waiting' state if we do not get immediate connection
    QTimer::singleShot(1000, this, SLOT(slotWaitingForTrk()));
    d->m_device->sendTrkInitialPing();
    d->m_device->sendTrkMessage(TrkDisconnect); // Disconnect, as trk might be still connected
    d->m_device->sendTrkMessage(TrkSupported, TrkCallback(this, &Launcher::handleSupportMask));
    d->m_device->sendTrkMessage(TrkCpuType, TrkCallback(this, &Launcher::handleCpuType));
    d->m_device->sendTrkMessage(TrkVersions, TrkCallback(this, &Launcher::handleTrkVersion));
    if (d->m_startupActions != ActionPingOnly)
        d->m_device->sendTrkMessage(TrkConnect, TrkCallback(this, &Launcher::handleConnect));
    return true;
}

void Launcher::slotWaitingForTrk()
{
    // Set temporary state if we are still in connected state
    if (state() == Connecting)
        setState(WaitingForTrk);
}

void Launcher::handleConnect(const TrkResult &result)
{
    if (result.errorCode()) {
        emit canNotConnect(result.errorString());
        return;
    }
    setState(Connected);
    if (d->m_startupActions & ActionCopy)
        copyFileToRemote();
    else if (d->m_startupActions & ActionInstall)
        installRemotePackage();
    else if (d->m_startupActions & ActionRun)
        startInferiorIfNeeded();
    else if (d->m_startupActions & ActionDownload)
        copyFileFromRemote();
}

void Launcher::setVerbose(int v)
{
    d->m_verbose = v;
    d->m_device->setVerbose(v);
}

void Launcher::logMessage(const QString &msg)
{
    if (d->m_verbose)
        qDebug() << "LAUNCHER: " << qPrintable(msg);
}

void Launcher::handleFinished()
{
    if (d->m_closeDevice)
        d->m_device->close();
    emit finished();
}

void Launcher::terminate()
{
    switch (state()) {
    case DeviceDescriptionReceived:
    case Connected:
        if (d->m_session.pid) {
            QByteArray ba;
            appendShort(&ba, 0x0000, TargetByteOrder);
            appendInt(&ba, d->m_session.pid, TargetByteOrder);
            d->m_device->sendTrkMessage(TrkDeleteItem, TrkCallback(this, &Launcher::handleRemoteProcessKilled), ba);
            return;
        }
        if (d->m_copyState.copyFileHandle)
            closeRemoteFile(true);
        disconnectTrk();
        break;
    case Disconnected:
        break;
    case Connecting:
    case WaitingForTrk:
        setState(Disconnected);
        handleFinished();
        break;
    }
}

void Launcher::handleRemoteProcessKilled(const TrkResult &result)
{
    Q_UNUSED(result)
    disconnectTrk();
}

QString Launcher::msgStopped(uint pid, uint tid, uint address, const QString &why)
{
    return QString::fromLatin1("Process %1, thread %2 stopped at 0x%3: %4").
            arg(pid).arg(tid).arg(address, 0, 16).
            arg(why.isEmpty() ? QString::fromLatin1("<Unknown reason>") : why);
}

bool Launcher::parseNotifyStopped(const QByteArray &dataBA,
                                  uint *pid, uint *tid, uint *address,
                                  QString *why /* = 0 */)
{
    if (why)
        why->clear();
    *address = *pid = *tid = 0;
    if (dataBA.size() < 12)
        return false;
    const char *data = dataBA.data();
    *address = extractInt(data);
    *pid = extractInt(data + 4);
    *tid = extractInt(data + 8);
    if (why && dataBA.size() >= 14) {
        const unsigned short len = extractShort(data + 12);
        if (len > 0)
            *why = QString::fromLatin1(data + 14, len);
    }
    return true;
}

void Launcher::handleResult(const TrkResult &result)
{
    QByteArray prefix = "READ BUF:                                       ";
    QByteArray str = result.toString().toUtf8();
    if (result.isDebugOutput) { // handle application output
        QString msg;
        if (result.multiplex == MuxTextTrace) {
            if (result.data.length() > 8) {
            quint64 timestamp = extractInt64(result.data) & 0x0FFFFFFFFFFFFFFFULL;
            quint64 secs = timestamp / 1000000000;
            quint64 ns = timestamp % 1000000000;
            msg = QString("[%1.%2] %3").arg(secs).arg(ns,9,10,QLatin1Char('0')).arg(QString(result.data.mid(8)));
            logMessage("TEXT TRACE: " + msg);
            }
        } else {
            logMessage("APPLICATION OUTPUT: " + stringFromArray(result.data));
            msg = result.data;
        }
        msg.replace("\r\n", "\n");
        if(!msg.endsWith('\n')) msg.append('\n');
        emit applicationOutputReceived(msg);
        return;
    }
    switch (result.code) {
        case TrkNotifyAck:
            break;
        case TrkNotifyNak: { // NAK
            logMessage(prefix + QLatin1String("NAK: ") + str);
            //logMessage(prefix << "TOKEN: " << result.token);
            logMessage(prefix + QLatin1String("ERROR: ") + errorMessage(result.data.at(0)));
            break;
        }
        case TrkNotifyStopped: { // Notified Stopped
            QString reason;
            uint pc;
            uint pid;
            uint tid;
            parseNotifyStopped(result.data, &pid, &tid, &pc, &reason);
            logMessage(prefix + msgStopped(pid, tid, pc, reason));
            emit(processStopped(pc, pid, tid, reason));
            d->m_device->sendTrkAck(result.token);
            break;
        }
        case TrkNotifyException: { // Notify Exception (obsolete)
            logMessage(prefix + QLatin1String("NOTE: EXCEPTION  ") + str);
            d->m_device->sendTrkAck(result.token);
            break;
        }
        case TrkNotifyInternalError: { //
            logMessage(prefix + QLatin1String("NOTE: INTERNAL ERROR: ") + str);
            d->m_device->sendTrkAck(result.token);
            break;
        }

        // target->host OS notification
        case TrkNotifyCreated: { // Notify Created

            if (result.data.size() < 10)
                break;
            const char *data = result.data.constData();
            const byte error = result.data.at(0);
            Q_UNUSED(error)
            const byte type = result.data.at(1); // type: 1 byte; for dll item, this value is 2.
            const uint tid = extractInt(data + 6); //threadID: 4 bytes
            Q_UNUSED(tid)
            if (type == kDSOSDLLItem && result.data.size() >=20) {
                const Library lib = Library(result);
                d->m_session.libraries.push_back(lib);
                emit libraryLoaded(lib);
            }
            QByteArray ba;
            ba.append(result.data.mid(2, 8));
            d->m_device->sendTrkMessage(TrkContinue, TrkCallback(), ba, "CONTINUE");
            break;
        }
        case TrkNotifyDeleted: { // NotifyDeleted
            const ushort itemType = (unsigned char)result.data.at(1);
            const uint pid = result.data.size() >= 6 ? extractShort(result.data.constData() + 2) : 0;
            const uint tid = result.data.size() >= 10 ? extractShort(result.data.constData() + 6) : 0;
            Q_UNUSED(tid)
            const ushort len = result.data.size() > 12 ? extractShort(result.data.constData() + 10) : ushort(0);
            const QString name = len ? QString::fromAscii(result.data.mid(12, len)) : QString();
            logMessage(QString::fromLatin1("%1 %2 UNLOAD: %3").
                       arg(QString::fromAscii(prefix)).arg(itemType ? QLatin1String("LIB") : QLatin1String("PROCESS")).
                       arg(name));
            d->m_device->sendTrkAck(result.token);
            if (itemType == kDSOSProcessItem // process
                && result.data.size() >= 10
                && d->m_session.pid == extractInt(result.data.data() + 6)) {
                    if (d->m_startupActions & ActionDownload)
                        copyFileFromRemote();
                    else
                        disconnectTrk();
            }
            else if (itemType == kDSOSDLLItem && len) {
                // Remove libraries of process.
                for (QList<Library>::iterator it = d->m_session.libraries.begin(); it != d->m_session.libraries.end(); ) {
                    if ((*it).pid == pid && (*it).name == name) {
                        emit libraryUnloaded(*it);
                        it = d->m_session.libraries.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            break;
        }
        case TrkNotifyProcessorStarted: { // NotifyProcessorStarted
            logMessage(prefix + QLatin1String("NOTE: PROCESSOR STARTED: ") + str);
            d->m_device->sendTrkAck(result.token);
            break;
        }
        case TrkNotifyProcessorStandBy: { // NotifyProcessorStandby
            logMessage(prefix + QLatin1String("NOTE: PROCESSOR STANDBY: ") + str);
            d->m_device->sendTrkAck(result.token);
            break;
        }
        case TrkNotifyProcessorReset: { // NotifyProcessorReset
            logMessage(prefix + QLatin1String("NOTE: PROCESSOR RESET: ") + str);
            d->m_device->sendTrkAck(result.token);
            break;
        }
        default: {
            logMessage(prefix + QLatin1String("INVALID: ") + str);
            break;
        }
    }
}

QString Launcher::deviceDescription(unsigned verbose) const
{
    return d->m_session.deviceDescription(verbose);
}

void Launcher::handleTrkVersion(const TrkResult &result)
{
    if (result.errorCode() || result.data.size() < 5) {
        if (d->m_startupActions == ActionPingOnly) {
            setState(Disconnected);
            handleFinished();
        }
        return;
    }
    d->m_session.trkAppVersion.trkMajor = result.data.at(1);
    d->m_session.trkAppVersion.trkMinor = result.data.at(2);
    d->m_session.trkAppVersion.protocolMajor = result.data.at(3);
    d->m_session.trkAppVersion.protocolMinor = result.data.at(4);
    setState(DeviceDescriptionReceived);
    const QString msg = deviceDescription();
    emit deviceDescriptionReceived(trkServerName(), msg);
    // Ping mode: Log & Terminate
    if (d->m_startupActions == ActionPingOnly) {
        qWarning("%s", qPrintable(msg));
        setState(Disconnected);
        handleFinished();
    }
}

static inline QString msgCannotOpenRemoteFile(const QString &fileName, const QString &message)
{
    return Launcher::tr("Cannot open remote file '%1': %2").arg(fileName, message);
}

static inline QString msgCannotOpenLocalFile(const QString &fileName, const QString &message)
{
    return Launcher::tr("Cannot open '%1': %2").arg(fileName, message);
}

void Launcher::handleFileCreation(const TrkResult &result)
{
    if (result.errorCode() || result.data.size() < 6) {
        const QString msg = msgCannotOpenRemoteFile(d->m_copyState.destinationFileNames.at(d->m_copyState.currentFileName), result.errorString());
        logMessage(msg);
        emit canNotCreateFile(d->m_copyState.destinationFileNames.at(d->m_copyState.currentFileName), msg);
        disconnectTrk();
        return;
    }
    const char *data = result.data.data();
    d->m_copyState.copyFileHandle = extractInt(data + 2);
    const QString localFileName = d->m_copyState.sourceFileNames.at(d->m_copyState.currentFileName);
    QFile file(localFileName);
    d->m_copyState.position = 0;
    if (!file.open(QIODevice::ReadOnly)) {
        const QString msg = msgCannotOpenLocalFile(localFileName, file.errorString());
        logMessage(msg);
        emit canNotOpenLocalFile(localFileName, msg);
        closeRemoteFile(true);
        disconnectTrk();
        return;
    }
    d->m_copyState.data.reset(new QByteArray(file.readAll()));
    file.close();
    continueCopying();
}

void Launcher::handleFileOpened(const TrkResult &result)
{
    if (result.errorCode() || result.data.size() < 6) {
        const QString msg = msgCannotOpenRemoteFile(d->m_downloadState.sourceFileName, result.errorString());
        logMessage(msg);
        emit canNotOpenFile(d->m_downloadState.sourceFileName, msg);
        disconnectTrk();
        return;
    }
    d->m_downloadState.position = 0;
    const QString localFileName = d->m_downloadState.destinationFileName;
    bool opened = false;
    if (localFileName == QLatin1String("-")) {
        d->m_downloadState.localFile.reset(new QFile);
        opened = d->m_downloadState.localFile->open(stdout, QFile::WriteOnly);
    } else {
        d->m_downloadState.localFile.reset(new QFile(localFileName));
        opened = d->m_downloadState.localFile->open(QFile::WriteOnly | QFile::Truncate);
    }
    if (!opened) {
        const QString msg = msgCannotOpenLocalFile(localFileName, d->m_downloadState.localFile->errorString());
        logMessage(msg);
        emit canNotOpenLocalFile(localFileName, msg);
        closeRemoteFile(true);
        disconnectTrk();
    }
    continueReading();
}

void Launcher::continueReading()
{
    QByteArray ba;
    appendInt(&ba, d->m_downloadState.copyFileHandle, TargetByteOrder);
    appendShort(&ba, 2048, TargetByteOrder);
    d->m_device->sendTrkMessage(TrkReadFile, TrkCallback(this, &Launcher::handleRead), ba);
}

void Launcher::handleRead(const TrkResult &result)
{
    if (result.errorCode() || result.data.size() < 4) {
        d->m_downloadState.localFile->close();
        closeRemoteFile(true);
        disconnectTrk();
    } else {
        int length = extractShort(result.data.data() + 2);
        //TRK doesn't tell us the file length, so we need to keep reading until it returns 0 length
        if (length > 0) {
            d->m_downloadState.localFile->write(result.data.data() + 4, length);
            continueReading();
        } else {
            closeRemoteFile(true);
            disconnectTrk();
        }
    }
}

void Launcher::handleCopy(const TrkResult &result)
{
    if (result.errorCode() || result.data.size() < 4) {
        closeRemoteFile(true);
        emit canNotWriteFile(d->m_copyState.destinationFileNames.at(d->m_copyState.currentFileName), result.errorString());
        disconnectTrk();
    } else {
        continueCopying(extractShort(result.data.data() + 2));
    }
}

void Launcher::continueCopying(uint lastCopiedBlockSize)
{
    qint64 size = d->m_copyState.data->length();
    d->m_copyState.position += lastCopiedBlockSize;
    if (size == 0)
        emit copyProgress(100);
    else {
        const qint64 hundred = 100;
        const qint64 percent = qMin( (d->m_copyState.position * hundred) / size, hundred);
        emit copyProgress(static_cast<int>(percent));
    }
    if (d->m_copyState.position < size) {
        QByteArray ba;
        appendInt(&ba, d->m_copyState.copyFileHandle, TargetByteOrder);
        appendString(&ba, d->m_copyState.data->mid(d->m_copyState.position, 2048), TargetByteOrder, false);
        d->m_device->sendTrkMessage(TrkWriteFile, TrkCallback(this, &Launcher::handleCopy), ba);
    } else {
        closeRemoteFile();
    }
}

void Launcher::closeRemoteFile(bool failed)
{
    QByteArray ba;
    appendInt(&ba, d->m_copyState.copyFileHandle, TargetByteOrder);
    appendDateTime(&ba, QDateTime::currentDateTime(), TargetByteOrder);
    d->m_device->sendTrkMessage(TrkCloseFile,
                               failed ? TrkCallback() : TrkCallback(this, &Launcher::handleFileCopied),
                               ba);
    d->m_copyState.data.reset(0);
    d->m_copyState.copyFileHandle = 0;
    d->m_copyState.position = 0;
}

void Launcher::handleFileCopied(const TrkResult &result)
{
    if (result.errorCode())
        emit canNotCloseFile(d->m_copyState.destinationFileNames.at(d->m_copyState.currentFileName), result.errorString());

    ++d->m_copyState.currentFileName;

    if (d->m_startupActions & ActionInstall && d->m_copyState.currentFileName < d->m_copyState.sourceFileNames.size()) {
        copyFileToRemote();
    } else if (d->m_startupActions & ActionInstall) {
        installRemotePackage();
    } else if (d->m_startupActions & ActionRun) {
        startInferiorIfNeeded();
    } else if (d->m_startupActions & ActionDownload) {
        copyFileFromRemote();
    } else {
        disconnectTrk();
    }
}

void Launcher::handleCpuType(const TrkResult &result)
{
    logMessage("HANDLE CPU TYPE: " + result.toString());
    if(result.errorCode() || result.data.size() < 7)
        return;
    //---TRK------------------------------------------------------
    //  Command: 0x80 Acknowledge
    //    Error: 0x00
    // [80 03 00  04 00 00 04 00 00 00]
    d->m_session.cpuMajor = result.data.at(1);
    d->m_session.cpuMinor = result.data.at(2);
    d->m_session.bigEndian = result.data.at(3);
    d->m_session.defaultTypeSize = result.data.at(4);
    d->m_session.fpTypeSize = result.data.at(5);
    d->m_session.extended1TypeSize = result.data.at(6);
    //d->m_session.extended2TypeSize = result.data[6];
}

void Launcher::handleCreateProcess(const TrkResult &result)
{
    if (result.errorCode()) {
        emit canNotRun(result.errorString());
        disconnectTrk();
        return;
    }
    //  40 00 00]
    //logMessage("       RESULT: " + result.toString());
    // [80 08 00   00 00 01 B5   00 00 01 B6   78 67 40 00   00 40 00 00]
    const char *data = result.data.data();
    d->m_session.pid = extractInt(data + 1);
    d->m_session.tid = extractInt(data + 5);
    d->m_session.codeseg = extractInt(data + 9);
    d->m_session.dataseg = extractInt(data + 13);
    if (d->m_verbose) {
        const QString msg = QString::fromLatin1("Process id: %1 Thread id: %2 code: 0x%3 data: 0x%4").
                            arg(d->m_session.pid).arg(d->m_session.tid).arg(d->m_session.codeseg, 0, 16).
                            arg(d->m_session.dataseg,  0 ,16);
        logMessage(msg);
    }
    emit applicationRunning(d->m_session.pid);
    //create a "library" entry for the executable which launched the process
    Library lib;
    lib.pid = d->m_session.pid;
    lib.codeseg = d->m_session.codeseg;
    lib.dataseg = d->m_session.dataseg;
    lib.name = d->m_fileName.toUtf8();
    d->m_session.libraries << lib;
    emit libraryLoaded(lib);

    QByteArray ba;
    appendInt(&ba, d->m_session.pid);
    appendInt(&ba, d->m_session.tid);
    d->m_device->sendTrkMessage(TrkContinue, TrkCallback(), ba, "CONTINUE");
}

void Launcher::handleWaitForFinished(const TrkResult &result)
{
    logMessage("   FINISHED: " + stringFromArray(result.data));
    setState(Disconnected);
    handleFinished();
}

void Launcher::handleSupportMask(const TrkResult &result)
{
    if (result.errorCode() || result.data.size() < 32)
        return;
    const char *data = result.data.data() + 1;

    if (d->m_verbose > 1) {
        QString str = QLatin1String("SUPPORTED: ");
        for (int i = 0; i < 32; ++i) {
            for (int j = 0; j < 8; ++j) {
                if (data[i] & (1 << j)) {
                    str.append(QString::number(i * 8 + j, 16));
                    str.append(QLatin1Char(' '));
                }
            }
        }
        logMessage(str);
    }
}

void Launcher::cleanUp()
{
    //
    //---IDE------------------------------------------------------
    //  Command: 0x41 Delete Item
    //  Sub Cmd: Delete Process
    //ProcessID: 0x0000071F (1823)
    // [41 24 00 00 00 00 07 1F]
    QByteArray ba(2, char(0));
    appendInt(&ba, d->m_session.pid);
    d->m_device->sendTrkMessage(TrkDeleteItem, TrkCallback(), ba, "Delete process");

    //---TRK------------------------------------------------------
    //  Command: 0x80 Acknowledge
    //    Error: 0x00
    // [80 24 00]

    //---IDE------------------------------------------------------
    //  Command: 0x1C Clear Break
    // [1C 25 00 00 00 0A 78 6A 43 40]

        //---TRK------------------------------------------------------
        //  Command: 0xA1 Notify Deleted
        // [A1 09 00 00 00 00 00 00 00 00 07 1F]
        //---IDE------------------------------------------------------
        //  Command: 0x80 Acknowledge
        //    Error: 0x00
        // [80 09 00]

    //---TRK------------------------------------------------------
    //  Command: 0x80 Acknowledge
    //    Error: 0x00
    // [80 25 00]

    //---IDE------------------------------------------------------
    //  Command: 0x1C Clear Break
    // [1C 26 00 00 00 0B 78 6A 43 70]
    //---TRK------------------------------------------------------
    //  Command: 0x80 Acknowledge
    //    Error: 0x00
    // [80 26 00]


    //---IDE------------------------------------------------------
    //  Command: 0x02 Disconnect
    // [02 27]
//    sendTrkMessage(0x02, TrkCallback(this, &Launcher::handleDisconnect));
    //---TRK------------------------------------------------------
    //  Command: 0x80 Acknowledge
    // Error: 0x00
}

void Launcher::disconnectTrk()
{
    d->m_device->sendTrkMessage(TrkDisconnect, TrkCallback(this, &Launcher::handleWaitForFinished));
}

void Launcher::copyFileToRemote()
{
    QFileInfo fileInfo(d->m_copyState.destinationFileNames.at(d->m_copyState.currentFileName));
    emit copyingStarted(fileInfo.fileName());
    QByteArray ba;
    ba.append(char(10)); //kDSFileOpenWrite | kDSFileOpenBinary
    appendString(&ba, d->m_copyState.destinationFileNames.at(d->m_copyState.currentFileName).toLocal8Bit(), TargetByteOrder, false);
    d->m_device->sendTrkMessage(TrkOpenFile, TrkCallback(this, &Launcher::handleFileCreation), ba);
}

void Launcher::copyFileFromRemote()
{
    QFileInfo fileInfo(d->m_downloadState.sourceFileName);
    emit copyingStarted(fileInfo.fileName());
    QByteArray ba;
    ba.append(char(9)); //kDSFileOpenRead | kDSFileOpenBinary
    appendString(&ba, d->m_downloadState.sourceFileName.toLocal8Bit(), TargetByteOrder, false);
    d->m_device->sendTrkMessage(TrkOpenFile, TrkCallback(this, &Launcher::handleFileOpened), ba);
}

void Launcher::installRemotePackageSilently()
{
    emit installingStarted(d->m_installFileNames.at(d->m_currentInstallFileName));
    d->m_currentInstallationStep = InstallationModeSilent;
    QByteArray ba;
    ba.append(static_cast<char>(QChar::toUpper((ushort)d->m_installationDrive)));
    appendString(&ba, d->m_installFileNames.at(d->m_currentInstallFileName).toLocal8Bit(), TargetByteOrder, false);
    d->m_device->sendTrkMessage(TrkInstallFile, TrkCallback(this, &Launcher::handleInstallPackageFinished), ba);
}

void Launcher::installRemotePackageByUser()
{
    emit installingStarted(d->m_installFileNames.at(d->m_currentInstallFileName));
    d->m_currentInstallationStep = InstallationModeUser;
    QByteArray ba;
    appendString(&ba, d->m_installFileNames.at(d->m_currentInstallFileName).toLocal8Bit(), TargetByteOrder, false);
    d->m_device->sendTrkMessage(TrkInstallFile2, TrkCallback(this, &Launcher::handleInstallPackageFinished), ba);
}

void Launcher::installRemotePackage()
{
    switch (installationMode()) {
    case InstallationModeSilent:
    case InstallationModeSilentAndUser:
        installRemotePackageSilently();
        break;
    case InstallationModeUser:
        installRemotePackageByUser();
        break;
    default:
        break;
    }
}

void Launcher::handleInstallPackageFinished(const TrkResult &result)
{
    if (result.errorCode()) {
        if (installationMode() == InstallationModeSilentAndUser
            && d->m_currentInstallationStep & InstallationModeSilent) {
            installRemotePackageByUser();
            return;
        }
        emit canNotInstall(d->m_installFileNames.at(d->m_currentInstallFileName), result.errorString());
        disconnectTrk();
        return;
    }

    ++d->m_currentInstallFileName;

    if (d->m_currentInstallFileName == d->m_installFileNames.size())
        emit installingFinished();

    if (d->m_startupActions & ActionInstall && d->m_currentInstallFileName < d->m_installFileNames.size()) {
        installRemotePackage();
    } else if (d->m_startupActions & ActionRun) {
        startInferiorIfNeeded();
    } else if (d->m_startupActions & ActionDownload) {
        copyFileFromRemote();
    } else {
        disconnectTrk();
    }
}

QByteArray Launcher::startProcessMessage(const QString &executable,
                                         const QString &arguments)
{
    // It's not started yet
    QByteArray ba;
    appendShort(&ba, 0, TargetByteOrder); // create new process (kDSOSProcessItem)
    ba.append(char(0)); // options - currently unused
    // One string consisting of binary terminated by '\0' and arguments terminated by '\0'
    QByteArray commandLineBa = executable.toLocal8Bit();
    commandLineBa.append(char(0));
    if (!arguments.isEmpty())
        commandLineBa.append(arguments.toLocal8Bit());
    appendString(&ba, commandLineBa, TargetByteOrder, true);
    return ba;
}

QByteArray Launcher::readMemoryMessage(uint pid, uint tid, uint from, uint len)
{
    QByteArray ba;
    ba.reserve(11);
    ba.append(char(0x8)); // Options, FIXME: why?
    appendShort(&ba, len);
    appendInt(&ba, from);
    appendInt(&ba, pid);
    appendInt(&ba, tid);
    return ba;
}

QByteArray Launcher::readRegistersMessage(uint pid, uint tid)
{
    QByteArray ba;
    ba.reserve(15);
    ba.append(char(0)); // Register set, only 0 supported
    appendShort(&ba, 0); //R0
    appendShort(&ba, 16); // last register CPSR
    appendInt(&ba, pid);
    appendInt(&ba, tid);
    return ba;
}

void Launcher::startInferiorIfNeeded()
{
    emit startingApplication();
    if (d->m_session.pid != 0) {
        logMessage("Process already 'started'");
        return;
    }

    d->m_device->sendTrkMessage(TrkCreateItem, TrkCallback(this, &Launcher::handleCreateProcess),
                                startProcessMessage(d->m_fileName, d->m_commandLineArgs)); // Create Item
}

void Launcher::resumeProcess(uint pid, uint tid)
{
    QByteArray ba;
    appendInt(&ba, pid, BigEndian);
    appendInt(&ba, tid, BigEndian);
    d->m_device->sendTrkMessage(TrkContinue, TrkCallback(), ba, "CONTINUE");
}

// Acquire a device from SymbianDeviceManager, return 0 if not available.
Launcher *Launcher::acquireFromDeviceManager(const QString &serverName,
                                             QObject *parent,
                                             QString *errorMessage)
{
    SymbianUtils::SymbianDeviceManager *sdm = SymbianUtils::SymbianDeviceManager::instance();
    const QSharedPointer<trk::TrkDevice> device = sdm->acquireDevice(serverName);
    if (device.isNull()) {
        if (serverName.isEmpty())
            *errorMessage = tr("No device is connected. Please connect a device and try again.");
        else
            *errorMessage = tr("Unable to acquire a device for port '%1'. It appears to be in use.").arg(serverName);
        return 0;
    }
    // Wire release signal.
    Launcher *rc = new Launcher(trk::Launcher::ActionPingOnly, device, parent);
    connect(rc, SIGNAL(deviceDescriptionReceived(QString,QString)),
            sdm, SLOT(setAdditionalInformation(QString,QString)));
    connect(rc, SIGNAL(destroyed(QString)), sdm, SLOT(releaseDevice(QString)));
    return rc;
}

// Preliminary release of device, disconnecting the signal.
void Launcher::releaseToDeviceManager(Launcher *launcher)
{
    Q_ASSERT(launcher);

    SymbianUtils::SymbianDeviceManager *sdm = SymbianUtils::SymbianDeviceManager::instance();
    // Disentangle launcher and its device, remove connection from destroyed
    launcher->setCloseDevice(false);
    TrkDevice *device = launcher->trkDevice().data();
    launcher->disconnect(device);
    device->disconnect(launcher);
    launcher->disconnect(sdm);
    sdm->releaseDevice(launcher->trkServerName());
}

void Launcher::getRegistersAndCallStack(uint pid, uint tid)
{
    d->m_device->sendTrkMessage(TrkReadRegisters,
                                TrkCallback(this, &Launcher::handleReadRegisters),
                                Launcher::readRegistersMessage(pid, tid));
    d->m_crashReportState.fetchingStackPID = pid;
    d->m_crashReportState.fetchingStackTID = tid;
}

void Launcher::handleReadRegisters(const TrkResult &result)
{
    if(result.errorCode() || result.data.size() < (17*4)) {
        terminate();
        return;
    }
    const char* data = result.data.constData() + 1;
    d->m_crashReportState.registers.clear();
    d->m_crashReportState.stack.clear();
    for (int i=0;i<17;i++) {
        uint r = extractInt(data);
        data += 4;
        d->m_crashReportState.registers.append(r);
    }
    d->m_crashReportState.sp = d->m_crashReportState.registers.at(13);

    const ushort len = 1024 - (d->m_crashReportState.sp % 1024); //read to 1k boundary first
    const QByteArray ba = Launcher::readMemoryMessage(d->m_crashReportState.fetchingStackPID,
                                                      d->m_crashReportState.fetchingStackTID,
                                                      d->m_crashReportState.sp,
                                                      len);
    d->m_device->sendTrkMessage(TrkReadMemory, TrkCallback(this, &Launcher::handleReadStack), ba);
    d->m_crashReportState.sp += len;
}

void Launcher::handleReadStack(const TrkResult &result)
{
    if (result.errorCode()) {
        //error implies memory fault when reaching end of stack
        emit registersAndCallStackReadComplete(d->m_crashReportState.registers, d->m_crashReportState.stack);
        return;
    }

    const uint len = extractShort(result.data.constData() + 1);
    d->m_crashReportState.stack.append(result.data.mid(3, len));

    if (d->m_crashReportState.sp - d->m_crashReportState.registers.at(13) > 0x10000) {
        //read enough stack, stop here
        emit registersAndCallStackReadComplete(d->m_crashReportState.registers, d->m_crashReportState.stack);
        return;
    }
    //read 1k more
    const QByteArray ba = Launcher::readMemoryMessage(d->m_crashReportState.fetchingStackPID,
                                                      d->m_crashReportState.fetchingStackTID,
                                                      d->m_crashReportState.sp,
                                                      1024);
    d->m_device->sendTrkMessage(TrkReadMemory, TrkCallback(this, &Launcher::handleReadStack), ba);
    d->m_crashReportState.sp += 1024;
}

} // namespace trk
