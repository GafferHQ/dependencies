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

#include "audiocapturesession.h"
#include "audiomediarecordercontrol.h"

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

AudioMediaRecorderControl::AudioMediaRecorderControl(QObject *parent)
    : QMediaRecorderControl(parent)
{
    m_session = qobject_cast<AudioCaptureSession*>(parent);
    connect(m_session, SIGNAL(positionChanged(qint64)),
            this, SIGNAL(durationChanged(qint64)));
    connect(m_session, SIGNAL(stateChanged(QMediaRecorder::State)),
            this, SIGNAL(stateChanged(QMediaRecorder::State)));
    connect(m_session, SIGNAL(statusChanged(QMediaRecorder::Status)),
            this, SIGNAL(statusChanged(QMediaRecorder::Status)));
    connect(m_session, SIGNAL(actualLocationChanged(QUrl)),
            this, SIGNAL(actualLocationChanged(QUrl)));
    connect(m_session, &AudioCaptureSession::volumeChanged,
            this, &AudioMediaRecorderControl::volumeChanged);
    connect(m_session, &AudioCaptureSession::mutedChanged,
            this, &AudioMediaRecorderControl::mutedChanged);
    connect(m_session, SIGNAL(error(int,QString)),
            this, SIGNAL(error(int,QString)));
}

AudioMediaRecorderControl::~AudioMediaRecorderControl()
{
}

QUrl AudioMediaRecorderControl::outputLocation() const
{
    return m_session->outputLocation();
}

bool AudioMediaRecorderControl::setOutputLocation(const QUrl& sink)
{
    return m_session->setOutputLocation(sink);
}

QMediaRecorder::State AudioMediaRecorderControl::state() const
{
    return m_session->state();
}

QMediaRecorder::Status AudioMediaRecorderControl::status() const
{
    return m_session->status();
}

qint64 AudioMediaRecorderControl::duration() const
{
    return m_session->position();
}

bool AudioMediaRecorderControl::isMuted() const
{
    return m_session->isMuted();
}

qreal AudioMediaRecorderControl::volume() const
{
    return m_session->volume();
}

void AudioMediaRecorderControl::setState(QMediaRecorder::State state)
{
    m_session->setState(state);
}

void AudioMediaRecorderControl::setMuted(bool muted)
{
    m_session->setMuted(muted);
}

void AudioMediaRecorderControl::setVolume(qreal volume)
{
    m_session->setVolume(volume);
}

QT_END_NAMESPACE
