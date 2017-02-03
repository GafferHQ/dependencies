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

#ifndef VIRTUALSERIALPORT_H
#define VIRTUALSERIALPORT_H

#include <QtCore/QIODevice>
#include <QtCore/QString>
#include <QtCore/QMutex>

QT_BEGIN_NAMESPACE
class QWaitCondition;
QT_END_NAMESPACE

#include "symbianutils_global.h"

namespace SymbianUtils {

class VirtualSerialDevicePrivate;

class SYMBIANUTILS_EXPORT VirtualSerialDevice : public QIODevice
{
    Q_OBJECT
public:
    explicit VirtualSerialDevice(const QString &name, QObject *parent = 0);
    ~VirtualSerialDevice();

    bool open(OpenMode mode);
    void close();
    const QString &getPortName() const;
    void flush();

    qint64 bytesAvailable() const;
    bool isSequential() const;
    bool waitForBytesWritten(int msecs);
    bool waitForReadyRead(int msecs);

protected:
    qint64 readData(char *data, qint64 maxSize);
    qint64 writeData(const char *data, qint64 maxSize);

private:
    Q_DISABLE_COPY(VirtualSerialDevice)
    void platInit();
    void platClose();
    void emitBytesWrittenIfNeeded(QMutexLocker &locker, qint64 len);

private:
    QString portName;
    mutable QMutex lock;
    QList<QByteArray> pendingWrites;
    bool emittingBytesWritten;
    QWaitCondition* waiterForBytesWritten;
    VirtualSerialDevicePrivate *d;

// Platform-specific stuff
#ifdef Q_OS_WIN
private:
    qint64 writeNextBuffer(QMutexLocker &locker);
    void doWriteCompleted(QMutexLocker &locker);
private slots:
    void writeCompleted();
    void commEventOccurred();
#endif

#ifdef Q_OS_UNIX
private:
    bool tryWrite(const char *data, qint64 maxSize, qint64 &bytesWritten);
    enum FlushPendingOption {
        NothingSpecial = 0,
        StopAfterWritingOneBuffer = 1,
        EmitBytesWrittenAsync = 2, // Needed so we don't emit bytesWritten signal directly from writeBytes
    };
    Q_DECLARE_FLAGS(FlushPendingOptions, FlushPendingOption)
    bool tryFlushPendingBuffers(QMutexLocker& locker, FlushPendingOptions flags = NothingSpecial);

private slots:
    void writeHasUnblocked(int fileHandle);

signals:
    void AsyncCall_emitBytesWrittenIfNeeded(qint64 len);

#endif

};

} // namespace SymbianUtils

#endif // VIRTUALSERIALPORT_H
