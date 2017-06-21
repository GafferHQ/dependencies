/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmediaplaylistnavigator_p.h"
#include "qmediaplaylistprovider_p.h"
#include "qmediaplaylist.h"
#include "qmediaobject_p.h"

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

class QMediaPlaylistNullProvider : public QMediaPlaylistProvider
{
public:
    QMediaPlaylistNullProvider() :QMediaPlaylistProvider() {}
    virtual ~QMediaPlaylistNullProvider() {}
    virtual int mediaCount() const {return 0;}
    virtual QMediaContent media(int) const { return QMediaContent(); }
};

Q_GLOBAL_STATIC(QMediaPlaylistNullProvider, _q_nullMediaPlaylist)

class QMediaPlaylistNavigatorPrivate
{
    Q_DECLARE_NON_CONST_PUBLIC(QMediaPlaylistNavigator)
public:
    QMediaPlaylistNavigatorPrivate()
        :playlist(0),
        currentPos(-1),
        lastValidPos(-1),
        playbackMode(QMediaPlaylist::Sequential),
        randomPositionsOffset(-1)
    {
    }

    QMediaPlaylistProvider *playlist;
    int currentPos;
    int lastValidPos; //to be used with CurrentItemOnce playback mode
    QMediaPlaylist::PlaybackMode playbackMode;
    QMediaContent currentItem;

    mutable QList<int> randomModePositions;
    mutable int randomPositionsOffset;

    int nextItemPos(int steps = 1) const;
    int previousItemPos(int steps = 1) const;

    void _q_mediaInserted(int start, int end);
    void _q_mediaRemoved(int start, int end);
    void _q_mediaChanged(int start, int end);

    QMediaPlaylistNavigator *q_ptr;
};


int QMediaPlaylistNavigatorPrivate::nextItemPos(int steps) const
{
    if (playlist->mediaCount() == 0)
        return -1;

    if (steps == 0)
        return currentPos;

    switch (playbackMode) {
        case QMediaPlaylist::CurrentItemOnce:
            return /*currentPos == -1 ? lastValidPos :*/ -1;
        case QMediaPlaylist::CurrentItemInLoop:
            return currentPos;
        case QMediaPlaylist::Sequential:
            {
                int nextPos = currentPos+steps;
                return nextPos < playlist->mediaCount() ? nextPos : -1;
            }
        case QMediaPlaylist::Loop:
            return (currentPos+steps) % playlist->mediaCount();
        case QMediaPlaylist::Random:
            {
                //TODO: limit the history size

                if (randomPositionsOffset == -1) {
                    randomModePositions.clear();
                    randomModePositions.append(currentPos);
                    randomPositionsOffset = 0;
                }

                while (randomModePositions.size() < randomPositionsOffset+steps+1)
                    randomModePositions.append(-1);
                int res = randomModePositions[randomPositionsOffset+steps];
                if (res<0 || res >= playlist->mediaCount()) {
                    res = qrand() % playlist->mediaCount();
                    randomModePositions[randomPositionsOffset+steps] = res;
                }

                return res;
            }
    }

    return -1;
}

int QMediaPlaylistNavigatorPrivate::previousItemPos(int steps) const
{
    if (playlist->mediaCount() == 0)
        return -1;

    if (steps == 0)
        return currentPos;

    switch (playbackMode) {
        case QMediaPlaylist::CurrentItemOnce:
            return /*currentPos == -1 ? lastValidPos :*/ -1;
        case QMediaPlaylist::CurrentItemInLoop:
            return currentPos;
        case QMediaPlaylist::Sequential:
            {
                int prevPos = currentPos == -1 ? playlist->mediaCount() - steps : currentPos - steps;
                return prevPos>=0 ? prevPos : -1;
            }
        case QMediaPlaylist::Loop:
            {
                int prevPos = currentPos - steps;
                while (prevPos<0)
                    prevPos += playlist->mediaCount();
                return prevPos;
            }
        case QMediaPlaylist::Random:
            {
                //TODO: limit the history size

                if (randomPositionsOffset == -1) {
                    randomModePositions.clear();
                    randomModePositions.append(currentPos);
                    randomPositionsOffset = 0;
                }

                while (randomPositionsOffset-steps < 0) {
                    randomModePositions.prepend(-1);
                    randomPositionsOffset++;
                }

                int res = randomModePositions[randomPositionsOffset-steps];
                if (res<0 || res >= playlist->mediaCount()) {
                    res = qrand() % playlist->mediaCount();
                    randomModePositions[randomPositionsOffset-steps] = res;
                }

                return res;
            }
    }

    return -1;
}

/*!
    \class QMediaPlaylistNavigator
    \internal

    \brief The QMediaPlaylistNavigator class provides navigation for a media playlist.
    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_playback

    \sa QMediaPlaylist, QMediaPlaylistProvider
*/


/*!
    Constructs a media playlist navigator for a \a playlist.

    The \a parent is passed to QObject.
 */
QMediaPlaylistNavigator::QMediaPlaylistNavigator(QMediaPlaylistProvider *playlist, QObject *parent)
    : QObject(parent)
    , d_ptr(new QMediaPlaylistNavigatorPrivate)
{
    d_ptr->q_ptr = this;

    setPlaylist(playlist ? playlist : _q_nullMediaPlaylist());
}

/*!
    Destroys a media playlist navigator.
 */

QMediaPlaylistNavigator::~QMediaPlaylistNavigator()
{
    delete d_ptr;
}


/*! \property QMediaPlaylistNavigator::playbackMode
    Contains the playback mode.
 */
QMediaPlaylist::PlaybackMode QMediaPlaylistNavigator::playbackMode() const
{
    return d_func()->playbackMode;
}

/*!
    Sets the playback \a mode.
 */
void QMediaPlaylistNavigator::setPlaybackMode(QMediaPlaylist::PlaybackMode mode)
{
    Q_D(QMediaPlaylistNavigator);
    if (d->playbackMode == mode)
        return;

    if (mode == QMediaPlaylist::Random) {
        d->randomPositionsOffset = 0;
        d->randomModePositions.append(d->currentPos);
    } else if (d->playbackMode == QMediaPlaylist::Random) {
        d->randomPositionsOffset = -1;
        d->randomModePositions.clear();
    }

    d->playbackMode = mode;

    emit playbackModeChanged(mode);
    emit surroundingItemsChanged();
}

/*!
    Returns the playlist being navigated.
*/

QMediaPlaylistProvider *QMediaPlaylistNavigator::playlist() const
{
    return d_func()->playlist;
}

/*!
    Sets the \a playlist to navigate.
*/
void QMediaPlaylistNavigator::setPlaylist(QMediaPlaylistProvider *playlist)
{
    Q_D(QMediaPlaylistNavigator);

    if (d->playlist == playlist)
        return;

    if (d->playlist) {
        d->playlist->disconnect(this);
    }

    if (playlist) {
        d->playlist = playlist;
    } else {
        //assign to shared readonly null playlist
        d->playlist = _q_nullMediaPlaylist();
    }

    connect(d->playlist, SIGNAL(mediaInserted(int,int)), SLOT(_q_mediaInserted(int,int)));
    connect(d->playlist, SIGNAL(mediaRemoved(int,int)), SLOT(_q_mediaRemoved(int,int)));
    connect(d->playlist, SIGNAL(mediaChanged(int,int)), SLOT(_q_mediaChanged(int,int)));

    d->randomPositionsOffset = -1;
    d->randomModePositions.clear();

    if (d->currentPos != -1) {
        d->currentPos = -1;
        emit currentIndexChanged(-1);
    }

    if (!d->currentItem.isNull()) {
        d->currentItem = QMediaContent();
        emit activated(d->currentItem); //stop playback
    }
}

/*! \property QMediaPlaylistNavigator::currentItem

  Contains the media at the current position in the playlist.

  \sa currentIndex()
*/

QMediaContent QMediaPlaylistNavigator::currentItem() const
{
    return itemAt(d_func()->currentPos);
}

/*! \fn QMediaContent QMediaPlaylistNavigator::nextItem(int steps) const

  Returns the media that is \a steps positions ahead of the current
  position in the playlist.

  \sa nextIndex()
*/
QMediaContent QMediaPlaylistNavigator::nextItem(int steps) const
{
    return itemAt(nextIndex(steps));
}

/*!
  Returns the media that is \a steps positions behind the current
  position in the playlist.

  \sa previousIndex()
 */
QMediaContent QMediaPlaylistNavigator::previousItem(int steps) const
{
    return itemAt(previousIndex(steps));
}

/*!
    Returns the media at a \a position in the playlist.
 */
QMediaContent QMediaPlaylistNavigator::itemAt(int position) const
{
    return d_func()->playlist->media(position);
}

/*! \property QMediaPlaylistNavigator::currentIndex

  Contains the position of the current media.

  If no media is current, the property contains -1.

  \sa nextIndex(), previousIndex()
*/

int QMediaPlaylistNavigator::currentIndex() const
{
    return d_func()->currentPos;
}

/*!
  Returns a position \a steps ahead of the current position
  accounting for the playbackMode().

  If the position is beyond the end of the playlist, this value
  returned is -1.

  \sa currentIndex(), previousIndex(), playbackMode()
*/

int QMediaPlaylistNavigator::nextIndex(int steps) const
{
    return d_func()->nextItemPos(steps);
}

/*!

  Returns a position \a steps behind the current position accounting
  for the playbackMode().

  If the position is prior to the beginning of the playlist this will
  return -1.

  \sa currentIndex(), nextIndex(), playbackMode()
*/
int QMediaPlaylistNavigator::previousIndex(int steps) const
{
    return d_func()->previousItemPos(steps);
}

/*!
  Advances to the next item in the playlist.

  \sa previous(), jump(), playbackMode()
 */
void QMediaPlaylistNavigator::next()
{
    Q_D(QMediaPlaylistNavigator);

    int nextPos = d->nextItemPos();

    if ( playbackMode() == QMediaPlaylist::Random )
            d->randomPositionsOffset++;

    jump(nextPos);
}

/*!
  Returns to the previous item in the playlist,

  \sa next(), jump(), playbackMode()
 */
void QMediaPlaylistNavigator::previous()
{
    Q_D(QMediaPlaylistNavigator);

    int prevPos = d->previousItemPos();
    if ( playbackMode() == QMediaPlaylist::Random )
        d->randomPositionsOffset--;

    jump(prevPos);
}

/*!
  Jumps to a new \a position in the playlist.
 */
void QMediaPlaylistNavigator::jump(int position)
{
    Q_D(QMediaPlaylistNavigator);

    if (position < -1 || position >= d->playlist->mediaCount())
        position = -1;

    if (position != -1)
        d->lastValidPos = position;

    if (playbackMode() == QMediaPlaylist::Random) {
        if (d->randomModePositions[d->randomPositionsOffset] != position) {
            d->randomModePositions.clear();
            d->randomModePositions.append(position);
            d->randomPositionsOffset = 0;
        }
    }

    if (position != -1)
        d->currentItem = d->playlist->media(position);
    else
        d->currentItem = QMediaContent();

    if (position != d->currentPos) {
        d->currentPos = position;
        emit currentIndexChanged(d->currentPos);
        emit surroundingItemsChanged();
    }

    emit activated(d->currentItem);
}

/*!
    \internal
*/
void QMediaPlaylistNavigatorPrivate::_q_mediaInserted(int start, int end)
{
    Q_Q(QMediaPlaylistNavigator);

    if (currentPos >= start) {
        currentPos = end-start+1;
        q->jump(currentPos);
    }

    //TODO: check if they really changed
    emit q->surroundingItemsChanged();
}

/*!
    \internal
*/
void QMediaPlaylistNavigatorPrivate::_q_mediaRemoved(int start, int end)
{
    Q_Q(QMediaPlaylistNavigator);

    if (currentPos > end) {
        currentPos = currentPos - end-start+1;
        q->jump(currentPos);
    } else if (currentPos >= start) {
        //current item was removed
        currentPos = qMin(start, playlist->mediaCount()-1);
        q->jump(currentPos);
    }

    //TODO: check if they really changed
    emit q->surroundingItemsChanged();
}

/*!
    \internal
*/
void QMediaPlaylistNavigatorPrivate::_q_mediaChanged(int start, int end)
{
    Q_Q(QMediaPlaylistNavigator);

    if (currentPos >= start && currentPos<=end) {
        QMediaContent src = playlist->media(currentPos);
        if (src != currentItem) {
            currentItem = src;
            emit q->activated(src);
        }
    }

    //TODO: check if they really changed
    emit q->surroundingItemsChanged();
}

/*!
    \fn QMediaPlaylistNavigator::activated(const QMediaContent &media)

    Signals that the current \a media has changed.
*/

/*!
    \fn QMediaPlaylistNavigator::currentIndexChanged(int position)

    Signals the \a position of the current media has changed.
*/

/*!
    \fn QMediaPlaylistNavigator::playbackModeChanged(QMediaPlaylist::PlaybackMode mode)

    Signals that the playback \a mode has changed.
*/

/*!
    \fn QMediaPlaylistNavigator::surroundingItemsChanged()

    Signals that media immediately surrounding the current position has changed.
*/

#include "moc_qmediaplaylistnavigator_p.cpp"
QT_END_NAMESPACE

