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

#include "playlistfileparser_p.h"
#include <qfileinfo.h>
#include <QtCore/QDebug>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include "qmediaobject_p.h"
#include <private/qobject_p.h>
#include "qmediametadata.h"

QT_BEGIN_NAMESPACE

namespace {
class ParserBase : public QObject
{
    Q_OBJECT
public:
    ParserBase(QObject *parent)
        : QObject(parent)
    {
    }

    virtual void parseLine(int lineIndex, const QString& line, const QUrl& root) = 0;

protected:
    QUrl expandToFullPath(const QUrl &root, const QString &line)
    {
        // On Linux, backslashes are not converted to forward slashes :/
        if (line.startsWith(QLatin1String("//")) || line.startsWith(QLatin1String("\\\\"))) {
            // Network share paths are not resolved
            return QUrl::fromLocalFile(line);
        }

        QUrl url(line);
        if (url.scheme().isEmpty()) {
            // Resolve it relative to root
            if (root.isLocalFile())
                return root.resolved(QUrl::fromLocalFile(line));
            else
                return root.resolved(url);
        } else if (url.scheme().length() == 1) {
            // Assume it's a drive letter for a Windows path
            url = QUrl::fromLocalFile(line);
        }

        return url;
    }

Q_SIGNALS:
    void newItem(const QVariant& content);
    void finished();
    void error(QPlaylistFileParser::ParserError err, const QString& errorMsg);
};

class M3UParser : public ParserBase
{
public:
    M3UParser(QObject *parent)
        : ParserBase(parent)
        , m_extendedFormat(false)
    {
    }


    /*
     *
    Extended M3U directives

    #EXTM3U - header - must be first line of file
    #EXTINF - extra info - length (seconds), title
    #EXTINF - extra info - length (seconds), artist '-' title

    Example

    #EXTM3U
    #EXTINF:123, Sample artist - Sample title
    C:\Documents and Settings\I\My Music\Sample.mp3
    #EXTINF:321,Example Artist - Example title
    C:\Documents and Settings\I\My Music\Greatest Hits\Example.ogg

     */
    void parseLine(int lineIndex, const QString& line, const QUrl& root)
    {
        if (line[0] == '#' ) {
            if (m_extendedFormat) {
                if (line.startsWith(QLatin1String("#EXTINF:"))) {
                    m_extraInfo.clear();
                    int artistStart = line.indexOf(QLatin1String(","), 8);
                    bool ok = false;
                    int length = line.midRef(8, artistStart < 8 ? -1 : artistStart - 8).trimmed().toInt(&ok);
                    if (ok && length > 0) {
                        //convert from second to milisecond
                        m_extraInfo[QMediaMetaData::Duration] = QVariant(length * 1000);
                    }
                    if (artistStart > 0) {
                        int titleStart = getSplitIndex(line, artistStart);
                        if (titleStart > artistStart) {
                            m_extraInfo[QMediaMetaData::Author] = line.midRef(artistStart + 1,
                                                             titleStart - artistStart - 1).trimmed().toString().
                                                             replace(QLatin1String("--"), QLatin1String("-"));
                            m_extraInfo[QMediaMetaData::Title] = line.midRef(titleStart + 1).trimmed().toString().
                                                   replace(QLatin1String("--"), QLatin1String("-"));
                        } else {
                            m_extraInfo[QMediaMetaData::Title] = line.midRef(artistStart + 1).trimmed().toString().
                                                   replace(QLatin1String("--"), QLatin1String("-"));
                        }
                    }
                }
            } else if (lineIndex == 0 && line.startsWith(QLatin1String("#EXTM3U"))) {
                m_extendedFormat = true;
            }
        } else {
            m_extraInfo[QLatin1String("url")] = expandToFullPath(root, line);
            emit newItem(QVariant(m_extraInfo));
            m_extraInfo.clear();
        }
    }

    int getSplitIndex(const QString& line, int startPos)
    {
        if (startPos < 0)
            startPos = 0;
        const QChar* buf = line.data();
        for (int i = startPos; i < line.length(); ++i) {
            if (buf[i] == '-') {
                if (i == line.length() - 1)
                    return i;
                ++i;
                if (buf[i] != '-')
                    return i - 1;
            }
        }
        return -1;
    }

private:
    bool            m_extendedFormat;
    QVariantMap     m_extraInfo;
};

class PLSParser : public ParserBase
{
    Q_OBJECT
public:
    PLSParser(QObject *parent)
        : ParserBase(parent)
    {
    }

/*
 *
The format is essentially that of an INI file structured as follows:

Header

    * [playlist] : This tag indicates that it is a Playlist File

Track Entry
Assuming track entry #X

    * FileX : Variable defining location of stream.
    * TitleX : Defines track title.
    * LengthX : Length in seconds of track. Value of -1 indicates indefinite.

Footer

    * NumberOfEntries : This variable indicates the number of tracks.
    * Version : Playlist version. Currently only a value of 2 is valid.

[playlist]

File1=Alternative\everclear - SMFTA.mp3

Title1=Everclear - So Much For The Afterglow

Length1=233

File2=http://www.site.com:8000/listen.pls

Title2=My Cool Stream

Length5=-1

NumberOfEntries=2

Version=2
*/
    void parseLine(int, const QString &line, const QUrl &root)
    {
        // We ignore everything but 'File' entries, since that's the only thing we care about.
        if (!line.startsWith(QLatin1String("File")))
            return;

        QString value = getValue(line);
        if (value.isEmpty())
            return;

        emit newItem(expandToFullPath(root, value));
    }

    QString getValue(const QString& line) {
        int start = line.indexOf('=');
        if (start < 0)
            return QString();
        return line.midRef(start + 1).trimmed().toString();
    }
};
}

/////////////////////////////////////////////////////////////////////////////////////////////////

class QPlaylistFileParserPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QPlaylistFileParser)
public:
    QPlaylistFileParserPrivate()
        : m_source(0)
        , m_scanIndex(0)
        , m_utf8(false)
        , m_lineIndex(-1)
        , m_type(QPlaylistFileParser::UNKNOWN)
        , m_currentParser(0)
    {
    }

    void _q_handleData();
    void _q_handleError();
    void _q_handleParserError(QPlaylistFileParser::ParserError err, const QString& errorMsg);
    void _q_handleParserFinished();

    QNetworkReply  *m_source;
    QByteArray      m_buffer;
    int             m_scanIndex;
    QUrl            m_root;
    bool            m_utf8;
    int             m_lineIndex;
    QPlaylistFileParser::FileType m_type;
    ParserBase     *m_currentParser;
    QNetworkAccessManager m_mgr;

private:
    void processLine(int startIndex, int length);
};

#define LINE_LIMIT  4096
#define READ_LIMIT  64

void QPlaylistFileParserPrivate::processLine(int startIndex, int length)
{
    Q_Q(QPlaylistFileParser);
    m_lineIndex++;

    if (!m_currentParser) {
        Q_ASSERT(!m_currentParser);

        QString mimeType = m_source->header(QNetworkRequest::ContentTypeHeader).toString();
        m_type = QPlaylistFileParser::findPlaylistType(m_root.toString(), mimeType, m_buffer.constData(), m_buffer.size());

        switch (m_type) {
        case QPlaylistFileParser::UNKNOWN:
            emit q->error(QPlaylistFileParser::FormatError,
                          QPlaylistFileParser::tr("%1 playlist type is unknown").arg(m_root.toString()));
            q->stop();
            return;
        case QPlaylistFileParser::M3U:
            m_currentParser = new M3UParser(q);
            break;
        case QPlaylistFileParser::M3U8:
            m_currentParser = new M3UParser(q);
            m_utf8 = true;
            break;
        case QPlaylistFileParser::PLS:
            m_currentParser = new PLSParser(q);
            break;
        }
        Q_ASSERT(m_currentParser);
        QObject::connect(m_currentParser, SIGNAL(newItem(QVariant)), q, SIGNAL(newItem(QVariant)));
        QObject::connect(m_currentParser, SIGNAL(finished()), q, SLOT(_q_handleParserFinished()));
        QObject::connect(m_currentParser, SIGNAL(error(QPlaylistFileParser::ParserError,QString)),
                         q, SLOT(_q_handleParserError(QPlaylistFileParser::ParserError,QString)));
    }

    QString line;

    if (m_utf8) {
        line = QString::fromUtf8(m_buffer.constData() + startIndex, length).trimmed();
    } else {
        line = QString::fromLatin1(m_buffer.constData() + startIndex, length).trimmed();
    }
    if (line.isEmpty())
        return;

    Q_ASSERT(m_currentParser);
    m_currentParser->parseLine(m_lineIndex, line, m_root);
}

void QPlaylistFileParserPrivate::_q_handleData()
{
    Q_Q(QPlaylistFileParser);
    while (m_source->bytesAvailable()) {
        int expectedBytes = qMin(READ_LIMIT, int(qMin(m_source->bytesAvailable(),
                                                      qint64(LINE_LIMIT - m_buffer.size()))));
        m_buffer.push_back(m_source->read(expectedBytes));
        int processedBytes = 0;
        while (m_scanIndex < m_buffer.length()) {
            char s = m_buffer[m_scanIndex];
            if (s == '\r' || s == '\n') {
                int l = m_scanIndex - processedBytes;
                if (l > 0)
                    processLine(processedBytes, l);
                processedBytes = m_scanIndex + 1;
                if (!m_source) {
                    //some error happened, so exit parsing
                    return;
                }
            }
            m_scanIndex++;
        }

        if (m_buffer.length() - processedBytes >= LINE_LIMIT) {
            qWarning() << "error parsing playlist["<< m_root << "] with line content >= 4096 bytes.";
            emit q->error(QPlaylistFileParser::FormatError, QPlaylistFileParser::tr("invalid line in playlist file"));
            q->stop();
            return;
        }

        if (m_source->isFinished() && !m_source->bytesAvailable()) {
            //last line
            processLine(processedBytes, -1);
            break;
        }

        Q_ASSERT(m_buffer.length() == m_scanIndex);
        if (processedBytes == 0)
            continue;

        int copyLength = m_buffer.length() - processedBytes;
        if (copyLength > 0) {
            Q_ASSERT(copyLength <= READ_LIMIT);
            m_buffer = m_buffer.right(copyLength);
        } else {
            m_buffer.clear();
        }
        m_scanIndex = 0;
    }

    if (m_source->isFinished()) {
        _q_handleParserFinished();
    }
}

void QPlaylistFileParserPrivate::_q_handleError()
{
    Q_Q(QPlaylistFileParser);
    emit q->error(QPlaylistFileParser::NetworkError, m_source->errorString());
    q->stop();
}

void QPlaylistFileParserPrivate::_q_handleParserError(QPlaylistFileParser::ParserError err, const QString& errorMsg)
{
    Q_Q(QPlaylistFileParser);
    emit q->error(err, errorMsg);
}

void QPlaylistFileParserPrivate::_q_handleParserFinished()
{
    Q_Q(QPlaylistFileParser);
    bool isParserValid = (m_currentParser != 0);
    if (!isParserValid)
        emit q->error(QPlaylistFileParser::FormatNotSupportedError, QPlaylistFileParser::tr("Empty file provided"));

    q->stop();

    if (isParserValid)
        emit q->finished();
}


QPlaylistFileParser::QPlaylistFileParser(QObject *parent)
    : QObject(*new QPlaylistFileParserPrivate, parent)
{

}

QPlaylistFileParser::FileType QPlaylistFileParser::findPlaylistType(const QString& uri, const QString& mime, const void *data, quint32 size)
{
    if (!data || !size)
        return UNKNOWN;

    FileType uriType = UNKNOWN;
    QString suffix = QFileInfo(uri).suffix().toLower();
    if (suffix == QLatin1String("m3u"))
        uriType = M3U;
    else if (suffix == QLatin1String("m3u8"))
        uriType = M3U8;
    else if (suffix == QLatin1String("pls"))
        uriType = PLS;

    FileType mimeType = UNKNOWN;
    if (mime == QLatin1String("text/uri-list") || mime == QLatin1String("audio/x-mpegurl") || mime == QLatin1String("audio/mpegurl"))
        mimeType = QPlaylistFileParser::M3U;
    else if (mime == QLatin1String("application/x-mpegURL") || mime == QLatin1String("application/vnd.apple.mpegurl"))
        mimeType = QPlaylistFileParser::M3U8;
    else if (mime == QLatin1String("audio/x-scpls"))
        mimeType = QPlaylistFileParser::PLS;

    FileType bufferType = UNKNOWN;
    if (size >= 7 && strncmp((const char*)data, "#EXTM3U", 7) == 0)
        bufferType = M3U;
    else if (size >= 10 && strncmp((const char*)data, "[playlist]", 10) == 0)
        bufferType = PLS;
    else {
        // Make sure every line is a text string
        quint32 n;
        for (n = 0; n < size; n++)
            if (!QChar(QLatin1Char(((const char*)data)[n])).isPrint())
                break;
        if (n == size)
            bufferType = M3U;
    }

    if (bufferType == M3U && (uriType == M3U8 || mimeType == M3U8))
        bufferType = M3U8;

    if (bufferType != UNKNOWN)
        return bufferType;

    if (uriType != UNKNOWN)
        return uriType;

    if (mimeType != UNKNOWN)
        return mimeType;

    return UNKNOWN;
}

void QPlaylistFileParser::start(const QNetworkRequest& request, bool utf8)
{
    Q_D(QPlaylistFileParser);
    stop();

    d->m_type = UNKNOWN;
    d->m_utf8 = utf8;
    d->m_root = request.url();

    if (d->m_root.isLocalFile() && !QFile::exists(d->m_root.toLocalFile())) {
        emit error(NetworkError, QString(tr("%1 does not exist")).arg(d->m_root.toString()));
        return;
    }

    d->m_source = d->m_mgr.get(request);

    connect(d->m_source, SIGNAL(readyRead()), this, SLOT(_q_handleData()));
    connect(d->m_source, SIGNAL(finished()), this, SLOT(_q_handleData()));
    connect(d->m_source, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(_q_handleError()));

    d->_q_handleData();
}

void QPlaylistFileParser::stop()
{
    Q_D(QPlaylistFileParser);
    if (d->m_currentParser) {
        disconnect(d->m_currentParser, SIGNAL(newItem(QVariant)), this, SIGNAL(newItem(QVariant)));
        disconnect(d->m_currentParser, SIGNAL(finished()), this, SLOT(_q_handleParserFinished()));
        disconnect(d->m_currentParser, SIGNAL(error(QPlaylistFileParser::ParserError,QString)),
                   this, SLOT(_q_handleParserError(QPlaylistFileParser::ParserError,QString)));
        d->m_currentParser->deleteLater();
        d->m_currentParser = 0;
    }

    d->m_buffer.clear();
    d->m_scanIndex = 0;
    d->m_lineIndex = -1;
    if (d->m_source) {
        disconnect(d->m_source, SIGNAL(readyRead()), this, SLOT(_q_handleData()));
        disconnect(d->m_source, SIGNAL(finished()), this, SLOT(_q_handleData()));
        disconnect(d->m_source, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(_q_handleError()));
        d->m_source->deleteLater();
        d->m_source = 0;
    }
}

#include "moc_playlistfileparser_p.cpp"
#include "playlistfileparser.moc"
QT_END_NAMESPACE
