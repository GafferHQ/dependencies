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
#include "bbcameravideoencodersettingscontrol.h"

#include "bbcamerasession.h"

QT_BEGIN_NAMESPACE

BbCameraVideoEncoderSettingsControl::BbCameraVideoEncoderSettingsControl(BbCameraSession *session, QObject *parent)
    : QVideoEncoderSettingsControl(parent)
    , m_session(session)
{
}

QList<QSize> BbCameraVideoEncoderSettingsControl::supportedResolutions(const QVideoEncoderSettings &settings, bool *continuous) const
{
    return m_session->supportedResolutions(settings, continuous);
}

QList<qreal> BbCameraVideoEncoderSettingsControl::supportedFrameRates(const QVideoEncoderSettings &settings, bool *continuous) const
{
    return m_session->supportedFrameRates(settings, continuous);
}

QStringList BbCameraVideoEncoderSettingsControl::supportedVideoCodecs() const
{
    return QStringList() << QLatin1String("none") << QLatin1String("avc1") << QLatin1String("h264");
}

QString BbCameraVideoEncoderSettingsControl::videoCodecDescription(const QString &codecName) const
{
    if (codecName == QLatin1String("none"))
        return tr("No compression");
    else if (codecName == QLatin1String("avc1"))
        return tr("AVC1 compression");
    else if (codecName == QLatin1String("h264"))
        return tr("H264 compression");

    return QString();
}

QVideoEncoderSettings BbCameraVideoEncoderSettingsControl::videoSettings() const
{
    return m_session->videoSettings();
}

void BbCameraVideoEncoderSettingsControl::setVideoSettings(const QVideoEncoderSettings &settings)
{
    m_session->setVideoSettings(settings);
}

QT_END_NAMESPACE
