/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QDECLARATIVEAUDIOSAMPLE_P_H
#define QDECLARATIVEAUDIOSAMPLE_P_H

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

#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QSoundBuffer;
class QDeclarativeAudioEngine;

class QDeclarativeAudioSample : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(QUrl source READ source WRITE setSource)
    Q_PROPERTY(bool preloaded READ isPreloaded WRITE setPreloaded)
    Q_PROPERTY(bool streaming READ isStreaming WRITE setStreaming)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)

public:
    QDeclarativeAudioSample(QObject *parent = 0);
    ~QDeclarativeAudioSample();

    QString name() const;
    void setName(const QString& name);

    QUrl source() const;
    void setSource(const QUrl& url);

    bool isStreaming() const;
    void setStreaming(bool streaming);

    bool isPreloaded() const;
    void setPreloaded(bool preloaded);

    QDeclarativeAudioEngine *engine() const;
    void setEngine(QDeclarativeAudioEngine *);

    bool isLoaded() const;

    QSoundBuffer* soundBuffer() const;

    //called by QDeclarativeAudioEngine
    void init();

Q_SIGNALS:
    void loadedChanged();

public Q_SLOTS:
    void load();

private:
    Q_DISABLE_COPY(QDeclarativeAudioSample);
    QString m_name;
    QUrl m_url;
    bool m_streaming;
    bool m_preloaded;
    QSoundBuffer *m_soundBuffer;
    QDeclarativeAudioEngine *m_engine;
};

QT_END_NAMESPACE

#endif
