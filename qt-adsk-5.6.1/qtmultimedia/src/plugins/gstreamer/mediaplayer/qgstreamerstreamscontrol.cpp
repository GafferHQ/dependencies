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

#include "qgstreamerstreamscontrol.h"
#include "qgstreamerplayersession.h"

QGstreamerStreamsControl::QGstreamerStreamsControl(QGstreamerPlayerSession *session, QObject *parent)
    :QMediaStreamsControl(parent), m_session(session)
{
    connect(m_session, SIGNAL(streamsChanged()), SIGNAL(streamsChanged()));
}

QGstreamerStreamsControl::~QGstreamerStreamsControl()
{
}

int QGstreamerStreamsControl::streamCount()
{
    return m_session->streamCount();
}

QMediaStreamsControl::StreamType QGstreamerStreamsControl::streamType(int streamNumber)
{
    return m_session->streamType(streamNumber);
}

QVariant QGstreamerStreamsControl::metaData(int streamNumber, const QString &key)
{
    return m_session->streamProperties(streamNumber).value(key);
}

bool QGstreamerStreamsControl::isActive(int streamNumber)
{
    return streamNumber != -1 && streamNumber == m_session->activeStream(streamType(streamNumber));
}

void QGstreamerStreamsControl::setActive(int streamNumber, bool state)
{
    QMediaStreamsControl::StreamType type = m_session->streamType(streamNumber);
    if (type == QMediaStreamsControl::UnknownStream)
        return;

    if (state)
        m_session->setActiveStream(type, streamNumber);
    else {
        //only one active stream of certain type is supported
        if (m_session->activeStream(type) == streamNumber)
            m_session->setActiveStream(type, -1);
    }
}

