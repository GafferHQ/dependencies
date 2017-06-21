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

#include "qvideoencodersettingscontrol.h"
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

QT_BEGIN_NAMESPACE

/*!
    \class QVideoEncoderSettingsControl

    \inmodule QtMultimedia


    \ingroup multimedia_control

    \brief The QVideoEncoderSettingsControl class provides access to the settings
    of a media service that performs video encoding.

    If a QMediaService supports encoding video data it will implement
    QVideoEncoderSettingsControl.  This control provides information about the limits
    of restricted video encoder options and allows the selection of a set of
    video encoder settings as specified in a QVideoEncoderSettings object.

    The functionality provided by this control is exposed to application code
    through the QMediaRecorder class.

    The interface name of QVideoEncoderSettingsControl is \c org.qt-project.qt.videoencodersettingscontrol/5.0 as
    defined in QVideoEncoderSettingsControl_iid.

    \sa QMediaRecorder, QVideoEncoderSettings, QMediaService::requestControl()
*/

/*!
    \macro QVideoEncoderSettingsControl_iid

    \c org.qt-project.qt.videoencodersettingscontrol/5.0

    Defines the interface name of the QVideoEncoderSettingsControl class.

    \relates QVideoEncoderSettingsControl
*/

/*!
    Create a new video encoder settings control object with the given \a parent.
*/
QVideoEncoderSettingsControl::QVideoEncoderSettingsControl(QObject *parent)
    :QMediaControl(parent)
{
}

/*!
    Destroys a video encoder settings control.
*/
QVideoEncoderSettingsControl::~QVideoEncoderSettingsControl()
{
}

/*!
    \fn QVideoEncoderSettingsControl::supportedVideoCodecs() const

    Returns the list of supported video codecs.
*/

/*!
    \fn QVideoEncoderSettingsControl::videoCodecDescription(const QString &codec) const

    Returns a description of a video \a codec.
*/

/*!
    \fn QVideoEncoderSettingsControl::supportedResolutions(const QVideoEncoderSettings &settings = QVideoEncoderSettings(),
                                                   bool *continuous = 0) const

    Returns a list of supported resolutions.

    If non null video \a settings parameter is passed,
    the returned list is reduced to resolution supported with partial settings like
    \l {QVideoEncoderSettings::setCodec()}{video codec} or
    \l {QVideoEncoderSettings::setFrameRate()}{frame rate} applied.

    If the encoder supports arbitrary resolutions within the supported resolutions range,
    *\a continuous is set to true, otherwise *\a continuous is set to false.

    \sa QVideoEncoderSettings::resolution()
*/

/*!
    \fn QVideoEncoderSettingsControl::supportedFrameRates(const QVideoEncoderSettings &settings = QVideoEncoderSettings(),
                                                  bool *continuous = 0) const

    Returns a list of supported frame rates.

    If non null video \a settings parameter is passed,
    the returned list is reduced to frame rates supported with partial settings like
    \l {QVideoEncoderSettings::setCodec()}{video codec} or
    \l {QVideoEncoderSettings::setResolution()}{video resolution} applied.

    If the encoder supports arbitrary frame rates within the supported range,
    *\a continuous is set to true, otherwise *\a continuous is set to false.

    \sa QVideoEncoderSettings::frameRate()
*/

/*!
    \fn QVideoEncoderSettingsControl::videoSettings() const

    Returns the video encoder settings.

    The returned value may be different tha passed to QVideoEncoderSettingsControl::setVideoSettings()
    if the settings contains the default or undefined parameters.
    In this case if the undefined parameters are already resolved, they should be returned.
*/

/*!
    \fn QVideoEncoderSettingsControl::setVideoSettings(const QVideoEncoderSettings &settings)

    Sets the selected video encoder \a settings.
*/

#include "moc_qvideoencodersettingscontrol.cpp"
QT_END_NAMESPACE

