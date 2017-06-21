/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwebenginedownloaditem.h"

#include "qwebenginedownloaditem_p.h"
#include "qwebengineprofile_p.h"

QT_BEGIN_NAMESPACE

using QtWebEngineCore::BrowserContextAdapterClient;

static inline QWebEngineDownloadItem::DownloadState toDownloadState(int state)
{
    switch (state) {
    case BrowserContextAdapterClient::DownloadInProgress:
        return QWebEngineDownloadItem::DownloadInProgress;
    case BrowserContextAdapterClient::DownloadCompleted:
        return QWebEngineDownloadItem::DownloadCompleted;
    case BrowserContextAdapterClient::DownloadCancelled:
        return QWebEngineDownloadItem::DownloadCancelled;
    case BrowserContextAdapterClient::DownloadInterrupted:
        return QWebEngineDownloadItem::DownloadInterrupted;
    default:
        Q_UNREACHABLE();
        return QWebEngineDownloadItem::DownloadCancelled;
    }
}

/*!
    \class QWebEngineDownloadItem
    \brief The QWebEngineDownloadItem class provides information about a download.

    \since 5.5

    \inmodule QtWebEngineWidgets

    QWebEngineDownloadItem stores the state of a download to be used to manage requested downloads.
*/

QWebEngineDownloadItemPrivate::QWebEngineDownloadItemPrivate(QWebEngineProfilePrivate *p, const QUrl &url)
    : profile(p)
    , downloadFinished(false)
    , downloadId(-1)
    , downloadState(QWebEngineDownloadItem::DownloadCancelled)
    , downloadUrl(url)
    , totalBytes(-1)
    , receivedBytes(0)
{
}

QWebEngineDownloadItemPrivate::~QWebEngineDownloadItemPrivate()
{
    profile->downloadDestroyed(downloadId);
}

void QWebEngineDownloadItemPrivate::update(const BrowserContextAdapterClient::DownloadItemInfo &info)
{
    Q_Q(QWebEngineDownloadItem);

    Q_ASSERT(downloadState != QWebEngineDownloadItem::DownloadRequested);

    if (toDownloadState(info.state) != downloadState) {
        downloadState = toDownloadState(info.state);
        Q_EMIT q->stateChanged(downloadState);
    }

    if (info.receivedBytes != receivedBytes || info.totalBytes != totalBytes) {
        receivedBytes = info.receivedBytes;
        totalBytes = info.totalBytes;
        Q_EMIT q->downloadProgress(receivedBytes, totalBytes);
    }

    downloadFinished = downloadState != QWebEngineDownloadItem::DownloadInProgress;

    if (downloadFinished)
        Q_EMIT q->finished();
}

/*!
    Accepts the current download request, which will start the download.

    \sa finished(), stateChanged()
*/

void QWebEngineDownloadItem::accept()
{
    Q_D(QWebEngineDownloadItem);

    if (d->downloadState != QWebEngineDownloadItem::DownloadRequested)
        return;

    d->downloadState = QWebEngineDownloadItem::DownloadInProgress;
    Q_EMIT stateChanged(d->downloadState);
}

/*!
    Cancels the current download.

    \sa finished(), stateChanged()
*/

void QWebEngineDownloadItem::cancel()
{
    Q_D(QWebEngineDownloadItem);

    QWebEngineDownloadItem::DownloadState state = d->downloadState;

    if (state == QWebEngineDownloadItem::DownloadCompleted
            || state == QWebEngineDownloadItem::DownloadCancelled)
        return;

    d->downloadState = QWebEngineDownloadItem::DownloadCancelled;
    Q_EMIT stateChanged(d->downloadState);

    // We directly cancel the download request if the user cancels
    // before it even started, so no need to notify the profile here.
    if (state == QWebEngineDownloadItem::DownloadInProgress)
        d->profile->cancelDownload(d->downloadId);
}

/*!
    Returns the download item's ID.
*/

quint32 QWebEngineDownloadItem::id() const
{
    Q_D(const QWebEngineDownloadItem);
    return d->downloadId;
}

/*!
    \fn QWebEngineDownloadItem::finished()

    This signal is emitted whenever the download finishes.

    \sa state(), isFinished()
*/

/*!
    \fn QWebEngineDownloadItem::stateChanged(DownloadState state)

    This signal is emitted whenever the download's \a state changes.

    \sa state(), QWebEngineDownloadItem::DownloadState
*/

/*!
    \fn QWebEngineDownloadItem::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)

    This signal is emitted whenever the download's \a bytesReceived or
    \a bytesTotal changes.

    \sa totalBytes(), receivedBytes()
*/

/*!
    \enum QWebEngineDownloadItem::DownloadState

    This enum describes the state of the download:

    \value DownloadRequested Download has been requested, but has not been accepted yet.
    \value DownloadInProgress Download is in progress.
    \value DownloadCompleted Download completed successfully.
    \value DownloadCancelled Download has been cancelled.
    \value DownloadInterrupted Download has been interrupted (by the server or because of lost
            connectivity).
*/

/*!
    Returns the download item's current state.

    \sa QWebEngineDownloadItem::DownloadState
*/

QWebEngineDownloadItem::DownloadState QWebEngineDownloadItem::state() const
{
    Q_D(const QWebEngineDownloadItem);
    return d->downloadState;
}

/*!
    Returns the the total amount of data to download in bytes.

    \c -1 means the size is unknown.
*/

qint64 QWebEngineDownloadItem::totalBytes() const
{
    Q_D(const QWebEngineDownloadItem);
    return d->totalBytes;
}

/*!
    Returns the amount of data in bytes that has been downloaded so far.

    \c -1 means the size is unknown.
*/

qint64 QWebEngineDownloadItem::receivedBytes() const
{
    Q_D(const QWebEngineDownloadItem);
    return d->receivedBytes;
}

/*!
    Returns the download's origin URL.
*/

QUrl QWebEngineDownloadItem::url() const
{
    Q_D(const QWebEngineDownloadItem);
    return d->downloadUrl;
}

/*!
    \since 5.6

    Returns the MIME type of the download.
*/

QString QWebEngineDownloadItem::mimeType() const
{
    Q_D(const QWebEngineDownloadItem);
    return d->mimeType;
}

/*!
    Returns the full target path where data is being downloaded to.

    The path includes the file name. The default suggested path is the standard download location
    and file name is deduced not to overwrite already existing files.
*/

QString QWebEngineDownloadItem::path() const
{
    Q_D(const QWebEngineDownloadItem);
    return d->downloadPath;
}

/*!
    Sets the full target path to download the file to.

    The \a path should also include the file name. The download path can only be set in response
    to the QWebEngineProfile::downloadRequested() signal before the download is accepted.
    Past that point, this function has no effect on the download item's state.
*/
void QWebEngineDownloadItem::setPath(QString path)
{
    Q_D(QWebEngineDownloadItem);
    if (d->downloadState != QWebEngineDownloadItem::DownloadRequested) {
        qWarning("Setting the download path is not allowed after the download has been accepted.");
        return;
    }

    d->downloadPath = path;
}

/*!
    Returns whether this download is finished (not in progress).

    \sa finished(), state(),
*/

bool QWebEngineDownloadItem::isFinished() const
{
    Q_D(const QWebEngineDownloadItem);
    return d->downloadFinished;
}

QWebEngineDownloadItem::QWebEngineDownloadItem(QWebEngineDownloadItemPrivate *p, QObject *parent)
    : QObject(parent)
    , d_ptr(p)
{
    p->q_ptr = this;
}

/*! \internal
*/
QWebEngineDownloadItem::~QWebEngineDownloadItem()
{
}

QT_END_NAMESPACE
