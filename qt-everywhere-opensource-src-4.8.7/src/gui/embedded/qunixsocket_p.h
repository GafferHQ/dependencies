/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QUNIXSOCKET_P_H
#define QUNIXSOCKET_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtNetwork/qabstractsocket.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qlist.h>
#include <QtCore/qshareddata.h>

extern "C" {
#include <sys/types.h>
};

QT_BEGIN_NAMESPACE

class QUnixSocketRights;
class QUnixSocketRightsPrivate;
class QUnixSocketPrivate;
class QUnixSocketMessagePrivate;
struct iovec;

class Q_GUI_EXPORT QUnixSocketRights {
public:
    QUnixSocketRights(int);
    ~QUnixSocketRights();

    QUnixSocketRights(const QUnixSocketRights &);
    QUnixSocketRights & operator=(const QUnixSocketRights &);

    bool isValid() const;

    int dupFd() const;
    int peekFd() const;

private:
    friend class QUnixSocket;
    QUnixSocketRights(int,int);
    QSharedDataPointer<QUnixSocketRightsPrivate> d;
};

class Q_GUI_EXPORT QUnixSocketMessage {
public:
    QUnixSocketMessage();
    QUnixSocketMessage(const QByteArray &);
    QUnixSocketMessage(const QByteArray &, const QList<QUnixSocketRights> &);
    QUnixSocketMessage(const QUnixSocketMessage &);
    QUnixSocketMessage(const iovec*, int);
    QUnixSocketMessage & operator=(const QUnixSocketMessage &);
    ~QUnixSocketMessage();

    void setBytes(const QByteArray &);
    void setRights(const QList<QUnixSocketRights> &);

    const QList<QUnixSocketRights> & rights() const;
    bool rightsWereTruncated() const;

    const QByteArray & bytes() const;

    pid_t processId() const;
    uid_t userId() const;
    gid_t groupId() const;

    void setProcessId(pid_t);
    void setUserId(uid_t);
    void setGroupId(gid_t);

    bool isValid() const;
private:
    friend class QUnixSocket;
    friend class QUnixSocketPrivate;
    QSharedDataPointer<QUnixSocketMessagePrivate> d;
};

class Q_GUI_EXPORT QUnixSocket : public QIODevice
{
    Q_OBJECT
public:
    QUnixSocket(QObject * = 0);
    QUnixSocket(qint64, qint64, QObject * = 0);
    virtual ~QUnixSocket();

    enum SocketState {
        UnconnectedState = QAbstractSocket::UnconnectedState,
        HostLookupState = QAbstractSocket::HostLookupState,
        ConnectingState = QAbstractSocket::ConnectingState,
        ConnectedState = QAbstractSocket::ConnectedState,
        BoundState = QAbstractSocket::BoundState,
        ClosingState = QAbstractSocket::ClosingState,
        ListeningState = QAbstractSocket::ListeningState,
    };

    enum SocketError { NoError, InvalidPath, ResourceError,
                       NonexistentPath, ConnectionRefused, UnknownError,
                       ReadFailure, WriteFailure };

    bool connect(const QByteArray & path);
    bool setSocketDescriptor(int socketDescriptor);
    int socketDescriptor() const;
    void abort();
    void close();

    bool flush();

    SocketError error() const;

    SocketState state() const;
    QByteArray address() const;

    qint64 bytesAvailable() const;
    qint64 bytesToWrite() const;

    qint64 readBufferSize() const;
    void setReadBufferSize(qint64 size);
    qint64 rightsBufferSize() const;
    void setRightsBufferSize(qint64 size);

    bool canReadLine() const;

    qint64 write(const char * data, qint64 maxSize)
    { return QIODevice::write(data, maxSize); }
    qint64 write(const QByteArray & byteArray)
    { return QIODevice::write(byteArray); }
    qint64 read(char * data, qint64 maxSize)
    { return QIODevice::read(data, maxSize); }
    QByteArray read(qint64 maxSize)
    { return QIODevice::read(maxSize); }

    qint64 write(const QUnixSocketMessage &);
    QUnixSocketMessage read();

    virtual bool isSequential() const;
    virtual bool waitForReadyRead(int msec = 300);
    virtual bool waitForBytesWritten(int msec = 300);

Q_SIGNALS:
    void stateChanged(SocketState socketState);

protected:
    virtual qint64 readData(char * data, qint64 maxSize);
    virtual qint64 writeData (const char * data, qint64 maxSize);

private:
    QUnixSocket(const QUnixSocket &);
    QUnixSocket & operator=(const QUnixSocket &);

    QUnixSocketPrivate * d;
};

QT_END_NAMESPACE

#endif // QUNIXSOCKET_P_H
