/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
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
#include "bbcameracontrol.h"

#include "bbcamerasession.h"

QT_BEGIN_NAMESPACE

BbCameraControl::BbCameraControl(BbCameraSession *session, QObject *parent)
    : QCameraControl(parent)
    , m_session(session)
{
    connect(m_session, SIGNAL(statusChanged(QCamera::Status)), this, SIGNAL(statusChanged(QCamera::Status)));
    connect(m_session, SIGNAL(stateChanged(QCamera::State)), this, SIGNAL(stateChanged(QCamera::State)));
    connect(m_session, SIGNAL(error(int,QString)), this, SIGNAL(error(int,QString)));
    connect(m_session, SIGNAL(captureModeChanged(QCamera::CaptureModes)), this, SIGNAL(captureModeChanged(QCamera::CaptureModes)));
}

QCamera::State BbCameraControl::state() const
{
    return m_session->state();
}

void BbCameraControl::setState(QCamera::State state)
{
    m_session->setState(state);
}

QCamera::CaptureModes BbCameraControl::captureMode() const
{
    return m_session->captureMode();
}

void BbCameraControl::setCaptureMode(QCamera::CaptureModes mode)
{
    m_session->setCaptureMode(mode);
}

QCamera::Status BbCameraControl::status() const
{
    return m_session->status();
}

bool BbCameraControl::isCaptureModeSupported(QCamera::CaptureModes mode) const
{
    return m_session->isCaptureModeSupported(mode);
}

bool BbCameraControl::canChangeProperty(PropertyChangeType /* changeType */, QCamera::Status /* status */) const
{
    return false;
}

QT_END_NAMESPACE
