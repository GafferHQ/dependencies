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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#ifndef QAUDIODEVICEINFO_SYMBIAN_P_H
#define QAUDIODEVICEINFO_SYMBIAN_P_H

#include <QtCore/QMap>
#include <QtMultimedia/qaudioengine.h>
#include <sounddevice.h>

QT_BEGIN_NAMESPACE

namespace SymbianAudio {
class DevSoundWrapper;
}

class QAudioDeviceInfoInternal
    :   public QAbstractAudioDeviceInfo
{
    Q_OBJECT

public:
    QAudioDeviceInfoInternal(QByteArray device, QAudio::Mode mode);
    ~QAudioDeviceInfoInternal();

    // QAbstractAudioDeviceInfo
    QAudioFormat preferredFormat() const;
    bool isFormatSupported(const QAudioFormat &format) const;
    QAudioFormat nearestFormat(const QAudioFormat &format) const;
    QString deviceName() const;
    QStringList codecList();
    QList<int> frequencyList();
    QList<int> channelsList();
    QList<int> sampleSizeList();
    QList<QAudioFormat::Endian> byteOrderList();
    QList<QAudioFormat::SampleType> sampleTypeList();
    static QByteArray defaultInputDevice();
    static QByteArray defaultOutputDevice();
    static QList<QByteArray> availableDevices(QAudio::Mode);

private slots:
    void devsoundInitializeComplete(int err);

private:
    void getSupportedFormats() const;

private:
    mutable bool m_initializing;
    int m_intializationResult;

    QString m_deviceName;
    QAudio::Mode m_mode;

    struct Capabilities
    {
        QList<int> m_frequencies;
        QList<int> m_channels;
        QList<int> m_sampleSizes;
        QList<QAudioFormat::Endian> m_byteOrders;
        QList<QAudioFormat::SampleType> m_sampleTypes;
    };

    // Mutable to allow lazy initialization when called from const-qualified
    // public functions (isFormatSupported, nearestFormat)
    mutable bool m_updated;
    mutable QMap<QString, Capabilities> m_capabilities;
    mutable Capabilities m_unionCapabilities;
};

QT_END_NAMESPACE

#endif
