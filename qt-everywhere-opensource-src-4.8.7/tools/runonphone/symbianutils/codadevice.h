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

#ifndef CODAENGINE_H
#define CODAENGINE_H

#include "symbianutils_global.h"
#include "codamessage.h"
#include "callback.h"
#include "json.h"

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>
#include <QtCore/QVector>
#include <QtCore/QVariant>
#include <QtCore/QStringList>
#include <QtCore/QDateTime>

QT_BEGIN_NAMESPACE
class QIODevice;
class QTextStream;
QT_END_NAMESPACE

namespace Coda {

struct CodaDevicePrivate;
struct Breakpoint;

/* Command error handling in TCF:
 * 1) 'Severe' errors (JSON format, parameter format): Trk emits a
 *     nonstandard message (\3\2 error parameters) and closes the connection.
 * 2) Protocol errors: 'N' without error message is returned.
 * 3) Errors in command execution: 'R' with a TCF error hash is returned
 *    (see CodaCommandError). */

/* Error code return in 'R' reply to command
 * (see top of 'Services' documentation). */
struct SYMBIANUTILS_EXPORT CodaCommandError {
    CodaCommandError();
    void clear();
    bool isError() const;
    operator bool() const { return isError(); }
    QString toString() const;
    void write(QTextStream &str) const;
    bool parse(const QVector<JsonValue> &values);

    quint64 timeMS; // Since 1.1.1970
    qint64 code;
    QByteArray format; // message
    // 'Alternative' meaning, like altOrg="POSIX"/altCode=<some errno>
    QByteArray alternativeOrganization;
    qint64 alternativeCode;
};

/* Answer to a Tcf command passed to the callback. */
struct SYMBIANUTILS_EXPORT CodaCommandResult {
    enum Type
    {
        SuccessReply,       // 'R' and no error -> all happy.
        CommandErrorReply,  // 'R' with CodaCommandError received
        ProgressReply,      // 'P', progress indicator
        FailReply           // 'N' Protocol NAK, severe error
    };

    explicit CodaCommandResult(Type t = SuccessReply);
    explicit CodaCommandResult(char typeChar, Services service,
                                 const QByteArray &request,
                                 const QVector<JsonValue> &values,
                                 const QVariant &cookie);

    QString toString() const;
    QString errorString() const;
    operator bool() const { return type == SuccessReply || type == ProgressReply; }

    static QDateTime tcfTimeToQDateTime(quint64 tcfTimeMS);

    Type type;
    Services service;
    QByteArray request;
    CodaCommandError commandError;
    QVector<JsonValue> values;
    QVariant cookie;
};

// Response to stat/fstat
struct SYMBIANUTILS_EXPORT CodaStatResponse
{
    CodaStatResponse();

    quint64 size;
    QDateTime modTime;
    QDateTime accessTime;
};

typedef trk::Callback<const CodaCommandResult &> CodaCallback;

/* CodaDevice: TCF communication helper using an asynchronous QIODevice
 * implementing the TCF protocol according to:
http://dev.eclipse.org/svnroot/dsdp/org.eclipse.tm.tcf/trunk/docs/TCF%20Specification.html
http://dev.eclipse.org/svnroot/dsdp/org.eclipse.tm.tcf/trunk/docs/TCF%20Services.html
 * Commands can be sent along with callbacks that are passed a
 * CodaCommandResult and an opaque QVariant cookie. In addition, events are emitted.
 *
 * CODA notes:
 * - Commands are accepted only after receiving the Locator Hello event
 * - Serial communication initiation sequence:
 *     Send serial ping from host sendSerialPing() -> receive pong response with
 *     version information -> Send Locator Hello Event -> Receive Locator Hello Event
 *     -> Commands are accepted.
 * - WLAN communication initiation sequence:
 *     Receive Locator Hello Event from CODA -> Commands are accepted.
 */

class SYMBIANUTILS_EXPORT CodaDevice : public QObject
{
    Q_PROPERTY(unsigned verbose READ verbose WRITE setVerbose)
    Q_PROPERTY(bool serialFrame READ serialFrame WRITE setSerialFrame)
    Q_OBJECT
public:
    // Flags for FileSystem:open
    enum FileSystemOpenFlags
    {
        FileSystem_TCF_O_READ = 0x00000001,
        FileSystem_TCF_O_WRITE = 0x00000002,
        FileSystem_TCF_O_APPEND = 0x00000004,
        FileSystem_TCF_O_CREAT = 0x00000008,
        FileSystem_TCF_O_TRUNC = 0x00000010,
        FileSystem_TCF_O_EXCL = 0x00000020
    };

    enum MessageType
    {
        MessageWithReply,
        MessageWithoutReply, /* Non-standard: "Settings:set" command does not reply */
        NoopMessage
    };

    typedef QSharedPointer<QIODevice> IODevicePtr;

    explicit CodaDevice(QObject *parent = 0);
    virtual ~CodaDevice();

    unsigned verbose() const;
    bool serialFrame() const;
    void setSerialFrame(bool);

    // Mapping of register names to indices for multi-requests.
    // Register names can be retrieved via 'Registers:getChildren' (requires
    // context id to be stripped).
    QVector<QByteArray> registerNames() const;
    void setRegisterNames(const QVector<QByteArray>& n);

    IODevicePtr device() const;
    IODevicePtr takeDevice();
    void setDevice(const IODevicePtr &dp);

    // Serial Only: Initiate communication. Will emit serialPong() signal with version.
    void sendSerialPing(bool pingOnly = false);

    // Send with parameters from string (which may contain '\0').
    void sendCodaMessage(MessageType mt, Services service, const char *command,
                           const char *commandParameters, int commandParametersLength,
                           const CodaCallback &callBack = CodaCallback(),
                           const QVariant &cookie = QVariant());

    void sendCodaMessage(MessageType mt, Services service, const char *command,
                           const QByteArray &commandParameters,
                           const CodaCallback &callBack = CodaCallback(),
                           const QVariant &cookie = QVariant());

    // Convenience messages: Start a process
    void sendProcessStartCommand(const CodaCallback &callBack,
                                 const QString &binary,
                                 unsigned uid,
                                 QStringList arguments = QStringList(),
                                 QString workingDirectory = QString(),
                                 bool debugControl = true,
                                 const QStringList &additionalLibraries = QStringList(),
                                 const QVariant &cookie = QVariant());

    // Just launch a process, don't attempt to attach the debugger to it
    void sendRunProcessCommand(const CodaCallback &callBack,
                               const QString &processName,
                               QStringList arguments = QStringList(),
                               const QVariant &cookie = QVariant());

    // Preferred over Processes:Terminate by TCF TRK.
    void sendRunControlTerminateCommand(const CodaCallback &callBack,
                                        const QByteArray &id,
                                        const QVariant &cookie = QVariant());

    void sendProcessTerminateCommand(const CodaCallback &callBack,
                                     const QByteArray &id,
                                     const QVariant &cookie = QVariant());

    // Non-standard: Remove executable from settings.
    // Probably needs to be called after stopping. This command has no response.
    void sendSettingsRemoveExecutableCommand(const QString &binaryIn,
                                             unsigned uid,
                                             const QStringList &additionalLibraries = QStringList(),
                                             const QVariant &cookie = QVariant());

    void sendRunControlSuspendCommand(const CodaCallback &callBack,
                                      const QByteArray &id,
                                      const QVariant &cookie = QVariant());

    // Resume / Step (see RunControlResumeMode).
    void sendRunControlResumeCommand(const CodaCallback &callBack,
                                     const QByteArray &id,
                                     RunControlResumeMode mode,
                                     unsigned count /* = 1, currently ignored. */,
                                     quint64 rangeStart, quint64 rangeEnd,
                                     const QVariant &cookie = QVariant());

    // Convenience to resume a suspended process
    void sendRunControlResumeCommand(const CodaCallback &callBack,
                                     const QByteArray &id,
                                     const QVariant &cookie = QVariant());

    void sendBreakpointsAddCommand(const CodaCallback &callBack,
                                   const Breakpoint &b,
                                   const QVariant &cookie = QVariant());

    void sendBreakpointsRemoveCommand(const CodaCallback &callBack,
                                      const QByteArray &id,
                                      const QVariant &cookie = QVariant());

    void sendBreakpointsRemoveCommand(const CodaCallback &callBack,
                                      const QVector<QByteArray> &id,
                                      const QVariant &cookie = QVariant());

    void sendBreakpointsEnableCommand(const CodaCallback &callBack,
                                      const QByteArray &id,
                                      bool enable,
                                      const QVariant &cookie = QVariant());

    void sendBreakpointsEnableCommand(const CodaCallback &callBack,
                                      const QVector<QByteArray> &id,
                                      bool enable,
                                      const QVariant &cookie = QVariant());


    void sendMemoryGetCommand(const CodaCallback &callBack,
                              const QByteArray &contextId,
                              quint64 start, quint64 size,
                              const QVariant &cookie = QVariant());

    void sendMemorySetCommand(const CodaCallback &callBack,
                              const QByteArray &contextId,
                              quint64 start, const QByteArray& data,
                              const QVariant &cookie = QVariant());

    // Get register names (children of context).
    // It is possible to recurse from  thread id down to single registers.
    void sendRegistersGetChildrenCommand(const CodaCallback &callBack,
                                         const QByteArray &contextId,
                                         const QVariant &cookie = QVariant());

    // Register get
    void sendRegistersGetCommand(const CodaCallback &callBack,
                                 const QByteArray &contextId,
                                 QByteArray id,
                                 const QVariant &cookie);

    void sendRegistersGetMCommand(const CodaCallback &callBack,
                                  const QByteArray &contextId,
                                  const QVector<QByteArray> &ids,
                                  const QVariant &cookie = QVariant());

    // Convenience to get a range of register "R0" .. "R<n>".
    // Cookie will be an int containing "start".
    void sendRegistersGetMRangeCommand(const CodaCallback &callBack,
                                 const QByteArray &contextId,
                                  unsigned start, unsigned count);

    // Set register
    void sendRegistersSetCommand(const CodaCallback &callBack,
                                 const QByteArray &contextId,
                                 QByteArray ids,
                                 const QByteArray &value, // binary value
                                 const QVariant &cookie = QVariant());
    // Set register
    void sendRegistersSetCommand(const CodaCallback &callBack,
                                 const QByteArray &contextId,
                                 unsigned registerNumber,
                                 const QByteArray &value, // binary value
                                 const QVariant &cookie = QVariant());

    // File System
    void sendFileSystemOpenCommand(const CodaCallback &callBack,
                                   const QByteArray &name,
                                   unsigned flags = FileSystem_TCF_O_READ,
                                   const QVariant &cookie = QVariant());

    void sendFileSystemFstatCommand(const CodaCallback &callBack,
                                   const QByteArray &handle,
                                   const QVariant &cookie = QVariant());

    void sendFileSystemReadCommand(const Coda::CodaCallback &callBack,
                                   const QByteArray &handle,
                                   unsigned int offset,
                                   unsigned int size,
                                   const QVariant &cookie = QVariant());

    void sendFileSystemWriteCommand(const CodaCallback &callBack,
                                    const QByteArray &handle,
                                    const QByteArray &data,
                                    unsigned offset = 0,
                                    const QVariant &cookie = QVariant());

    void sendFileSystemCloseCommand(const CodaCallback &callBack,
                                   const QByteArray &handle,
                                   const QVariant &cookie = QVariant());

    // Symbian Install
    void sendSymbianInstallSilentInstallCommand(const CodaCallback &callBack,
                                                const QByteArray &file,
                                                const QByteArray &targetDrive,
                                                const QVariant &cookie = QVariant());

    void sendSymbianInstallUIInstallCommand(const CodaCallback &callBack,
                                            const QByteArray &file,
                                            const QVariant &cookie = QVariant());

    void sendSymbianInstallGetPackageInfoCommand(const Coda::CodaCallback &callBack,
                                                 const QList<quint32> &packages,
                                                 const QVariant &cookie = QVariant());

    void sendLoggingAddListenerCommand(const CodaCallback &callBack,
                                       const QVariant &cookie = QVariant());

    void sendSymbianUninstallCommand(const Coda::CodaCallback &callBack,
                                                 const quint32 package,
                                                 const QVariant &cookie = QVariant());

    // SymbianOs Data
    void sendSymbianOsDataGetThreadsCommand(const CodaCallback &callBack,
                                            const QVariant &cookie = QVariant());

    void sendSymbianOsDataFindProcessesCommand(const CodaCallback &callBack,
                                                const QByteArray &processName,
                                                const QByteArray &uid,
                                                const QVariant &cookie = QVariant());

    void sendSymbianOsDataGetQtVersionCommand(const CodaCallback &callBack,
                                              const QVariant &cookie = QVariant());

    void sendSymbianOsDataGetRomInfoCommand(const CodaCallback &callBack,
                                            const QVariant &cookie = QVariant());

    void sendSymbianOsDataGetHalInfoCommand(const CodaCallback &callBack,
                                            const QStringList &keys = QStringList(),
                                            const QVariant &cookie = QVariant());

    // DebugSessionControl
    void sendDebugSessionControlSessionStartCommand(const CodaCallback &callBack,
                                            const QVariant &cookie = QVariant());

    void sendDebugSessionControlSessionEndCommand(const CodaCallback &callBack,
                                            const QVariant &cookie = QVariant());

    // Settings
    void sendSettingsEnableLogCommand();

    void writeCustomData(char protocolId, const QByteArray &aData);

    static QByteArray parseMemoryGet(const CodaCommandResult &r);
    static QVector<QByteArray> parseRegisterGetChildren(const CodaCommandResult &r);
    static CodaStatResponse parseStat(const CodaCommandResult &r);

signals:
    void genericTcfEvent(int service, const QByteArray &name, const QVector<JsonValue> &value);
    void tcfEvent(const Coda::CodaEvent &knownEvent);
    void unknownEvent(uchar protocolId, const QByteArray& data);
    void serialPong(const QString &codaVersion);

    void logMessage(const QString &);
    void error(const QString &);

public slots:
    void setVerbose(unsigned v);

private slots:
    void slotDeviceError();
    void slotDeviceSocketStateChanged();
    void slotDeviceReadyRead();

private:
    void deviceReadyReadSerial();
    void deviceReadyReadTcp();

    bool checkOpen();
    void checkSendQueue();
    void writeMessage(QByteArray data, bool ensureTerminating0 = true);
    void emitLogMessage(const QString &);
    inline int parseMessage(const QByteArray &);
    void processMessage(const QByteArray &message);
    inline void processSerialMessage(const QByteArray &message);
    int parseTcfCommandReply(char type, const QVector<QByteArray> &tokens);
    int parseTcfEvent(const QVector<QByteArray> &tokens);

private:
    QPair<int, int> findSerialHeader(QByteArray &in);
    CodaDevicePrivate *d;
};

} // namespace Coda

#endif // CODAENGINE_H
