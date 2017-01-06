/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtMultimedia module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qaudiodevicefactory_p.h"
#include <QtMultimedia/qaudioengine.h>
#include <QtMultimedia/qaudiodeviceinfo.h>

#include <QtCore/qmap.h>

QT_BEGIN_NAMESPACE

class QAudioDeviceInfoPrivate : public QSharedData
{
public:
    QAudioDeviceInfoPrivate():info(0) {}
    QAudioDeviceInfoPrivate(const QString &r, const QByteArray &h, QAudio::Mode m):
        realm(r), handle(h), mode(m)
    {
        info = QAudioDeviceFactory::audioDeviceInfo(realm, handle, mode);
    }

    QAudioDeviceInfoPrivate(const QAudioDeviceInfoPrivate &other):
        QSharedData(other),
        realm(other.realm), handle(other.handle), mode(other.mode)
    {
        info = QAudioDeviceFactory::audioDeviceInfo(realm, handle, mode);
    }

    QAudioDeviceInfoPrivate& operator=(const QAudioDeviceInfoPrivate &other)
    {
        delete info;

        realm = other.realm;
        handle = other.handle;
        mode = other.mode;
        info = QAudioDeviceFactory::audioDeviceInfo(realm, handle, mode);
        return *this;
    }

    ~QAudioDeviceInfoPrivate()
    {
        delete info;
    }

    QString     realm;
    QByteArray  handle;
    QAudio::Mode mode;
    QAbstractAudioDeviceInfo*   info;
};


/*!
    \class QAudioDeviceInfo
    \brief The QAudioDeviceInfo class provides an interface to query audio devices and their functionality.
    \inmodule QtMultimedia
    \ingroup multimedia

    \since 4.6

    QAudioDeviceInfo lets you query for audio devices--such as sound
    cards and USB headsets--that are currently available on the system.
    The audio devices available are dependent on the platform or audio plugins installed.

    You can also query each device for the formats it supports. A
    format in this context is a set consisting of a specific byte
    order, channel, codec, frequency, sample rate, and sample type.  A
    format is represented by the QAudioFormat class.

    The values supported by the the device for each of these
    parameters can be fetched with
    supportedByteOrders(), supportedChannelCounts(), supportedCodecs(),
    supportedSampleRates(), supportedSampleSizes(), and
    supportedSampleTypes(). The combinations supported are dependent on the platform,
    audio plugins installed and the audio device capabilities. If you need a specific format, you can check if
    the device supports it with isFormatSupported(), or fetch a
    supported format that is as close as possible to the format with
    nearestFormat(). For instance:

    \snippet doc/src/snippets/audio/main.cpp 6
    \dots 8
    \snippet doc/src/snippets/audio/main.cpp 7

    A QAudioDeviceInfo is used by Qt to construct
    classes that communicate with the device--such as
    QAudioInput, and QAudioOutput. The static
    functions defaultInputDevice(), defaultOutputDevice(), and
    availableDevices() let you get a list of all available
    devices. Devices are fetch according to the value of mode
    this is specified by the QAudio::Mode enum.
    The QAudioDeviceInfo returned are only valid for the QAudio::Mode.

    For instance:

    \code
    foreach(const QAudioDeviceInfo &deviceInfo, QAudioDeviceInfo::availableDevices(QAudio::AudioOutput))
        qDebug() << "Device name: " << deviceInfo.deviceName();
    \endcode

    In this code sample, we loop through all devices that are able to output
    sound, i.e., play an audio stream in a supported format. For each device we
    find, we simply print the deviceName().

    \sa QAudioOutput, QAudioInput
*/

/*!
    Constructs an empty QAudioDeviceInfo object.
*/

QAudioDeviceInfo::QAudioDeviceInfo():
    d(new QAudioDeviceInfoPrivate)
{
}

/*!
    Constructs a copy of \a other.
*/

QAudioDeviceInfo::QAudioDeviceInfo(const QAudioDeviceInfo& other):
    d(other.d)
{
}

/*!
    Destroy this audio device info.
*/

QAudioDeviceInfo::~QAudioDeviceInfo()
{
}

/*!
    Sets the QAudioDeviceInfo object to be equal to \a other.
*/

QAudioDeviceInfo& QAudioDeviceInfo::operator=(const QAudioDeviceInfo &other)
{
    d = other.d;
    return *this;
}

/*!
    Returns whether this QAudioDeviceInfo object holds a device definition.
*/

bool QAudioDeviceInfo::isNull() const
{
    return d->info == 0;
}

/*!
    Returns human readable name of audio device.

    Device names vary depending on platform/audio plugin being used.

    They are a unique string identifiers for the audio device.

    eg. default, Intel, U0x46d0x9a4
*/

QString QAudioDeviceInfo::deviceName() const
{
    return isNull() ? QString() : d->info->deviceName();
}

/*!
    Returns true if \a settings are supported by the audio device of this QAudioDeviceInfo.
*/

bool QAudioDeviceInfo::isFormatSupported(const QAudioFormat &settings) const
{
    return isNull() ? false : d->info->isFormatSupported(settings);
}

/*!
    Returns QAudioFormat of default settings.

    These settings are provided by the platform/audio plugin being used.

    They also are dependent on the QAudio::Mode being used.

    A typical audio system would provide something like:
    \list
    \o Input settings: 8000Hz mono 8 bit.
    \o Output settings: 44100Hz stereo 16 bit little endian.
    \endlist
*/

QAudioFormat QAudioDeviceInfo::preferredFormat() const
{
    return isNull() ? QAudioFormat() : d->info->preferredFormat();
}

/*!
    Returns closest QAudioFormat to \a settings that system audio supports.

    These settings are provided by the platform/audio plugin being used.

    They also are dependent on the QAudio::Mode being used.
*/

QAudioFormat QAudioDeviceInfo::nearestFormat(const QAudioFormat &settings) const
{
    if (isFormatSupported(settings))
        return settings;

    QAudioFormat nearest = settings;

    nearest.setCodec(QLatin1String("audio/pcm"));

    if (nearest.sampleType() == QAudioFormat::Unknown) {
        QAudioFormat preferred = preferredFormat();
        nearest.setSampleType(preferred.sampleType());
    }

    QMap<int,int> testFrequencies;
    QList<int> frequenciesAvailable = supportedFrequencies();
    QMap<int,int> testSampleSizes;
    QList<int> sampleSizesAvailable = supportedSampleSizes();

    // Get sorted sampleSizes (equal to and ascending values only)
    if (sampleSizesAvailable.contains(settings.sampleSize()))
        testSampleSizes.insert(0,settings.sampleSize());
    sampleSizesAvailable.removeAll(settings.sampleSize());
    foreach (int size, sampleSizesAvailable) {
        int larger  = (size > settings.sampleSize()) ? size : settings.sampleSize();
        int smaller = (size > settings.sampleSize()) ? settings.sampleSize() : size;
        if (size >= settings.sampleSize()) {
            int diff = larger - smaller;
            testSampleSizes.insert(diff, size);
        }
    }

    // Get sorted frequencies (equal to and ascending values only)
    if (frequenciesAvailable.contains(settings.frequency()))
        testFrequencies.insert(0,settings.frequency());
    frequenciesAvailable.removeAll(settings.frequency());
    foreach (int frequency, frequenciesAvailable) {
        int larger  = (frequency > settings.frequency()) ? frequency : settings.frequency();
        int smaller = (frequency > settings.frequency()) ? settings.frequency() : frequency;
        if (frequency >= settings.frequency()) {
            int diff = larger - smaller;
            testFrequencies.insert(diff, frequency);
        }
    }

    // Try to find nearest
    // Check ascending frequencies, ascending sampleSizes
    QMapIterator<int, int> sz(testSampleSizes);
    while (sz.hasNext()) {
        sz.next();
        nearest.setSampleSize(sz.value());
        QMapIterator<int, int> i(testFrequencies);
        while (i.hasNext()) {
            i.next();
            nearest.setFrequency(i.value());
            if (isFormatSupported(nearest))
                return nearest;
        }
    }

    //Fallback
    return preferredFormat();
}

/*!
    Returns a list of supported codecs.

    All platform and plugin implementations should provide support for:

    "audio/pcm" - Linear PCM

    For writing plugins to support additional codecs refer to:

    http://www.iana.org/assignments/media-types/audio/
*/

QStringList QAudioDeviceInfo::supportedCodecs() const
{
    return isNull() ? QStringList() : d->info->codecList();
}

/*!
    Returns a list of supported sample rates.

    \since 4.7
*/

QList<int> QAudioDeviceInfo::supportedSampleRates() const
{
    return supportedFrequencies();
}

/*!
    \obsolete

    Use supportedSampleRates() instead.
*/

QList<int> QAudioDeviceInfo::supportedFrequencies() const
{
    return isNull() ? QList<int>() : d->info->frequencyList();
}

/*!
    Returns a list of supported channel counts.

    \since 4.7
*/

QList<int> QAudioDeviceInfo::supportedChannelCounts() const
{
    return supportedChannels();
}

/*!
    \obsolete

    Use supportedChannelCount() instead.
*/

QList<int> QAudioDeviceInfo::supportedChannels() const
{
    return isNull() ? QList<int>() : d->info->channelsList();
}

/*!
    Returns a list of supported sample sizes.
*/

QList<int> QAudioDeviceInfo::supportedSampleSizes() const
{
    return isNull() ? QList<int>() : d->info->sampleSizeList();
}

/*!
    Returns a list of supported byte orders.
*/

QList<QAudioFormat::Endian> QAudioDeviceInfo::supportedByteOrders() const
{
    return isNull() ? QList<QAudioFormat::Endian>() : d->info->byteOrderList();
}

/*!
    Returns a list of supported sample types.
*/

QList<QAudioFormat::SampleType> QAudioDeviceInfo::supportedSampleTypes() const
{
    return isNull() ? QList<QAudioFormat::SampleType>() : d->info->sampleTypeList();
}

/*!
    Returns the name of the default input audio device.
    All platform and audio plugin implementations provide a default audio device to use.
*/

QAudioDeviceInfo QAudioDeviceInfo::defaultInputDevice()
{
    return QAudioDeviceFactory::defaultInputDevice();
}

/*!
    Returns the name of the default output audio device.
    All platform and audio plugin implementations provide a default audio device to use.
*/

QAudioDeviceInfo QAudioDeviceInfo::defaultOutputDevice()
{
    return QAudioDeviceFactory::defaultOutputDevice();
}

/*!
    Returns a list of audio devices that support \a mode.
*/

QList<QAudioDeviceInfo> QAudioDeviceInfo::availableDevices(QAudio::Mode mode)
{
    return QAudioDeviceFactory::availableDevices(mode);
}


/*!
    \internal
*/

QAudioDeviceInfo::QAudioDeviceInfo(const QString &realm, const QByteArray &handle, QAudio::Mode mode):
    d(new QAudioDeviceInfoPrivate(realm, handle, mode))
{
}

/*!
    \internal
*/

QString QAudioDeviceInfo::realm() const
{
    return d->realm;
}

/*!
    \internal
*/

QByteArray QAudioDeviceInfo::handle() const
{
    return d->handle;
}


/*!
    \internal
*/

QAudio::Mode QAudioDeviceInfo::mode() const
{
    return d->mode;
}

QT_END_NAMESPACE

