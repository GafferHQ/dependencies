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

#include "avfcameradebug.h"
#include "avfcameracontrol.h"
#include "avfcamerasession.h"
#include "avfcameraservice.h"

QT_USE_NAMESPACE

AVFCameraControl::AVFCameraControl(AVFCameraService *service, QObject *parent)
   : QCameraControl(parent)
   , m_session(service->session())
   , m_state(QCamera::UnloadedState)
   , m_lastStatus(QCamera::UnloadedStatus)
   , m_captureMode(QCamera::CaptureStillImage)
{
    Q_UNUSED(service);
    connect(m_session, SIGNAL(stateChanged(QCamera::State)), SLOT(updateStatus()));
}

AVFCameraControl::~AVFCameraControl()
{
}

QCamera::State AVFCameraControl::state() const
{
    return m_state;
}

void AVFCameraControl::setState(QCamera::State state)
{
    if (m_state == state)
        return;
    m_state = state;
    m_session->setState(state);

    Q_EMIT stateChanged(m_state);
    updateStatus();
}

QCamera::Status AVFCameraControl::status() const
{
    static QCamera::Status statusTable[3][3] = {
        { QCamera::UnloadedStatus, QCamera::UnloadingStatus, QCamera::StoppingStatus }, //Unloaded state
        { QCamera::LoadingStatus,  QCamera::LoadedStatus,    QCamera::StoppingStatus }, //Loaded state
        { QCamera::LoadingStatus,  QCamera::StartingStatus,  QCamera::ActiveStatus } //ActiveState
    };

    return statusTable[m_state][m_session->state()];
}

void AVFCameraControl::updateStatus()
{
    QCamera::Status newStatus = status();

    if (m_lastStatus != newStatus) {
        qDebugCamera() << "Camera status changed: " << m_lastStatus << " -> " << newStatus;
        m_lastStatus = newStatus;
        Q_EMIT statusChanged(m_lastStatus);
    }
}

QCamera::CaptureModes AVFCameraControl::captureMode() const
{
    return m_captureMode;
}

void AVFCameraControl::setCaptureMode(QCamera::CaptureModes mode)
{
    if (m_captureMode == mode)
        return;

    if (!isCaptureModeSupported(mode)) {
        Q_EMIT error(QCamera::NotSupportedFeatureError, tr("Requested capture mode is not supported"));
        return;
    }

    m_captureMode = mode;
    Q_EMIT captureModeChanged(mode);
}

bool AVFCameraControl::isCaptureModeSupported(QCamera::CaptureModes mode) const
{
    //all the capture modes are supported, including QCamera::CaptureStillImage | QCamera::CaptureVideo
    return (mode & (QCamera::CaptureStillImage | QCamera::CaptureVideo)) == mode;
}

bool AVFCameraControl::canChangeProperty(QCameraControl::PropertyChangeType changeType, QCamera::Status status) const
{
    Q_UNUSED(changeType);
    Q_UNUSED(status);

    return true;
}

#include "moc_avfcameracontrol.cpp"
