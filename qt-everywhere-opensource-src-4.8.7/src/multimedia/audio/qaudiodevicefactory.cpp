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

#include <QtCore/qdebug.h>
#include <QtMultimedia/qaudioengine.h>
#include <QtMultimedia/qaudioengineplugin.h>
#include <private/qfactoryloader_p.h>
#include "qaudiodevicefactory_p.h"

#ifndef QT_NO_AUDIO_BACKEND
#if defined(Q_OS_WIN)
#include "qaudiodeviceinfo_win32_p.h"
#include "qaudiooutput_win32_p.h"
#include "qaudioinput_win32_p.h"
#elif defined(Q_OS_MAC)
#include "qaudiodeviceinfo_mac_p.h"
#include "qaudiooutput_mac_p.h"
#include "qaudioinput_mac_p.h"
#elif defined(HAS_ALSA)
#include "qaudiodeviceinfo_alsa_p.h"
#include "qaudiooutput_alsa_p.h"
#include "qaudioinput_alsa_p.h"
#elif defined(Q_OS_SYMBIAN)
#include "qaudiodeviceinfo_symbian_p.h"
#include "qaudiooutput_symbian_p.h"
#include "qaudioinput_symbian_p.h"
#endif
#endif

QT_BEGIN_NAMESPACE


#if !defined(QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
Q_GLOBAL_STATIC_WITH_ARGS(QFactoryLoader, loader,
        (QAudioEngineFactoryInterface_iid, QLatin1String("/audio"), Qt::CaseInsensitive))
#endif

class QNullDeviceInfo : public QAbstractAudioDeviceInfo
{
public:
    QAudioFormat preferredFormat() const { qWarning()<<"using null deviceinfo, none available"; return QAudioFormat(); }
    bool isFormatSupported(const QAudioFormat& ) const { return false; }
    QAudioFormat nearestFormat(const QAudioFormat& ) const { return QAudioFormat(); }
    QString deviceName() const { return QString(); }
    QStringList codecList() { return QStringList(); }
    QList<int> frequencyList()  { return QList<int>(); }
    QList<int> channelsList() { return QList<int>(); }
    QList<int> sampleSizeList() { return QList<int>(); }
    QList<QAudioFormat::Endian> byteOrderList() { return QList<QAudioFormat::Endian>(); }
    QList<QAudioFormat::SampleType> sampleTypeList() { return QList<QAudioFormat::SampleType>(); }
};

class QNullInputDevice : public QAbstractAudioInput
{
public:
    QIODevice* start(QIODevice* ) { qWarning()<<"using null input device, none available"; return 0; }
    void stop() {}
    void reset() {}
    void suspend() {}
    void resume() {}
    int bytesReady() const { return 0; }
    int periodSize() const { return 0; }
    void setBufferSize(int ) {}
    int bufferSize() const  { return 0; }
    void setNotifyInterval(int ) {}
    int notifyInterval() const { return 0; }
    qint64 processedUSecs() const { return 0; }
    qint64 elapsedUSecs() const { return 0; }
    QAudio::Error error() const { return QAudio::OpenError; }
    QAudio::State state() const { return QAudio::StoppedState; }
    QAudioFormat format() const { return QAudioFormat(); }
};

class QNullOutputDevice : public QAbstractAudioOutput
{
public:
    QIODevice* start(QIODevice* ) { qWarning()<<"using null output device, none available"; return 0; }
    void stop() {}
    void reset() {}
    void suspend() {}
    void resume() {}
    int bytesFree() const { return 0; }
    int periodSize() const { return 0; }
    void setBufferSize(int ) {}
    int bufferSize() const  { return 0; }
    void setNotifyInterval(int ) {}
    int notifyInterval() const { return 0; }
    qint64 processedUSecs() const { return 0; }
    qint64 elapsedUSecs() const { return 0; }
    QAudio::Error error() const { return QAudio::OpenError; }
    QAudio::State state() const { return QAudio::StoppedState; }
    QAudioFormat format() const { return QAudioFormat(); }
};

QList<QAudioDeviceInfo> QAudioDeviceFactory::availableDevices(QAudio::Mode mode)
{
    QList<QAudioDeviceInfo> devices;
#ifndef QT_NO_AUDIO_BACKEND
#if (defined(Q_OS_WIN) || defined(Q_OS_MAC) || defined(HAS_ALSA) || defined(Q_OS_SYMBIAN))
    foreach (const QByteArray &handle, QAudioDeviceInfoInternal::availableDevices(mode))
        devices << QAudioDeviceInfo(QLatin1String("builtin"), handle, mode);
#endif
#endif
#if !defined(QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
    QFactoryLoader* l = loader();

    foreach (QString const& key, l->keys()) {
        QAudioEngineFactoryInterface* plugin = qobject_cast<QAudioEngineFactoryInterface*>(l->instance(key));
        if (plugin) {
            foreach (QByteArray const& handle, plugin->availableDevices(mode))
                devices << QAudioDeviceInfo(key, handle, mode);
        }

        delete plugin;
    }
#endif
    return devices;
}

QAudioDeviceInfo QAudioDeviceFactory::defaultInputDevice()
{
#if !defined(QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
    QAudioEngineFactoryInterface* plugin = qobject_cast<QAudioEngineFactoryInterface*>(loader()->instance(QLatin1String("default")));

    if (plugin) {
        QList<QByteArray> list = plugin->availableDevices(QAudio::AudioInput);
        if (list.size() > 0)
            return QAudioDeviceInfo(QLatin1String("default"), list.at(0), QAudio::AudioInput);
    }
#endif

#ifndef QT_NO_AUDIO_BACKEND
#if (defined(Q_OS_WIN) || defined(Q_OS_MAC) || defined(HAS_ALSA) || defined(Q_OS_SYMBIAN))
    return QAudioDeviceInfo(QLatin1String("builtin"), QAudioDeviceInfoInternal::defaultInputDevice(), QAudio::AudioInput);
#endif
#endif
    return QAudioDeviceInfo();
}

QAudioDeviceInfo QAudioDeviceFactory::defaultOutputDevice()
{
#if !defined(QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
    QAudioEngineFactoryInterface* plugin = qobject_cast<QAudioEngineFactoryInterface*>(loader()->instance(QLatin1String("default")));

    if (plugin) {
        QList<QByteArray> list = plugin->availableDevices(QAudio::AudioOutput);
        if (list.size() > 0)
            return QAudioDeviceInfo(QLatin1String("default"), list.at(0), QAudio::AudioOutput);
    }
#endif

#ifndef QT_NO_AUDIO_BACKEND
#if (defined(Q_OS_WIN) || defined(Q_OS_MAC) || defined(HAS_ALSA) || defined(Q_OS_SYMBIAN))
    return QAudioDeviceInfo(QLatin1String("builtin"), QAudioDeviceInfoInternal::defaultOutputDevice(), QAudio::AudioOutput);
#endif
#endif
    return QAudioDeviceInfo();
}

QAbstractAudioDeviceInfo* QAudioDeviceFactory::audioDeviceInfo(const QString &realm, const QByteArray &handle, QAudio::Mode mode)
{
    QAbstractAudioDeviceInfo *rc = 0;

#ifndef QT_NO_AUDIO_BACKEND
#if (defined(Q_OS_WIN) || defined(Q_OS_MAC) || defined(HAS_ALSA) || defined(Q_OS_SYMBIAN))
    if (realm == QLatin1String("builtin"))
        return new QAudioDeviceInfoInternal(handle, mode);
#endif
#endif
#if !defined(QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
    QAudioEngineFactoryInterface* plugin =
        qobject_cast<QAudioEngineFactoryInterface*>(loader()->instance(realm));

    if (plugin)
        rc = plugin->createDeviceInfo(handle, mode);
#endif
    return rc == 0 ? new QNullDeviceInfo() : rc;
}

QAbstractAudioInput* QAudioDeviceFactory::createDefaultInputDevice(QAudioFormat const &format)
{
    return createInputDevice(defaultInputDevice(), format);
}

QAbstractAudioOutput* QAudioDeviceFactory::createDefaultOutputDevice(QAudioFormat const &format)
{
    return createOutputDevice(defaultOutputDevice(), format);
}

QAbstractAudioInput* QAudioDeviceFactory::createInputDevice(QAudioDeviceInfo const& deviceInfo, QAudioFormat const &format)
{
    if (deviceInfo.isNull())
        return new QNullInputDevice();
#ifndef QT_NO_AUDIO_BACKEND
#if (defined(Q_OS_WIN) || defined(Q_OS_MAC) || defined(HAS_ALSA) || defined(Q_OS_SYMBIAN))
    if (deviceInfo.realm() == QLatin1String("builtin"))
        return new QAudioInputPrivate(deviceInfo.handle(), format);
#endif
#endif
#if !defined(QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
    QAudioEngineFactoryInterface* plugin =
        qobject_cast<QAudioEngineFactoryInterface*>(loader()->instance(deviceInfo.realm()));

    if (plugin)
        return plugin->createInput(deviceInfo.handle(), format);
#endif
    return new QNullInputDevice();
}

QAbstractAudioOutput* QAudioDeviceFactory::createOutputDevice(QAudioDeviceInfo const& deviceInfo, QAudioFormat const &format)
{
    if (deviceInfo.isNull())
        return new QNullOutputDevice();
#ifndef QT_NO_AUDIO_BACKEND
#if (defined(Q_OS_WIN) || defined(Q_OS_MAC) || defined(HAS_ALSA) || defined(Q_OS_SYMBIAN))
    if (deviceInfo.realm() == QLatin1String("builtin"))
        return new QAudioOutputPrivate(deviceInfo.handle(), format);
#endif
#endif
#if !defined(QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
    QAudioEngineFactoryInterface* plugin =
        qobject_cast<QAudioEngineFactoryInterface*>(loader()->instance(deviceInfo.realm()));

    if (plugin)
        return plugin->createOutput(deviceInfo.handle(), format);
#endif
    return new QNullOutputDevice();
}

QT_END_NAMESPACE

