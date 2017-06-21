/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "avfmediaplayerservice.h"
#include "avfmediaplayersession.h"
#include "avfmediaplayercontrol.h"
#include "avfmediaplayermetadatacontrol.h"
#include "avfvideooutput.h"
#include "avfvideorenderercontrol.h"
#ifndef QT_NO_WIDGETS
# include "avfvideowidgetcontrol.h"
#endif
#include "avfvideowindowcontrol.h"

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_8, __IPHONE_6_0)
#import <AVFoundation/AVFoundation.h>
#endif

QT_USE_NAMESPACE

AVFMediaPlayerService::AVFMediaPlayerService(QObject *parent)
    : QMediaService(parent)
    , m_videoOutput(0)
    , m_enableRenderControl(true)
{
    m_session = new AVFMediaPlayerSession(this);
    m_control = new AVFMediaPlayerControl(this);
    m_control->setSession(m_session);
    m_playerMetaDataControl = new AVFMediaPlayerMetaDataControl(m_session, this);

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_8, __IPHONE_6_0)
    // AVPlayerItemVideoOutput is available in SDK
    #if QT_MAC_DEPLOYMENT_TARGET_BELOW(__MAC_10_8, __IPHONE_6_0)
    // might not be available at runtime
        #if defined(Q_OS_IOS)
            m_enableRenderControl = [AVPlayerItemVideoOutput class] != 0;
        #endif
    #endif
#endif

    connect(m_control, SIGNAL(mediaChanged(QMediaContent)), m_playerMetaDataControl, SLOT(updateTags()));
}

AVFMediaPlayerService::~AVFMediaPlayerService()
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO;
#endif
    delete m_session;
}

QMediaControl *AVFMediaPlayerService::requestControl(const char *name)
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO << name;
#endif

    if (qstrcmp(name, QMediaPlayerControl_iid) == 0)
        return m_control;

    if (qstrcmp(name, QMetaDataReaderControl_iid) == 0)
        return m_playerMetaDataControl;


    if (m_enableRenderControl && (qstrcmp(name, QVideoRendererControl_iid) == 0)) {
        if (!m_videoOutput)
            m_videoOutput = new AVFVideoRendererControl(this);

        m_session->setVideoOutput(qobject_cast<AVFVideoOutput*>(m_videoOutput));
        return m_videoOutput;
    }

#ifndef QT_NO_WIDGETS
    if (qstrcmp(name, QVideoWidgetControl_iid) == 0) {
        if (!m_videoOutput)
            m_videoOutput = new AVFVideoWidgetControl(this);

        m_session->setVideoOutput(qobject_cast<AVFVideoOutput*>(m_videoOutput));
        return m_videoOutput;
    }
#endif
    if (qstrcmp(name, QVideoWindowControl_iid) == 0) {
        if (!m_videoOutput)
            m_videoOutput = new AVFVideoWindowControl(this);

        m_session->setVideoOutput(qobject_cast<AVFVideoOutput*>(m_videoOutput));
        return m_videoOutput;
    }
    return 0;
}

void AVFMediaPlayerService::releaseControl(QMediaControl *control)
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO << control;
#endif
    if (m_videoOutput == control) {
        AVFVideoRendererControl *renderControl = qobject_cast<AVFVideoRendererControl*>(m_videoOutput);

        if (renderControl)
            renderControl->setSurface(0);

        m_videoOutput = 0;
        m_session->setVideoOutput(0);

        delete control;
    }
}
