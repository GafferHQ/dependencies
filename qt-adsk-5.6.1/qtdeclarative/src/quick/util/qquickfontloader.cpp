/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickfontloader_p.h"

#include <qqmlcontext.h>
#include <qqmlengine.h>

#include <QStringList>
#include <QUrl>
#include <QDebug>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFontDatabase>

#include <private/qobject_p.h>
#include <qqmlinfo.h>
#include <qqmlfile.h>

#include <QtCore/QCoreApplication>

QT_BEGIN_NAMESPACE

#define FONTLOADER_MAXIMUM_REDIRECT_RECURSION 16

class QQuickFontObject : public QObject
{
Q_OBJECT

public:
    explicit QQuickFontObject(int _id = -1);

    void download(const QUrl &url, QNetworkAccessManager *manager);

Q_SIGNALS:
    void fontDownloaded(const QString&, QQuickFontLoader::Status);

private Q_SLOTS:
    void replyFinished();

public:
    int id;

private:
    QNetworkReply *reply;
    int redirectCount;

    Q_DISABLE_COPY(QQuickFontObject)
};

QQuickFontObject::QQuickFontObject(int _id)
    : QObject(0), id(_id), reply(0), redirectCount(0) {}


void QQuickFontObject::download(const QUrl &url, QNetworkAccessManager *manager)
{
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
    reply = manager->get(req);
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
}

void QQuickFontObject::replyFinished()
{
    if (reply) {
        redirectCount++;
        if (redirectCount < FONTLOADER_MAXIMUM_REDIRECT_RECURSION) {
            QVariant redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
            if (redirect.isValid()) {
                QUrl url = reply->url().resolved(redirect.toUrl());
                QNetworkAccessManager *manager = reply->manager();
                reply->deleteLater();
                reply = 0;
                download(url, manager);
                return;
            }
        }
        redirectCount = 0;

        if (!reply->error()) {
            id = QFontDatabase::addApplicationFontFromData(reply->readAll());
            if (id != -1)
                emit fontDownloaded(QFontDatabase::applicationFontFamilies(id).at(0), QQuickFontLoader::Ready);
            else
                emit fontDownloaded(QString(), QQuickFontLoader::Error);
        } else {
            qWarning("%s: Unable to load font '%s': %s", Q_FUNC_INFO,
                     qPrintable(reply->url().toString()), qPrintable(reply->errorString()));
            emit fontDownloaded(QString(), QQuickFontLoader::Error);
        }
        reply->deleteLater();
        reply = 0;
    }
}


class QQuickFontLoaderPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQuickFontLoader)

public:
    QQuickFontLoaderPrivate() : status(QQuickFontLoader::Null) {}

    QUrl url;
    QString name;
    QQuickFontLoader::Status status;
};

static void q_QFontLoaderFontsStaticReset();
static void q_QFontLoaderFontsAddReset()
{
    qAddPostRoutine(q_QFontLoaderFontsStaticReset);
}
class QFontLoaderFonts
{
public:
    QFontLoaderFonts()
    {
        qAddPostRoutine(q_QFontLoaderFontsStaticReset);
        qAddPreRoutine(q_QFontLoaderFontsAddReset);
    }

    ~QFontLoaderFonts()
    {
        qRemovePostRoutine(q_QFontLoaderFontsStaticReset);
        reset();
    }


    void reset()
    {
        QVector<QQuickFontObject *> deleted;
        QHash<QUrl, QQuickFontObject*>::iterator it;
        for (it = map.begin(); it != map.end(); ++it) {
            if (!deleted.contains(it.value())) {
                deleted.append(it.value());
                delete it.value();
            }
        }
        map.clear();
    }

    QHash<QUrl, QQuickFontObject *> map;
};
Q_GLOBAL_STATIC(QFontLoaderFonts, fontLoaderFonts);

static void q_QFontLoaderFontsStaticReset()
{
    fontLoaderFonts()->reset();
}

/*!
    \qmltype FontLoader
    \instantiates QQuickFontLoader
    \inqmlmodule QtQuick
    \ingroup qtquick-text-utility
    \brief Allows fonts to be loaded by name or URL

    The FontLoader type is used to load fonts by name or URL.

    The \l status indicates when the font has been loaded, which is useful
    for fonts loaded from remote sources.

    For example:
    \qml
    import QtQuick 2.0

    Column {
        FontLoader { id: fixedFont; name: "Courier" }
        FontLoader { id: webFont; source: "http://www.mysite.com/myfont.ttf" }

        Text { text: "Fixed-size font"; font.family: fixedFont.name }
        Text { text: "Fancy font"; font.family: webFont.name }
    }
    \endqml

    \sa {Qt Quick Examples - Text#Fonts}{Qt Quick Examples - Text Fonts}
*/
QQuickFontLoader::QQuickFontLoader(QObject *parent)
    : QObject(*(new QQuickFontLoaderPrivate), parent)
{
}

QQuickFontLoader::~QQuickFontLoader()
{
}

/*!
    \qmlproperty url QtQuick::FontLoader::source
    The url of the font to load.
*/
QUrl QQuickFontLoader::source() const
{
    Q_D(const QQuickFontLoader);
    return d->url;
}

void QQuickFontLoader::setSource(const QUrl &url)
{
    Q_D(QQuickFontLoader);
    if (url == d->url)
        return;
    d->url = url;
    emit sourceChanged();

    QString localFile = QQmlFile::urlToLocalFileOrQrc(d->url);
    if (!localFile.isEmpty()) {
        if (!fontLoaderFonts()->map.contains(d->url)) {
            int id = QFontDatabase::addApplicationFont(localFile);
            if (id != -1) {
                updateFontInfo(QFontDatabase::applicationFontFamilies(id).at(0), Ready);
                QQuickFontObject *fo = new QQuickFontObject(id);
                fontLoaderFonts()->map[d->url] = fo;
            } else {
                updateFontInfo(QString(), Error);
            }
        } else {
            updateFontInfo(QFontDatabase::applicationFontFamilies(fontLoaderFonts()->map[d->url]->id).at(0), Ready);
        }
    } else {
        if (!fontLoaderFonts()->map.contains(d->url)) {
            QQuickFontObject *fo = new QQuickFontObject;
            fontLoaderFonts()->map[d->url] = fo;
            fo->download(d->url, qmlEngine(this)->networkAccessManager());
            d->status = Loading;
            emit statusChanged();
            QObject::connect(fo, SIGNAL(fontDownloaded(QString,QQuickFontLoader::Status)),
                this, SLOT(updateFontInfo(QString,QQuickFontLoader::Status)));
        } else {
            QQuickFontObject *fo = fontLoaderFonts()->map[d->url];
            if (fo->id == -1) {
                d->status = Loading;
                emit statusChanged();
                QObject::connect(fo, SIGNAL(fontDownloaded(QString,QQuickFontLoader::Status)),
                    this, SLOT(updateFontInfo(QString,QQuickFontLoader::Status)));
            }
            else
                updateFontInfo(QFontDatabase::applicationFontFamilies(fo->id).at(0), Ready);
        }
    }
}

void QQuickFontLoader::updateFontInfo(const QString& name, QQuickFontLoader::Status status)
{
    Q_D(QQuickFontLoader);

    if (name != d->name) {
        d->name = name;
        emit nameChanged();
    }
    if (status != d->status) {
        if (status == Error)
            qmlInfo(this) << "Cannot load font: \"" << d->url.toString() << '"';
        d->status = status;
        emit statusChanged();
    }
}

/*!
    \qmlproperty string QtQuick::FontLoader::name

    This property holds the name of the font family.
    It is set automatically when a font is loaded using the \c url property.

    Use this to set the \c font.family property of a \c Text item.

    Example:
    \qml
    Item {
        width: 200; height: 50

        FontLoader {
            id: webFont
            source: "http://www.mysite.com/myfont.ttf"
        }
        Text {
            text: "Fancy font"
            font.family: webFont.name
        }
    }
    \endqml
*/
QString QQuickFontLoader::name() const
{
    Q_D(const QQuickFontLoader);
    return d->name;
}

void QQuickFontLoader::setName(const QString &name)
{
    Q_D(QQuickFontLoader);
    if (d->name == name)
        return;
    d->name = name;
    emit nameChanged();
    d->status = Ready;
    emit statusChanged();
}

/*!
    \qmlproperty enumeration QtQuick::FontLoader::status

    This property holds the status of font loading.  It can be one of:
    \list
    \li FontLoader.Null - no font has been set
    \li FontLoader.Ready - the font has been loaded
    \li FontLoader.Loading - the font is currently being loaded
    \li FontLoader.Error - an error occurred while loading the font
    \endlist

    Use this status to provide an update or respond to the status change in some way.
    For example, you could:

    \list
    \li Trigger a state change:
    \qml
        State { name: 'loaded'; when: loader.status == FontLoader.Ready }
    \endqml

    \li Implement an \c onStatusChanged signal handler:
    \qml
        FontLoader {
            id: loader
            onStatusChanged: if (loader.status == FontLoader.Ready) console.log('Loaded')
        }
    \endqml

    \li Bind to the status value:
    \qml
        Text { text: loader.status == FontLoader.Ready ? 'Loaded' : 'Not loaded' }
    \endqml
    \endlist
*/
QQuickFontLoader::Status QQuickFontLoader::status() const
{
    Q_D(const QQuickFontLoader);
    return d->status;
}

QT_END_NAMESPACE

#include <qquickfontloader.moc>
