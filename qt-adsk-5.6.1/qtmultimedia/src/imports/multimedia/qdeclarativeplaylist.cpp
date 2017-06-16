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

#include "qdeclarativeplaylist_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype PlaylistItem
    \instantiates QDeclarativePlaylistItem
    \since 5.6

    \inqmlmodule QtMultimedia
    \ingroup multimedia_qml
    \ingroup multimedia_audio_qml
    \ingroup multimedia_video_qml
    \brief Defines an item in a Playlist.

    \sa Playlist
*/

/*!
    \qmlproperty url QtMultimedia::PlaylistItem::source

    This property holds the source URL of the item.

    \sa Playlist
*/
QDeclarativePlaylistItem::QDeclarativePlaylistItem(QObject *parent)
    : QObject(parent)
{
}

QUrl QDeclarativePlaylistItem::source() const
{
    return m_source;
}

void QDeclarativePlaylistItem::setSource(const QUrl &source)
{
    m_source = source;
}

/*!
    \qmltype Playlist
    \instantiates QDeclarativePlaylist
    \since 5.6
    \brief For specifying a list of media to be played.

    \inqmlmodule QtMultimedia
    \ingroup multimedia_qml
    \ingroup multimedia_audio_qml
    \ingroup multimedia_video_qml

    The Playlist type provides a way to play a list of media with the MediaPlayer, Audio and Video
    types. It can be used as a data source for view elements (such as ListView) and other elements
    that interact with model data (such as Repeater). When used as a data model, each playlist
    item's source URL can be accessed using the \c source role.

    \qml
    import QtQuick 2.0
    import QtMultimedia 5.6

    Item {
        width: 400;
        height: 300;

        Audio {
            id: player;
            playlist: Playlist {
                id: playlist
                PlaylistItem { source: "song1.ogg"; }
                PlaylistItem { source: "song2.ogg"; }
                PlaylistItem { source: "song3.ogg"; }
            }
        }

        ListView {
            model: playlist;
            delegate: Text {
                font.pixelSize: 16;
                text: source;
            }
        }

        MouseArea {
            anchors.fill: parent;
            onPressed: {
                if (player.playbackState != Audio.PlayingState) {
                    player.play();
                } else {
                    player.pause();
                }
            }
        }
    }
    \endqml

    \sa MediaPlayer, Audio, Video
*/

void QDeclarativePlaylist::_q_mediaAboutToBeInserted(int start, int end)
{
    emit itemAboutToBeInserted(start, end);

    beginInsertRows(QModelIndex(), start, end);
}

void QDeclarativePlaylist::_q_mediaInserted(int start, int end)
{
    endInsertRows();

    emit itemCountChanged();
    emit itemInserted(start, end);
}

void QDeclarativePlaylist::_q_mediaAboutToBeRemoved(int start, int end)
{
    emit itemAboutToBeRemoved(start, end);

    beginRemoveRows(QModelIndex(), start, end);
}

void QDeclarativePlaylist::_q_mediaRemoved(int start, int end)
{
    endRemoveRows();

    emit itemCountChanged();
    emit itemRemoved(start, end);
}

void QDeclarativePlaylist::_q_mediaChanged(int start, int end)
{
    emit dataChanged(createIndex(start, 0), createIndex(end, 0));
    emit itemChanged(start, end);
}

void QDeclarativePlaylist::_q_loadFailed()
{
    m_error = m_playlist->error();
    m_errorString = m_playlist->errorString();

    emit error(Error(m_error), m_errorString);
    emit errorChanged();
    emit loadFailed();
}

QDeclarativePlaylist::QDeclarativePlaylist(QObject *parent)
    : QAbstractListModel(parent)
    , m_playlist(0)
    , m_error(QMediaPlaylist::NoError)
    , m_readOnly(false)
{
}

QDeclarativePlaylist::~QDeclarativePlaylist()
{
    delete m_playlist;
}

/*!
    \qmlproperty enumeration QtMultimedia::Playlist::playbackMode

    This property holds the order in which items in the playlist are played.

    \table
    \header \li Value \li Description
    \row \li CurrentItemOnce
        \li The current item is played only once.
    \row \li CurrentItemInLoop
        \li The current item is played repeatedly in a loop.
    \row \li Sequential
        \li Playback starts from the current and moves through each successive item until the last
           is reached and then stops. The next item is a null item when the last one is currently
           playing.
    \row \li Loop
        \li Playback restarts at the first item after the last has finished playing.
    \row \li Random
        \li Play items in random order.
    \endtable
 */
QDeclarativePlaylist::PlaybackMode QDeclarativePlaylist::playbackMode() const
{
    return PlaybackMode(m_playlist->playbackMode());
}

void QDeclarativePlaylist::setPlaybackMode(PlaybackMode mode)
{
    if (playbackMode() == mode)
        return;

    m_playlist->setPlaybackMode(QMediaPlaylist::PlaybackMode(mode));
}

/*!
    \qmlproperty url QtMultimedia::Playlist::currentItemsource

    This property holds the source URL of the current item in the playlist.
 */
QUrl QDeclarativePlaylist::currentItemSource() const
{
    return m_playlist->currentMedia().canonicalUrl();
}

/*!
    \qmlproperty int QtMultimedia::Playlist::currentIndex

    This property holds the position of the current item in the playlist.
 */
int QDeclarativePlaylist::currentIndex() const
{
    return m_playlist->currentIndex();
}

void QDeclarativePlaylist::setCurrentIndex(int index)
{
    if (currentIndex() == index)
        return;

    m_playlist->setCurrentIndex(index);
}

/*!
    \qmlproperty int QtMultimedia::Playlist::itemCount

    This property holds the number of items in the playlist.
 */
int QDeclarativePlaylist::itemCount() const
{
    return m_playlist->mediaCount();
}

/*!
    \qmlproperty bool QtMultimedia::Playlist::readOnly

    This property indicates if the playlist can be modified.
 */
bool QDeclarativePlaylist::readOnly() const
{
    // There's no signal to tell whether or not the read only state changed, so we consider it fixed
    // after its initial retrieval in componentComplete().
    return m_readOnly;
}

/*!
    \qmlproperty enumeration QtMultimedia::Playlist::error

    This property holds the error condition of the playlist.

    \table
    \header \li Value \li Description
    \row \li NoError
        \li No errors
    \row \li FormatError
        \li Format error.
    \row \li FormatNotSupportedError
        \li Format not supported.
    \row \li NetworkError
        \li Network error.
    \row \li AccessDeniedError
        \li Access denied error.
    \endtable
 */
QDeclarativePlaylist::Error QDeclarativePlaylist::error() const
{
    return Error(m_error);
}

/*!
    \qmlproperty string QtMultimedia::Playlist::errorString

    This property holds a string describing the current error condition of the playlist.
*/
QString QDeclarativePlaylist::errorString() const
{
    return m_errorString;
}

/*!
    \qmlmethod url QtMultimedia::Playlist::itemSource(index)

    Returns the source URL of the item at the given \a index in the playlist.
*/
QUrl QDeclarativePlaylist::itemSource(int index)
{
    return m_playlist->media(index).canonicalUrl();
}

/*!
    \qmlmethod int QtMultimedia::Playlist::nextIndex(steps)

    Returns the index of the item in the playlist which would be current after calling next()
    \a steps times.

    Returned value depends on the size of the playlist, the current position and the playback mode.

    \sa playbackMode, previousIndex()
*/
int QDeclarativePlaylist::nextIndex(int steps)
{
    return m_playlist->nextIndex(steps);
}

/*!
    \qmlmethod int QtMultimedia::Playlist::previousIndex(steps)

    Returns the index of the item in the playlist which would be current after calling previous()
    \a steps times.

    Returned value depends on the size of the playlist, the current position and the playback mode.

    \sa playbackMode, nextIndex()
*/
int QDeclarativePlaylist::previousIndex(int steps)
{
    return m_playlist->previousIndex(steps);
}

/*!
    \qmlmethod QtMultimedia::Playlist::next()

    Advances to the next item in the playlist.
*/
void QDeclarativePlaylist::next()
{
    m_playlist->next();
}

/*!
    \qmlmethod QtMultimedia::Playlist::previous()

    Returns to the previous item in the playlist.
*/
void QDeclarativePlaylist::previous()
{
    m_playlist->previous();
}

/*!
    \qmlmethod QtMultimedia::Playlist::shuffle()

    Shuffles items in the playlist.
*/
void QDeclarativePlaylist::shuffle()
{
    m_playlist->shuffle();
}

/*!
    \qmlmethod QtMultimedia::Playlist::load(location, format)

    Loads a playlist from the given \a location. If \a format is specified, it is used, otherwise
    the format is guessed from the location name and the data.

    New items are appended to the playlist.

    \c onloaded() is emitted if the playlist loads successfully, otherwise \c onLoadFailed() is
    emitted with \l error and \l errorString defined accordingly.
*/
void QDeclarativePlaylist::load(const QUrl &location, const QString &format)
{
    m_error = QMediaPlaylist::NoError;
    m_errorString = QString();
    emit errorChanged();
    m_playlist->load(location, format.toLatin1().constData());
}

/*!
    \qmlmethod bool QtMultimedia::Playlist::save(location, format)

    Saves the playlist to the given \a location. If \a format is specified, it is used, otherwise
    the format is guessed from the location name.

    Returns true if the playlist is saved successfully.
*/
bool QDeclarativePlaylist::save(const QUrl &location, const QString &format)
{
    return m_playlist->save(location, format.toLatin1().constData());
}

/*!
    \qmlmethod bool QtMultimedia::Playlist::addItem(source)

    Appends the \a source URL to the playlist.

    Returns true if the \a source is added successfully.
*/
bool QDeclarativePlaylist::addItem(const QUrl &source)
{
    return m_playlist->addMedia(QMediaContent(source));
}

/*!
    \qmlmethod bool QtMultimedia::Playlist::insertItem(index, source)

    Inserts the \a source URL to the playlist at the given \a index.

    Returns true if the \a source is added successfully.
*/
bool QDeclarativePlaylist::insertItem(int index, const QUrl &source)
{
    return m_playlist->insertMedia(index, QMediaContent(source));
}

/*!
    \qmlmethod bool QtMultimedia::Playlist::removeItem(index)

    Removed the item at the given \a index from the playlist.

    Returns true if the \a source is removed successfully.
*/
bool QDeclarativePlaylist::removeItem(int index)
{
    return m_playlist->removeMedia(index);
}

/*!
    \qmlmethod bool QtMultimedia::Playlist::clear()

    Removes all the items from the playlist.

    Returns true if the operation is successful.
*/
bool QDeclarativePlaylist::clear()
{
    return m_playlist->clear();
}

int QDeclarativePlaylist::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_playlist->mediaCount();
}

QVariant QDeclarativePlaylist::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(role);

    if (!index.isValid())
        return QVariant();

    return m_playlist->media(index.row()).canonicalUrl();
}

QHash<int, QByteArray> QDeclarativePlaylist::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[SourceRole] = "source";
    return roleNames;
}

void QDeclarativePlaylist::classBegin()
{
    m_playlist = new QMediaPlaylist(this);

    connect(m_playlist, SIGNAL(currentIndexChanged(int)),
            this, SIGNAL(currentIndexChanged()));
    connect(m_playlist, SIGNAL(playbackModeChanged(QMediaPlaylist::PlaybackMode)),
            this, SIGNAL(playbackModeChanged()));
    connect(m_playlist, SIGNAL(currentMediaChanged(QMediaContent)),
            this, SIGNAL(currentItemSourceChanged()));
    connect(m_playlist, SIGNAL(mediaAboutToBeInserted(int,int)),
            this, SLOT(_q_mediaAboutToBeInserted(int,int)));
    connect(m_playlist, SIGNAL(mediaInserted(int,int)),
            this, SLOT(_q_mediaInserted(int,int)));
    connect(m_playlist, SIGNAL(mediaAboutToBeRemoved(int,int)),
            this, SLOT(_q_mediaAboutToBeRemoved(int,int)));
    connect(m_playlist, SIGNAL(mediaRemoved(int,int)),
            this, SLOT(_q_mediaRemoved(int,int)));
    connect(m_playlist, SIGNAL(mediaChanged(int,int)),
            this, SLOT(_q_mediaChanged(int,int)));
    connect(m_playlist, SIGNAL(loaded()),
            this, SIGNAL(loaded()));
    connect(m_playlist, SIGNAL(loadFailed()),
            this, SLOT(_q_loadFailed()));

    if (m_playlist->isReadOnly()) {
        m_readOnly = true;
        emit readOnlyChanged();
    }
}

void QDeclarativePlaylist::componentComplete()
{
}

/*!
    \qmlsignal QtMultimedia::Audio::itemAboutToBeInserted(start, end)

    This signal is emitted when items are to be inserted into the playlist at \a start and ending at
    \a end.

    The corresponding handler is \c onItemAboutToBeInserted.
*/

/*!
    \qmlsignal QtMultimedia::Audio::itemInserted(start, end)

    This signal is emitted after items have been inserted into the playlist. The new items are those
    between \a start and \a end inclusive.

    The corresponding handler is \c onItemInserted.
*/

/*!
    \qmlsignal QtMultimedia::Audio::itemAboutToBeRemoved(start, end)

    This signal emitted when items are to be deleted from the playlist at \a start and ending at
    \a end.

    The corresponding handler is \c onItemAboutToBeRemoved.
*/

/*!
    \qmlsignal QtMultimedia::Audio::itemRemoved(start, end)

    This signal is emitted after items have been removed from the playlist. The removed items are
    those between \a start and \a end inclusive.

    The corresponding handler is \c onMediaRemoved.
*/

/*!
    \qmlsignal QtMultimedia::Audio::itemChanged(start, end)

    This signal is emitted after items have been changed in the playlist between \a start and
    \a end positions inclusive.

    The corresponding handler is \c onItemChanged.
*/

/*!
    \qmlsignal QtMultimedia::Audio::loaded()

    This signal is emitted when the playlist loading succeeded.

    The corresponding handler is \c onLoaded.
*/

/*!
    \qmlsignal QtMultimedia::Audio::loadFailed()

    This signal is emitted when the playlist loading failed. \l error and \l errorString can be
    checked for more information on the failure.

    The corresponding handler is \c onLoadFailed.
*/

QT_END_NAMESPACE

#include "moc_qdeclarativeplaylist_p.cpp"
