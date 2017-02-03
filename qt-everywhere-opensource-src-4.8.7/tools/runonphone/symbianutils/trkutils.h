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

#ifndef DEBUGGER_TRK_UTILS
#define DEBUGGER_TRK_UTILS

#include "symbianutils_global.h"

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

QT_BEGIN_NAMESPACE
class QDateTime;
QT_END_NAMESPACE

namespace trk {

typedef unsigned char byte;
struct TrkResult;

enum Command {
    //meta commands
    TrkPing = 0x00,
    TrkConnect = 0x01,
    TrkDisconnect = 0x02,
    TrkReset = 0x03,
    TrkVersions = 0x04,
    TrkSupported = 0x05,
    TrkCpuType = 0x06,
    TrkConfigTransport = 0x07,
    TrkVersions2 = 0x08,
    TrkHostVersions = 0x09,

    //state commands
    TrkReadMemory = 0x10,
    TrkWriteMemory = 0x11,
    TrkReadRegisters = 0x12,
    TrkWriteRegisters = 0x13,
    TrkFillMemory = 0x14,
    TrkCopyMemory = 0x15,
    TrkFlushCache = 0x16,

    //execution commands
    TrkContinue = 0x18,
    TrkStep = 0x19,
    TrkStop = 0x1a,
    TrkSetBreak = 0x1b,
    TrkClearBreak = 0x1c,
    TrkDownload = 0x1d,
    TrkModifyBreakThread = 0x1e,

    //host -> target IO management
    TrkNotifyFileInput = 0x20,
    TrkBlockFileIo = 0x21,

    //host -> target os commands
    TrkCreateItem = 0x40,
    TrkDeleteItem = 0x41,
    TrkReadInfo = 0x42,
    TrkWriteInfo = 0x43,

    TrkWriteFile = 0x48,
    TrkReadFile = 0x49,
    TrkOpenFile = 0x4a,
    TrkCloseFile = 0x4b,
    TrkPositionFile = 0x4c,
    TrkInstallFile = 0x4d,
    TrkInstallFile2 = 0x4e,

    TrkPhoneSwVersion = 0x4f,
    TrkPhoneName = 0x50,
    TrkVersions3 = 0x51,

    //replies
    TrkNotifyAck = 0x80,
    TrkNotifyNak = 0xff,

    //target -> host notification
    TrkNotifyStopped = 0x90,
    TrkNotifyException = 0x91,
    TrkNotifyInternalError = 0x92,
    TrkNotifyStopped2 = 0x94,

    //target -> host OS notification
    TrkNotifyCreated = 0xa0,
    TrkNotifyDeleted = 0xa1,
    TrkNotifyProcessorStarted = 0xa2,
    TrkNotifyProcessorStandBy = 0xa6,
    TrkNotifyProcessorReset = 0xa7,

    //target -> host support commands (these are defined but not implemented in TRK)
    TrkDSWriteFile = 0xd0,
    TrkDSReadFile = 0xd1,
    TrkDSOpenFile = 0xd2,
    TrkDSCloseFile = 0xd3,
    TrkDSPositionFile = 0xd4
};

enum DSOSItemTypes {
    kDSOSProcessItem = 0x0000,
    kDSOSThreadItem = 0x0001,
    kDSOSDLLItem = 0x0002,
    kDSOSAppItem = 0x0003,
    kDSOSMemBlockItem = 0x0004,
    kDSOSProcAttachItem = 0x0005,
    kDSOSThreadAttachItem = 0x0006,
    kDSOSProcAttach2Item = 0x0007,
    kDSOSProcRunItem = 0x0008
    /* 0x0009 - 0x00ff reserved for general expansion */
    /* 0x0100 - 0xffff available for target-specific use */
};

enum SerialMultiplexor {
    MuxRaw = 0,
    MuxTextTrace = 0x0102,
    MuxTrk = 0x0190
};

inline byte extractByte(const char *data) { return *data; }
SYMBIANUTILS_EXPORT ushort extractShort(const char *data);
SYMBIANUTILS_EXPORT uint extractInt(const char *data);
SYMBIANUTILS_EXPORT quint64 extractInt64(const char *data);

SYMBIANUTILS_EXPORT QString quoteUnprintableLatin1(const QByteArray &ba);

// produces "xx xx xx "
SYMBIANUTILS_EXPORT QString stringFromArray(const QByteArray &ba, int maxLen = - 1);

enum Endianness
{
    LittleEndian,
    BigEndian,
    TargetByteOrder = BigEndian
};

SYMBIANUTILS_EXPORT void appendShort(QByteArray *ba, ushort s, Endianness = TargetByteOrder);
SYMBIANUTILS_EXPORT void appendInt(QByteArray *ba, uint i, Endianness = TargetByteOrder);
SYMBIANUTILS_EXPORT void appendString(QByteArray *ba, const QByteArray &str, Endianness = TargetByteOrder, bool appendNullTerminator = true);

struct SYMBIANUTILS_EXPORT Library
{
    Library();
    explicit Library(const TrkResult &r);

    QByteArray name;
    uint codeseg;
    uint dataseg;
     //library addresses are valid for a given process (depending on memory model, they might be loaded at the same address in all processes or not)
    uint pid;
};

struct SYMBIANUTILS_EXPORT TrkAppVersion
{
    TrkAppVersion();
    void reset();

    int trkMajor;
    int trkMinor;
    int protocolMajor;
    int protocolMinor;
};

struct SYMBIANUTILS_EXPORT Session
{
    Session();
    void reset();
    QString deviceDescription(unsigned verbose) const;
    QString toString() const;
    // Answer to qXfer::libraries
    QByteArray gdbLibraryList() const;
    // Answer to qsDllInfo, can be sent chunk-wise.
    QByteArray gdbQsDllInfo(int start = 0, int count = -1) const;

    // Trk feedback
    byte cpuMajor;
    byte cpuMinor;
    byte bigEndian;
    byte defaultTypeSize;
    byte fpTypeSize;
    byte extended1TypeSize;
    byte extended2TypeSize;
    TrkAppVersion trkAppVersion;
    uint pid;
    uint mainTid;
    uint tid;
    uint codeseg;
    uint dataseg;
    QHash<uint, uint> addressToBP;

    typedef QList<Library> Libraries;
    Libraries libraries;

    // Gdb request
    QStringList modules;
};

struct SYMBIANUTILS_EXPORT TrkResult
{
    TrkResult();
    void clear();
    QString toString() const;
    // 0 for no error.
    int errorCode() const;
    QString errorString() const;

    ushort multiplex;
    byte code;
    byte token;
    QByteArray data;
    QVariant cookie;
    bool isDebugOutput;
};

SYMBIANUTILS_EXPORT QByteArray errorMessage(byte code);
SYMBIANUTILS_EXPORT QByteArray hexNumber(uint n, int digits = 0);
SYMBIANUTILS_EXPORT QByteArray hexxNumber(uint n, int digits = 0); // prepends '0x', too
SYMBIANUTILS_EXPORT uint swapEndian(uint in);

} // namespace trk

#endif // DEBUGGER_TRK_UTILS
