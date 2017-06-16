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
#ifndef IOSAUDIODEVICEINFO_H
#define IOSAUDIODEVICEINFO_H

#include <qaudiosystem.h>

#if defined(Q_OS_OSX)
# include <CoreAudio/CoreAudio.h>
#endif

QT_BEGIN_NAMESPACE

class CoreAudioDeviceInfo : public QAbstractAudioDeviceInfo
{
    Q_OBJECT

public:
    CoreAudioDeviceInfo(const QByteArray &device, QAudio::Mode mode);
    ~CoreAudioDeviceInfo() {}

    QAudioFormat preferredFormat() const;
    bool isFormatSupported(const QAudioFormat &format) const;
    QString deviceName() const;
    QStringList supportedCodecs();
    QList<int> supportedSampleRates();
    QList<int> supportedChannelCounts();
    QList<int> supportedSampleSizes();
    QList<QAudioFormat::Endian> supportedByteOrders();
    QList<QAudioFormat::SampleType> supportedSampleTypes();

    static QByteArray defaultInputDevice();
    static QByteArray defaultOutputDevice();

    static QList<QByteArray> availableDevices(QAudio::Mode mode);

private:
#if defined(Q_OS_OSX)
    AudioDeviceID m_deviceId;
#endif

    QString m_device;
    QAudio::Mode m_mode;
};

QT_END_NAMESPACE

#endif
