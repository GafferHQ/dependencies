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

#ifndef QAUDIOENGINE_H
#define QAUDIOENGINE_H

#include <QtCore/qglobal.h>
#include <QtMultimedia/qaudio.h>
#include <QtMultimedia/qaudioformat.h>
#include <QtMultimedia/qaudiodeviceinfo.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Multimedia)

class Q_MULTIMEDIA_EXPORT QAbstractAudioDeviceInfo : public QObject
{
    Q_OBJECT

public:
    virtual QAudioFormat preferredFormat() const = 0;
    virtual bool isFormatSupported(const QAudioFormat &format) const = 0;
    virtual QAudioFormat nearestFormat(const QAudioFormat &format) const = 0;
    virtual QString deviceName() const = 0;
    virtual QStringList codecList() = 0;
    virtual QList<int> frequencyList() = 0;
    virtual QList<int> channelsList() = 0;
    virtual QList<int> sampleSizeList() = 0;
    virtual QList<QAudioFormat::Endian> byteOrderList() = 0;
    virtual QList<QAudioFormat::SampleType> sampleTypeList() = 0;
};

class Q_MULTIMEDIA_EXPORT QAbstractAudioOutput : public QObject
{
    Q_OBJECT

public:
    virtual QIODevice* start(QIODevice* device) = 0;
    virtual void stop() = 0;
    virtual void reset() = 0;
    virtual void suspend() = 0;
    virtual void resume() = 0;
    virtual int bytesFree() const = 0;
    virtual int periodSize() const = 0;
    virtual void setBufferSize(int value) = 0;
    virtual int bufferSize() const = 0;
    virtual void setNotifyInterval(int milliSeconds) = 0;
    virtual int notifyInterval() const = 0;
    virtual qint64 processedUSecs() const = 0;
    virtual qint64 elapsedUSecs() const = 0;
    virtual QAudio::Error error() const = 0;
    virtual QAudio::State state() const = 0;
    virtual QAudioFormat format() const = 0;

Q_SIGNALS:
    void stateChanged(QAudio::State);
    void notify();
};

class Q_MULTIMEDIA_EXPORT QAbstractAudioInput : public QObject
{
    Q_OBJECT

public:
    virtual QIODevice* start(QIODevice* device) = 0;
    virtual void stop() = 0;
    virtual void reset() = 0;
    virtual void suspend()  = 0;
    virtual void resume() = 0;
    virtual int bytesReady() const = 0;
    virtual int periodSize() const = 0;
    virtual void setBufferSize(int value) = 0;
    virtual int bufferSize() const = 0;
    virtual void setNotifyInterval(int milliSeconds) = 0;
    virtual int notifyInterval() const = 0;
    virtual qint64 processedUSecs() const = 0;
    virtual qint64 elapsedUSecs() const = 0;
    virtual QAudio::Error error() const = 0;
    virtual QAudio::State state() const = 0;
    virtual QAudioFormat format() const = 0;

Q_SIGNALS:
    void stateChanged(QAudio::State);
    void notify();
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QAUDIOENGINE_H
