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

#include "qapplication.h"

#ifndef QT_NO_SOUND

#include "qsound.h"
#include "qpaintdevice.h"
#include "qwsdisplay_qws.h"
#include "qsound_p.h"

#include "qsoundqss_qws.h"

#include "qhash.h"
#include "qfileinfo.h"

#include "qbytearray.h"
#include "quuid.h"
#include "qdatastream.h"
#include "qcopchannel_qws.h"
#include "qbuffer.h"


QT_BEGIN_NAMESPACE

#ifdef MEDIA_SERVER

#define SERVER_CHANNEL "QPE/MediaServer"

class QCopMessage : public QDataStream
{
public:
    QCopMessage( const QString& channel, const QString& message )
        : QDataStream( new QBuffer ), m_channel( channel ), m_message( message )
    {
        device()->open( QIODevice::WriteOnly );
    }

    ~QCopMessage()
    {
        QCopChannel::send( m_channel, m_message, ((QBuffer*)device())->buffer() );
        delete device();
    }

private:
    QString m_channel;
    QString m_message;
};

#endif // MEDIA_SERVER

class QAuServerQWS;

class QAuBucketQWS : public QObject, public QAuBucket
{
    Q_OBJECT
public:
    QAuBucketQWS( QAuServerQWS*, QSound*, QObject* parent = 0 );

    ~QAuBucketQWS();

#ifndef MEDIA_SERVER
    int id() const { return id_; }
#endif

    QSound* sound() const { return sound_; }

#ifdef MEDIA_SERVER
    void play();

    void stop();
#endif

signals:
    // Only for Media Server
    void done( QAuBucketQWS* );

private slots:
    // Only for Media Server
    void processMessage( const QString& msg, const QByteArray& data );

private:
#ifdef MEDIA_SERVER
    QCopChannel *m_channel;
    QUuid m_id;
#endif

#ifndef MEDIA_SERVER
    int id_;
#endif
    QSound *sound_;
    QAuServerQWS *server_;

    static int next;
};

int QAuBucketQWS::next = 0;

class QAuServerQWS : public QAuServer
{
    Q_OBJECT
public:
    QAuServerQWS( QObject* parent );

    void init( QSound* s )
    {
        QAuBucketQWS *bucket = new QAuBucketQWS( this, s );
#ifdef MEDIA_SERVER
        connect( bucket, SIGNAL(done(QAuBucketQWS*)),
            this, SLOT(complete(QAuBucketQWS*)) );
#endif
        setBucket( s, bucket );
    }

#ifndef MEDIA_SERVER
    // Register bucket
    void insert( QAuBucketQWS *bucket )
    {
        buckets.insert( bucket->id(), bucket );
    }

    // Remove bucket from register
    void remove( QAuBucketQWS *bucket )
    {
        buckets.remove( bucket->id() );
    }
#endif

    void play( QSound* s )
    {
        QString filepath = QFileInfo( s->fileName() ).absoluteFilePath();
#if defined(QT_NO_QWS_SOUNDSERVER)
        server->playFile( bucket( s )->id(), filepath );
#elif defined(MEDIA_SERVER)
        bucket( s )->play();
#else
        client->play( bucket( s )->id(), filepath );
#endif
    }

    void stop( QSound* s )
    {
#if defined(QT_NO_QWS_SOUNDSERVER)
        server->stopFile( bucket( s )->id() );
#elif defined(MEDIA_SERVER)
        bucket( s )->stop();
#else
        client->stop( bucket( s )->id() );
#endif
    }

    bool okay() { return true; }

private slots:
    // Continue playing sound if loops remain
    void complete( int id )
    {
#ifndef MEDIA_SERVER
        QAuBucketQWS *bucket = find( id );
        if( bucket ) {
            QSound *sound = bucket->sound();
            if( decLoop( sound ) ) {
                play( sound );
            }
        }
#else
        Q_UNUSED(id);
#endif
    }

    // Only for Media Server
    void complete( QAuBucketQWS* bucket )
    {
#ifndef MEDIA_SERVER
        Q_UNUSED(bucket);
#else
        QSound *sound = bucket->sound();
        if( decLoop( sound ) ) {
            play( sound );
        }
#endif
    }

protected:
    QAuBucketQWS* bucket( QSound *s )
    {
        return (QAuBucketQWS*)QAuServer::bucket( s );
    }

private:
#ifndef MEDIA_SERVER
    // Find registered bucket with given id, return null if none found
    QAuBucketQWS* find( int id )
    {
        QHash<int, QAuBucketQWS*>::Iterator it = buckets.find( id );
        if( it != buckets.end() ) {
            return it.value();
        }

        return 0;
    }

    QHash<int, QAuBucketQWS*> buckets; // ### possible problem with overlapping keys

#ifdef QT_NO_QWS_SOUNDSERVER
    QWSSoundServer *server;
#else
    QWSSoundClient *client;
#endif

#endif // MEDIA_SERVER
};

QAuServerQWS::QAuServerQWS(QObject* parent) :
    QAuServer(parent)
{
#ifndef MEDIA_SERVER
    setObjectName(QLatin1String("qauserverqws"));

#ifdef QT_NO_QWS_SOUNDSERVER
    server = new QWSSoundServer( this ); // ### only suitable for single application

    connect( server, SIGNAL(soundCompleted(int)),
        this, SLOT(complete(int)) );
#else
    client = new QWSSoundClient( this ); // ### requires successful connection

    connect( client, SIGNAL(soundCompleted(int)),
        this, SLOT(complete(int)) );
#endif

#endif // MEDIA_SERVER
}

QAuBucketQWS::QAuBucketQWS( QAuServerQWS *server, QSound *sound, QObject* parent )
    : QObject( parent ), sound_( sound ), server_( server )
{
#ifdef MEDIA_SERVER
    m_id = QUuid::createUuid();

    sound->setObjectName( m_id.toString() );

    m_channel = new QCopChannel(QLatin1String("QPE/QSound/") + m_id, this );
    connect( m_channel, SIGNAL(received(QString,QByteArray)),
        this, SLOT(processMessage(QString,QByteArray)) );

    {
        QCopMessage message( QLatin1String(SERVER_CHANNEL), QLatin1String("subscribe(QUuid)") );
        message << m_id;
    }

    {
        QString filepath = QFileInfo( sound_->fileName() ).absoluteFilePath();
        QCopMessage message( QLatin1String(SERVER_CHANNEL), QLatin1String("open(QUuid,QString)") );
        message << m_id << filepath;
    }
#else
    id_ = next++;
    server_->insert( this );
#endif
}

#ifdef MEDIA_SERVER
void QAuBucketQWS::play()
{
    QString filepath = QFileInfo( sound_->fileName() ).absoluteFilePath();

    QCopMessage message( QLatin1String(SERVER_CHANNEL), QLatin1String("play(QUuid)") );
    message << m_id;
}

void QAuBucketQWS::stop()
{
    QCopMessage message( QLatin1String(SERVER_CHANNEL), QLatin1String("stop(QUuid)") );
    message << m_id;
}
#endif // MEDIA_SERVER

void QAuBucketQWS::processMessage( const QString& msg, const QByteArray& data )
{
    Q_UNUSED(data);
#ifndef MEDIA_SERVER
    Q_UNUSED(msg);
#else
    if( msg == QLatin1String("done()") ) {
        emit done( this );
    }
#endif
}

QAuBucketQWS::~QAuBucketQWS()
{
#ifdef MEDIA_SERVER
    QCopMessage message( QLatin1String(SERVER_CHANNEL), QLatin1String("revoke(QUuid)") );
    message << m_id;
#else
    server_->remove( this );
#endif
}


QAuServer* qt_new_audio_server()
{
    return new QAuServerQWS(qApp);
}

#include "qsound_qws.moc"

#endif // QT_NO_SOUND

QT_END_NAMESPACE
