/*  This file is part of the KDE project.

Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).

This library is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 2.1 or 3 of the License.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this library.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "audiooutput.h"
#include "audioplayer.h"
#include "defs.h"
#include "dummyplayer.h"
#include "utils.h"
#include "utils.h"

#ifdef PHONON_MMF_VIDEO_SURFACES
#include "videoplayer_surface.h"
#else
#include "videoplayer_dsa.h"
#endif

#include "videowidget.h"

#include "mediaobject.h"

#include <QDir>
#include <QResource>
#include <QUrl>
#include <cdbcols.h>
#include <cdblen.h>
#include <commdb.h>
#include <mmf/common/mmfcontrollerframeworkbase.h>

QT_BEGIN_NAMESPACE

using namespace Phonon;
using namespace Phonon::MMF;

/*! \class MMF::MediaObject
  \internal
*/

//-----------------------------------------------------------------------------
// Constructor / destructor
//-----------------------------------------------------------------------------

MMF::MediaObject::MediaObject(QObject *parent) : MMF::MediaNode::MediaNode(parent)
                                               , m_recognizerOpened(false)
                                               , m_nextSourceSet(false)
                                               , m_file(0)
                                               , m_resource(0)
{
    m_player.reset(new DummyPlayer());

    TRACE_CONTEXT(MediaObject::MediaObject, EAudioApi);
    TRACE_ENTRY_0();

    const int err = m_fileServer.Connect();
    QT_TRAP_THROWING(User::LeaveIfError(err));

    parent->installEventFilter(this);
    m_iap = KUseDefaultIap;

    TRACE_EXIT_0();
}

MMF::MediaObject::~MediaObject()
{
    TRACE_CONTEXT(MediaObject::~MediaObject, EAudioApi);
    TRACE_ENTRY_0();

    parent()->removeEventFilter(this);
    delete m_resource;

    if (m_file)
        m_file->Close();
    delete m_file;

    m_fileServer.Close();
    m_recognizer.Close();

    TRACE_EXIT_0();
}


//-----------------------------------------------------------------------------
// Recognizer
//-----------------------------------------------------------------------------

bool MMF::MediaObject::openRecognizer()
{
    TRACE_CONTEXT(MediaObject::openRecognizer, EAudioInternal);

    if (!m_recognizerOpened) {
        TInt err = m_recognizer.Connect();
        if (KErrNone != err) {
            TRACE("RApaLsSession::Connect error %d", err);
            return false;
        }

        // This must be called in order to be able to share file handles with
        // the recognizer server (see fileMediaType function).
        err = m_fileServer.ShareProtected();
        if (KErrNone != err) {
            TRACE("RFs::ShareProtected error %d", err);
            return false;
        }

        m_recognizerOpened = true;
    }

    return true;
}

MMF::MediaType MMF::MediaObject::fileMediaType
(const QString& fileName)
{
    TRACE_CONTEXT(MediaObject::fileMediaType, EAudioInternal);

    MediaType result = MediaTypeUnknown;

    if (openRecognizer()) {
        TInt err = openFileHandle(fileName);
        const QHBufC nativeFileName(QDir::toNativeSeparators(fileName));
        if (KErrNone == err) {
            TDataRecognitionResult recognizerResult;
            err = m_recognizer.RecognizeData(*m_file, recognizerResult);
            if (KErrNone == err) {
                const TPtrC mimeType = recognizerResult.iDataType.Des();
                result = Utils::mimeTypeToMediaType(mimeType);
            } else {
                TRACE("RApaLsSession::RecognizeData filename %S error %d", nativeFileName.data(), err);
            }
        } else {
            TRACE("RFile::Open filename %S error %d", nativeFileName.data(), err);
        }
    }

    return result;
}

int MMF::MediaObject::openFileHandle(const QString &fileName)
{
    TRACE_CONTEXT(MediaObject::openFileHandle, EAudioInternal);
    const QHBufC nativeFileName(QDir::toNativeSeparators(fileName));
    TRACE_ENTRY("filename %S", nativeFileName.data());
    if (m_file)
        m_file->Close();
    delete m_file;
    m_file = 0;
    m_file = new RFile;
    TInt err = m_file->Open(m_fileServer, *nativeFileName, EFileRead | EFileShareReadersOrWriters);
    return err;
}

MMF::MediaType MMF::MediaObject::bufferMediaType(const uchar *data, qint64 size)
{
    TRACE_CONTEXT(MediaObject::bufferMediaType, EAudioInternal);
    MediaType result = MediaTypeUnknown;
    if (openRecognizer()) {
        TDataRecognitionResult recognizerResult;
        const TPtrC8 des(data, size);
        const TInt err = m_recognizer.RecognizeData(KNullDesC, des, recognizerResult);
        if (KErrNone == err) {
            const TPtrC mimeType = recognizerResult.iDataType.Des();
            result = Utils::mimeTypeToMediaType(mimeType);
        } else {
            TRACE("RApaLsSession::RecognizeData error %d", err);
        }
    }
    return result;
}

//-----------------------------------------------------------------------------
// MediaObjectInterface
//-----------------------------------------------------------------------------

void MMF::MediaObject::play()
{
    m_player->play();
}

void MMF::MediaObject::pause()
{
    m_player->pause();
}

void MMF::MediaObject::stop()
{
    m_player->stop();
}

void MMF::MediaObject::seek(qint64 ms)
{
    m_player->seek(ms);

    if (state() == PausedState or state() == PlayingState) {
        emit tick(currentTime());
    }
}

qint32 MMF::MediaObject::tickInterval() const
{
    return m_player->tickInterval();
}

void MMF::MediaObject::setTickInterval(qint32 interval)
{
    m_player->setTickInterval(interval);
}

bool MMF::MediaObject::hasVideo() const
{
    return m_player->hasVideo();
}

bool MMF::MediaObject::isSeekable() const
{
    return m_player->isSeekable();
}

Phonon::State MMF::MediaObject::state() const
{
    return m_player->state();
}

qint64 MMF::MediaObject::currentTime() const
{
    return m_player->currentTime();
}

QString MMF::MediaObject::errorString() const
{
    return m_player->errorString();
}

Phonon::ErrorType MMF::MediaObject::errorType() const
{
    return m_player->errorType();
}

qint64 MMF::MediaObject::totalTime() const
{
    return m_player->totalTime();
}

MediaSource MMF::MediaObject::source() const
{
    return m_source;
}

void MMF::MediaObject::setSource(const MediaSource &source)
{
    switchToSource(source);
}

void MMF::MediaObject::switchToSource(const MediaSource &source)
{
    if (m_file)
        m_file->Close();
    delete m_file;
    m_file = 0;

    delete m_resource;
    m_resource = 0;

    createPlayer(source);
    m_source = source;
    m_player->open();
    emit currentSourceChanged(m_source);
}

void MMF::MediaObject::createPlayer(const MediaSource &source)
{
    TRACE_CONTEXT(MediaObject::createPlayer, EAudioApi);
    TRACE_ENTRY("state %d source.type %d", state(), source.type());
    TRACE_ENTRY("source.type %d", source.type());

    MediaType mediaType = MediaTypeUnknown;

    AbstractPlayer* oldPlayer = m_player.data();

    const bool oldPlayerHasVideo = oldPlayer->hasVideo();
    const bool oldPlayerSeekable = oldPlayer->isSeekable();

    QString errorMessage;

    // Determine media type
    switch (source.type()) {
    case MediaSource::LocalFile:
        mediaType = fileMediaType(source.fileName());
        break;

    case MediaSource::Url:
        {
            const QUrl url(source.url());
            if (url.scheme() == QLatin1String("file")) {
                mediaType = fileMediaType(url.toLocalFile());
            }
            else {
                // Streaming playback is generally not supported by the implementation
                // of the audio player API, so we use CVideoPlayerUtility for both
                // audio and video streaming.
                mediaType = MediaTypeVideo;
            }
        }
        break;

    case MediaSource::Invalid:
    case MediaSource::Disc:
        errorMessage = tr("Error opening source: type not supported");
        break;

    case MediaSource::Stream:
        {
            const QString fileName = source.url().toLocalFile();
            if (fileName.startsWith(QLatin1String(":/")) || fileName.startsWith(QLatin1String("qrc://"))) {
                Q_ASSERT(!m_resource);
                m_resource = new QResource(fileName);
                if (m_resource->isValid()) {
                    if (m_resource->isCompressed())
                        errorMessage = tr("Error opening source: resource is compressed");
                    else
		        mediaType = bufferMediaType(m_resource->data(), m_resource->size());
		} else {
                    errorMessage = tr("Error opening source: resource not valid");
                }
            } else {
                errorMessage = tr("Error opening source: type not supported");
            }
        }
        break;

    case MediaSource::Empty:
        TRACE_0("Empty media source");
        break;
    }

    if (oldPlayer)
        oldPlayer->close();

    AbstractPlayer* newPlayer = 0;

    // Construct newPlayer using oldPlayer (if not 0) in order to copy
    // parameters (volume, prefinishMark, transitionTime) which may have
    // been set on oldPlayer.

    switch (mediaType) {
    case MediaTypeUnknown:
        TRACE_0("Media type could not be determined");
        newPlayer = new DummyPlayer(oldPlayer);
        errorMessage = tr("Error opening source: media type could not be determined");
        break;

    case MediaTypeAudio:
        newPlayer = new AudioPlayer(this, oldPlayer);
        break;

    case MediaTypeVideo:
#ifdef PHONON_MMF_VIDEO_SURFACES
        newPlayer = SurfaceVideoPlayer::create(this, oldPlayer);
#else
        newPlayer = DsaVideoPlayer::create(this, oldPlayer);
#endif
        break;
    }

    if (oldPlayer)
        emit abstractPlayerChanged(0);
    m_player.reset(newPlayer);
    emit abstractPlayerChanged(newPlayer);

    if (oldPlayerHasVideo != hasVideo()) {
        emit hasVideoChanged(hasVideo());
    }

    if (oldPlayerSeekable != isSeekable()) {
        emit seekableChanged(isSeekable());
    }

    connect(m_player.data(), SIGNAL(totalTimeChanged(qint64)), SIGNAL(totalTimeChanged(qint64)));
    connect(m_player.data(), SIGNAL(stateChanged(Phonon::State,Phonon::State)), SIGNAL(stateChanged(Phonon::State,Phonon::State)));
    connect(m_player.data(), SIGNAL(finished()), SIGNAL(finished()));
    connect(m_player.data(), SIGNAL(bufferStatus(int)), SIGNAL(bufferStatus(int)));
    connect(m_player.data(), SIGNAL(metaDataChanged(QMultiMap<QString,QString>)), SIGNAL(metaDataChanged(QMultiMap<QString,QString>)));
    connect(m_player.data(), SIGNAL(aboutToFinish()), SIGNAL(aboutToFinish()));
    connect(m_player.data(), SIGNAL(prefinishMarkReached(qint32)), SIGNAL(prefinishMarkReached(qint32)));
    connect(m_player.data(), SIGNAL(prefinishMarkReached(qint32)), SLOT(handlePrefinishMarkReached(qint32)));
    connect(m_player.data(), SIGNAL(tick(qint64)), SIGNAL(tick(qint64)));

    // We need to call setError() after doing the connects, otherwise the
    // error won't be received.
    if (!errorMessage.isEmpty()) {
        Q_ASSERT(m_player);
        m_player->setError(errorMessage);
    }

    TRACE_EXIT_0();
}

void MMF::MediaObject::setNextSource(const MediaSource &source)
{
    m_nextSource = source;
    m_nextSourceSet = true;
}

qint32 MMF::MediaObject::prefinishMark() const
{
    return m_player->prefinishMark();
}

void MMF::MediaObject::setPrefinishMark(qint32 mark)
{
    m_player->setPrefinishMark(mark);
}

qint32 MMF::MediaObject::transitionTime() const
{
    return m_player->transitionTime();
}

void MMF::MediaObject::setTransitionTime(qint32 time)
{
    m_player->setTransitionTime(time);
}

void MMF::MediaObject::volumeChanged(qreal volume)
{
    m_player->volumeChanged(volume);
}

RFile* MMF::MediaObject::file() const
{
    return m_file;
}

QResource* MMF::MediaObject::resource() const
{
    return m_resource;
}

//-----------------------------------------------------------------------------
// MediaNode
//-----------------------------------------------------------------------------

void MMF::MediaObject::connectMediaObject(MediaObject * /*mediaObject*/)
{
    // This function should never be called - see MediaNode::setMediaObject()
    Q_ASSERT_X(false, Q_FUNC_INFO,
        "Connection of MediaObject to MediaObject");
}

void MMF::MediaObject::disconnectMediaObject(MediaObject * /*mediaObject*/)
{
    // This function should never be called - see MediaNode::setMediaObject()
    Q_ASSERT_X(false, Q_FUNC_INFO,
        "Disconnection of MediaObject from MediaObject");
}


//-----------------------------------------------------------------------------
// Video output
//-----------------------------------------------------------------------------

void MMF::MediaObject::setVideoOutput(AbstractVideoOutput* videoOutput)
{
    m_player->setVideoOutput(videoOutput);
}


AbstractPlayer *MMF::MediaObject::abstractPlayer() const
{
    return m_player.data();
}

//-----------------------------------------------------------------------------
// Playlist support
//-----------------------------------------------------------------------------

void MMF::MediaObject::switchToNextSource()
{
    if (m_nextSourceSet) {
        m_nextSourceSet = false;
        switchToSource(m_nextSource);
        play();
    } else {
        emit finished();
    }
}

//-----------------------------------------------------------------------------
// IAP support
//-----------------------------------------------------------------------------

int MMF::MediaObject::currentIAP() const
{
    return m_iap;
}

bool MMF::MediaObject::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::DynamicPropertyChange ) {
        QDynamicPropertyChangeEvent* dynamicEvent = static_cast<QDynamicPropertyChangeEvent*>(event);
        if (dynamicEvent->propertyName() == "InternetAccessPointName") {
            QVariant value = watched->property("InternetAccessPointName");
            if (value.isValid()) {
                QString iapName = value.toString();
                TRAPD(err, setIAPIdFromNameL(iapName));
                if (err)
                    m_player->setError(tr("Failed to set requested IAP"), err);
            }
        }
    }
    return false;
}

void MMF::MediaObject::setIAPIdFromNameL(const QString& iapString)
{
    TRACE_CONTEXT(MediaObject::getIapIdFromName, EVideoInternal);
    TBuf<KCommsDbSvrMaxColumnNameLength> iapDes = qt_QString2TPtrC(iapString);
    CCommsDatabase *commsDb = CCommsDatabase::NewL(EDatabaseTypeIAP);
    CleanupStack::PushL(commsDb);
    commsDb->ShowHiddenRecords();
    CCommsDbTableView *view = commsDb->OpenTableLC(TPtrC(IAP));
    for (TInt l = view->GotoFirstRecord(); l != KErrNotFound; l = view->GotoNextRecord()) {
        TBuf<KCommsDbSvrMaxColumnNameLength> iapName;
        view->ReadTextL(TPtrC(COMMDB_NAME), iapName);
        TRACE("found IAP %S", &iapName);
        if (iapName.CompareF(iapDes) == 0) {
            TUint32 uiap;
            view->ReadUintL(TPtrC(COMMDB_ID), uiap);
            TRACE("matched IAP %S, setting m_iap %d", &iapName, uiap);
            m_iap = uiap;
            break;
        }
    }
    CleanupStack::PopAndDestroy(2); // commsDb, view
}

//-----------------------------------------------------------------------------
// Other private functions
//-----------------------------------------------------------------------------

void MMF::MediaObject::handlePrefinishMarkReached(qint32 time)
{
    emit tick(time);
}


QT_END_NAMESPACE

