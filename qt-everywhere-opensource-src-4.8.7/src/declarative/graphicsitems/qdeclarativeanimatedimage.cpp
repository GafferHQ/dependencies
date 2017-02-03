/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "private/qdeclarativeanimatedimage_p.h"
#include "private/qdeclarativeanimatedimage_p_p.h"

#ifndef QT_NO_MOVIE

#include <qdeclarativeinfo.h>
#include <private/qdeclarativeengine_p.h>

#include <QMovie>
#include <QNetworkRequest>
#include <QNetworkReply>

QT_BEGIN_NAMESPACE

/*!
    \qmlclass AnimatedImage QDeclarativeAnimatedImage
    \inherits Image
    \since 4.7
    \ingroup basic-visual-elements

    The AnimatedImage element extends the features of the \l Image element, providing
    a way to play animations stored as images containing a series of frames,
    such as those stored in GIF files.

    Information about the current frame and totla length of the animation can be
    obtained using the \l currentFrame and \l frameCount properties. You can
    start, pause and stop the animation by changing the values of the \l playing
    and \l paused properties.

    The full list of supported formats can be determined with QMovie::supportedFormats().

    \section1 Example Usage

    \beginfloatleft
    \image animatedimageitem.gif
    \endfloat

    The following QML shows how to display an animated image and obtain information
    about its state, such as the current frame and total number of frames.
    The result is an animated image with a simple progress indicator underneath it.

    \clearfloat
    \snippet doc/src/snippets/declarative/animatedimage.qml document

    \sa BorderImage, Image
*/

/*!
    \qmlproperty url AnimatedImage::source

    This property holds the URL that refers to the source image.

    AnimatedImage can handle any image format supported by Qt, loaded from any
    URL scheme supported by Qt.

    \sa QDeclarativeImageProvider
*/

/*!
    \qmlproperty bool AnimatedImage::asynchronous

    Specifies that images on the local filesystem should be loaded
    asynchronously in a separate thread.  The default value is
    false, causing the user interface thread to block while the
    image is loaded.  Setting \a asynchronous to true is useful where
    maintaining a responsive user interface is more desirable
    than having images immediately visible.

    Note that this property is only valid for images read from the
    local filesystem.  Images loaded via a network resource (e.g. HTTP)
    are always loaded asynchonously.
*/

/*!
    \qmlproperty bool AnimatedImage::cache
    \since QtQuick 1.1

    Specifies whether the image should be cached. The default value is
    true. Setting \a cache to false is useful when dealing with large images,
    to make sure that they aren't cached at the expense of small 'ui element' images.
*/

/*!
    \qmlproperty bool AnimatedImage::mirror
    \since QtQuick 1.1

    This property holds whether the image should be horizontally inverted
    (effectively displaying a mirrored image).

    The default value is false.
*/

QDeclarativeAnimatedImage::QDeclarativeAnimatedImage(QDeclarativeItem *parent)
    : QDeclarativeImage(*(new QDeclarativeAnimatedImagePrivate), parent)
{
}

QDeclarativeAnimatedImage::~QDeclarativeAnimatedImage()
{
    Q_D(QDeclarativeAnimatedImage);
    delete d->_movie;
}

/*!
  \qmlproperty bool AnimatedImage::paused
  This property holds whether the animated image is paused.

  By default, this property is false. Set it to true when you want to pause
  the animation.
*/
bool QDeclarativeAnimatedImage::isPaused() const
{
    Q_D(const QDeclarativeAnimatedImage);
    if(!d->_movie)
        return false;
    return d->_movie->state()==QMovie::Paused;
}

void QDeclarativeAnimatedImage::setPaused(bool pause)
{
    Q_D(QDeclarativeAnimatedImage);
    if(pause == d->paused)
        return;
    d->paused = pause;
    if(!d->_movie)
        return;
    d->_movie->setPaused(pause);
}
/*!
  \qmlproperty bool AnimatedImage::playing
  This property holds whether the animated image is playing.

  By default, this property is true, meaning that the animation
  will start playing immediately.
*/
bool QDeclarativeAnimatedImage::isPlaying() const
{
    Q_D(const QDeclarativeAnimatedImage);
    if (!d->_movie)
        return false;
    return d->_movie->state()!=QMovie::NotRunning;
}

void QDeclarativeAnimatedImage::setPlaying(bool play)
{
    Q_D(QDeclarativeAnimatedImage);
    if(play == d->playing)
        return;
    d->playing = play;
    if (!d->_movie)
        return;
    if (play)
        d->_movie->start();
    else
        d->_movie->stop();
}

/*!
  \qmlproperty int AnimatedImage::currentFrame
  \qmlproperty int AnimatedImage::frameCount

  currentFrame is the frame that is currently visible. By monitoring this property
  for changes, you can animate other items at the same time as the image.

  frameCount is the number of frames in the animation. For some animation formats,
  frameCount is unknown and has a value of zero.
*/
int QDeclarativeAnimatedImage::currentFrame() const
{
    Q_D(const QDeclarativeAnimatedImage);
    if (!d->_movie)
        return d->preset_currentframe;
    return d->_movie->currentFrameNumber();
}

void QDeclarativeAnimatedImage::setCurrentFrame(int frame)
{
    Q_D(QDeclarativeAnimatedImage);
    if (!d->_movie) {
        d->preset_currentframe = frame;
        return;
    }
    d->_movie->jumpToFrame(frame);
}

int QDeclarativeAnimatedImage::frameCount() const
{
    Q_D(const QDeclarativeAnimatedImage);
    if (!d->_movie)
        return 0;
    return d->_movie->frameCount();
}

void QDeclarativeAnimatedImage::setSource(const QUrl &url)
{
    Q_D(QDeclarativeAnimatedImage);
    if (url == d->url)
        return;

    delete d->_movie;
    d->_movie = 0;

    if (d->reply) {
        d->reply->deleteLater();
        d->reply = 0;
    }

    d->url = url;
    emit sourceChanged(d->url);

    if (isComponentComplete())
        load();
}

void QDeclarativeAnimatedImage::load()
{
    Q_D(QDeclarativeAnimatedImage);

    QDeclarativeImageBase::Status oldStatus = d->status;
    qreal oldProgress = d->progress;

    if (d->url.isEmpty()) {
        delete d->_movie;
        d->setPixmap(QPixmap());
        d->progress = 0;
        d->status = Null;
        if (d->status != oldStatus)
            emit statusChanged(d->status);
        if (d->progress != oldProgress)
            emit progressChanged(d->progress);
    } else {
#ifndef QT_NO_LOCALFILE_OPTIMIZED_QML
        QString lf = QDeclarativeEnginePrivate::urlToLocalFileOrQrc(d->url);
        if (!lf.isEmpty()) {
            //### should be unified with movieRequestFinished
            d->_movie = new QMovie(lf);
            if (!d->_movie->isValid()){
                qmlInfo(this) << "Error Reading Animated Image File " << d->url.toString();
                delete d->_movie;
                d->_movie = 0;
                d->status = Error;
                if (d->status != oldStatus)
                    emit statusChanged(d->status);
                return;
            }
            connect(d->_movie, SIGNAL(stateChanged(QMovie::MovieState)),
                    this, SLOT(playingStatusChanged()));
            connect(d->_movie, SIGNAL(frameChanged(int)),
                    this, SLOT(movieUpdate()));
            d->_movie->setCacheMode(QMovie::CacheAll);
            if(d->playing)
                d->_movie->start();
            else
                d->_movie->jumpToFrame(0);
            if(d->paused)
                d->_movie->setPaused(true);
            d->setPixmap(d->_movie->currentPixmap());
            d->status = Ready;
            d->progress = 1.0;
            if (d->status != oldStatus)
                emit statusChanged(d->status);
            if (d->progress != oldProgress)
                emit progressChanged(d->progress);
            return;
        }
#endif
        d->status = Loading;
        d->progress = 0;
        emit statusChanged(d->status);
        emit progressChanged(d->progress);
        QNetworkRequest req(d->url);
        req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
        d->reply = qmlEngine(this)->networkAccessManager()->get(req);
        QObject::connect(d->reply, SIGNAL(finished()),
                         this, SLOT(movieRequestFinished()));
        QObject::connect(d->reply, SIGNAL(downloadProgress(qint64,qint64)),
                         this, SLOT(requestProgress(qint64,qint64)));
    }
}

#define ANIMATEDIMAGE_MAXIMUM_REDIRECT_RECURSION 16

void QDeclarativeAnimatedImage::movieRequestFinished()
{
    Q_D(QDeclarativeAnimatedImage);

    d->redirectCount++;
    if (d->redirectCount < ANIMATEDIMAGE_MAXIMUM_REDIRECT_RECURSION) {
        QVariant redirect = d->reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redirect.isValid()) {
            QUrl url = d->reply->url().resolved(redirect.toUrl());
            d->reply->deleteLater();
            d->reply = 0;
            setSource(url);
            return;
        }
    }
    d->redirectCount=0;

    d->_movie = new QMovie(d->reply);
    if (!d->_movie->isValid()){
#ifndef QT_NO_DEBUG_STREAM
        qmlInfo(this) << "Error Reading Animated Image File " << d->url;
#endif
        delete d->_movie;
        d->_movie = 0;
        d->status = Error;
        emit statusChanged(d->status);
        return;
    }
    connect(d->_movie, SIGNAL(stateChanged(QMovie::MovieState)),
            this, SLOT(playingStatusChanged()));
    connect(d->_movie, SIGNAL(frameChanged(int)),
            this, SLOT(movieUpdate()));
    d->_movie->setCacheMode(QMovie::CacheAll);
    if(d->playing)
        d->_movie->start();
    if (d->paused || !d->playing) {
        d->_movie->jumpToFrame(d->preset_currentframe);
        d->preset_currentframe = 0;
    }
    if(d->paused)
        d->_movie->setPaused(true);
    d->setPixmap(d->_movie->currentPixmap());
    d->status = Ready;
    emit statusChanged(d->status);
}

void QDeclarativeAnimatedImage::movieUpdate()
{
    Q_D(QDeclarativeAnimatedImage);
    d->setPixmap(d->_movie->currentPixmap());
    emit frameChanged();
}

void QDeclarativeAnimatedImage::playingStatusChanged()
{
    Q_D(QDeclarativeAnimatedImage);
    if((d->_movie->state() != QMovie::NotRunning) != d->playing){
        d->playing = (d->_movie->state() != QMovie::NotRunning);
        emit playingChanged();
    }
    if((d->_movie->state() == QMovie::Paused) != d->paused){
        d->playing = (d->_movie->state() == QMovie::Paused);
        emit pausedChanged();
    }
}

void QDeclarativeAnimatedImage::componentComplete()
{
    Q_D(QDeclarativeAnimatedImage);
    QDeclarativeItem::componentComplete(); // NOT QDeclarativeImage
    if (d->url.isValid())
        load();
    if (!d->reply) {
        setCurrentFrame(d->preset_currentframe);
        d->preset_currentframe = 0;
    }
}

QT_END_NAMESPACE

#endif // QT_NO_MOVIE
