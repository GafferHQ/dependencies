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


#ifndef QAUDIOINPUTALSA_H
#define QAUDIOINPUTALSA_H

#include <alsa/asoundlib.h>

#include <QtCore/qfile.h>
#include <QtCore/qdebug.h>
#include <QtCore/qtimer.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qdatetime.h>

#include <QtMultimedia/qaudio.h>
#include <QtMultimedia/qaudiodeviceinfo.h>
#include <QtMultimedia/qaudioengine.h>

QT_BEGIN_NAMESPACE

class InputPrivate;

class QAudioInputPrivate : public QAbstractAudioInput
{
    Q_OBJECT
public:
    QAudioInputPrivate(const QByteArray &device, const QAudioFormat& audioFormat);
    ~QAudioInputPrivate();

    qint64 read(char* data, qint64 len);

    QIODevice* start(QIODevice* device = 0);
    void stop();
    void reset();
    void suspend();
    void resume();
    int bytesReady() const;
    int periodSize() const;
    void setBufferSize(int value);
    int bufferSize() const;
    void setNotifyInterval(int milliSeconds);
    int notifyInterval() const;
    qint64 processedUSecs() const;
    qint64 elapsedUSecs() const;
    QAudio::Error error() const;
    QAudio::State state() const;
    QAudioFormat format() const;
    bool resuming;
    snd_pcm_t* handle;
    qint64 totalTimeValue;
    QIODevice* audioSource;
    QAudioFormat settings;
    QAudio::Error errorState;
    QAudio::State deviceState;

private slots:
    void userFeed();
    bool deviceReady();

private:
    int checkBytesReady();
    int xrun_recovery(int err);
    int setFormat();
    bool open();
    void close();
    void drain();

    QTimer* timer;
    QElapsedTimer timeStamp;
    QElapsedTimer clockStamp;
    qint64 elapsedTimeOffset;
    int intervalTime;
    char* audioBuffer;
    int bytesAvailable;
    QByteArray m_device;
    bool pullMode;
    int buffer_size;
    int period_size;
    unsigned int buffer_time;
    unsigned int period_time;
    snd_pcm_uframes_t buffer_frames;
    snd_pcm_uframes_t period_frames;
    snd_async_handler_t* ahandler;
    snd_pcm_access_t access;
    snd_pcm_format_t pcmformat;
    snd_timestamp_t* timestamp;
    snd_pcm_hw_params_t *hwparams;
};

class InputPrivate : public QIODevice
{
    Q_OBJECT
public:
    InputPrivate(QAudioInputPrivate* audio);
    ~InputPrivate();

    qint64 readData( char* data, qint64 len);
    qint64 writeData(const char* data, qint64 len);

    void trigger();
private:
    QAudioInputPrivate *audioDevice;
};

QT_END_NAMESPACE

#endif
