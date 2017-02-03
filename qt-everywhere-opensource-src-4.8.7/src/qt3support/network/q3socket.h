/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3Support module of the Qt Toolkit.
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

#ifndef Q3SOCKET_H
#define Q3SOCKET_H

#include <QtCore/qiodevice.h>
#include <QtNetwork/qhostaddress.h> // int->QHostAddress conversion

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Qt3Support)

class Q3SocketPrivate;
class Q3SocketDevice;

class Q_COMPAT_EXPORT Q3Socket : public QIODevice
{
    Q_OBJECT
public:
    enum Error {
	ErrConnectionRefused,
	ErrHostNotFound,
	ErrSocketRead
    };

    Q3Socket( QObject *parent=0, const char *name=0 );
    virtual ~Q3Socket();

    enum State { Idle, HostLookup, Connecting,
		 Connected, Closing,
		 Connection=Connected };
    State	 state() const;

    int		 socket() const;
    virtual void setSocket( int );

    Q3SocketDevice *socketDevice();
    virtual void setSocketDevice( Q3SocketDevice * );

#ifndef QT_NO_DNS
    virtual void connectToHost( const QString &host, Q_UINT16 port );
#endif
    QString	 peerName() const;

    // Implementation of QIODevice abstract virtual functions
    bool	 open( OpenMode mode );
    bool      open(int mode) { return open((OpenMode)mode); }
    void	 close();
    bool	 flush();
    Offset	 size() const;
    Offset	 at() const;
    bool	 at( Offset );
    bool	 atEnd() const;

    qint64	 bytesAvailable() const;
    Q_ULONG	 waitForMore( int msecs, bool *timeout  ) const;
    Q_ULONG	 waitForMore( int msecs ) const; // ### Qt 4.0: merge the two overloads
    qint64	 bytesToWrite() const;
    void	 clearPendingData();

    int		 getch();
    int		 putch( int );
    int		 ungetch(int);

    bool	 canReadLine() const;

    Q_UINT16	 port() const;
    Q_UINT16	 peerPort() const;
    QHostAddress address() const;
    QHostAddress peerAddress() const;

    void	 setReadBufferSize( Q_ULONG );
    Q_ULONG	 readBufferSize() const;

    inline bool  isSequential() const { return true; }

Q_SIGNALS:
    void	 hostFound();
    void	 connected();
    void	 connectionClosed();
    void	 delayedCloseFinished();
    void	 readyRead();
    void	 bytesWritten( int nbytes );
    void	 error( int );

protected Q_SLOTS:
    virtual void sn_read( bool force=false );
    virtual void sn_write();

protected:
    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);

private Q_SLOTS:
    void	tryConnecting();
    void	emitErrorConnectionRefused();

private:
    Q3SocketPrivate *d;

    bool	 consumeWriteBuf( Q_ULONG nbytes );
    void	 tryConnection();
    void         setSocketIntern( int socket );

private:	// Disabled copy constructor and operator=
#if defined(Q_DISABLE_COPY)
    Q3Socket( const Q3Socket & );
    Q3Socket &operator=( const Q3Socket & );
#endif
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // Q3SOCKET_H
