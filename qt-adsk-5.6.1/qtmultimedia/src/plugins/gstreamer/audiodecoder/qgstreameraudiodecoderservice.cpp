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

#include <QtCore/qvariant.h>
#include <QtCore/qdebug.h>

#include "qgstreameraudiodecoderservice.h"
#include "qgstreameraudiodecodercontrol.h"
#include "qgstreameraudiodecodersession.h"

QT_BEGIN_NAMESPACE

QGstreamerAudioDecoderService::QGstreamerAudioDecoderService(QObject *parent)
    : QMediaService(parent)
{
    m_session = new QGstreamerAudioDecoderSession(this);
    m_control = new QGstreamerAudioDecoderControl(m_session, this);
}

QGstreamerAudioDecoderService::~QGstreamerAudioDecoderService()
{
}

QMediaControl *QGstreamerAudioDecoderService::requestControl(const char *name)
{
    if (qstrcmp(name, QAudioDecoderControl_iid) == 0)
        return m_control;

    return 0;
}

void QGstreamerAudioDecoderService::releaseControl(QMediaControl *control)
{
    Q_UNUSED(control);
}

QT_END_NAMESPACE
